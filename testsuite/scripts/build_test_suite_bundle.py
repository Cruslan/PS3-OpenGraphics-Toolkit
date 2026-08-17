#!/usr/bin/env python3
"""
PS3 RSX Conformance Test Suite Bundle Builder.
Compiles all .fcg test shaders with rsxcomp and generates C data source/header
containing metadata, descriptions, opcodes, and embedded .fpo microcode for all 57 tests.
"""

import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BASE_DIR = os.path.dirname(SCRIPT_DIR)
TOOLKIT_DIR = os.path.abspath(os.path.join(BASE_DIR, ".."))
default_rsxcomp = os.path.join(TOOLKIT_DIR, "build", "rsxcomp")
if not os.path.isfile(default_rsxcomp):
    default_rsxcomp = os.path.join(TOOLKIT_DIR, "rsxcomp", "rsxcomp")
if not os.path.isfile(default_rsxcomp):
    default_rsxcomp = os.path.join(TOOLKIT_DIR, "rsxcomp")
RSXCOMP = os.environ.get("PS3_SHADER_COMPILER", default_rsxcomp)
SHADERS_DIR = os.path.join(BASE_DIR, "shaders")
INC_DIR = os.path.join(BASE_DIR, "include")
SRC_DIR = os.path.join(BASE_DIR, "source")
BUILD_DIR = os.path.join(BASE_DIR, "build")

test_entries = [
    # 1: Sphere RT
    {
        "id": 1,
        "name": "Ray Tracing Chrome Sphere",
        "category": "RSX GPU Hardware Ray Tracing Pipeline",
        "opcode": "NV47 / G70 24 Fragment Pipelines @ 500 MHz",
        "visual_desc": "Original RSX Ray Tracing scene: Floor reflections, dynamic shadows, and animated chrome sphere.",
        "is_stub": False,
        "is_sphere_rt": True,
        "shader_file": "rsxrt.fcg"
    },
    # 2..55: SM3.0 Intrinsics
    {
        "id": 2,
        "name": "add / +",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "Native ADD / ADDR instruction",
        "visual_desc": "Red and green horizontal color waves combine to create bright yellow at their intersection.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_02_add.fcg"
    },
    {
        "id": 3,
        "name": "sub / -",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "Operand negate bit (hw[2] |= NEGATE) + ADD",
        "visual_desc": "Moving bright light beam subtracted from background gradient creates a dynamic high-contrast shadow.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_03_sub.fcg"
    },
    {
        "id": 4,
        "name": "mul / *",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "Native MUL / MULR instruction",
        "visual_desc": "Rotating coordinate grid modulated by radial luminance coefficient expanding from the center.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_04_mul.fcg"
    },
    {
        "id": 5,
        "name": "mad / fma",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "Single-cycle 128-bit MAD / MADR (a * b + c)",
        "visual_desc": "Frequency-modulated rings scaled and offset via single-cycle MAD to generate rich interference patterns.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_05_mad.fcg"
    },
    {
        "id": 6,
        "name": "div / /",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "RCP + MUL hardware reciprocal pipeline",
        "visual_desc": "Spatial coordinates divided to form stepped, radially sliced chromatic pixel blocks outward from center.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_06_div.fcg"
    },
    {
        "id": 7,
        "name": "rcp",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "Native RCP / RCPR (IEEE-754 1/x)",
        "visual_desc": "Luminous sphere formed by 1/x hyperbolic attenuation decaying outward from focal center.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_07_rcp.fcg"
    },
    {
        "id": 8,
        "name": "rsqrt / rsq",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "Native RSQ / RSQR (1 / sqrt(x))",
        "visual_desc": "Point light source radiating with inverse square root (1/sqrt) physical light attenuation curve.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_08_rsq.fcg"
    },
    {
        "id": 9,
        "name": "sqrt",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "RSQ + RCP or RSQ + MUL pipeline",
        "visual_desc": "Square root (sqrt) producing a smooth parabolic expanding color gradient outward from center.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_09_sqrt.fcg"
    },
    {
        "id": 10,
        "name": "abs",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "Source register Absolute bitfield (hw[1] |= (1 << 29))",
        "visual_desc": "Absolute value (abs) across X and Y axes creating flawless four-quadrant mirror symmetry.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_10_abs.fcg"
    },
    {
        "id": 11,
        "name": "neg / -x",
        "category": "Shader Model 3.0 Basic Arithmetic",
        "opcode": "Source register Negate bitfield (NVFX_FP_REG_NEGATE)",
        "visual_desc": "Color gradient phase inverted via register negation (negate) creating opposing chromatic flow.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_11_neg.fcg"
    },
    {
        "id": 12,
        "name": "sin",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "Native SIN / SINR hardware Taylor/Chebyshev ALU",
        "visual_desc": "Smooth animated circular sine waves propagating outward in rippling concentric rings.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_12_sin.fcg"
    },
    {
        "id": 13,
        "name": "cos",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "Native COS / COSR hardware trigonometry ALU",
        "visual_desc": "Harmonic cosine rings rippling outward with 90-degree quadrature phase offset relative to sine.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_13_cos.fcg"
    },
    {
        "id": 14,
        "name": "tan",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "SIN + COS + RCP + MUL (sin(x) / cos(x))",
        "visual_desc": "Periodic asymptotic transitions of tangent function streaming across screen as vertical stripes.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_14_tan.fcg"
    },
    {
        "id": 15,
        "name": "asin",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "Minimax rational polynomial approximation",
        "visual_desc": "Arcsine function generating spherical horizon compression with steepening slope towards edges.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_15_asin.fcg"
    },
    {
        "id": 16,
        "name": "acos",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "(PI / 2.0) - asin(x) transformation",
        "visual_desc": "Arccosine function generating linear angular transition between 0 and Pi along vertical axis.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_16_acos.fcg"
    },
    {
        "id": 17,
        "name": "atan",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "Rational minimax polynomial approximation",
        "visual_desc": "Arctangent function producing smooth S-curve chromatic band steep at center and flattening at edges.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_17_atan.fcg"
    },
    {
        "id": 18,
        "name": "atan2",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "Sign-aware 4-quadrant atan(y / x) emulation",
        "visual_desc": "Full 360-degree angular atan2 producing continuous spectrum color wheel rotating around origin.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_18_atan2.fcg"
    },
    {
        "id": 19,
        "name": "radians",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "MUL(deg, PI / 180.0) constant factor scaling",
        "visual_desc": "Degrees-to-radians conversion forming 12 discrete equiangular chromatic compass sectors.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_19_radians.fcg"
    },
    {
        "id": 20,
        "name": "degrees",
        "category": "Shader Model 3.0 Trigonometry",
        "opcode": "MUL(rad, 180.0 / PI) constant factor scaling",
        "visual_desc": "Radians-to-degrees conversion displaying scaled angular chromatic quantized stepped rings.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_20_degrees.fcg"
    },
    {
        "id": 21,
        "name": "exp2",
        "category": "Shader Model 3.0 Exponential & Logarithmic",
        "opcode": "Native EX2 / EX2R (Hardware 2^x)",
        "visual_desc": "Base-2 exponential growth (exp2) producing illumination curve steepening rapidly to the right.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_21_exp2.fcg"
    },
    {
        "id": 22,
        "name": "exp",
        "category": "Shader Model 3.0 Exponential & Logarithmic",
        "opcode": "MUL(x, log2(e)) + EX2",
        "visual_desc": "Natural exponential (exp) Gaussian distribution generating smooth radiant light corona at center.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_22_exp.fcg"
    },
    {
        "id": 23,
        "name": "log2",
        "category": "Shader Model 3.0 Exponential & Logarithmic",
        "opcode": "Native LG2 / LG2R (Hardware log2)",
        "visual_desc": "Base-2 logarithm (log2) compressing high-range inputs to deliver wide dynamic range.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_23_log2.fcg"
    },
    {
        "id": 24,
        "name": "log",
        "category": "Shader Model 3.0 Exponential & Logarithmic",
        "opcode": "LG2(x) * (1 / log2(e))",
        "visual_desc": "Natural logarithm (log) rendering logarithmic tonemapping gradient across radial distance.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_24_log.fcg"
    },
    {
        "id": 25,
        "name": "log10",
        "category": "Shader Model 3.0 Exponential & Logarithmic",
        "opcode": "LG2(x) * log10(2)",
        "visual_desc": "Base-10 logarithm (log10) forming decibel-scale stepped chromatic intensity bands.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_25_log10.fcg"
    },
    {
        "id": 26,
        "name": "pow",
        "category": "Shader Model 3.0 Exponential & Logarithmic",
        "opcode": "LG2(x) * y -> EX2 or POW",
        "visual_desc": "Power function (pow) generating high-intensity specular highlight concentrated at center.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_26_pow.fcg"
    },
    {
        "id": 27,
        "name": "floor",
        "category": "Shader Model 3.0 Rounding & Truncation",
        "opcode": "Native FLR / FLRR instruction",
        "visual_desc": "Floor rounding quantizing viewport into 12x12 chunky mosaic pixel tiles.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_27_floor.fcg"
    },
    {
        "id": 28,
        "name": "ceil",
        "category": "Shader Model 3.0 Rounding & Truncation",
        "opcode": "-FLR(-x) emulation",
        "visual_desc": "Ceiling rounding forming upward-stepped staircase quantization blocks.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_28_ceil.fcg"
    },
    {
        "id": 29,
        "name": "frac / fract",
        "category": "Shader Model 3.0 Rounding & Truncation",
        "opcode": "Native FRC / FRCR instruction",
        "visual_desc": "Fractional part (frac) tiling viewport into repeating 8x8 sawtooth gradient grid.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_29_frac.fcg"
    },
    {
        "id": 30,
        "name": "round",
        "category": "Shader Model 3.0 Rounding & Truncation",
        "opcode": "FLR(x + 0.5) transformation",
        "visual_desc": "Nearest integer rounding (round) forming sharply bounded constant color stripes.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_30_round.fcg"
    },
    {
        "id": 31,
        "name": "trunc",
        "category": "Shader Model 3.0 Rounding & Truncation",
        "opcode": "sign(x) * floor(abs(x))",
        "visual_desc": "Truncation toward zero (trunc) forming widened symmetric step plateau around origin.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_31_trunc.fcg"
    },
    {
        "id": 32,
        "name": "sign",
        "category": "Shader Model 3.0 Rounding & Truncation",
        "opcode": "(x > 0) - (x < 0) (SGT, SLT, SUB)",
        "visual_desc": "Sign function (sign) generating 4 solid red, green, blue, and yellow quadrant blocks.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_32_sign.fcg"
    },
    {
        "id": 33,
        "name": "min",
        "category": "Shader Model 3.0 Clamping & Interpolation",
        "opcode": "Native MIN / MINR instruction",
        "visual_desc": "Minimum function (min) computing intersection volume of two moving circular patterns.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_33_min.fcg"
    },
    {
        "id": 34,
        "name": "max",
        "category": "Shader Model 3.0 Clamping & Interpolation",
        "opcode": "Native MAX / MAXR instruction",
        "visual_desc": "Maximum function (max) rendering composite union mask of two moving light beams.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_34_max.fcg"
    },
    {
        "id": 35,
        "name": "clamp",
        "category": "Shader Model 3.0 Clamping & Interpolation",
        "opcode": "MIN(MAX(x, min_val), max_val)",
        "visual_desc": "Range clamp flattening oscillating wave crests and troughs between 0.2 and 0.8.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_35_clamp.fcg"
    },
    {
        "id": 36,
        "name": "saturate",
        "category": "Shader Model 3.0 Clamping & Interpolation",
        "opcode": "Hardware output saturation modifier (NVFX_FP_OP_OUT_SAT)",
        "visual_desc": "Saturation function locking dynamic HDR intensities strictly into [0.0, 1.0] interval.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_36_saturate.fcg"
    },
    {
        "id": 37,
        "name": "lerp / mix",
        "category": "Shader Model 3.0 Clamping & Interpolation",
        "opcode": "MAD(weight, b - a, a) single-cycle interpolation",
        "visual_desc": "Linear interpolation (lerp/mix) providing seamless chromatic transition between cyan and magenta.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_37_lerp.fcg"
    },
    {
        "id": 38,
        "name": "step",
        "category": "Shader Model 3.0 Clamping & Interpolation",
        "opcode": "Native SGE(x, edge) instruction",
        "visual_desc": "Step threshold splitting viewport into razor-sharp black and white halves at screen center.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_38_step.fcg"
    },
    {
        "id": 39,
        "name": "smoothstep",
        "category": "Shader Model 3.0 Clamping & Interpolation",
        "opcode": "Hermite interpolation: t = clamp; t*t*(3 - 2*t)",
        "visual_desc": "Smoothstep Hermite interpolation rendering glowing ring with softly feathered boundaries.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_39_smoothstep.fcg"
    },
    {
        "id": 40,
        "name": "dot (2D)",
        "category": "Shader Model 3.0 Vector Algebra",
        "opcode": "Native DP2 / DP2A instruction",
        "visual_desc": "2D dot product (dot2/DP2) forming linear luminance ramp parallel to rotating 2D direction vector.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_40_dot2.fcg"
    },
    {
        "id": 41,
        "name": "dot (3D)",
        "category": "Shader Model 3.0 Vector Algebra",
        "opcode": "Native DP3 instruction",
        "visual_desc": "3D dot product (dot3/DP3) illuminating 3D diffuse shaded sphere under orbital dynamic point light.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_41_dot3.fcg"
    },
    {
        "id": 42,
        "name": "dot (4D)",
        "category": "Shader Model 3.0 Vector Algebra",
        "opcode": "Native DP4 / DPH instruction",
        "visual_desc": "4D dot product (dot4/DP4) executing 4-dimensional homogeneous coordinate projection transform.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_42_dot4.fcg"
    },
    {
        "id": 43,
        "name": "cross",
        "category": "Shader Model 3.0 Vector Algebra",
        "opcode": "Swizzled MUL + MAD instruction pair",
        "visual_desc": "3D cross product computing surface normal orthogonal to tangent and binormal vectors.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_43_cross.fcg"
    },
    {
        "id": 44,
        "name": "length",
        "category": "Shader Model 3.0 Vector Algebra",
        "opcode": "DP3(v, v) -> RSQ -> RCP pipeline",
        "visual_desc": "Vector length generating concentric circular Euclidean distance isocontour rings.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_44_length.fcg"
    },
    {
        "id": 45,
        "name": "distance",
        "category": "Shader Model 3.0 Vector Algebra",
        "opcode": "length(p1 - p0)",
        "visual_desc": "Euclidean distance between two moving coordinates creating dynamic dipolar field interaction.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_45_distance.fcg"
    },
    {
        "id": 46,
        "name": "normalize",
        "category": "Shader Model 3.0 Vector Algebra",
        "opcode": "v * RSQ(dot(v, v)) single-cycle normalization",
        "visual_desc": "Unit vector normalization mapping 3D hemisphere surface normals to RGB color channels.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_46_normalize.fcg"
    },
    {
        "id": 47,
        "name": "reflect",
        "category": "Shader Model 3.0 Optics & Physics",
        "opcode": "i - 2.0 * dot(i, n) * n (DP3 + MAD)",
        "visual_desc": "Optical reflection (reflect) projecting dynamic virtual environment map onto spinning sphere.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_47_reflect.fcg"
    },
    {
        "id": 48,
        "name": "refract",
        "category": "Shader Model 3.0 Optics & Physics",
        "opcode": "Snell's law vector refraction microcode sequence",
        "visual_desc": "Snell's law refraction optically distorting background grid behind spherical glass lens.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_48_refract.fcg"
    },
    {
        "id": 49,
        "name": "faceforward",
        "category": "Shader Model 3.0 Optics & Physics",
        "opcode": "dot(ng, i) < 0 ? n : -n conditional selection",
        "visual_desc": "Faceforward function dynamically orienting surface normals toward viewer based on incident ray.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_49_faceforward.fcg"
    },
    {
        "id": 50,
        "name": "slt / <",
        "category": "Shader Model 3.0 Relational Logic",
        "opcode": "Native SLT / SLTR instruction",
        "visual_desc": "Set-on-less-than comparison (slt / <) partitioning screen into binary contrasting halves along diagonal.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_50_slt.fcg"
    },
    {
        "id": 51,
        "name": "sge / >=",
        "category": "Shader Model 3.0 Relational Logic",
        "opcode": "Native SGE / SGER instruction",
        "visual_desc": "Set-on-greater-equal comparison (sge / >=) generating sharp circular binary threshold mask.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_51_sge.fcg"
    },
    {
        "id": 52,
        "name": "sgt / >",
        "category": "Shader Model 3.0 Relational Logic",
        "opcode": "Native SGT / SGTR instruction",
        "visual_desc": "Set-on-greater-than comparison (sgt / >) rendering moving high-contrast binary zebra pattern.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_52_sgt.fcg"
    },
    {
        "id": 53,
        "name": "sle / <=",
        "category": "Shader Model 3.0 Relational Logic",
        "opcode": "Native SLE / SLER instruction",
        "visual_desc": "Set-on-less-equal comparison (sle / <=) illuminating inner circular disk with binary threshold.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_53_sle.fcg"
    },
    {
        "id": 54,
        "name": "seq / ==",
        "category": "Shader Model 3.0 Relational Logic",
        "opcode": "Native SEQ / SEQR instruction",
        "visual_desc": "Set-on-equal comparison (seq / ==) illuminating fine animated laser scanline across matching coordinates.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_54_seq.fcg"
    },
    {
        "id": 55,
        "name": "sne / !=",
        "category": "Shader Model 3.0 Relational Logic",
        "opcode": "Native SNE / SNER instruction",
        "visual_desc": "Set-on-not-equal comparison (sne / !=) illuminating entire viewport except narrow central crosshair axes.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_55_sne.fcg"
    },
    # 56..57: Advanced SM3.0 Features
    {
        "id": 56,
        "name": "Screen-Space Derivatives (ddx / ddy / fwidth)",
        "category": "Pixel/Fragment Shader 3.0",
        "opcode": "2x2 Pixel Quad Hardware ALU (DDX / DDY)",
        "visual_desc": "2x2 pixel quad finite difference derivatives detecting sharp geometric contour edge boundaries.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_56_deriv.fcg"
    },
    {
        "id": 57,
        "name": "Pixel Discard (discard / kil)",
        "category": "Pixel/Fragment Shader 3.0",
        "opcode": "Native KIL instruction (NV40_3D_FP_CONTROL_KIL)",
        "visual_desc": "Conditional pixel kill (discard / kil) carving perforated circular hole grid revealing background.",
        "is_stub": False,
        "is_sphere_rt": False,
        "shader_file": "test_57_discard.fcg"
    }
]

print(f"Building {len(test_entries)} Conformance Test Suite Shaders...")

# Compile all shader files to .fpo
compiled_fpos = {}

os.makedirs(BUILD_DIR, exist_ok=True)

for entry in test_entries:
    if entry["shader_file"]:
        src_path = os.path.join(SHADERS_DIR, entry["shader_file"])
        out_fpo = os.path.join(BUILD_DIR, f"{os.path.splitext(entry['shader_file'])[0]}.tmp.fpo")
        cmd = [RSXCOMP, "-f", "-i", src_path, "-o", out_fpo]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res.returncode != 0:
            print(f"[ERROR] Failed to compile {src_path}:\n{res.stderr}")
            sys.exit(1)
        with open(out_fpo, "rb") as f:
            compiled_fpos[entry["id"]] = f.read()

total_count = len(test_entries)

# Generate C header
header_content = f"""/**
 * PS3 Reality Synthesizer (RSX) - Shader Model 3.0 Conformance Test Suite Data
 * Autogenerated metadata and binary lookup table for all {total_count} test scenes.
 */

#ifndef __TEST_SUITE_DATA_H__
#define __TEST_SUITE_DATA_H__

#include <stdint.h>
#include <stdbool.h>
#include <rsx/rsx_program.h>

#define TOTAL_TEST_COUNT {total_count}

typedef struct {{
    int         id;
    const char* name;
    const char* category;
    const char* opcode;
    const char* visual_desc;
    bool        is_stub;
    bool        is_sphere_rt;
    const uint8_t* fpo_data;
    uint32_t    fpo_size;
}} TestSuiteMetadata;

extern const TestSuiteMetadata g_test_suite_entries[TOTAL_TEST_COUNT];

#endif /* __TEST_SUITE_DATA_H__ */
"""

with open(os.path.join(INC_DIR, "test_suite_data.h"), "w") as f:
    f.write(header_content)

# Generate C source
src_lines = [
    '/**',
    f' * PS3 Reality Synthesizer (RSX) - Shader Model 3.0 Conformance Test Suite Data Arrays',
    f' * Embedded .fpo microcode byte arrays aligned to 16 bytes for all {total_count} test scenes.',
    ' */',
    '',
    '#include "test_suite_data.h"',
    ''
]

# Write byte arrays for compiled shaders
for entry in test_entries:
    t_id = entry["id"]
    if t_id in compiled_fpos:
        data = compiled_fpos[t_id]
        var_name = f"g_shader_fpo_test_{t_id:02d}"
        src_lines.append(f"static const uint8_t {var_name}[{len(data)}] __attribute__((aligned(16))) = {{")
        # Format bytes 16 per line
        for chunk_i in range(0, len(data), 16):
            chunk = data[chunk_i:chunk_i+16]
            hex_bytes = ", ".join(f"0x{b:02X}" for b in chunk)
            comma = "," if chunk_i + 16 < len(data) else ""
            src_lines.append(f"    {hex_bytes}{comma}")
        src_lines.append("};\n")

src_lines.append("const TestSuiteMetadata g_test_suite_entries[TOTAL_TEST_COUNT] = {")

for i, entry in enumerate(test_entries):
    t_id = entry["id"]
    is_stub_str = "true" if entry["is_stub"] else "false"
    is_rt_str = "true" if entry["is_sphere_rt"] else "false"
    if t_id in compiled_fpos:
        fpo_ptr = f"g_shader_fpo_test_{t_id:02d}"
        fpo_sz = f"sizeof(g_shader_fpo_test_{t_id:02d})"
    else:
        fpo_ptr = "NULL"
        fpo_sz = "0"
    
    # Escape quotes
    name_esc = entry['name'].replace('"', '\\"')
    cat_esc = entry['category'].replace('"', '\\"')
    op_esc = entry['opcode'].replace('"', '\\"')
    v_esc = entry['visual_desc'].replace('"', '\\"')
    
    comma = "," if i < len(test_entries) - 1 else ""
    src_lines.append(f"""    {{
        .id = {entry['id']},
        .name = "{name_esc}",
        .category = "{cat_esc}",
        .opcode = "{op_esc}",
        .visual_desc = "{v_esc}",
        .is_stub = {is_stub_str},
        .is_sphere_rt = {is_rt_str},
        .fpo_data = {fpo_ptr},
        .fpo_size = {fpo_sz}
    }}{comma}""")

src_lines.append("};\n")

with open(os.path.join(SRC_DIR, "test_suite_data.c"), "w") as f:
    f.write("\n".join(src_lines))

print("Successfully generated include/test_suite_data.h and source/test_suite_data.c!")
