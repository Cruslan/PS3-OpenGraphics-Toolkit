/**
 * PS3 Reality Synthesizer (RSX) Hardware Ray Tracing Engine
 * Common Shared Definitions, Vector Types, and Math Structures
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <tiny3d.h>

#define RENDER_WIDTH            848
#define RENDER_HEIGHT           480

/* Fully transparent background for HUD and UI text */
#define FONT_BKCOLOR_TRANSPARENT_BLACK 0x00000000
#define FONT_BKCOLOR_TRANSPARENT       0x00000000

/**
 * 3D Float Vector structure aligned to 16 bytes for SIMD / GCM compatibility
 */
typedef struct __attribute__((aligned(16))) {
    float x;
    float y;
    float z;
    float w; /* 16-byte alignment padding */
} Vec3;

/**
 * Camera state structure
 */
typedef struct __attribute__((aligned(16))) {
    Vec3 pos;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    float fov_scale;
    float aspect_ratio;
    float padding[2];
} CameraData;

/**
 * Reflective Sphere primitive definition
 */
typedef struct __attribute__((aligned(16))) {
    Vec3 center;
    float radius;
    float radius_sq;
    float reflectivity;
    float shininess;
    Vec3 color;
} SphereData;

/**
 * Checkered Ground Plane definition
 */
typedef struct __attribute__((aligned(16))) {
    float height_y;
    float tile_size;
    float reflectivity;
    float padding;
    Vec3 color_dark;
    Vec3 color_light;
} PlaneData;

/**
 * Dynamic Point Light definition
 */
typedef struct __attribute__((aligned(16))) {
    Vec3 pos;
    Vec3 color;
    float intensity;
    float ambient;
    float padding[2];
} LightData;

/**
 * Render a 2D colored rectangular bounding box using Tiny3D
 */
static inline void draw_ui_box(float x, float y, float w, float h, uint32_t color) {
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, 0.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x + w, y, 0.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x + w, y + h, 0.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x, y + h, 0.0f);
    tiny3d_VertexColor(color);
    tiny3d_End();
}

#endif /* __COMMON_H__ */
