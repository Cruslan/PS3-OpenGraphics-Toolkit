/**
 * PS3 Reality Synthesizer (RSX) - Conformance Test Suite Bundle Builder
 *
 * Compiles all HLSL/Cg (.fcg) test shaders using rsxcomp and generates C data
 * source (source/test_suite_data.c) and header (include/test_suite_data.h) files
 * containing test metadata, visual descriptions, hardware opcodes, and 16-byte
 * aligned embedded .fpo microcode byte arrays for all 57 conformance tests.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TOTAL_TESTS 86
#define MAX_PATH_LEN 1024
#define MAX_CMD_LEN 2048

/* Metadata definition structure for test suite items */
typedef struct {
    int         id;
    const char* name;
    const char* category;
    const char* opcode;
    const char* visual_desc;
    bool        is_stub;
    bool        is_sphere_rt;
    const char* shader_file;
} TestEntryDefinition;

/* Table of all 57 RSX Shader Model 3.0 conformance test scenes */
static const TestEntryDefinition g_test_definitions[TOTAL_TESTS] = {
    /* 1: Sphere Ray Tracing */
    {
        .id = 1,
        .name = "Ray Tracing Chrome Sphere",
        .category = "RSX GPU Hardware Ray Tracing Pipeline",
        .opcode = "NV47 / G70 24 Fragment Pipelines @ 500 MHz",
        .visual_desc = "Original RSX Ray Tracing scene: Floor reflections, dynamic shadows, and animated chrome sphere.",
        .is_stub = false,
        .is_sphere_rt = true,
        .shader_file = "test_01_rsxrt.fcg"
    },
    /* 2..55: SM3.0 Intrinsics & Core ALU Operations */
    {
        .id = 2,
        .name = "add / +",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "Native ADD / ADDR instruction",
        .visual_desc = "Red and green horizontal color waves combine to create bright yellow at their intersection.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_02_add.fcg"
    },
    {
        .id = 3,
        .name = "sub / -",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "Operand negate bit (hw[2] |= NEGATE) + ADD",
        .visual_desc = "Moving bright light beam subtracted from background gradient creates a dynamic high-contrast shadow.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_03_sub.fcg"
    },
    {
        .id = 4,
        .name = "mul / *",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "Native MUL / MULR instruction",
        .visual_desc = "Rotating coordinate grid modulated by radial luminance coefficient expanding from the center.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_04_mul.fcg"
    },
    {
        .id = 5,
        .name = "mad / fma",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "Single-cycle 128-bit MAD / MADR (a * b + c)",
        .visual_desc = "Frequency-modulated rings scaled and offset via single-cycle MAD to generate rich interference patterns.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_05_mad.fcg"
    },
    {
        .id = 6,
        .name = "div / /",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "RCP + MUL hardware reciprocal pipeline",
        .visual_desc = "Spatial coordinates divided to form stepped, radially sliced chromatic pixel blocks outward from center.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_06_div.fcg"
    },
    {
        .id = 7,
        .name = "rcp",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "Native RCP / RCPR (IEEE-754 1/x)",
        .visual_desc = "Luminous sphere formed by 1/x hyperbolic attenuation decaying outward from focal center.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_07_rcp.fcg"
    },
    {
        .id = 8,
        .name = "rsqrt / rsq",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "Native RSQ / RSQR (1 / sqrt(x))",
        .visual_desc = "Point light source radiating with inverse square root (1/sqrt) physical light attenuation curve.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_08_rsq.fcg"
    },
    {
        .id = 9,
        .name = "sqrt",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "RSQ + RCP or RSQ + MUL pipeline",
        .visual_desc = "Square root (sqrt) producing a smooth parabolic expanding color gradient outward from center.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_09_sqrt.fcg"
    },
    {
        .id = 10,
        .name = "abs",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "Source register Absolute bitfield (hw[1] |= (1 << 29))",
        .visual_desc = "Absolute value (abs) across X and Y axes creating flawless four-quadrant mirror symmetry.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_10_abs.fcg"
    },
    {
        .id = 11,
        .name = "neg / -x",
        .category = "Shader Model 3.0 Basic Arithmetic",
        .opcode = "Source register Negate bitfield (NVFX_FP_REG_NEGATE)",
        .visual_desc = "Color gradient phase inverted via register negation (negate) creating opposing chromatic flow.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_11_neg.fcg"
    },
    {
        .id = 12,
        .name = "sin",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "Native SIN / SINR hardware Taylor/Chebyshev ALU",
        .visual_desc = "Smooth animated circular sine waves propagating outward in rippling concentric rings.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_12_sin.fcg"
    },
    {
        .id = 13,
        .name = "cos",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "Native COS / COSR hardware trigonometry ALU",
        .visual_desc = "Harmonic cosine rings rippling outward with 90-degree quadrature phase offset relative to sine.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_13_cos.fcg"
    },
    {
        .id = 14,
        .name = "tan",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "SIN + COS + RCP + MUL (sin(x) / cos(x))",
        .visual_desc = "Periodic asymptotic transitions of tangent function streaming across screen as vertical stripes.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_14_tan.fcg"
    },
    {
        .id = 15,
        .name = "asin",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "Minimax rational polynomial approximation",
        .visual_desc = "Arcsine function generating spherical horizon compression with steepening slope towards edges.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_15_asin.fcg"
    },
    {
        .id = 16,
        .name = "acos",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "(PI / 2.0) - asin(x) transformation",
        .visual_desc = "Arccosine function generating linear angular transition between 0 and Pi along vertical axis.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_16_acos.fcg"
    },
    {
        .id = 17,
        .name = "atan",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "Rational minimax polynomial approximation",
        .visual_desc = "Arctangent function producing smooth S-curve chromatic band steep at center and flattening at edges.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_17_atan.fcg"
    },
    {
        .id = 18,
        .name = "atan2",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "Sign-aware 4-quadrant atan(y / x) emulation",
        .visual_desc = "Full 360-degree angular atan2 producing continuous spectrum color wheel rotating around origin.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_18_atan2.fcg"
    },
    {
        .id = 19,
        .name = "radians",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "MUL(deg, PI / 180.0) constant factor scaling",
        .visual_desc = "Degrees-to-radians conversion forming 12 discrete equiangular chromatic compass sectors.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_19_radians.fcg"
    },
    {
        .id = 20,
        .name = "degrees",
        .category = "Shader Model 3.0 Trigonometry",
        .opcode = "MUL(rad, 180.0 / PI) constant factor scaling",
        .visual_desc = "Radians-to-degrees conversion displaying scaled angular chromatic quantized stepped rings.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_20_degrees.fcg"
    },
    {
        .id = 21,
        .name = "exp2",
        .category = "Shader Model 3.0 Exponential & Logarithmic",
        .opcode = "Native EX2 / EX2R (Hardware 2^x)",
        .visual_desc = "Base-2 exponential growth (exp2) producing illumination curve steepening rapidly to the right.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_21_exp2.fcg"
    },
    {
        .id = 22,
        .name = "exp",
        .category = "Shader Model 3.0 Exponential & Logarithmic",
        .opcode = "MUL(x, log2(e)) + EX2",
        .visual_desc = "Natural exponential (exp) Gaussian distribution generating smooth radiant light corona at center.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_22_exp.fcg"
    },
    {
        .id = 23,
        .name = "log2",
        .category = "Shader Model 3.0 Exponential & Logarithmic",
        .opcode = "Native LG2 / LG2R (Hardware log2)",
        .visual_desc = "Base-2 logarithm (log2) compressing high-range inputs to deliver wide dynamic range.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_23_log2.fcg"
    },
    {
        .id = 24,
        .name = "log",
        .category = "Shader Model 3.0 Exponential & Logarithmic",
        .opcode = "LG2(x) * (1 / log2(e))",
        .visual_desc = "Natural logarithm (log) rendering logarithmic tonemapping gradient across radial distance.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_24_log.fcg"
    },
    {
        .id = 25,
        .name = "log10",
        .category = "Shader Model 3.0 Exponential & Logarithmic",
        .opcode = "LG2(x) * log10(2)",
        .visual_desc = "Base-10 logarithm (log10) forming decibel-scale stepped chromatic intensity bands.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_25_log10.fcg"
    },
    {
        .id = 26,
        .name = "pow",
        .category = "Shader Model 3.0 Exponential & Logarithmic",
        .opcode = "LG2(x) * y -> EX2 or POW",
        .visual_desc = "Power function (pow) generating high-intensity specular highlight concentrated at center.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_26_pow.fcg"
    },
    {
        .id = 27,
        .name = "floor",
        .category = "Shader Model 3.0 Rounding & Truncation",
        .opcode = "Native FLR / FLRR instruction",
        .visual_desc = "Floor rounding quantizing viewport into 12x12 chunky mosaic pixel tiles.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_27_floor.fcg"
    },
    {
        .id = 28,
        .name = "ceil",
        .category = "Shader Model 3.0 Rounding & Truncation",
        .opcode = "-FLR(-x) emulation",
        .visual_desc = "Ceiling rounding forming upward-stepped staircase quantization blocks.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_28_ceil.fcg"
    },
    {
        .id = 29,
        .name = "frac / fract",
        .category = "Shader Model 3.0 Rounding & Truncation",
        .opcode = "Native FRC / FRCR instruction",
        .visual_desc = "Fractional part (frac) tiling viewport into repeating 8x8 sawtooth gradient grid.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_29_frac.fcg"
    },
    {
        .id = 30,
        .name = "round",
        .category = "Shader Model 3.0 Rounding & Truncation",
        .opcode = "FLR(x + 0.5) transformation",
        .visual_desc = "Nearest integer rounding (round) forming sharply bounded constant color stripes.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_30_round.fcg"
    },
    {
        .id = 31,
        .name = "trunc",
        .category = "Shader Model 3.0 Rounding & Truncation",
        .opcode = "sign(x) * floor(abs(x))",
        .visual_desc = "Truncation toward zero (trunc) forming widened symmetric step plateau around origin.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_31_trunc.fcg"
    },
    {
        .id = 32,
        .name = "sign",
        .category = "Shader Model 3.0 Rounding & Truncation",
        .opcode = "(x > 0) - (x < 0) (SGT, SLT, SUB)",
        .visual_desc = "Sign function (sign) generating 4 solid red, green, blue, and yellow quadrant blocks.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_32_sign.fcg"
    },
    {
        .id = 33,
        .name = "min",
        .category = "Shader Model 3.0 Clamping & Interpolation",
        .opcode = "Native MIN / MINR instruction",
        .visual_desc = "Minimum function (min) computing intersection volume of two moving circular patterns.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_33_min.fcg"
    },
    {
        .id = 34,
        .name = "max",
        .category = "Shader Model 3.0 Clamping & Interpolation",
        .opcode = "Native MAX / MAXR instruction",
        .visual_desc = "Maximum function (max) rendering composite union mask of two moving light beams.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_34_max.fcg"
    },
    {
        .id = 35,
        .name = "clamp",
        .category = "Shader Model 3.0 Clamping & Interpolation",
        .opcode = "MIN(MAX(x, min_val), max_val)",
        .visual_desc = "Range clamp flattening oscillating wave crests and troughs between 0.2 and 0.8.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_35_clamp.fcg"
    },
    {
        .id = 36,
        .name = "saturate",
        .category = "Shader Model 3.0 Clamping & Interpolation",
        .opcode = "Hardware output saturation modifier (NVFX_FP_OP_OUT_SAT)",
        .visual_desc = "Saturation function locking dynamic HDR intensities strictly into [0.0, 1.0] interval.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_36_saturate.fcg"
    },
    {
        .id = 37,
        .name = "lerp / mix",
        .category = "Shader Model 3.0 Clamping & Interpolation",
        .opcode = "MAD(weight, b - a, a) single-cycle interpolation",
        .visual_desc = "Linear interpolation (lerp/mix) providing seamless chromatic transition between cyan and magenta.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_37_lerp.fcg"
    },
    {
        .id = 38,
        .name = "step",
        .category = "Shader Model 3.0 Clamping & Interpolation",
        .opcode = "Native SGE(x, edge) instruction",
        .visual_desc = "Step threshold splitting viewport into razor-sharp black and white halves at screen center.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_38_step.fcg"
    },
    {
        .id = 39,
        .name = "smoothstep",
        .category = "Shader Model 3.0 Clamping & Interpolation",
        .opcode = "Hermite interpolation: t = clamp; t*t*(3 - 2*t)",
        .visual_desc = "Smoothstep Hermite interpolation rendering glowing ring with softly feathered boundaries.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_39_smoothstep.fcg"
    },
    {
        .id = 40,
        .name = "dot (2D)",
        .category = "Shader Model 3.0 Vector Algebra",
        .opcode = "Native DP2 / DP2A instruction",
        .visual_desc = "2D dot product (dot2/DP2) forming linear luminance ramp parallel to rotating 2D direction vector.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_40_dot2.fcg"
    },
    {
        .id = 41,
        .name = "dot (3D)",
        .category = "Shader Model 3.0 Vector Algebra",
        .opcode = "Native DP3 instruction",
        .visual_desc = "3D dot product (dot3/DP3) illuminating 3D diffuse shaded sphere under orbital dynamic point light.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_41_dot3.fcg"
    },
    {
        .id = 42,
        .name = "dot (4D)",
        .category = "Shader Model 3.0 Vector Algebra",
        .opcode = "Native DP4 / DPH instruction",
        .visual_desc = "4D dot product (dot4/DP4) executing 4-dimensional homogeneous coordinate projection transform.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_42_dot4.fcg"
    },
    {
        .id = 43,
        .name = "cross",
        .category = "Shader Model 3.0 Vector Algebra",
        .opcode = "Swizzled MUL + MAD instruction pair",
        .visual_desc = "3D cross product computing surface normal orthogonal to tangent and binormal vectors.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_43_cross.fcg"
    },
    {
        .id = 44,
        .name = "length",
        .category = "Shader Model 3.0 Vector Algebra",
        .opcode = "DP3(v, v) -> RSQ -> RCP pipeline",
        .visual_desc = "Vector length generating concentric circular Euclidean distance isocontour rings.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_44_length.fcg"
    },
    {
        .id = 45,
        .name = "distance",
        .category = "Shader Model 3.0 Vector Algebra",
        .opcode = "length(p1 - p0)",
        .visual_desc = "Euclidean distance between two moving coordinates creating dynamic dipolar field interaction.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_45_distance.fcg"
    },
    {
        .id = 46,
        .name = "normalize",
        .category = "Shader Model 3.0 Vector Algebra",
        .opcode = "v * RSQ(dot(v, v)) single-cycle normalization",
        .visual_desc = "Unit vector normalization mapping 3D hemisphere surface normals to RGB color channels.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_46_normalize.fcg"
    },
    {
        .id = 47,
        .name = "reflect",
        .category = "Shader Model 3.0 Optics & Physics",
        .opcode = "i - 2.0 * dot(i, n) * n (DP3 + MAD)",
        .visual_desc = "Optical reflection (reflect) projecting dynamic virtual environment map onto spinning sphere.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_47_reflect.fcg"
    },
    {
        .id = 48,
        .name = "refract",
        .category = "Shader Model 3.0 Optics & Physics",
        .opcode = "Snell's law vector refraction microcode sequence",
        .visual_desc = "Snell's law refraction optically distorting background grid behind spherical glass lens.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_48_refract.fcg"
    },
    {
        .id = 49,
        .name = "faceforward",
        .category = "Shader Model 3.0 Optics & Physics",
        .opcode = "dot(ng, i) < 0 ? n : -n conditional selection",
        .visual_desc = "Faceforward function dynamically orienting surface normals toward viewer based on incident ray.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_49_faceforward.fcg"
    },
    {
        .id = 50,
        .name = "slt / <",
        .category = "Shader Model 3.0 Relational Logic",
        .opcode = "Native SLT / SLTR instruction",
        .visual_desc = "Set-on-less-than comparison (slt / <) partitioning screen into binary contrasting halves along diagonal.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_50_slt.fcg"
    },
    {
        .id = 51,
        .name = "sge / >=",
        .category = "Shader Model 3.0 Relational Logic",
        .opcode = "Native SGE / SGER instruction",
        .visual_desc = "Set-on-greater-equal comparison (sge / >=) generating sharp circular binary threshold mask.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_51_sge.fcg"
    },
    {
        .id = 52,
        .name = "sgt / >",
        .category = "Shader Model 3.0 Relational Logic",
        .opcode = "Native SGT / SGTR instruction",
        .visual_desc = "Set-on-greater-than comparison (sgt / >) rendering moving high-contrast binary zebra pattern.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_52_sgt.fcg"
    },
    {
        .id = 53,
        .name = "sle / <=",
        .category = "Shader Model 3.0 Relational Logic",
        .opcode = "Native SLE / SLER instruction",
        .visual_desc = "Set-on-less-equal comparison (sle / <=) illuminating inner circular disk with binary threshold.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_53_sle.fcg"
    },
    {
        .id = 54,
        .name = "seq / ==",
        .category = "Shader Model 3.0 Relational Logic",
        .opcode = "Native SEQ / SEQR instruction",
        .visual_desc = "Set-on-equal comparison (seq / ==) illuminating fine animated laser scanline across matching coordinates.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_54_seq.fcg"
    },
    {
        .id = 55,
        .name = "sne / !=",
        .category = "Shader Model 3.0 Relational Logic",
        .opcode = "Native SNE / SNER instruction",
        .visual_desc = "Set-on-not-equal comparison (sne / !=) illuminating entire viewport except narrow central crosshair axes.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_55_sne.fcg"
    },
    /* 56..57: Advanced SM3.0 Features */
    {
        .id = 56,
        .name = "Screen-Space Derivatives (ddx / ddy / fwidth)",
        .category = "Pixel/Fragment Shader 3.0",
        .opcode = "2x2 Pixel Quad Hardware ALU (DDX / DDY)",
        .visual_desc = "2x2 pixel quad finite difference derivatives detecting sharp geometric contour edge boundaries.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_56_deriv.fcg"
    },
    {
        .id = 57,
        .name = "Pixel Discard (discard / kil)",
        .category = "Pixel/Fragment Shader 3.0",
        .opcode = "Native KIL instruction (NV40_3D_FP_CONTROL_KIL)",
        .visual_desc = "Conditional pixel kill (discard / kil) carving perforated circular hole grid revealing background.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_57_discard.fcg"
    },
    /* 58..68: Vertex Shader 3.0 Architecture Tests */
    {
        .id = 58,
        .name = "Vertex Input Attributes (v0-v15)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "16 Input Registers (in_mask 0x0000 - 0xFFFF)",
        .visual_desc = "16-channel vertex input stream bus decoding and multi-attribute projection.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_58_v_input_attributes.fcg"
    },
    {
        .id = 59,
        .name = "Vertex Output Registers (o0-o15)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "16 Output Registers: Pos (o0), Colors (o2-o3), UVs (o7-o14)",
        .visual_desc = "Multi-channel rasterizer interpolator routing with simultaneous color and UV streams.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_59_v_output_registers.fcg"
    },
    {
        .id = 60,
        .name = "Vertex Temporary Registers (r0-r31)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "32 Vector Temporaries (dst_reg & 0x1F allocation)",
        .visual_desc = "Deeply chained high-order Chebyshev polynomial evaluation across 32 temporary registers.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_60_v_temporary_registers.fcg"
    },
    {
        .id = 61,
        .name = "Vertex Constant Pool (c0-c467)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "468 Float4 Parameter Pool & Dynamic Offset Table",
        .visual_desc = "Multi-harmonic uniform constant parameter array indexing and modulation.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_61_v_constant_pool.fcg"
    },
    {
        .id = 62,
        .name = "Vertex Address Register (a0.x)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "1x a0.x Address Register / ARL, ARR, ARA",
        .visual_desc = "Relative address indexing across discrete harmonic palettes and lookup tables.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_62_v_address_register.fcg"
    },
    {
        .id = 63,
        .name = "Vertex Texture Fetch / VTF (s0-s3)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "4 Independent VTF Units / TXL (tex2Dlod) Instruction",
        .visual_desc = "Topographic elevation field & surface normal relief sampling via vertex texture units.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_63_v_texture_fetch.fcg"
    },
    {
        .id = 64,
        .name = "Vertex Dual-Issue Engine",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "Simultaneous 1 Vector ALU + 1 Scalar ALU Dual-Issue",
        .visual_desc = "Parallel single-cycle execution of vector transforms and scalar transcendental curves.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_64_v_dual_issue.fcg"
    },
    {
        .id = 65,
        .name = "Vertex Predicate Register (p0)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "Condition Code Masking / NVFX_VP_INST_COND_*",
        .visual_desc = "Zero-overhead conditional execution and condition code bitfield masking.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_65_v_predicate_register.fcg"
    },
    {
        .id = 66,
        .name = "Vertex Point Size Output (PSIZE)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "Hardware o[1] / Point Size Output Register (oPts)",
        .visual_desc = "Dynamic per-vertex point size scaling & radial particle sprite rasterization.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_66_v_point_size.fcg"
    },
    {
        .id = 67,
        .name = "Vertex Fog Factor Output (FOGC)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "Hardware o[5] / Fog Factor Register (oFog)",
        .visual_desc = "Atmospheric depth haze blending via linear and exponential fog equations.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_67_v_fog_factor.fcg"
    },
    {
        .id = 68,
        .name = "Vertex Flow Control (Call Stack)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "4-Level Hardware Call Stack / BRA, CAL, RET",
        .visual_desc = "Multi-pass subroutine invocation accumulating ambient, diffuse & specular light.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_68_v_flow_control.fcg"
    },
    {
        .id = 69,
        .name = "Multiple Render Targets (MRT / G-Buffer)",
        .category = "Pixel/Fragment Shader 3.0",
        .opcode = "Simultaneous 4x MRT ROP Outputs (R0, R2, R3, R4) + DEPTH (R1)",
        .visual_desc = "4-quadrant deferred G-Buffer preview: Diffuse Albedo, World Normals, Material Map, and Emissive/AO.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_69_mrt_gbuffer.fcg"
    },
    {
        .id = 70,
        .name = "Dynamic Fragment Flow Control & Branching",
        .category = "Pixel/Fragment Shader 3.0",
        .opcode = "Zero-Overhead Condition Code Predication (CMP, SLT, MAD Select)",
        .visual_desc = "Multi-branch conditional evaluation across divergent SIMD quads comparing fractal loops and Voronoi cells.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_70_dynamic_branching.fcg"
    },
    {
        .id = 71,
        .name = "Vertex Shader Dynamic Loops (LOOP/REP/BRK)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "Hardware Scalar Loop Engine / LOOP, REP, BRK",
        .visual_desc = "Multi-octave procedural wave simulation with dynamic loop counter and early-exit break threshold.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_71_v_dynamic_loops.fcg"
    },
    {
        .id = 72,
        .name = "Dedicated Integer Constants (i0-i15)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "Hardware Integer Vector Constants / PARAM_INT4",
        .visual_desc = "Discrete integer lattice subdivision, modular indexing, and procedural cellular tile quantization.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_72_v_integer_constants.fcg"
    },
    {
        .id = 73,
        .name = "Dedicated Boolean Constants (b0-b15)",
        .category = "Vertex Shader 3.0 Architecture",
        .opcode = "Hardware Boolean Condition Vectors / PARAM_BOOL4",
        .visual_desc = "Static branch condition masks dynamically toggling ambient, diffuse, specular, and rim lighting.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_73_v_boolean_constants.fcg"
    },
    {
        .id = 74,
        .name = "Hardware Shadow & Depth Comparison Samplers",
        .category = "Texture Sampling & Filtering",
        .opcode = "Hardware Depth Comparison Samplers / SHADOW2D & PCF",
        .visual_desc = "Light-space depth projection, depth comparison test, and 9-tap percentage-closer filtering (PCF) soft shadow.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_74_shadow_map_pcf.fcg"
    },
    {
        .id = 75,
        .name = "Native 3D Geometry & Direct Vertex Shader Execution",
        .category = "VS3.0 Hardware Pipeline",
        .opcode = "Direct .vcg binding via rsxVertexProgramLoad / Dynamic 3D Mesh Transform",
        .visual_desc = "Dynamic 3D parametric torus knot projection with harmonic surface vertex displacement and Blinn-Phong lighting.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_75_native_3d_geometry.fcg"
    },
    {
        .id = 76,
        .name = "Fragment Shader Custom Depth Output (oDepth)",
        .category = "Fragment & Z-Buffer Engine",
        .opcode = "Direct hardware depth replacement write (gl_FragDepth / NVFX_FP_REG_DEPTH / R1.z)",
        .visual_desc = "Procedural raymarched undulating terrain with custom non-linear depth replacement and isodepth contours.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_76_custom_depth_output.fcg"
    },
    {
        .id = 77,
        .name = "Explicit Texture Derivatives & LOD Bias (TXD/TXB)",
        .category = "Texture Sampling Units",
        .opcode = "Manual gradient sampling (tex2Dgrad / TXD with explicit dPdx, dPdy) & LOD bias (tex2Dbias / TXB)",
        .visual_desc = "Dynamic anisotropic mip-level gradient visualization displaying explicit dPdx, dPdy directional derivatives and LOD bias shifting.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_77_texture_derivatives_lod.fcg"
    },
    {
        .id = 78,
        .name = "3D Volumetric & Cube Map Texture Sampling",
        .category = "Texture Sampling Units",
        .opcode = "3D volume slicing (tex3D / NVFX_FP_OP_OPCODE_TXP) & cubic environment maps (texCUBE)",
        .visual_desc = "Procedural 3D volumetric density slice rendering alongside 6-face cubic environment reflection mapped sphere.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_78_volume_cube_sampling.fcg"
    },
    {
        .id = 79,
        .name = "Centroid MSAA Sub-Pixel Interpolation Qualifier",
        .category = "Rasterizer & Interpolators",
        .opcode = "centroid attribute modifier preventing multi-sample sub-pixel edge bleeding",
        .visual_desc = "Sub-pixel 4x MSAA star polygon edge anti-aliasing contrasting standard center interpolation with centroid clamping.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_79_centroid_interpolation.fcg"
    },
    {
        .id = 80,
        .name = "Two-Sided Shading & Front-Facing Register (VFACE)",
        .category = "Rasterizer & Fragment ALU",
        .opcode = "Hardware primitive orientation bit (VFACE / gl_FrontFacing condition mask)",
        .visual_desc = "Dynamic 3D twisting Mobius ribbon rendering front faces in electric cyan and back faces in metallic copper.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_80_two_sided_vface.fcg"
    },
    {
        .id = 81,
        .name = "Hardware Vector Packing & Unpacking Instructions",
        .category = "Fragment ALU Packing",
        .opcode = "Native packing (PK2, PK4, PK16, PKB, PKG) & unpacking (UP2, UP4, UP16, UPB, UPG)",
        .visual_desc = "Real-time 8-bit/4-bit vector quantization and dequantization displaying HDR color compression and error heatmap.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_81_vector_packing_unpacking.fcg"
    },
    {
        .id = 82,
        .name = "Hardware Lighting & Attenuation Coefficients",
        .category = "Fragment & Vertex ALU",
        .opcode = "Native Blinn-Phong lighting evaluator (LIT, LIF) and quadratic distance vector (DST)",
        .visual_desc = "Multi-light omni and spot illumination evaluating hardware LIT coefficients and DST quadratic distance attenuation.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_82_hardware_lighting_lit_dst.fcg"
    },
    {
        .id = 83,
        .name = "Bump Environment Mapping (BEM) Matrix Perturbation",
        .category = "Texture ALU Subsystem",
        .opcode = "Hardware matrix perturbation & sampling (BEM, TEXBEM, TXPBEM)",
        .visual_desc = "Dynamic liquid ripple bump mapping perturbing 2x2 reflection matrix coordinates over chromatic environment backdrop.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_83_bump_env_mapping_bem.fcg"
    },
    {
        .id = 84,
        .name = "Projective Texture Mapping (TXP)",
        .category = "Texture Sampling Units",
        .opcode = "Hardware projective coordinate division (u/w, v/w, z/w) via TXP",
        .visual_desc = "Conical spotlight slide projector casting perspective-divided (u/w, v/w) target grid onto 3D inclined plane.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_84_projective_texture_txp.fcg"
    },
    {
        .id = 85,
        .name = "16-Bit Halfword Bitwise Register Operations",
        .category = "Fragment ALU Architecture",
        .opcode = "Native 16-bit register bitwise merging (OR16_HI, OR16_LO)",
        .visual_desc = "Procedural bitwise logic gates (AND/OR/XOR/Shift) creating Sierpinski fractals and halfword packed stipple masks.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_85_bitwise_halfword_ops.fcg"
    },
    {
        .id = 86,
        .name = "Extended Address Register Modes & Call Stack Ops",
        .category = "Vertex Shader Engine",
        .opcode = "Rounding/absolute address indexing (ARR, ARA) and subroutine stack ops (PSH, POP, CLI, CLB)",
        .visual_desc = "Multi-tier nested procedural lighting synthesis utilizing simulated address offset registers and stack subroutine branch levels.",
        .is_stub = false,
        .is_sphere_rt = false,
        .shader_file = "test_86_extended_address_stack.fcg"
    }
};

/* Buffer structure for holding compiled binary shader microcode */
typedef struct {
    uint8_t* data;
    size_t   size;
} CompiledShaderBuffer;

/* Reads entire binary file into a newly allocated buffer */
static uint8_t* read_file_bytes(const char* filepath, size_t* out_size) {
    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    long file_len = ftell(fp);
    if (file_len < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    uint8_t* buffer = (uint8_t*)malloc((size_t)file_len);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)file_len, fp);
    fclose(fp);

    if (read_bytes != (size_t)file_len) {
        free(buffer);
        return NULL;
    }

    *out_size = (size_t)file_len;
    return buffer;
}

/* Escape string for C literal */
static void write_escaped_string(FILE* fp, const char* str) {
    while (*str) {
        if (*str == '"' || *str == '\\') {
            fputc('\\', fp);
        }
        fputc(*str, fp);
        str++;
    }
}

/* Print CLI usage information */
static void print_usage(const char* prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  --rsxcomp <path>     Path to rsxcomp compiler executable\n");
    printf("  --shaders-dir <dir>  Path to shaders directory (default: shaders)\n");
    printf("  --include-dir <dir>  Path to include directory (default: include)\n");
    printf("  --source-dir <dir>   Path to source directory (default: source)\n");
    printf("  --build-dir <dir>    Path to build directory (default: build)\n");
    printf("  --help               Display this help message\n");
}

int main(int argc, char* argv[]) {
    char rsxcomp_path[MAX_PATH_LEN] = "";
    char shaders_dir[MAX_PATH_LEN] = "shaders";
    char include_dir[MAX_PATH_LEN] = "include";
    char source_dir[MAX_PATH_LEN] = "source";
    char build_dir[MAX_PATH_LEN] = "build";

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rsxcomp") == 0 && i + 1 < argc) {
            strncpy(rsxcomp_path, argv[++i], sizeof(rsxcomp_path) - 1);
        } else if (strcmp(argv[i], "--shaders-dir") == 0 && i + 1 < argc) {
            strncpy(shaders_dir, argv[++i], sizeof(shaders_dir) - 1);
        } else if (strcmp(argv[i], "--include-dir") == 0 && i + 1 < argc) {
            strncpy(include_dir, argv[++i], sizeof(include_dir) - 1);
        } else if (strcmp(argv[i], "--source-dir") == 0 && i + 1 < argc) {
            strncpy(source_dir, argv[++i], sizeof(source_dir) - 1);
        } else if (strcmp(argv[i], "--build-dir") == 0 && i + 1 < argc) {
            strncpy(build_dir, argv[++i], sizeof(build_dir) - 1);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Resolve rsxcomp path if not specified via CLI */
    if (rsxcomp_path[0] == '\0') {
        const char* env_compiler = getenv("PS3_SHADER_COMPILER");
        if (env_compiler && env_compiler[0] != '\0') {
            strncpy(rsxcomp_path, env_compiler, sizeof(rsxcomp_path) - 1);
        } else if (access("build/rsxcomp", X_OK) == 0) {
            strcpy(rsxcomp_path, "build/rsxcomp");
        } else if (access("../build/rsxcomp", X_OK) == 0) {
            strcpy(rsxcomp_path, "../build/rsxcomp");
        } else if (access("../rsxcomp/rsxcomp", X_OK) == 0) {
            strcpy(rsxcomp_path, "../rsxcomp/rsxcomp");
        } else {
            strcpy(rsxcomp_path, "rsxcomp");
        }
    }

    /* Ensure build directory exists */
    mkdir(build_dir, 0755);

    printf("Building %d Conformance Test Suite Shaders with '%s'...\n", TOTAL_TESTS, rsxcomp_path);

    CompiledShaderBuffer compiled_fpos[TOTAL_TESTS];
    memset(compiled_fpos, 0, sizeof(compiled_fpos));

    /* Compile all test shaders to .tmp.fpo binary microcode */
    for (int i = 0; i < TOTAL_TESTS; i++) {
        const TestEntryDefinition* entry = &g_test_definitions[i];
        if (!entry->shader_file || entry->shader_file[0] == '\0') {
            continue;
        }

        char src_path[MAX_PATH_LEN];
        char out_fpo[MAX_PATH_LEN];
        snprintf(src_path, sizeof(src_path), "%s/%s", shaders_dir, entry->shader_file);

        /* Derive base filename for temporary fpo output */
        char base_name[128];
        strncpy(base_name, entry->shader_file, sizeof(base_name) - 1);
        char* dot = strrchr(base_name, '.');
        if (dot) *dot = '\0';

        snprintf(out_fpo, sizeof(out_fpo), "%s/%s.tmp.fpo", build_dir, base_name);

        char cmd[MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd), "\"%s\" -f -i \"%s\" -o \"%s\"", rsxcomp_path, src_path, out_fpo);

        int res = system(cmd);
        if (res != 0) {
            fprintf(stderr, "[ERROR] Failed to compile shader %s (exit code %d)\n", src_path, res);
            return 1;
        }

        size_t fpo_size = 0;
        uint8_t* fpo_data = read_file_bytes(out_fpo, &fpo_size);
        if (!fpo_data) {
            fprintf(stderr, "[ERROR] Failed to read compiled microcode from %s\n", out_fpo);
            return 1;
        }

        compiled_fpos[i].data = fpo_data;
        compiled_fpos[i].size = fpo_size;
    }

    /* Generate include/test_suite_data.h header file */
    char header_path[MAX_PATH_LEN];
    snprintf(header_path, sizeof(header_path), "%s/test_suite_data.h", include_dir);
    FILE* header_fp = fopen(header_path, "w");
    if (!header_fp) {
        fprintf(stderr, "[ERROR] Failed to open %s for writing\n", header_path);
        return 1;
    }

    fprintf(header_fp, "/**\n");
    fprintf(header_fp, " * PS3 Reality Synthesizer (RSX) - Shader Model 3.0 Conformance Test Suite Data\n");
    fprintf(header_fp, " * Autogenerated metadata and binary lookup table for all %d test scenes.\n", TOTAL_TESTS);
    fprintf(header_fp, " */\n\n");
    fprintf(header_fp, "#ifndef __TEST_SUITE_DATA_H__\n");
    fprintf(header_fp, "#define __TEST_SUITE_DATA_H__\n\n");
    fprintf(header_fp, "#include <stdint.h>\n");
    fprintf(header_fp, "#include <stdbool.h>\n");
    fprintf(header_fp, "#include <rsx/rsx_program.h>\n\n");
    fprintf(header_fp, "#define TOTAL_TEST_COUNT %d\n\n", TOTAL_TESTS);
    fprintf(header_fp, "typedef struct {\n");
    fprintf(header_fp, "    int         id;\n");
    fprintf(header_fp, "    const char* name;\n");
    fprintf(header_fp, "    const char* category;\n");
    fprintf(header_fp, "    const char* opcode;\n");
    fprintf(header_fp, "    const char* visual_desc;\n");
    fprintf(header_fp, "    bool        is_stub;\n");
    fprintf(header_fp, "    bool        is_sphere_rt;\n");
    fprintf(header_fp, "    const uint8_t* fpo_data;\n");
    fprintf(header_fp, "    uint32_t    fpo_size;\n");
    fprintf(header_fp, "} TestSuiteMetadata;\n\n");
    fprintf(header_fp, "extern const TestSuiteMetadata g_test_suite_entries[TOTAL_TEST_COUNT];\n\n");
    fprintf(header_fp, "#endif /* __TEST_SUITE_DATA_H__ */\n");
    fclose(header_fp);

    /* Generate source/test_suite_data.c source file */
    char source_path[MAX_PATH_LEN];
    snprintf(source_path, sizeof(source_path), "%s/test_suite_data.c", source_dir);
    FILE* src_fp = fopen(source_path, "w");
    if (!src_fp) {
        fprintf(stderr, "[ERROR] Failed to open %s for writing\n", source_path);
        return 1;
    }

    fprintf(src_fp, "/**\n");
    fprintf(src_fp, " * PS3 Reality Synthesizer (RSX) - Shader Model 3.0 Conformance Test Suite Data Arrays\n");
    fprintf(src_fp, " * Embedded .fpo microcode byte arrays aligned to 16 bytes for all %d test scenes.\n", TOTAL_TESTS);
    fprintf(src_fp, " */\n\n");
    fprintf(src_fp, "#include \"test_suite_data.h\"\n\n");

    /* Emit aligned byte arrays for compiled shader binaries */
    for (int i = 0; i < TOTAL_TESTS; i++) {
        if (compiled_fpos[i].data != NULL && compiled_fpos[i].size > 0) {
            int t_id = g_test_definitions[i].id;
            size_t sz = compiled_fpos[i].size;
            uint8_t* data = compiled_fpos[i].data;

            fprintf(src_fp, "static const uint8_t g_shader_fpo_test_%02d[%zu] __attribute__((aligned(16))) = {\n", t_id, sz);
            for (size_t chunk_i = 0; chunk_i < sz; chunk_i += 16) {
                fprintf(src_fp, "   ");
                size_t end_chunk = (chunk_i + 16 < sz) ? (chunk_i + 16) : sz;
                for (size_t b_idx = chunk_i; b_idx < end_chunk; b_idx++) {
                    fprintf(src_fp, " 0x%02X", data[b_idx]);
                    if (b_idx + 1 < sz) {
                        fprintf(src_fp, ",");
                    }
                }
                fprintf(src_fp, "\n");
            }
            fprintf(src_fp, "};\n\n");
        }
    }

    /* Emit master metadata array */
    fprintf(src_fp, "const TestSuiteMetadata g_test_suite_entries[TOTAL_TEST_COUNT] = {\n");
    for (int i = 0; i < TOTAL_TESTS; i++) {
        const TestEntryDefinition* entry = &g_test_definitions[i];
        int t_id = entry->id;

        fprintf(src_fp, "    {\n");
        fprintf(src_fp, "        .id = %d,\n", entry->id);

        fprintf(src_fp, "        .name = \"");
        write_escaped_string(src_fp, entry->name);
        fprintf(src_fp, "\",\n");

        fprintf(src_fp, "        .category = \"");
        write_escaped_string(src_fp, entry->category);
        fprintf(src_fp, "\",\n");

        fprintf(src_fp, "        .opcode = \"");
        write_escaped_string(src_fp, entry->opcode);
        fprintf(src_fp, "\",\n");

        fprintf(src_fp, "        .visual_desc = \"");
        write_escaped_string(src_fp, entry->visual_desc);
        fprintf(src_fp, "\",\n");

        fprintf(src_fp, "        .is_stub = %s,\n", entry->is_stub ? "true" : "false");
        fprintf(src_fp, "        .is_sphere_rt = %s,\n", entry->is_sphere_rt ? "true" : "false");

        if (compiled_fpos[i].data != NULL && compiled_fpos[i].size > 0) {
            fprintf(src_fp, "        .fpo_data = g_shader_fpo_test_%02d,\n", t_id);
            fprintf(src_fp, "        .fpo_size = sizeof(g_shader_fpo_test_%02d)\n", t_id);
        } else {
            fprintf(src_fp, "        .fpo_data = NULL,\n");
            fprintf(src_fp, "        .fpo_size = 0\n");
        }

        fprintf(src_fp, "    }%s\n", (i < TOTAL_TESTS - 1) ? "," : "");
    }
    fprintf(src_fp, "};\n");
    fclose(src_fp);

    /* Free allocated memory */
    for (int i = 0; i < TOTAL_TESTS; i++) {
        if (compiled_fpos[i].data) {
            free(compiled_fpos[i].data);
        }
    }

    printf("Successfully generated %s and %s via native C build tool!\n", header_path, source_path);
    return 0;
}
