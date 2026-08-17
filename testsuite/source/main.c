/**
 * PS3 Reality Synthesizer (RSX) Hardware Ray Tracing Engine
 * Main Application & Execution Controller
 *
 * Direct standalone launcher for the RSX Hardware Ray Tracing engine.
 * Initializes the PlayStation 3 hardware subsystems (GCM/RSX, Tiny3D, DualShock 3 input,
 * and FreeType2/libfont TTF rasterizer) and directly enters the GPU ray tracing pipeline loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <sys/process.h>
#include <sys/time.h>
#include <sys/systime.h>
#include <sysmodule/sysmodule.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <sys/file.h>

#include <tiny3d.h>
#include <libfont.h>
#include <rsx/gcm_sys.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "../include/common.h"
#include "../include/gamepad.h"
#include "../include/rsxrt.h"
#include "../include/test_suite.h"

/* Set main PPU thread stack size to 1 MB */
SYS_PROCESS_PARAM(1001, 0x100000)

/* ========================================================================= */
/* GAMEPAD & CONTROLLER INPUT SUBSYSTEM                                      */
/* ========================================================================= */

static GamepadInput g_input;
static GamepadInput g_prev_input;
static bool         g_has_prev_input = false;

void gamepad_init(void) {
    ioPadInit(MAX_PORT_NUM);
    memset(&g_input, 0, sizeof(GamepadInput));
    memset(&g_prev_input, 0, sizeof(GamepadInput));
    g_has_prev_input = false;
}

const GamepadInput* gamepad_get_state(void) {
    return &g_input;
}

void gamepad_poll(void) {
    padInfo pad_info;
    padData current_raw;
    memset(&current_raw, 0, sizeof(padData));
    bool found_pad = false;
    
    /* 0. Always clear single-frame edge triggers on every poll cycle */
    g_input.pressed_up       = false;
    g_input.pressed_down     = false;
    g_input.pressed_left     = false;
    g_input.pressed_right    = false;
    g_input.pressed_cross    = false;
    g_input.pressed_circle   = false;
    g_input.pressed_triangle = false;
    g_input.pressed_square   = false;
    g_input.pressed_l1       = false;
    g_input.pressed_r1       = false;
    g_input.pressed_l2       = false;
    g_input.pressed_r2       = false;
    g_input.pressed_start    = false;
    g_input.pressed_select   = false;
    g_input.stick_up_pressed    = false;
    g_input.stick_down_pressed  = false;
    g_input.stick_left_pressed  = false;
    g_input.stick_right_pressed = false;
    
    ioPadGetInfo(&pad_info);
    for (int p = 0; p < MAX_PORT_NUM; ++p) {
        if (pad_info.status[p]) {
            if (ioPadGetData(p, &current_raw) == 0 && current_raw.len > 0) {
                found_pad = true;
                break;
            }
        }
    }
    
    if (!found_pad) {
        /* No active pad data in this frame, edge triggers remain false */
        return;
    }
    
    g_input.is_connected = true;
    
    /* 1. Extract Digital Button States from raw button[2] and button[3] uint16 bitmasks */
    uint16_t d1 = current_raw.button[2];
    uint16_t d2 = current_raw.button[3];
    
    bool cur_btn_left     = (d1 & PAD_CTRL_LEFT) != 0;
    bool cur_btn_down     = (d1 & PAD_CTRL_DOWN) != 0;
    bool cur_btn_right    = (d1 & PAD_CTRL_RIGHT) != 0;
    bool cur_btn_up       = (d1 & PAD_CTRL_UP) != 0;
    bool cur_btn_start    = (d1 & PAD_CTRL_START) != 0;
    bool cur_btn_r3       = (d1 & PAD_CTRL_R3) != 0;
    bool cur_btn_l3       = (d1 & PAD_CTRL_L3) != 0;
    bool cur_btn_select   = (d1 & PAD_CTRL_SELECT) != 0;
    
    bool cur_btn_square   = (d2 & PAD_CTRL_SQUARE) != 0;
    bool cur_btn_cross    = (d2 & PAD_CTRL_CROSS) != 0;
    bool cur_btn_circle   = (d2 & PAD_CTRL_CIRCLE) != 0;
    bool cur_btn_triangle = (d2 & PAD_CTRL_TRIANGLE) != 0;
    bool cur_btn_r1       = (d2 & PAD_CTRL_R1) != 0;
    bool cur_btn_l1       = (d2 & PAD_CTRL_L1) != 0;
    bool cur_btn_r2       = (d2 & PAD_CTRL_R2) != 0;
    bool cur_btn_l2       = (d2 & PAD_CTRL_L2) != 0;
    
    /* 2. Compute 1-Frame Edge Triggers against previous poll state */
    if (g_has_prev_input) {
        g_input.pressed_up       = cur_btn_up       && !g_prev_input.btn_up;
        g_input.pressed_down     = cur_btn_down     && !g_prev_input.btn_down;
        g_input.pressed_left     = cur_btn_left     && !g_prev_input.btn_left;
        g_input.pressed_right    = cur_btn_right    && !g_prev_input.btn_right;
        g_input.pressed_cross    = cur_btn_cross    && !g_prev_input.btn_cross;
        g_input.pressed_circle   = cur_btn_circle   && !g_prev_input.btn_circle;
        g_input.pressed_triangle = cur_btn_triangle && !g_prev_input.btn_triangle;
        g_input.pressed_square   = cur_btn_square   && !g_prev_input.btn_square;
        g_input.pressed_l1       = cur_btn_l1       && !g_prev_input.btn_l1;
        g_input.pressed_r1       = cur_btn_r1       && !g_prev_input.btn_r1;
        g_input.pressed_l2       = cur_btn_l2       && !g_prev_input.btn_l2;
        g_input.pressed_r2       = cur_btn_r2       && !g_prev_input.btn_r2;
        g_input.pressed_start    = cur_btn_start    && !g_prev_input.btn_start;
        g_input.pressed_select   = cur_btn_select   && !g_prev_input.btn_select;
    }
    
    g_input.btn_left     = cur_btn_left;
    g_input.btn_down     = cur_btn_down;
    g_input.btn_right    = cur_btn_right;
    g_input.btn_up       = cur_btn_up;
    g_input.btn_start    = cur_btn_start;
    g_input.btn_r3       = cur_btn_r3;
    g_input.btn_l3       = cur_btn_l3;
    g_input.btn_select   = cur_btn_select;
    g_input.btn_square   = cur_btn_square;
    g_input.btn_cross    = cur_btn_cross;
    g_input.btn_circle   = cur_btn_circle;
    g_input.btn_triangle = cur_btn_triangle;
    g_input.btn_r1       = cur_btn_r1;
    g_input.btn_l1       = cur_btn_l1;
    g_input.btn_r2       = cur_btn_r2;
    g_input.btn_l2       = cur_btn_l2;
    
    /* 3. Extract 8-bit Analog Sticks directly from registers button[4..7] */
    uint32_t raw_rx = (uint32_t)(current_raw.button[4] & 0x00FF);
    uint32_t raw_ry = (uint32_t)(current_raw.button[5] & 0x00FF);
    uint32_t raw_lx = (uint32_t)(current_raw.button[6] & 0x00FF);
    uint32_t raw_ly = (uint32_t)(current_raw.button[7] & 0x00FF);
    
    float flx = ((float)raw_lx - 128.0f) / 127.5f;
    float fly = ((float)raw_ly - 128.0f) / 127.5f;
    float frx = ((float)raw_rx - 128.0f) / 127.5f;
    float fry = ((float)raw_ry - 128.0f) / 127.5f;
    
    if (flx < -1.0f) flx = -1.0f; else if (flx > 1.0f) flx = 1.0f;
    if (fly < -1.0f) fly = -1.0f; else if (fly > 1.0f) fly = 1.0f;
    if (frx < -1.0f) frx = -1.0f; else if (frx > 1.0f) frx = 1.0f;
    if (fry < -1.0f) fry = -1.0f; else if (fry > 1.0f) fry = 1.0f;
    
    /* Apply Smooth Linear Deadzone with Continuous Remapping */
    const float dz = 0.10f;
    g_input.left_x  = (fabsf(flx) > dz) ? ((flx > 0.0f ? (flx - dz) : (flx + dz)) / (1.0f - dz)) : 0.0f;
    g_input.left_y  = (fabsf(fly) > dz) ? ((fly > 0.0f ? (fly - dz) : (fly + dz)) / (1.0f - dz)) : 0.0f;
    g_input.right_x = (fabsf(frx) > dz) ? ((frx > 0.0f ? (frx - dz) : (frx + dz)) / (1.0f - dz)) : 0.0f;
    g_input.right_y = (fabsf(fry) > dz) ? ((fry > 0.0f ? (fry - dz) : (fry + dz)) / (1.0f - dz)) : 0.0f;
    
    /* 4. Compute Analog Stick Directional Edge Triggers */
    static bool s_prev_stick_up = false;
    static bool s_prev_stick_down = false;
    static bool s_prev_stick_left = false;
    static bool s_prev_stick_right = false;
    
    bool cur_stick_up    = (g_input.left_y < -0.55f);
    bool cur_stick_down  = (g_input.left_y >  0.55f);
    bool cur_stick_left  = (g_input.left_x < -0.55f);
    bool cur_stick_right = (g_input.left_x >  0.55f);
    
    g_input.stick_up_pressed    = cur_stick_up    && !s_prev_stick_up;
    g_input.stick_down_pressed  = cur_stick_down  && !s_prev_stick_down;
    g_input.stick_left_pressed  = cur_stick_left  && !s_prev_stick_left;
    g_input.stick_right_pressed = cur_stick_right && !s_prev_stick_right;
    
    s_prev_stick_up    = cur_stick_up;
    s_prev_stick_down  = cur_stick_down;
    s_prev_stick_left  = cur_stick_left;
    s_prev_stick_right = cur_stick_right;
    
    /* Cache full input state for next frame comparison */
    g_prev_input = g_input;
    g_has_prev_input = true;
}

/* Performance Timers */
static uint64_t g_last_time_usec = 0;
static float    g_fps = 60.0f;

/* Built-in 8x8 ASCII Bitmap Font */
static const uint8_t g_font_8x8_data[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* '!' */
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, /* '"' */
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, /* '#' */
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, /* '$' */
    {0x00,0x66,0xA6,0xD8,0x1B,0x65,0x66,0x00}, /* '%' */
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, /* '&' */
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* '\'' */
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* '(' */
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* ')' */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* '*' */
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* '+' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* ',' */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* '-' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* '.' */
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, /* '/' */
    {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00}, /* '0' */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* '1' */
    {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00}, /* '2' */
    {0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, /* '3' */
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, /* '4' */
    {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00}, /* '5' */
    {0x7C,0xC6,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, /* '6' */
    {0xFE,0x06,0x0C,0x18,0x30,0x60,0xC0,0x00}, /* '7' */
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, /* '8' */
    {0x7C,0xC6,0xC6,0x7E,0x06,0xC6,0x7C,0x00}, /* '9' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, /* ':' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, /* ';' */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* '<' */
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* '=' */
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, /* '>' */
    {0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, /* '?' */
    {0x7C,0xC6,0xDE,0xDE,0xDC,0xC0,0x7C,0x00}, /* '@' */
    {0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00}, /* 'A' */
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, /* 'B' */
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, /* 'C' */
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, /* 'D' */
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, /* 'E' */
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, /* 'F' */
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, /* 'G' */
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, /* 'H' */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 'I' */
    {0x1E,0x06,0x06,0x06,0x06,0xC6,0x7C,0x00}, /* 'J' */
    {0xC6,0xCC,0xD8,0xF0,0xD8,0xCC,0xC6,0x00}, /* 'K' */
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, /* 'L' */
    {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00}, /* 'M' */
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, /* 'N' */
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, /* 'O' */
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, /* 'P' */
    {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0E}, /* 'Q' */
    {0xFC,0x66,0x66,0x7C,0xD8,0xCC,0xC6,0x00}, /* 'R' */
    {0x7C,0xC6,0xE0,0x7C,0x0E,0xC6,0x7C,0x00}, /* 'S' */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* 'T' */
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, /* 'U' */
    {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00}, /* 'V' */
    {0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00}, /* 'W' */
    {0xC6,0x6C,0x38,0x38,0x6C,0xC6,0xC6,0x00}, /* 'X' */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, /* 'Y' */
    {0xFE,0x0E,0x1C,0x38,0x70,0xE0,0xFE,0x00}, /* 'Z' */
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* '[' */
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* '\' */
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* ']' */
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, /* '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* '_' */
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, /* '`' */
    {0x00,0x00,0x7C,0x06,0x7E,0xC6,0x7E,0x00}, /* 'a' */
    {0xE0,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, /* 'b' */
    {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00}, /* 'c' */
    {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x7E,0x00}, /* 'd' */
    {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00}, /* 'e' */
    {0x1C,0x36,0x30,0x78,0x30,0x30,0x78,0x00}, /* 'f' */
    {0x00,0x00,0x7E,0xCC,0xCC,0x7C,0x0C,0xF8}, /* 'g' */
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, /* 'h' */
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* 'i' */
    {0x06,0x00,0x0E,0x06,0x06,0x66,0x3C,0x00}, /* 'j' */
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, /* 'k' */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 'l' */
    {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0x00}, /* 'm' */
    {0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, /* 'n' */
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, /* 'o' */
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, /* 'p' */
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, /* 'q' */
    {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, /* 'r' */
    {0x00,0x00,0x7E,0xC0,0x7C,0x06,0xFC,0x00}, /* 's' */
    {0x30,0x30,0x7C,0x30,0x30,0x34,0x18,0x00}, /* 't' */
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, /* 'u' */
    {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, /* 'v' */
    {0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x6C,0x00}, /* 'w' */
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, /* 'x' */
    {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0xFC}, /* 'y' */
    {0x00,0x00,0xFE,0x1C,0x38,0x70,0xFE,0x00}, /* 'z' */
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, /* '{' */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* '|' */
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, /* '}' */
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}  /* '~' */
};

/* Microsecond timestamp using native PS3 LV2 syscall */
static inline uint64_t get_time_usec(void) {
    uint64_t sec = 0, nsec = 0;
    sysGetCurrentTime(&sec, &nsec);
    return sec * 1000000ULL + (nsec / 1000ULL);
}

/* System event callback */
static void sysutil_exit_callback(uint64_t status, uint64_t param, void* userdata) {
    (void)param; (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) {
        test_suite_cleanup();
        rsxrt_cleanup();
        tiny3d_Exit();
        exit(0);
    }
}

/* ========================================================================= */
/* SONY OFFICIAL TTF FONT SUBSYSTEM (FreeType 2 + libfont)                  */
/* ========================================================================= */

/* Embedded TrueType Font binary generated via bin2s */
extern const uint8_t font_ttf[];
extern const uint32_t font_ttf_size;

static FT_Library g_ft_lib;
static FT_Face    g_ft_face = NULL;
static bool       g_using_ttf = false;
static uint8_t*   g_loaded_font_buffer = NULL;

/* Direct LV2 Kernel File Loader for /dev_flash */
static uint8_t* load_file_lv2(const char* path, size_t* out_size) {
    s32 fd = -1;
    if (sysLv2FsOpen(path, 0, &fd, 0, NULL, 0) != 0 || fd < 0) {
        return NULL;
    }
    
    sysFSStat st;
    if (sysLv2FsFStat(fd, &st) != 0 || st.st_size <= 0) {
        sysLv2FsClose(fd);
        return NULL;
    }
    
    uint8_t* buf = (uint8_t*)malloc(st.st_size);
    if (!buf) {
        sysLv2FsClose(fd);
        return NULL;
    }
    
    u64 bytes_read = 0;
    if (sysLv2FsRead(fd, buf, st.st_size, &bytes_read) != 0 || bytes_read != (u64)st.st_size) {
        free(buf);
        sysLv2FsClose(fd);
        return NULL;
    }
    
    sysLv2FsClose(fd);
    if (out_size) *out_size = (size_t)st.st_size;
    return buf;
}

/* TTF Glyph Rasterization Callback for Hermes' Tiny3D / libfont */
static void ttf_render_glyph_cb(u8 chr, u8 *bitmap, short *w, short *h, short *y_correction) {
    if (!g_ft_face || chr < 32 || chr > 126) {
        *w = 0; *h = 0; *y_correction = 0;
        return;
    }
    
    memset(bitmap, 0, 32 * 32);
    
    if (chr == ' ') {
        *w = 10; *h = 0; *y_correction = 0;
        return;
    }
    
    FT_UInt glyph_index = FT_Get_Char_Index(g_ft_face, (FT_ULong)chr);
    if (glyph_index == 0) {
        *w = 12; *h = 24; *y_correction = 0;
        return;
    }
    
    if (FT_Load_Glyph(g_ft_face, glyph_index, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT) != 0) {
        *w = 12; *h = 24; *y_correction = 0;
        return;
    }
    
    FT_GlyphSlot slot = g_ft_face->glyph;
    FT_Bitmap* ft_bmp = &slot->bitmap;
    
    *w = (short)(slot->advance.x >> 6);
    *h = (short)ft_bmp->rows;
    *y_correction = (short)((g_ft_face->size->metrics.ascender >> 6) - slot->bitmap_top);
    
    int src_w = (int)ft_bmp->width;
    int src_h = (int)ft_bmp->rows;
    
    for (int r = 0; r < src_h && r < 32; ++r) {
        for (int c = 0; c < src_w && c < 32; ++c) {
            bitmap[r * 32 + c] = ft_bmp->buffer[r * ft_bmp->pitch + c];
        }
    }
}

static void init_system_fonts(void) {
    static const char* const sony_ttf_paths[] = {
        "/dev_flash/data/font/SCE-PS3-RD-R-LATIN.TTF",
        "/dev_flash/data/font/SCE-PS3-SR-R-LATIN.TTF",
        "/dev_flash/data/font/SCE-PS3-VR-R-LATIN.TTF",
        "/dev_flash/data/font/SCE-PS3-DH-R-CBM.TTF",
        "/dev_flash/data/font/SCE-PS3-NR-R-JPN.TTF"
    };
    
    ResetFont();
    
    if (FT_Init_FreeType(&g_ft_lib) == 0) {
        /* 1. Try loading Sony official TTF from /dev_flash via LV2 filesystem */
        for (size_t i = 0; i < sizeof(sony_ttf_paths)/sizeof(sony_ttf_paths[0]); ++i) {
            size_t fsize = 0;
            uint8_t* fdata = load_file_lv2(sony_ttf_paths[i], &fsize);
            if (fdata && fsize > 0) {
                if (FT_New_Memory_Face(g_ft_lib, (const FT_Byte*)fdata, (FT_Long)fsize, 0, &g_ft_face) == 0) {
                    FT_Set_Pixel_Sizes(g_ft_face, 0, 24);
                    
                    u8* font_texture_mem = (u8*)tiny3d_AllocTexture(256 * 32 * 32 * sizeof(uint32_t));
                    if (font_texture_mem) {
                        AddFontFromTTF(font_texture_mem, 32, 126, 32, 32, ttf_render_glyph_cb);
                        g_using_ttf = true;
                        g_loaded_font_buffer = fdata;
                        SetCurrentFont(0);
                        return;
                    }
                }
                free(fdata);
            }
        }
        
        /* 2. Fallback to Embedded TrueType Font binary */
        if (font_ttf_size > 0) {
            if (FT_New_Memory_Face(g_ft_lib, (const FT_Byte*)font_ttf, (FT_Long)font_ttf_size, 0, &g_ft_face) == 0) {
                FT_Set_Pixel_Sizes(g_ft_face, 0, 24);
                
                u8* font_texture_mem = (u8*)tiny3d_AllocTexture(256 * 32 * 32 * sizeof(uint32_t));
                if (font_texture_mem) {
                    AddFontFromTTF(font_texture_mem, 32, 126, 32, 32, ttf_render_glyph_cb);
                    g_using_ttf = true;
                    SetCurrentFont(0);
                    return;
                }
            }
        }
    }
    
    /* 3. Fallback: Built-in 8x8 ASCII Font */
    uint32_t* font_texture_mem = (uint32_t*)tiny3d_AllocTexture(95 * 8 * 8 * sizeof(uint32_t));
    if (font_texture_mem) {
        AddFontFromBitmapArray(
            (uint8_t*)g_font_8x8_data,
            (uint8_t*)font_texture_mem,
            32, 126,
            8, 8,
            1,
            BIT7_FIRST_PIXEL
        );
    }
    SetCurrentFont(0);
    
    /* Enable Hardware Alpha Testing and Alpha Blending for transparent font rendering */
    tiny3d_AlphaTest(1, 0, TINY3D_ALPHA_FUNC_GREATER);
    tiny3d_BlendFunc(1, 
        TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
        TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ONE_MINUS_SRC_ALPHA,
        TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
}

/* ========================================================================= */
/* APPLICATION MAIN ENTRY POINT                                              */
/* ========================================================================= */
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    /* Load essential PRX system modules from /dev_flash */
    sysModuleLoad(SYSMODULE_FS);
    sysModuleLoad(SYSMODULE_IO);
    sysModuleLoad(SYSMODULE_GCM_SYS);
    sysModuleLoad(SYSMODULE_SYSUTIL);
    
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_exit_callback, NULL);
    
    /* Initialize Gamepad Subsystem */
    gamepad_init();
    
    /* Initialize Tiny3D Display Pipeline */
    tiny3d_Init(1024 * 1024);
    
    /* Enable Hardware Alpha Testing and Blending for transparency throughout the engine */
    tiny3d_AlphaTest(1, 0, TINY3D_ALPHA_FUNC_GREATER);
    tiny3d_BlendFunc(1, 
        TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
        TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_DST_ALPHA_ONE_MINUS_SRC_ALPHA,
        TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
    
    /* Initialize RSX Shader Model 3.0 Full Coverage Test Suite on boot */
    test_suite_init();
    
    /* Initialize Sony System TTF Font via FreeType 2 with 8x8 fallback */
    init_system_fonts();
    
    g_last_time_usec = get_time_usec();
    
    /* ===================================================================== */
    /* DIRECT RSX SHADER MODEL 3.0 TEST SUITE EXECUTION LOOP                */
    /* ===================================================================== */
    while (1) {
        /* Compute Dynamic Delta Time */
        uint64_t current_time_usec = get_time_usec();
        float delta_time = (float)(current_time_usec - g_last_time_usec) / 1000000.0f;
        g_last_time_usec = current_time_usec;
        
        if (delta_time <= 0.0f || delta_time > 0.1f) {
            delta_time = 1.0f / 60.0f;
        }
        g_fps = 0.9f * g_fps + 0.1f * (1.0f / delta_time);
        
        /* Poll and Update Gamepad State */
        gamepad_poll();
        
        /* Update and Render active RSX Test Suite scene */
        bool keep_running = test_suite_update_and_render(delta_time, &g_input, g_fps);
        if (!keep_running) {
            break;
        }
    }
    
    /* Clean shutdown when exiting to CrossMediaBar (XMB) */
    test_suite_cleanup();
    rsxrt_cleanup();
    tiny3d_Exit();
    exit(0);
    
    return 0;
}
