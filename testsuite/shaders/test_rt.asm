!!FP1.0
# RSX GPU Ray Tracing Fragment Program
# Inputs:
# fragment.texcoord[0] = (u, v, aspect, fov) in [-1, 1] range
# fragment.color = camera pos (xyz)
#
# Parameters:
PARAM sphere0 = {0.0, 1.0, 0.0, 1.0};
PARAM sphere1 = {-2.2, 0.7, 1.0, 0.7};
PARAM sphere2 = {2.2, 0.7, 1.0, 0.7};
PARAM light_pos = {3.0, 8.0, -4.0, 1.0};
PARAM plane_y = {-1.0, 0.0, 0.0, 0.0};
PARAM ambient = {0.15, 0.15, 0.2, 1.0};

TEMP ro, rd, oc, n, l, r;
TEMP t, tmin, disc, b, c;
TEMP col, hit_p;

# Normalized ray direction from UV
MOV ro, fragment.color;
MOV rd.xy, fragment.texcoord[0];
MOV rd.z, 1.0;
DP3 rd.w, rd, rd;
RSQ rd.w, rd.w;
MUL rd.xyz, rd, rd.w;

# Initialize background color (sky gradient)
LRP col.xyz, fragment.texcoord[0].y, {0.6, 0.75, 0.95, 1.0}, {0.1, 0.15, 0.25, 1.0};
MOV col.w, 1.0;
MOV tmin.x, 1000.0;

# Sphere 0 intersection
SUB oc.xyz, ro, sphere0;
DP3 b.x, oc, rd;
DP3 c.x, oc, oc;
MAD c.x, -sphere0.w, sphere0.w, c.x;
MUL disc.x, b.x, b.x;
SUB disc.x, disc.x, c.x;

# If disc > 0, compute t
RSQ disc.y, disc.x;
RCP disc.z, disc.y;
SUB t.x, -b.x, disc.z;

# Check if t > 0.001 and disc > 0
SLT disc.w, 0.0, disc.x;
SLT t.w, 0.001, t.x;
MUL t.w, t.w, disc.w;

# If hit and closer than tmin
SLT tmin.w, t.x, tmin.x;
MUL t.w, t.w, tmin.w;

# Update tmin
LRP tmin.x, t.w, t.x, tmin.x;

# Hit point & normal
MAD hit_p.xyz, rd, t.x, ro;
SUB n.xyz, hit_p, sphere0;
DP3 n.w, n, n;
RSQ n.w, n.w;
MUL n.xyz, n, n.w;

# Light vector
SUB l.xyz, light_pos, hit_p;
DP3 l.w, l, l;
RSQ l.w, l.w;
MUL l.xyz, l, l.w;

# Diffuse
DP3 l.x, n, l;
MAX l.x, l.x, 0.0;
MUL l.xyz, {0.9, 0.2, 0.2, 1.0}, l.x;
ADD l.xyz, l, ambient;

# Blend hit color
LRP col.xyz, t.w, l, col;

MOV result.color, col;
END
