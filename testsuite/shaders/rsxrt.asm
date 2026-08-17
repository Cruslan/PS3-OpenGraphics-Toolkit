!!FP1.0
# =============================================================================
# PlayStation 3 RSX Reality Synthesizer (NVIDIA G70 / NV47) Ray Tracing Engine
# Target: Shader Model 3.0 Fragment Program (FP32 vector ALUs)
# =============================================================================

# Dynamic Program Parameters updated by PPU
PARAM cam_pos   = program.local[0]; # Camera Position (x, y, z)
PARAM cam_fwd   = program.local[1]; # Camera Forward Vector (x, y, z)
PARAM cam_right = program.local[2]; # Camera Right Vector (x, y, z)
PARAM cam_up    = program.local[3]; # Camera Up Vector (x, y, z)
PARAM light_pos = program.local[4]; # Point Light Position (x, y, z)
PARAM sphere0   = program.local[5]; # Sphere 0: (x, y, z, radius)
PARAM sphere1   = program.local[6]; # Sphere 1: (x, y, z, radius)
PARAM sphere2   = program.local[7]; # Sphere 2: (x, y, z, radius)

# Material and Scene Constants
PARAM col_sph0  = {0.95, 0.22, 0.22, 0.65}; # Red Sphere (color.rgb, refl)
PARAM col_sph1  = {0.20, 0.90, 0.40, 0.60}; # Emerald Green Sphere
PARAM col_sph2  = {0.25, 0.60, 0.95, 0.70}; # Sapphire Blue Sphere
PARAM plane_cfg = {-1.0, 1.5, 0.40, 0.0};    # Floor (y_height, tile_size, refl)
PARAM sky_top   = {0.05, 0.12, 0.25, 1.0};   # Zenith Sky Color
PARAM sky_bot   = {0.50, 0.70, 0.90, 1.0};   # Horizon Sky Color
PARAM ambient   = {0.12, 0.14, 0.18, 1.0};   # Ambient Fill Light

# Register Declarations
TEMP ro, rd, oc, n, l, v, h;
TEMP t, tmin, disc, b, c;
TEMP col, hit_p, hit_mat, hit_refl;
TEMP r_ro, r_rd, r_oc, r_n, r_l, r_v, r_h, r_p;
TEMP r_t, r_tmin, r_disc, r_b, r_c, r_mat, r_col;
TEMP checker, fx, fz, diff, spec;

# =============================================================================
# 1. PRIMARY RAY GENERATION
# =============================================================================
MOV ro, cam_pos;
MOV rd.xyz, cam_fwd;
MAD rd.xyz, fragment.texcoord[0].x, cam_right, rd;
MAD rd.xyz, fragment.texcoord[0].y, cam_up, rd;
DP3 rd.w, rd, rd;
RSQ rd.w, rd.w;
MUL rd.xyz, rd, rd.w;

# Initialize Sky Gradient
ADD t.x, rd.y, 0.35;
MAX t.x, t.x, 0.0;
MIN t.x, t.x, 1.0;
LRP col.xyz, t.x, sky_bot, sky_top;
MOV col.w, 1.0;
MOV tmin.x, 9999.0;
MOV hit_mat.x, 0.0; # 0=Sky, 1=Sph0, 2=Sph1, 3=Sph2, 4=Floor

# =============================================================================
# 2. PRIMARY RAY INTERSECTIONS
# =============================================================================

# --- Sphere 0 ---
SUB oc.xyz, ro, sphere0;
DP3 b.x, oc, rd;
DP3 c.x, oc, oc;
MAD c.x, -sphere0.w, sphere0.w, c.x;
MUL disc.x, b.x, b.x;
SUB disc.x, disc.x, c.x;
RSQ disc.y, disc.x;
RCP disc.z, disc.y;
SUB t.x, -b.x, disc.z;

SLT disc.w, 0.0, disc.x;
SLT t.w, 0.001, t.x;
MUL t.w, t.w, disc.w;
SLT tmin.w, t.x, tmin.x;
MUL t.w, t.w, tmin.w;
LRP tmin.x, t.w, t.x, tmin.x;
LRP hit_mat.x, t.w, 1.0, hit_mat.x;

# --- Sphere 1 ---
SUB oc.xyz, ro, sphere1;
DP3 b.x, oc, rd;
DP3 c.x, oc, oc;
MAD c.x, -sphere1.w, sphere1.w, c.x;
MUL disc.x, b.x, b.x;
SUB disc.x, disc.x, c.x;
RSQ disc.y, disc.x;
RCP disc.z, disc.y;
SUB t.x, -b.x, disc.z;

SLT disc.w, 0.0, disc.x;
SLT t.w, 0.001, t.x;
MUL t.w, t.w, disc.w;
SLT tmin.w, t.x, tmin.x;
MUL t.w, t.w, tmin.w;
LRP tmin.x, t.w, t.x, tmin.x;
LRP hit_mat.x, t.w, 2.0, hit_mat.x;

# --- Sphere 2 ---
SUB oc.xyz, ro, sphere2;
DP3 b.x, oc, rd;
DP3 c.x, oc, oc;
MAD c.x, -sphere2.w, sphere2.w, c.x;
MUL disc.x, b.x, b.x;
SUB disc.x, disc.x, c.x;
RSQ disc.y, disc.x;
RCP disc.z, disc.y;
SUB t.x, -b.x, disc.z;

SLT disc.w, 0.0, disc.x;
SLT t.w, 0.001, t.x;
MUL t.w, t.w, disc.w;
SLT tmin.w, t.x, tmin.x;
MUL t.w, t.w, tmin.w;
LRP tmin.x, t.w, t.x, tmin.x;
LRP hit_mat.x, t.w, 3.0, hit_mat.x;

# --- Ground Plane ---
SUB t.y, plane_cfg.x, ro.y;
RCP t.z, rd.y;
MUL t.x, t.y, t.z;
SLT t.w, 0.001, t.x;
SLT t.y, rd.y, -0.0001;
MUL t.w, t.w, t.y;
SLT tmin.w, t.x, tmin.x;
MUL t.w, t.w, tmin.w;
LRP tmin.x, t.w, t.x, tmin.x;
LRP hit_mat.x, t.w, 4.0, hit_mat.x;

# =============================================================================
# 3. PRIMARY LIGHTING & SHADING
# =============================================================================
MAD hit_p.xyz, rd, tmin.x, ro;

# Compute Normal Vector
SUB n.xyz, hit_p, sphere0;
SLT t.x, 1.5, hit_mat.x;
SUB oc.xyz, hit_p, sphere1;
LRP n.xyz, t.x, oc, n;
SLT t.x, 2.5, hit_mat.x;
SUB oc.xyz, hit_p, sphere2;
LRP n.xyz, t.x, oc, n;
SLT t.x, 3.5, hit_mat.x;
LRP n.xyz, t.x, {0.0, 1.0, 0.0, 0.0}, n;

DP3 n.w, n, n;
RSQ n.w, n.w;
MUL n.xyz, n, n.w;

# Light Vector & Distance Attenuation
SUB l.xyz, light_pos, hit_p;
DP3 l.w, l, l;
RSQ l.w, l.w;
MUL l.xyz, l, l.w;

# Lambertian Diffuse
DP3 diff.x, n, l;
MAX diff.x, diff.x, 0.0;

# Blinn-Phong Specular
SUB v.xyz, ro, hit_p;
DP3 v.w, v, v;
RSQ v.w, v.w;
MUL v.xyz, v, v.w;
ADD h.xyz, l, v;
DP3 h.w, h, h;
RSQ h.w, h.w;
MUL h.xyz, h, h.w;
DP3 spec.x, n, h;
MAX spec.x, spec.x, 0.0;
POW spec.x, spec.x, 32.0;

# Base Color per Material
MOV hit_refl.x, col_sph0.w;
MOV diff.yzw, col_sph0.xxyz;
SLT t.x, 1.5, hit_mat.x;
LRP diff.yzw, t.x, col_sph1.xxyz, diff;
LRP hit_refl.x, t.x, col_sph1.w, hit_refl.x;
SLT t.x, 2.5, hit_mat.x;
LRP diff.yzw, t.x, col_sph2.xxyz, diff;
LRP hit_refl.x, t.x, col_sph2.w, hit_refl.x;

# Checkered Floor Color
MUL fx.x, hit_p.x, 0.5;
MUL fz.x, hit_p.z, 0.5;
FLR fx.y, fx.x;
FLR fz.y, fz.x;
ADD checker.x, fx.y, fz.y;
FRC checker.y, checker.x;
SLT checker.z, 0.25, checker.y;
LRP checker.xyz, checker.z, {0.85, 0.85, 0.90, 1.0}, {0.18, 0.20, 0.24, 1.0};
SLT t.x, 3.5, hit_mat.x;
LRP diff.yzw, t.x, checker.xxyz, diff;
LRP hit_refl.x, t.x, plane_cfg.z, hit_refl.x;

# Combine Diffuse + Ambient + Specular
MAD diff.xyz, diff.yzww, diff.x, ambient;
ADD diff.xyz, diff, spec.x;

# Blend Primary Hit
SLT t.x, 0.5, hit_mat.x;
LRP col.xyz, t.x, diff, col;

# =============================================================================
# 4. SECONDARY RAY REFLECTION BOUNCE
# =============================================================================
# Reflection Direction: r_rd = rd - 2*(rd . n)*n
DP3 t.x, rd, n;
MUL t.x, t.x, 2.0;
MAD r_rd.xyz, -t.x, n, rd;
DP3 r_rd.w, r_rd, r_rd;
RSQ r_rd.w, r_rd.w;
MUL r_rd.xyz, r_rd, r_rd.w;
MAD r_ro.xyz, n, 0.005, hit_p; # Offset to prevent self-intersection

# Secondary Sky
ADD r_t.x, r_rd.y, 0.35;
MAX r_t.x, r_t.x, 0.0;
MIN r_t.x, r_t.x, 1.0;
LRP r_col.xyz, r_t.x, sky_bot, sky_top;
MOV r_tmin.x, 9999.0;
MOV r_mat.x, 0.0;

# Secondary: Sphere 0
SUB r_oc.xyz, r_ro, sphere0;
DP3 r_b.x, r_oc, r_rd;
DP3 r_c.x, r_oc, r_oc;
MAD r_c.x, -sphere0.w, sphere0.w, r_c.x;
MUL r_disc.x, r_b.x, r_b.x;
SUB r_disc.x, r_disc.x, r_c.x;
RSQ r_disc.y, r_disc.x;
RCP r_disc.z, r_disc.y;
SUB r_t.x, -r_b.x, r_disc.z;

SLT r_disc.w, 0.0, r_disc.x;
SLT r_t.w, 0.001, r_t.x;
MUL r_t.w, r_t.w, r_disc.w;
SLT r_tmin.w, r_t.x, r_tmin.x;
MUL r_t.w, r_t.w, r_tmin.w;
LRP r_tmin.x, r_t.w, r_t.x, r_tmin.x;
LRP r_mat.x, r_t.w, 1.0, r_mat.x;

# Secondary: Sphere 1
SUB r_oc.xyz, r_ro, sphere1;
DP3 r_b.x, r_oc, r_rd;
DP3 r_c.x, r_oc, r_oc;
MAD r_c.x, -sphere1.w, sphere1.w, r_c.x;
MUL r_disc.x, r_b.x, r_b.x;
SUB r_disc.x, r_disc.x, r_c.x;
RSQ r_disc.y, r_disc.x;
RCP r_disc.z, r_disc.y;
SUB r_t.x, -r_b.x, r_disc.z;

SLT r_disc.w, 0.0, r_disc.x;
SLT r_t.w, 0.001, r_t.x;
MUL r_t.w, r_t.w, r_disc.w;
SLT r_tmin.w, r_t.x, r_tmin.x;
MUL r_t.w, r_t.w, r_tmin.w;
LRP r_tmin.x, r_t.w, r_t.x, r_tmin.x;
LRP r_mat.x, r_t.w, 2.0, r_mat.x;

# Secondary: Sphere 2
SUB r_oc.xyz, r_ro, sphere2;
DP3 r_b.x, r_oc, r_rd;
DP3 r_c.x, r_oc, r_oc;
MAD r_c.x, -sphere2.w, sphere2.w, r_c.x;
MUL r_disc.x, r_b.x, r_b.x;
SUB r_disc.x, r_disc.x, r_c.x;
RSQ r_disc.y, r_disc.x;
RCP r_disc.z, r_disc.y;
SUB r_t.x, -r_b.x, r_disc.z;

SLT r_disc.w, 0.0, r_disc.x;
SLT r_t.w, 0.001, r_t.x;
MUL r_t.w, r_t.w, r_disc.w;
SLT r_tmin.w, r_t.x, r_tmin.x;
MUL r_t.w, r_t.w, r_tmin.w;
LRP r_tmin.x, r_t.w, r_t.x, r_tmin.x;
LRP r_mat.x, r_t.w, 3.0, r_mat.x;

# Secondary: Ground Plane
SUB r_t.y, plane_cfg.x, r_ro.y;
RCP r_t.z, r_rd.y;
MUL r_t.x, r_t.y, r_t.z;
SLT r_t.w, 0.001, r_t.x;
SLT r_t.y, r_rd.y, -0.0001;
MUL r_t.w, r_t.w, r_t.y;
SLT r_tmin.w, r_t.x, r_tmin.x;
MUL r_t.w, r_t.w, r_tmin.w;
LRP r_tmin.x, r_t.w, r_t.x, r_tmin.x;
LRP r_mat.x, r_t.w, 4.0, r_mat.x;

# Secondary Shading
MAD r_p.xyz, r_rd, r_tmin.x, r_ro;
SUB r_n.xyz, r_p, sphere0;
SLT r_t.x, 1.5, r_mat.x;
SUB r_oc.xyz, r_p, sphere1;
LRP r_n.xyz, r_t.x, r_oc, r_n;
SLT r_t.x, 2.5, r_mat.x;
SUB r_oc.xyz, r_p, sphere2;
LRP r_n.xyz, r_t.x, r_oc, r_n;
SLT r_t.x, 3.5, r_mat.x;
LRP r_n.xyz, r_t.x, {0.0, 1.0, 0.0, 0.0}, r_n;
DP3 r_n.w, r_n, r_n;
RSQ r_n.w, r_n.w;
MUL r_n.xyz, r_n, r_n.w;

SUB r_l.xyz, light_pos, r_p;
DP3 r_l.w, r_l, r_l;
RSQ r_l.w, r_l.w;
MUL r_l.xyz, r_l, r_l.w;
DP3 r_disc.x, r_n, r_l;
MAX r_disc.x, r_disc.x, 0.0;

# Secondary Material Color
MOV r_disc.yzw, col_sph0.xxyz;
SLT r_t.x, 1.5, r_mat.x;
LRP r_disc.yzw, r_t.x, col_sph1.xxyz, r_disc;
SLT r_t.x, 2.5, r_mat.x;
LRP r_disc.yzw, r_t.x, col_sph2.xxyz, r_disc;

MUL fx.x, r_p.x, 0.5;
MUL fz.x, r_p.z, 0.5;
FLR fx.y, fx.x;
FLR fz.y, fz.x;
ADD checker.x, fx.y, fz.y;
FRC checker.y, checker.x;
SLT checker.z, 0.25, checker.y;
LRP checker.xyz, checker.z, {0.85, 0.85, 0.90, 1.0}, {0.18, 0.20, 0.24, 1.0};
SLT r_t.x, 3.5, r_mat.x;
LRP r_disc.yzw, r_t.x, checker.xxyz, r_disc;

MAD r_disc.xyz, r_disc.yzww, r_disc.x, ambient;
SLT r_t.x, 0.5, r_mat.x;
LRP r_col.xyz, r_t.x, r_disc, r_col;

# Blend Primary Color with Secondary Reflection
SLT t.x, 0.5, hit_mat.x;
MUL hit_refl.x, hit_refl.x, t.x;
LRP col.xyz, hit_refl.x, r_col, col;

MOV result.color, col;
END
