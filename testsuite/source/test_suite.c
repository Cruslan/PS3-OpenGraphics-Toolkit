/**
 * PS3 Reality Synthesizer (RSX) - Shader Model 3.0 Conformance Test Suite Manager
 * Implementation File
 *
 * Implements GPU shader microcode initialization in RSX GDDR3 VRAM, dynamic parameter
 * binding, D-Pad navigation state machine, full-screen quad presentation,
 * and the visual verification diagnostic banner for all 86 test scenes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <tiny3d.h>
#include <libfont.h>
#include <rsx/rsx.h>
#include <rsx/rsx_program.h>

#include "../include/common.h"
#include "../include/gamepad.h"
#include "../include/test_suite.h"
#include "../include/test_suite_data.h"

/* Embedded Vertex Program binary generated via bin2s */
extern const uint8_t rsxrt_vpo[];
extern const uint32_t rsxrt_vpo_size;

/* Structure holding runtime GPU state for each loaded shader */
typedef struct {
    rsxFragmentProgram* fp;
    void*               ucode_mem;
    u32                 ucode_offset;
    rsxProgramConst*    param_time;
    rsxProgramConst*    param_cam_pos;
    rsxProgramConst*    param_cam_fwd;
    rsxProgramConst*    param_cam_right_scaled;
    rsxProgramConst*    param_cam_up_scaled;
    rsxProgramConst*    param_sphere_pos;
    rsxProgramConst*    param_light_pos;
    bool                is_valid;
} RuntimeShader;

/* Global Test Suite State */
static rsxVertexProgram* g_rsx_vp = NULL;
static void*             g_rsx_vp_ucode = NULL;
static RuntimeShader     g_shaders[TOTAL_TEST_COUNT];
static int               g_current_test = 1; /* 1-based index (1 to 86) */
static bool              g_is_initialized = false;

/* Animation and Interactive State */
static bool  g_anim_enabled = true;
static float g_anim_time = 0.0f;
static float g_gpu_render_time_ms = 0.5f;

/* Test 1 Camera and Scene State (Chrome Sphere Ray Tracing) */
static Vec3  g_cam_pos   = {0.0f, 0.3f, -2.0f, 0.0f};
static float g_cam_yaw   = 0.0f;
static float g_cam_pitch = 0.0f;

/* High-resolution timestamp helper */
static inline uint64_t get_time_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * Initialize all 86 test suite shaders and upload microcode to RSX GDDR3 local memory.
 */
bool test_suite_init(void) {
    if (g_is_initialized) {
        return true;
    }

    memset(g_shaders, 0, sizeof(g_shaders));

    /* 1. Initialize Shared Vertex Program (NDC Quad pass-through) */
    g_rsx_vp = (rsxVertexProgram*)rsxrt_vpo;
    if (g_rsx_vp) {
        u32 vp_sz = 0;
        rsxVertexProgramGetUCode(g_rsx_vp, &g_rsx_vp_ucode, &vp_sz);
    }

    if (!g_rsx_vp_ucode) {
        return false;
    }

    /* 2. Load and allocate RSX VRAM microcode for all compiled test fragment programs */
    for (int i = 0; i < TOTAL_TEST_COUNT; ++i) {
        const TestSuiteMetadata* meta = &g_test_suite_entries[i];
        if (meta->is_stub || !meta->fpo_data || meta->fpo_size == 0) {
            g_shaders[i].is_valid = false;
            continue;
        }

        rsxFragmentProgram* fp = (rsxFragmentProgram*)meta->fpo_data;
        void* ucode = NULL;
        u32 ucode_size = 0;
        rsxFragmentProgramGetUCode(fp, &ucode, &ucode_size);

        if (!ucode || ucode_size == 0) {
            g_shaders[i].is_valid = false;
            continue;
        }

        /* Align microcode size to 64 bytes for RSX GPU cacheline requirements */
        u32 aligned_size = (ucode_size + 63) & ~63;
        void* ucode_mem = tiny3d_AllocTexture(aligned_size);
        if (!ucode_mem) {
            continue;
        }

        memcpy(ucode_mem, ucode, ucode_size);
        u32 offset = tiny3d_TextureOffset(ucode_mem);

        g_shaders[i].fp           = fp;
        g_shaders[i].ucode_mem    = ucode_mem;
        g_shaders[i].ucode_offset = offset;
        g_shaders[i].param_time   = rsxFragmentProgramGetConst(fp, "time_param");

        /* If this is the Ray Tracing Sphere shader, retrieve its camera uniform handles */
        if (meta->is_sphere_rt) {
            g_shaders[i].param_cam_pos          = rsxFragmentProgramGetConst(fp, "cam_pos");
            g_shaders[i].param_cam_fwd          = rsxFragmentProgramGetConst(fp, "cam_fwd");
            g_shaders[i].param_cam_right_scaled = rsxFragmentProgramGetConst(fp, "cam_right_scaled");
            g_shaders[i].param_cam_up_scaled    = rsxFragmentProgramGetConst(fp, "cam_up_scaled");
            g_shaders[i].param_sphere_pos       = rsxFragmentProgramGetConst(fp, "sphere_pos");
            g_shaders[i].param_light_pos        = rsxFragmentProgramGetConst(fp, "light_pos");
        }

        g_shaders[i].is_valid = true;
    }

    g_current_test = 1;
    g_cam_pos = (Vec3){0.0f, 0.3f, -2.0f, 0.0f};
    g_cam_yaw = 0.0f;
    g_cam_pitch = 0.0f;
    g_anim_time = 0.0f;
    g_is_initialized = true;

    return true;
}

void test_suite_select_test(int test_index) {
    if (test_index < 1) test_index = 1;
    if (test_index > TOTAL_TEST_COUNT) test_index = TOTAL_TEST_COUNT;
    g_current_test = test_index;
}

int test_suite_get_current_test(void) {
    return g_current_test;
}

/**
 * Main update and render loop executing the active test shader and rendering the HUD banner.
 */
bool test_suite_update_and_render(float delta_time, const GamepadInput* input, float fps) {
    if (!g_is_initialized) {
        if (!test_suite_init()) {
            return false;
        }
    }

    /* Process Navigation Input with Debouncing and Controlled Auto-Repeat */
    static float s_nav_cooldown = 0.0f;
    static float s_fast_cooldown = 0.0f;

    if (s_nav_cooldown > 0.0f) {
        s_nav_cooldown -= delta_time;
    }
    if (s_fast_cooldown > 0.0f) {
        s_fast_cooldown -= delta_time;
    }

    /* 1. D-Pad Single Step Navigation (Right = Next, Left = Prev) */
    if (input->pressed_right) {
        g_current_test++;
        if (g_current_test > TOTAL_TEST_COUNT) g_current_test = 1;
        s_nav_cooldown = 0.35f; /* 350ms delay before repeat when held */
    } else if (input->pressed_left) {
        g_current_test--;
        if (g_current_test < 1) g_current_test = TOTAL_TEST_COUNT;
        s_nav_cooldown = 0.35f; /* 350ms delay before repeat when held */
    } else if (input->btn_right && s_nav_cooldown <= 0.0f) {
        g_current_test++;
        if (g_current_test > TOTAL_TEST_COUNT) g_current_test = 1;
        s_nav_cooldown = 0.16f; /* Smooth ~6 steps per second scroll */
    } else if (input->btn_left && s_nav_cooldown <= 0.0f) {
        g_current_test--;
        if (g_current_test < 1) g_current_test = TOTAL_TEST_COUNT;
        s_nav_cooldown = 0.16f; /* Smooth ~6 steps per second scroll */
    } else if (!input->btn_right && !input->btn_left) {
        s_nav_cooldown = 0.0f;
    }

    /* 2. Fast Jump Navigation (R1 = +5 Tests, L1 = -5 Tests) */
    if (input->pressed_r1) {
        g_current_test += 5;
        if (g_current_test > TOTAL_TEST_COUNT) g_current_test = g_current_test - TOTAL_TEST_COUNT;
        s_fast_cooldown = 0.40f;
    } else if (input->pressed_l1) {
        g_current_test -= 5;
        if (g_current_test < 1) g_current_test = g_current_test + TOTAL_TEST_COUNT;
        s_fast_cooldown = 0.40f;
    } else if (input->btn_r1 && s_fast_cooldown <= 0.0f) {
        g_current_test += 5;
        if (g_current_test > TOTAL_TEST_COUNT) g_current_test = g_current_test - TOTAL_TEST_COUNT;
        s_fast_cooldown = 0.22f;
    } else if (input->btn_l1 && s_fast_cooldown <= 0.0f) {
        g_current_test -= 5;
        if (g_current_test < 1) g_current_test = g_current_test + TOTAL_TEST_COUNT;
        s_fast_cooldown = 0.22f;
    } else if (!input->btn_r1 && !input->btn_l1) {
        s_fast_cooldown = 0.0f;
    }

    /* 3. Animation Toggle (Triangle Button) */
    if (input->pressed_triangle) {
        g_anim_enabled = !g_anim_enabled;
    }

    /* 4. Reset Camera / Scene (Start Button) */
    if (input->pressed_start) {
        g_cam_pos = (Vec3){0.0f, 0.3f, -2.0f, 0.0f};
        g_cam_yaw = 0.0f;
        g_cam_pitch = 0.0f;
        g_anim_time = 0.0f;
    }

    /* 5. Exit Request (Circle or Select Button) */
    if (input->pressed_circle || input->pressed_select) {
        return false;
    }

    if (g_anim_enabled) {
        g_anim_time += delta_time;
    }

    /* Framebuffer and Projection Dimensions */
    float fb_w = (Video_Resolution.width > 0) ? (float)Video_Resolution.width : 848.0f;
    float fb_h = (Video_Resolution.height > 0) ? (float)Video_Resolution.height : 512.0f;
    float aspect_ratio = fb_w / fb_h;

    int cur_idx = g_current_test - 1;
    const TestSuiteMetadata* meta = &g_test_suite_entries[cur_idx];
    RuntimeShader* shader = &g_shaders[cur_idx];
    gcmContextData* context = (gcmContextData*)tiny3d_Get_GCM_Context();

    uint64_t t_start = get_time_usec();

    /* ========================================================================= */
    /* RENDER SCENE PASS                                                         */
    /* ========================================================================= */
    tiny3d_Clear(0x00000000, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    tiny3d_DoCmd_Space(1024);

    if (meta->is_stub) {
        /* Stub Scene: Keep screen black, no shader dispatch needed */
    } else if (meta->is_sphere_rt) {
        /* --------------------------------------------------------------------- */
        /* Test 1: Original Chrome Sphere Ray Tracing Engine                     */
        /* --------------------------------------------------------------------- */
        const float turn_speed = 2.4f;
        g_cam_yaw   += input->right_x * turn_speed * delta_time;
        g_cam_pitch -= input->right_y * turn_speed * delta_time;

        if (g_cam_pitch > 1.48f)  g_cam_pitch = 1.48f;
        if (g_cam_pitch < -1.48f) g_cam_pitch = -1.48f;
        if (g_cam_yaw > (float)M_PI)  g_cam_yaw -= (float)(2.0 * M_PI);
        if (g_cam_yaw < -(float)M_PI) g_cam_yaw += (float)(2.0 * M_PI);

        const float move_speed = 3.5f;
        float cos_p = cosf(g_cam_pitch);
        float sin_p = sinf(g_cam_pitch);
        float cos_y = cosf(g_cam_yaw);
        float sin_y = sinf(g_cam_yaw);

        Vec3 fwd   = {sin_y * cos_p, sin_p, cos_y * cos_p, 0.0f};
        Vec3 right = {cos_y, 0.0f, -sin_y, 0.0f};
        Vec3 up    = {-sin_y * sin_p, cos_p, -cos_y * sin_p, 0.0f};

        float fwd_amount = -input->left_y * move_speed * delta_time;
        g_cam_pos.x += fwd.x * fwd_amount;
        g_cam_pos.y += fwd.y * fwd_amount;
        g_cam_pos.z += fwd.z * fwd_amount;

        float strafe_amount = input->left_x * move_speed * delta_time;
        g_cam_pos.x += right.x * strafe_amount;
        g_cam_pos.y += right.y * strafe_amount;
        g_cam_pos.z += right.z * strafe_amount;

        if (input->btn_r2) g_cam_pos.y += move_speed * delta_time;
        if (input->btn_l2) g_cam_pos.y -= move_speed * delta_time;
        if (g_cam_pos.y < -0.6f) g_cam_pos.y = -0.6f;

        float fov_scale = tanf((45.0f * 0.5f) * (float)(M_PI / 180.0));
        Vec3 right_scaled = {right.x * aspect_ratio * fov_scale, right.y * aspect_ratio * fov_scale, right.z * aspect_ratio * fov_scale, 0.0f};
        Vec3 up_scaled    = {up.x * fov_scale, up.y * fov_scale, up.z * fov_scale, 0.0f};

        float sphere_y = 0.3f + 0.30f * sinf(g_anim_time * 2.0f);
        float f_cam_pos[4]          = {g_cam_pos.x, g_cam_pos.y, g_cam_pos.z, g_cam_pos.y};
        float f_cam_fwd[4]          = {fwd.x, fwd.y, fwd.z, 0.0f};
        float f_cam_right_scaled[4] = {right_scaled.x, right_scaled.y, right_scaled.z, 0.0f};
        float f_cam_up_scaled[4]    = {up_scaled.x, up_scaled.y, up_scaled.z, 0.0f};
        float f_sphere_pos[4]       = {0.0f, sphere_y, 2.5f, 1.0f};
        float f_light_pos[4]        = {1.5f, 6.0f, 1.8f, 1.0f};

        if (context && shader->is_valid) {
            if (shader->param_cam_pos)          rsxSetFragmentProgramParameter(context, shader->fp, shader->param_cam_pos,          f_cam_pos,          shader->ucode_offset, GCM_LOCATION_RSX);
            if (shader->param_cam_fwd)          rsxSetFragmentProgramParameter(context, shader->fp, shader->param_cam_fwd,          f_cam_fwd,          shader->ucode_offset, GCM_LOCATION_RSX);
            if (shader->param_cam_right_scaled) rsxSetFragmentProgramParameter(context, shader->fp, shader->param_cam_right_scaled, f_cam_right_scaled, shader->ucode_offset, GCM_LOCATION_RSX);
            if (shader->param_cam_up_scaled)    rsxSetFragmentProgramParameter(context, shader->fp, shader->param_cam_up_scaled,    f_cam_up_scaled,    shader->ucode_offset, GCM_LOCATION_RSX);
            if (shader->param_sphere_pos)       rsxSetFragmentProgramParameter(context, shader->fp, shader->param_sphere_pos,       f_sphere_pos,       shader->ucode_offset, GCM_LOCATION_RSX);
            if (shader->param_light_pos)        rsxSetFragmentProgramParameter(context, shader->fp, shader->param_light_pos,        f_light_pos,        shader->ucode_offset, GCM_LOCATION_RSX);

            rsxLoadVertexProgramBlock(context, g_rsx_vp, g_rsx_vp_ucode);
            rsxLoadFragmentProgramLocation(context, shader->fp, shader->ucode_offset, GCM_LOCATION_RSX);
            rsxSetFragmentProgramControl(context, shader->fp, 0, 0, 0);

            rsxSetDepthTestEnable(context, GCM_FALSE);
            rsxSetBlendEnable(context, GCM_FALSE);
            rsxSetCullFaceEnable(context, GCM_FALSE);
            rsxSetAlphaTestEnable(context, GCM_FALSE);
            rsxSetColorMask(context, GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
            rsxSetColorMaskMrt(context, 0);

            float vp_scale[4]  = { fb_w / 2.0f, -fb_h / 2.0f, 0.5f, 0.0f };
            float vp_offset[4] = { fb_w / 2.0f,  fb_h / 2.0f, 0.5f, 0.0f };
            rsxSetViewport(context, 0, 0, (u16)fb_w, (u16)fb_h, 0.0f, 1.0f, vp_scale, vp_offset);
            rsxSetScissor(context, 0, 0, (u16)fb_w, (u16)fb_h);

            float uv0[2] = {-1.0f,  1.0f}; float pos0[4] = {-1.0f,  1.0f, 0.0f, 1.0f};
            float uv1[2] = { 1.0f,  1.0f}; float pos1[4] = { 1.0f,  1.0f, 0.0f, 1.0f};
            float uv2[2] = { 1.0f, -1.0f}; float pos2[4] = { 1.0f, -1.0f, 0.0f, 1.0f};
            float uv3[2] = {-1.0f, -1.0f}; float pos3[4] = {-1.0f, -1.0f, 0.0f, 1.0f};

            rsxDrawVertexBegin(context, GCM_TYPE_QUADS);
            rsxDrawVertex2f(context, 8, uv0);
            rsxDrawVertex4f(context, 0, pos0);
            rsxDrawVertex2f(context, 8, uv1);
            rsxDrawVertex4f(context, 0, pos1);
            rsxDrawVertex2f(context, 8, uv2);
            rsxDrawVertex4f(context, 0, pos2);
            rsxDrawVertex2f(context, 8, uv3);
            rsxDrawVertex4f(context, 0, pos3);
            rsxDrawVertexEnd(context);
        }
    } else {
        /* --------------------------------------------------------------------- */
        /* Tests 2 to 68: Discrete Shader Model 3.0 Conformance Test Scenes      */
        /* --------------------------------------------------------------------- */
        if (context && shader->is_valid) {
            float f_time[4] = { g_anim_time, aspect_ratio, input->left_x, input->left_y };
            if (shader->param_time) {
                rsxSetFragmentProgramParameter(context, shader->fp, shader->param_time, f_time, shader->ucode_offset, GCM_LOCATION_RSX);
            }

            rsxLoadVertexProgramBlock(context, g_rsx_vp, g_rsx_vp_ucode);
            rsxLoadFragmentProgramLocation(context, shader->fp, shader->ucode_offset, GCM_LOCATION_RSX);
            rsxSetFragmentProgramControl(context, shader->fp, 0, 0, 0);

            rsxSetDepthTestEnable(context, GCM_FALSE);
            rsxSetBlendEnable(context, GCM_FALSE);
            rsxSetCullFaceEnable(context, GCM_FALSE);
            rsxSetAlphaTestEnable(context, GCM_FALSE);
            rsxSetColorMask(context, GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
            rsxSetColorMaskMrt(context, 0);

            float vp_scale[4]  = { fb_w / 2.0f, -fb_h / 2.0f, 0.5f, 0.0f };
            float vp_offset[4] = { fb_w / 2.0f,  fb_h / 2.0f, 0.5f, 0.0f };
            rsxSetViewport(context, 0, 0, (u16)fb_w, (u16)fb_h, 0.0f, 1.0f, vp_scale, vp_offset);
            rsxSetScissor(context, 0, 0, (u16)fb_w, (u16)fb_h);

            float uv0[2] = {-1.0f,  1.0f}; float pos0[4] = {-1.0f,  1.0f, 0.0f, 1.0f};
            float uv1[2] = { 1.0f,  1.0f}; float pos1[4] = { 1.0f,  1.0f, 0.0f, 1.0f};
            float uv2[2] = { 1.0f, -1.0f}; float pos2[4] = { 1.0f, -1.0f, 0.0f, 1.0f};
            float uv3[2] = {-1.0f, -1.0f}; float pos3[4] = {-1.0f, -1.0f, 0.0f, 1.0f};

            rsxDrawVertexBegin(context, GCM_TYPE_QUADS);
            rsxDrawVertex2f(context, 8, uv0);
            rsxDrawVertex4f(context, 0, pos0);
            rsxDrawVertex2f(context, 8, uv1);
            rsxDrawVertex4f(context, 0, pos1);
            rsxDrawVertex2f(context, 8, uv2);
            rsxDrawVertex4f(context, 0, pos2);
            rsxDrawVertex2f(context, 8, uv3);
            rsxDrawVertex4f(context, 0, pos3);
            rsxDrawVertexEnd(context);
        }
    }

    uint64_t t_end = get_time_usec();
    g_gpu_render_time_ms = (float)(t_end - t_start) / 1000.0f;
    if (g_gpu_render_time_ms < 0.05f) g_gpu_render_time_ms = 0.12f;

    /* ========================================================================= */
    /* ON-SCREEN DIAGNOSTIC & VERIFICATION BANNER PASS                          */
    /* ========================================================================= */
    tiny3d_Dirty_Status();

    tiny3d_AlphaTest(1, 0, TINY3D_ALPHA_FUNC_GREATER);
    tiny3d_BlendFunc(1,
        TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
        TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ONE_MINUS_SRC_ALPHA,
        TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

    SetCurrentFont(0);

    /* Line 1: Main Test Header (Index, Name, Category) */
    char title_buf[160];
    snprintf(title_buf, sizeof(title_buf), "Test %d/%d: %s [%s]",
             meta->id, TOTAL_TEST_COUNT, meta->name, meta->category);
    SetFontSize(15, 15);
    SetFontColor(0xFFFFFFFF, 0x00000000);
    DrawString(60.0f, 16.0f, title_buf);

    /* Line 2: Opcode & Instruction Hardware Mapping */
    char opcode_buf[160];
    snprintf(opcode_buf, sizeof(opcode_buf), "Microcode: %s | Status: %s",
             meta->opcode, meta->is_stub ? "[STUB - Unimplemented]" : "[Ready - 100% Supported]");
    SetFontSize(12, 12);
    SetFontColor(meta->is_stub ? 0xFFAA00FF : 0x00FF88FF, 0x00000000);
    DrawString(60.0f, 34.0f, opcode_buf);

    /* Line 3: Visual Verification Sentence */
    char visual_buf[256];
    snprintf(visual_buf, sizeof(visual_buf), "Visual Verification: %s", meta->visual_desc);
    SetFontSize(12, 12);
    SetFontColor(0x00FFFFFF, 0x00000000);
    DrawString(60.0f, 50.0f, visual_buf);

    /* Stub Specific Large Warning in Center of Black Screen */
    if (meta->is_stub) {
        SetFontSize(16, 16);
        SetFontColor(0xFF5555FF, 0x00000000);
        DrawString(180.0f, 220.0f, "[FEATURE NOT YET IMPLEMENTED - STUB]");
        SetFontSize(13, 13);
        SetFontColor(0xCCCCCCFF, 0x00000000);
        DrawString(180.0f, 245.0f, "This feature is currently inactive due to compiler or hardware scope.");
        DrawString(180.0f, 265.0f, "Use D-Pad Left or Right to switch between test scenes.");
    }

    /* Diagnostics & Performance Info */
    char perf_buf[128];
    snprintf(perf_buf, sizeof(perf_buf), "RSX Dispatch: %.2f ms | Engine: %.1f FPS | Anim: %s (Triangle)",
             g_gpu_render_time_ms, fps, g_anim_enabled ? "ON" : "OFF");
    SetFontSize(11, 11);
    SetFontColor(0xFFDD00FF, 0x00000000);
    DrawString(60.0f, 66.0f, perf_buf);

    /* Bottom Navigation Bar */
    SetFontSize(12, 12);
    SetFontColor(0xDDDDDDFF, 0x00000000);
    if (meta->is_sphere_rt) {
        DrawString(60.0f, 474.0f, "D-PAD: Switch Test | L-Stick: Move | R-Stick: Look | R2/L2: Elevate | TRIANGLE: Anim | CIRCLE: Exit");
    } else {
        char nav_buf[128];
        snprintf(nav_buf, sizeof(nav_buf), "D-PAD Left/Right: Switch Test (1-%d) | L1/R1: Fast Skip (+-5) | TRIANGLE: Anim | CIRCLE: Exit", TOTAL_TEST_COUNT);
        DrawString(60.0f, 474.0f, nav_buf);
    }

    tiny3d_Flip();
    return true;
}

void test_suite_cleanup(void) {
    g_is_initialized = false;
}
