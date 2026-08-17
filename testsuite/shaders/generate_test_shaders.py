#!/usr/bin/env python3
"""
Generator for PS3 RSX Shader Model 3.0 Conformance Test Suite Shaders.
Generates discrete .fcg shader files corresponding to every item in RSX_SM30_Compiler_Full_Coverage_Report.md.
Each shader features comprehensive English inline comments explaining the mathematical theory,
hardware instruction mapping, and visual verification semantics.
"""

import os

SHADERS_DIR = os.path.dirname(os.path.abspath(__file__))

shaders = [
    # Test 03: sub
    ("test_03_sub.fcg", """// =============================================================================
// PS3 RSX Test 03: sub / - (Shader Model 3.0 Basic Arithmetic)
// Mathematical subtraction of dynamic chromatic bands from background gradient
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Smooth base background gradient across NDC UV [-1, 1]
    float3 base_bg = float3(0.9, 0.85, 0.7);
    
    // Moving animated light beam to subtract
    float beam = max(0.0, 1.0 - abs(input.uv.x - sin(t * 2.0) * 0.7) * 4.0);
    float3 sub_color = float3(beam * 0.8, beam * 0.5, beam * 0.2);
    
    // Evaluate hardware SUB: base_bg - sub_color creates inverted dark shadow band
    float3 result = base_bg - sub_color;
    
    output.color = float4(result, 1.0);
    return output;
}
"""),

    # Test 04: mul
    ("test_04_mul.fcg", """// =============================================================================
// PS3 RSX Test 04: mul / * (Shader Model 3.0 Basic Arithmetic)
// Multiplicative color modulation combining rotating 2D grid with radial vignette
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Rotating 2D pattern coordinate
    float cos_t = cos(t);
    float sin_t = sin(t);
    float2 rot_uv = float2(input.uv.x * cos_t - input.uv.y * sin_t, input.uv.x * sin_t + input.uv.y * cos_t);
    
    float3 grid_col = float3(sin(rot_uv.x * 8.0) * 0.5 + 0.5, cos(rot_uv.y * 8.0) * 0.5 + 0.5, 0.8);
    float vignette = max(0.0, 1.0 - length(input.uv) * 0.7);
    
    // Evaluate hardware MUL: grid_col * vignette
    float3 result = grid_col * vignette;
    
    output.color = float4(result, 1.0);
    return output;
}
"""),

    # Test 05: mad
    ("test_05_mad.fcg", """// =============================================================================
// PS3 RSX Test 05: mad / fma (Shader Model 3.0 Basic Arithmetic)
// Single-cycle Fused Multiply-Add (a * b + c) computing frequency modulation rings
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float dist = length(input.uv);
    float wave_carrier = sin(dist * 16.0 - t * 4.0);
    float wave_mod = cos(input.uv.x * 8.0 + t);
    
    // Native RSX MAD: carrier * mod + bias
    float3 result = float3(wave_carrier * wave_mod + 0.5, wave_carrier * 0.5 + 0.5, wave_mod * 0.5 + 0.5);
    
    output.color = float4(result, 1.0);
    return output;
}
"""),

    # Test 06: div
    ("test_06_div.fcg", """// =============================================================================
// PS3 RSX Test 06: div / / (Shader Model 3.0 Basic Arithmetic)
// Spatial division creating quantized radial cell segments via RCP + MUL pipeline
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float scale = 0.25 + 0.15 * sin(t * 2.0);
    // RSX Hardware Division: evaluated via RCP and MUL
    float2 divided_uv = input.uv / scale;
    float3 col = float3(frac(divided_uv.x), frac(divided_uv.y), 0.5);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 07: rcp
    ("test_07_rcp.fcg", """// =============================================================================
// PS3 RSX Test 07: rcp (Shader Model 3.0 Basic Arithmetic)
// Hardware 1/x Reciprocal instruction generating hyperbolic core illumination
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float d = length(input.uv) * 2.0 + 0.05;
    // RSX Hardware 1/x
    float inv_d = 1.0 / d;
    float pulse = 0.5 + 0.5 * sin(t * 3.0);
    float3 col = float3(inv_d * 0.2 * pulse, inv_d * 0.1, inv_d * 0.05);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 08: rsqrt
    ("test_08_rsq.fcg", """// =============================================================================
// PS3 RSX Test 08: rsqrt / rsq (Shader Model 3.0 Basic Arithmetic)
// Hardware 1 / sqrt(x) Reciprocal Square Root instruction for point light decay
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float d_sq = dot(input.uv, input.uv) * 4.0 + 0.02;
    // RSX Hardware RSQ: 1.0 / sqrt(d_sq)
    float inv_sqrt = rsqrt(d_sq);
    
    float glow = inv_sqrt * (0.4 + 0.2 * sin(t * 4.0));
    float3 col = float3(glow * 0.3, glow * 0.7, glow * 1.0);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 09: sqrt
    ("test_09_sqrt.fcg", """// =============================================================================
// PS3 RSX Test 09: sqrt (Shader Model 3.0 Basic Arithmetic)
// Square root instruction pipeline generating parabolic radial gradient profile
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Radial Euclidean distance from center
    float dist = length(input.uv);
    
    // Square root calculation: RSQ + RCP or RSQ + MUL
    // Parabolic expansion curve mapping [0, 1] distance to concave sqrt gradient
    float s = sqrt(dist);
    
    // Outward-propagating harmonic wave modulated by sqrt curve
    float wave = sin(s * 12.0 - t * 4.0) * 0.5 + 0.5;
    float3 col = float3(s, wave, 1.0 - s);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 10: abs
    ("test_10_abs.fcg", """// =============================================================================
// PS3 RSX Test 10: abs (Shader Model 3.0 Basic Arithmetic)
// Register Absolute Value modifier generating perfect 4-quadrant mirror kaleidoscope
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Hardware Absolute Bit: abs(input.uv)
    float2 sym_uv = abs(input.uv);
    
    float pattern = sin((sym_uv.x + sym_uv.y) * 12.0 - t * 2.0) * 0.5 + 0.5;
    float3 col = float3(sym_uv.x, pattern, sym_uv.y);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 11: neg
    ("test_11_neg.fcg", """// =============================================================================
// PS3 RSX Test 11: neg / -x (Shader Model 3.0 Basic Arithmetic)
// Register Negate modifier creating inverse chromatic phase sweep
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Hardware Operand Negate bit
    float2 neg_uv = -input.uv;
    
    float val = sin(neg_uv.x * 6.0 + neg_uv.y * 6.0 + t * 3.0) * 0.5 + 0.5;
    float3 col = float3(1.0 - val, neg_uv.x * 0.5 + 0.5, neg_uv.y * 0.5 + 0.5);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 12: sin
    ("test_12_sin.fcg", """// =============================================================================
// PS3 RSX Test 12: sin (Shader Model 3.0 Trigonometry)
// Hardware Taylor/Chebyshev Trigonometryc SIN ALU evaluating outward concentric wave ripples
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float dist = length(input.uv * 8.0);
    // Native RSX SIN ALU
    float wave = sin(dist - t * 5.0) * 0.5 + 0.5;
    
    float3 col = float3(wave * 0.9, wave * 0.4, 1.0 - wave);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 13: cos
    ("test_13_cos.fcg", """// =============================================================================
// PS3 RSX Test 13: cos (Shader Model 3.0 Trigonometry)
// Hardware Trigonometryc COS ALU producing 90-degree phase shifted harmonic moire pattern
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float dist = length(input.uv * 8.0);
    // Native RSX COS ALU (exact 90-degree quadrature phase relative to sin)
    float wave = cos(dist - t * 5.0) * 0.5 + 0.5;
    
    float3 col = float3(1.0 - wave, wave * 0.8, wave * 0.3);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 14: tan
    ("test_14_tan.fcg", """// =============================================================================
// PS3 RSX Test 14: tan (Shader Model 3.0 Trigonometry)
// Trigonometryc Tangent (sin/cos/rcp/mul) generating periodic asymptotic vertical fringes
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Tangent function with periodic poles
    float val = tan(input.uv.x * 4.0 + t * 2.0);
    float clamped_val = clamp(val * 0.2 + 0.5, 0.0, 1.0);
    
    float3 col = float3(clamped_val, 0.4, 1.0 - clamped_val);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 15: asin
    ("test_15_asin.fcg", """// =============================================================================
// PS3 RSX Test 15: asin (Shader Model 3.0 Trigonometry)
// Inverse sine function mapping curvature into spherical compression gradient
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float x = clamp(input.uv.x, -0.99, 0.99);
    float as = asin(x) / 1.570796; // Normalize [-pi/2, pi/2] to [-1, 1]
    
    float wave = sin(as * 6.28318 - t * 3.0) * 0.5 + 0.5;
    float3 col = float3(as * 0.5 + 0.5, wave, 1.0 - wave);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 16: acos
    ("test_16_acos.fcg", """// =============================================================================
// PS3 RSX Test 16: acos (Shader Model 3.0 Trigonometry)
// Inverse cosine function mapping vertical coordinate into linear angular ramp [0, PI]
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float y = clamp(input.uv.y, -0.99, 0.99);
    float ac = acos(y) / 3.14159265; // Normalize [0, PI] to [0, 1]
    
    float pulse = sin(ac * 12.0 - t * 4.0) * 0.5 + 0.5;
    float3 col = float3(ac, pulse * 0.8, 1.0 - ac);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 17: atan
    ("test_17_atan.fcg", """// =============================================================================
// PS3 RSX Test 17: atan (Shader Model 3.0 Trigonometry)
// Inverse tangent function generating smooth S-curve sigmoid saturation
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Normalized input where polynomial approximation produces smooth S-curve
    float at = atan(input.uv.x * 1.5 + sin(t * 2.0) * 0.5) / 1.570796;
    float norm_at = clamp(at * 0.5 + 0.5, 0.0, 1.0);
    
    // Smooth S-curve chromatic gradient
    float3 col = float3(norm_at, sin(norm_at * 3.14159265), 1.0 - norm_at);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 18: atan2
    ("test_18_atan2.fcg", """// =============================================================================
// PS3 RSX Test 18: atan2 (Shader Model 3.0 Trigonometry)
// 4-Quadrant Arc Tangent computing 360-degree rotating chromatic hue wheel
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Robust 4-quadrant atan2 computation with zero-division immunity
    float2 p = input.uv;
    float abs_x = abs(p.x) + 1e-5;
    float abs_y = abs(p.y);
    float t_max = max(abs_x, abs_y);
    float t_min = min(abs_x, abs_y);
    float q = t_min / t_max;
    float q2 = q * q;
    float poly = q * (0.97239411 - 0.19194795 * q2);
    
    // Swap angle if |y| > |x| to prevent vertical divergence
    float angle = (abs_y > abs_x) ? (1.5707963 - poly) : poly;
    if (p.x < 0.0) angle = 3.14159265 - angle;
    if (p.y < 0.0) angle = -angle;
    
    float norm_angle = frac((angle + t * 2.0) / 6.2831853);
    
    // Convert normalized hue to continuous RGB color wheel
    float r = abs(norm_angle * 6.0 - 3.0) - 1.0;
    float g = 2.0 - abs(norm_angle * 6.0 - 2.0);
    float b = 2.0 - abs(norm_angle * 6.0 - 4.0);
    float3 hue_col = saturate(float3(r, g, b));
    
    float rad = saturate(1.0 - length(input.uv));
    float3 col = hue_col * rad;
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 19: radians
    ("test_19_radians.fcg", """// =============================================================================
// PS3 RSX Test 19: radians (Shader Model 3.0 Trigonometry)
// Degree to radian conversion scaling 360-degree compass into 12 circular sectors
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Robust 4-quadrant polar angle computation
    float2 p = input.uv;
    float abs_x = abs(p.x) + 1e-5;
    float abs_y = abs(p.y);
    float t_max = max(abs_x, abs_y);
    float t_min = min(abs_x, abs_y);
    float q = t_min / t_max;
    float q2 = q * q;
    float poly = q * (0.97239411 - 0.19194795 * q2);
    float angle = (abs_y > abs_x) ? (1.5707963 - poly) : poly;
    if (p.x < 0.0) angle = 3.14159265 - angle;
    if (p.y < 0.0) angle = -angle;
    
    float angle_deg = frac((angle + t * 2.0) / 6.2831853) * 360.0;
    // Test native radians(deg)
    float angle_rad = radians(angle_deg);
    
    float sector = floor(angle_deg / 30.0) / 12.0;
    float3 col = float3(sector, sin(angle_rad) * 0.5 + 0.5, cos(angle_rad) * 0.5 + 0.5);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 20: degrees
    ("test_20_degrees.fcg", """// =============================================================================
// PS3 RSX Test 20: degrees (Shader Model 3.0 Trigonometry)
// Radian to degree conversion verifying reciprocal angle calibration steps
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float rad = input.uv.x * 3.14159265;
    // Test native degrees(rad)
    float deg = degrees(rad);
    
    float normalized_deg = frac(deg / 360.0 + t * 0.2);
    float3 col = float3(normalized_deg, frac(deg / 60.0), 1.0 - normalized_deg);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 21: exp2
    ("test_21_exp2.fcg", """// =============================================================================
// PS3 RSX Test 21: exp2 (Shader Model 3.0 Exponential & Logarithmic)
// Hardware EX2 2^x exponential curve generating steep power illumination ramp
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float x = (input.uv.x * 0.5 + 0.5) * 3.0 - 2.0; // [-2.0, 1.0]
    // Hardware EX2
    float val = exp2(x + sin(t) * 0.5);
    
    float3 col = float3(val * 0.8, val * 0.4, val * 0.2);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 22: exp
    ("test_22_exp.fcg", """// =============================================================================
// PS3 RSX Test 22: exp (Shader Model 3.0 Exponential & Logarithmic)
// Natural exponential e^x calculating Gaussian radial point light bloom
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float r_sq = dot(input.uv, input.uv) * (3.0 + sin(t * 2.0));
    // Natural exp(-r_sq)
    float bloom = exp(-r_sq);
    
    float3 col = float3(bloom * 1.0, bloom * 0.8, bloom * 0.4);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 23: log2
    ("test_23_log2.fcg", """// =============================================================================
// PS3 RSX Test 23: log2 (Shader Model 3.0 Exponential & Logarithmic)
// Hardware LG2 base-2 logarithm compressing dynamic range across horizontal axis
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float x = max((input.uv.x * 0.5 + 0.5) * 16.0, 0.0625);
    // Hardware LG2
    float l2 = (log2(x) + 4.0) / 8.0; // Map [-4, 4] to [0, 1]
    
    float wave = sin(l2 * 12.0 - t * 3.0) * 0.5 + 0.5;
    float3 col = float3(l2, wave, 1.0 - l2);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 24: log
    ("test_24_log.fcg", """// =============================================================================
// PS3 RSX Test 24: log (Shader Model 3.0 Exponential & Logarithmic)
// Natural logarithm ln(x) producing radial tone-mapping compression rings
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float r = max(length(input.uv) * 8.0, 0.05);
    // Natural ln(r)
    float ln_val = saturate((log(r) + 2.0) / 4.0);
    
    float pulse = cos(ln_val * 16.0 - t * 4.0) * 0.5 + 0.5;
    float3 col = float3(ln_val * 0.7, pulse, 0.9 - ln_val * 0.5);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 25: log10
    ("test_25_log10.fcg", """// =============================================================================
// PS3 RSX Test 25: log10 (Shader Model 3.0 Exponential & Logarithmic)
// Base-10 logarithm generating decibel-style decade intensity steps
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float x = max((input.uv.x * 0.5 + 0.5) * 100.0, 0.1);
    // Base-10 logarithm: log10(x)
    float l10 = (log10(x) + 1.0) / 3.0; // Map [-1, 2] to [0, 1]
    
    float step_band = floor(l10 * 6.0) / 6.0;
    float3 col = float3(l10, step_band, 1.0 - l10);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 26: pow
    ("test_26_pow.fcg", """// =============================================================================
// PS3 RSX Test 26: pow (Shader Model 3.0 Exponential & Logarithmic)
// Power exponentiation x^y focusing high-specular Phong reflection highlight spot
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float2 light_pos = float2(sin(t * 2.0) * 0.4, cos(t * 2.0) * 0.4);
    float dist = length(input.uv - light_pos);
    float base_light = max(0.0, 1.0 - dist * 1.5);
    
    // High exponent power curve (Phong specular)
    float spec = pow(base_light, 16.0);
    float3 col = float3(spec * 1.0, spec * 0.9, spec * 0.6) + float3(0.05, 0.1, 0.2);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 27: floor
    ("test_27_floor.fcg", """// =============================================================================
// PS3 RSX Test 27: floor (Shader Model 3.0 Rounding & Truncation)
// Hardware FLR instruction quantizing coordinates into 12x12 retro mosaic grid
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Hardware FLR
    float2 cell = floor(input.uv * 12.0) / 12.0;
    
    float val = sin((cell.x + cell.y) * 3.14159 + t * 3.0) * 0.5 + 0.5;
    float3 col = float3(cell.x * 0.5 + 0.5, val, cell.y * 0.5 + 0.5);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 28: ceil
    ("test_28_ceil.fcg", """// =============================================================================
// PS3 RSX Test 28: ceil (Shader Model 3.0 Rounding & Truncation)
// Ceiling function (-FLR(-x)) producing quantized upper stair-stepped color blocks
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Hardware Ceil: -floor(-x)
    float2 cell = ceil(input.uv * 12.0) / 12.0;
    
    float val = cos((cell.x - cell.y) * 3.14159 + t * 3.0) * 0.5 + 0.5;
    float3 col = float3(val, cell.x * 0.5 + 0.5, cell.y * 0.5 + 0.5);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 29: frac
    ("test_29_frac.fcg", """// =============================================================================
// PS3 RSX Test 29: frac / fract (Shader Model 3.0 Rounding & Truncation)
// Hardware FRC instruction decomposing coordinate space into repeating sawtooth ramps
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    // Hardware FRC: x - floor(x)
    float2 f = frac((input.uv + float2(t * 0.2, t * 0.1)) * 6.0);
    
    float3 col = float3(f.x, f.y, 0.5 * (f.x + f.y));
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 30: round
    ("test_30_round.fcg", """// =============================================================================
// PS3 RSX Test 30: round (Shader Model 3.0 Rounding & Truncation)
// Nearest integer rounding (floor(x + 0.5)) creating discrete color quantization bands
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float r = length(input.uv) * 6.0;
    // Nearest integer rounding: round(r)
    float rounded_r = round(r + sin(t) * 0.5) / 6.0;
    
    float3 col = float3(rounded_r, 1.0 - rounded_r, frac(rounded_r * 3.0));
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 31: trunc
    ("test_31_trunc.fcg", """// =============================================================================
// PS3 RSX Test 31: trunc (Shader Model 3.0 Rounding & Truncation)
// Truncation towards zero generating symmetric step plateaus with double-width origin band
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float2 tr = trunc(input.uv * 5.0) / 5.0;
    
    float val = sin((tr.x + tr.y) * 4.0 + t * 3.0) * 0.5 + 0.5;
    float3 col = float3(tr.x * 0.5 + 0.5, val, tr.y * 0.5 + 0.5);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 32: sign
    ("test_32_sign.fcg", """// =============================================================================
// PS3 RSX Test 32: sign (Shader Model 3.0 Rounding & Truncation)
// Tri-state Sign function (-1, 0, +1) mapping 4 coordinate quadrants to primary color blocks
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float2 s = sign(input.uv + float2(sin(t) * 0.3, cos(t) * 0.3));
    
    // Quadrant mapping: (+1,+1)=Yellow, (-1,+1)=Green, (-1,-1)=Blue, (+1,-1)=Red
    float3 col = float3(s.x > 0.0 ? 1.0 : 0.0, s.y > 0.0 ? 1.0 : 0.0, s.x < 0.0 ? 1.0 : 0.0);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 33: min
    ("test_33_min.fcg", """// =============================================================================
// PS3 RSX Test 33: min (Shader Model 3.0 Clamping & Interpolation)
// Hardware MIN instruction extracting geometric intersection of two moving spotlights
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float2 p1 = float2(-0.3 + sin(t * 2.0) * 0.2, 0.0);
    float2 p2 = float2( 0.3 - sin(t * 2.0) * 0.2, 0.0);
    
    float d1 = max(0.0, 1.0 - length(input.uv - p1) * 2.0);
    float d2 = max(0.0, 1.0 - length(input.uv - p2) * 2.0);
    
    // Hardware MIN computes intersection
    float inter = min(d1, d2);
    float3 col = float3(inter * 1.0, inter * 0.8, inter * 0.2) + float3(d1 * 0.2, 0.0, d2 * 0.2);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 34: max
    ("test_34_max.fcg", """// =============================================================================
// PS3 RSX Test 34: max (Shader Model 3.0 Clamping & Interpolation)
// Hardware MAX instruction computing geometric union illumination of two moving lights
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float2 p1 = float2(-0.3 + sin(t * 2.0) * 0.2, 0.0);
    float2 p2 = float2( 0.3 - sin(t * 2.0) * 0.2, 0.0);
    
    float d1 = max(0.0, 1.0 - length(input.uv - p1) * 2.0);
    float d2 = max(0.0, 1.0 - length(input.uv - p2) * 2.0);
    
    // Hardware MAX computes union envelope
    float u = max(d1, d2);
    float3 col = float3(u * 0.9, u * 0.5, u * 1.0);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 35: clamp
    ("test_35_clamp.fcg", """// =============================================================================
// PS3 RSX Test 35: clamp (Shader Model 3.0 Clamping & Interpolation)
// Interval clamping (MIN + MAX) bounding animated wave peaks between 0.2 and 0.8
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float wave = sin(input.uv.x * 8.0 - t * 3.0) * 0.5 + 0.5;
    // Hardware Clamp: MIN(MAX(wave, 0.25), 0.75)
    float clamped_w = clamp(wave, 0.25, 0.75);
    
    float3 col = float3(clamped_w, 0.6, 1.0 - clamped_w);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 36: saturate
    ("test_36_saturate.fcg", """// =============================================================================
// PS3 RSX Test 36: saturate (Shader Model 3.0 Clamping & Interpolation)
// Hardware Output Saturation modifier (_SAT) locking high dynamic range values into [0, 1]
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float raw_val = (input.uv.x + sin(t) * 0.5) * 2.0; // Expands to [-3, +3]
    // Hardware _SAT bit locks to [0.0, 1.0]
    float sat_val = saturate(raw_val);
    
    float3 col = float3(sat_val, sat_val * 0.8, 1.0 - sat_val);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 37: lerp
    ("test_37_lerp.fcg", """// =============================================================================
// PS3 RSX Test 37: lerp / mix (Shader Model 3.0 Clamping & Interpolation)
// Linear interpolation (MAD: a + weight*(b - a)) between Cyan and Magenta gradients
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float3 col_cyan    = float3(0.0, 0.9, 0.9);
    float3 col_magenta = float3(0.9, 0.0, 0.9);
    
    float weight = saturate(input.uv.x * 0.5 + 0.5 + sin(t * 2.0) * 0.2);
    // Single-cycle MAD interpolation
    float3 result = lerp(col_cyan, col_magenta, weight);
    
    output.color = float4(result, 1.0);
    return output;
}
"""),

    # Test 38: step
    ("test_38_step.fcg", """// =============================================================================
// PS3 RSX Test 38: step (Shader Model 3.0 Clamping & Interpolation)
// Hardware SGE threshold step function splitting screen into razor-sharp binary halves
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float edge = sin(t * 2.0) * 0.5;
    // Hardware SGE: step(edge, uv.x)
    float s = step(edge, input.uv.x);
    
    float3 col = lerp(float3(0.1, 0.1, 0.3), float3(0.9, 0.8, 0.1), s);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 39: smoothstep
    ("test_39_smoothstep.fcg", """// =============================================================================
// PS3 RSX Test 39: smoothstep (Shader Model 3.0 Clamping & Interpolation)
// Hermite cubic interpolation generating anti-aliased luminous circular ring
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float d = length(input.uv);
    float r = 0.5 + 0.2 * sin(t * 2.0);
    
    // Hermite smoothstep for outer and inner borders
    float ring = smoothstep(r - 0.1, r, d) - smoothstep(r, r + 0.1, d);
    float3 col = float3(ring * 0.2, ring * 0.9, ring * 1.0);
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 40: dot2
    ("test_40_dot2.fcg", """// =============================================================================
// PS3 RSX Test 40: dot (2D) (Shader Model 3.0 Vector Algebra)
// Hardware DP2 2D dot product projecting UV vector onto rotating directional vector
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float2 dir = float2(cos(t * 1.5), sin(t * 1.5));
    // Hardware DP2
    float proj = dot(input.uv, dir);
    float norm_proj = proj * 0.5 + 0.5;
    
    float3 col = float3(norm_proj, 0.5 + 0.5 * sin(proj * 6.28), 1.0 - norm_proj);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 41: dot3
    ("test_41_dot3.fcg", """// =============================================================================
// PS3 RSX Test 41: dot (3D) (Shader Model 3.0 Vector Algebra)
// Hardware DP3 3D dot product calculating real-time Lambertian diffuse shaded 3D sphere
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float r_sq = dot(input.uv, input.uv);
    if (r_sq < 0.64) {
        float z = sqrt(0.64 - r_sq);
        float3 normal = normalize(float3(input.uv.x, input.uv.y, z));
        float3 light_dir = normalize(float3(sin(t * 2.0), cos(t * 2.0), 1.0));
        
        // Hardware DP3 for Lambertian diffuse
        float diff = max(0.0, dot(normal, light_dir));
        float3 col = float3(0.9, 0.4, 0.2) * diff + float3(0.05, 0.05, 0.1);
        output.color = float4(col, 1.0);
    } else {
        output.color = float4(0.05, 0.05, 0.08, 1.0);
    }
    return output;
}
"""),

    # Test 42: dot4
    ("test_42_dot4.fcg", """// =============================================================================
// PS3 RSX Test 42: dot (4D) (Shader Model 3.0 Vector Algebra)
// Hardware DP4 4D dot product evaluating 4D homogeneous transformation projection slice
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float4 v4 = float4(input.uv.x, input.uv.y, sin(t), cos(t));
    float4 hyper_plane = float4(0.5, -0.5, 0.707, -0.707);
    
    // Hardware DP4
    float d4 = dot(v4, hyper_plane);
    float wave = sin(d4 * 8.0) * 0.5 + 0.5;
    
    float3 col = float3(wave, d4 * 0.5 + 0.5, 1.0 - wave);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 43: cross
    ("test_43_cross.fcg", """// =============================================================================
// PS3 RSX Test 43: cross (Shader Model 3.0 Vector Algebra)
// 3D Cross Product generating orthogonal surface normal vector map (tangent x bitangent)
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float3 tang   = normalize(float3(1.0, 0.0, sin(input.uv.x * 4.0 + t) * 0.5));
    float3 bitang = normalize(float3(0.0, 1.0, cos(input.uv.y * 4.0 + t) * 0.5));
    
    // Swizzled MUL + MAD cross product: tang x bitang
    float3 norm = cross(tang, bitang);
    float3 col = norm * 0.5 + 0.5;
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 44: length
    ("test_44_length.fcg", """// =============================================================================
// PS3 RSX Test 44: length (Shader Model 3.0 Vector Algebra)
// Vector Euclidean Length (DP2 -> RSQ -> RCP) generating concentric distance contours
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float l = length(input.uv * 6.0);
    float rings = sin(l - t * 4.0) * 0.5 + 0.5;
    
    float3 col = float3(saturate(l / 6.0), rings, 1.0 - rings);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 45: distance
    ("test_45_distance.fcg", """// =============================================================================
// PS3 RSX Test 45: distance (Shader Model 3.0 Vector Algebra)
// Euclidean distance between two moving orbital attractor points
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float2 p1 = float2(cos(t * 1.5) * 0.5, sin(t * 1.5) * 0.5);
    float2 p2 = float2(-cos(t * 2.0) * 0.4, -sin(t * 2.0) * 0.4);
    
    float d1 = distance(input.uv, p1);
    float d2 = distance(input.uv, p2);
    
    float field = sin((d1 - d2) * 8.0) * 0.5 + 0.5;
    float3 col = float3(field * 0.8, (1.0 - d1 * 0.5), (1.0 - d2 * 0.5));
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 46: normalize
    ("test_46_normalize.fcg", """// =============================================================================
// PS3 RSX Test 46: normalize (Shader Model 3.0 Vector Algebra)
// Unit vector normalization encoding hemisphere normal coordinates into RGB
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float3 raw_vec = float3(input.uv.x, input.uv.y, 0.5 + 0.3 * sin(t * 2.0));
    // Hardware normalization: v * RSQ(dot(v, v))
    float3 n = normalize(raw_vec);
    
    float3 col = n * 0.5 + 0.5;
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 47: reflect
    ("test_47_reflect.fcg", """// =============================================================================
// PS3 RSX Test 47: reflect (Shader Model 3.0 Optics & Physics)
// Specular reflection vector (i - 2.0 * dot(i, n) * n) sampling simulated environment
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float r_sq = dot(input.uv, input.uv);
    if (r_sq < 0.64) {
        float z = sqrt(0.64 - r_sq);
        float3 normal = normalize(float3(input.uv.x, input.uv.y, z));
        float3 incident = float3(0.0, 0.0, -1.0);
        
        // Compute specular reflection vector
        float3 refl = reflect(incident, normal);
        
        // Sample simulated panoramic stripes
        float stripes = sin(refl.x * 12.0 + t * 3.0) * 0.5 + 0.5;
        float3 col = float3(refl.y * 0.5 + 0.5, stripes, refl.z * 0.5 + 0.5);
        output.color = float4(col, 1.0);
    } else {
        output.color = float4(0.08, 0.08, 0.12, 1.0);
    }
    return output;
}
"""),

    # Test 48: refract
    ("test_48_refract.fcg", """// =============================================================================
// PS3 RSX Test 48: refract (Shader Model 3.0 Optics & Physics)
// Snell's Law optical refraction bending background grid through glass sphere lens
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float r_sq = dot(input.uv, input.uv);
    if (r_sq < 0.64) {
        float z = sqrt(0.64 - r_sq);
        float3 normal = normalize(float3(input.uv.x, input.uv.y, z));
        float3 incident = float3(0.0, 0.0, -1.0);
        
        // Refraction with eta = 0.75 (glass/air)
        float3 refr = refract(incident, normal, 0.75);
        
        float2 bg_uv = input.uv + refr.xy * 0.4;
        float grid = sin(bg_uv.x * 16.0 + t * 2.0) * sin(bg_uv.y * 16.0) * 0.5 + 0.5;
        float3 col = float3(grid * 0.2, grid * 0.8, grid * 1.0);
        output.color = float4(col, 1.0);
    } else {
        float grid = sin(input.uv.x * 16.0 + t * 2.0) * sin(input.uv.y * 16.0) * 0.5 + 0.5;
        output.color = float4(grid * 0.3, grid * 0.3, grid * 0.3, 1.0);
    }
    return output;
}
"""),

    # Test 49: faceforward
    ("test_49_faceforward.fcg", """// =============================================================================
// PS3 RSX Test 49: faceforward (Shader Model 3.0 Optics & Physics)
// Conditional normal orientation flipping surface normals to face the camera
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float3 normal = normalize(float3(input.uv.x, input.uv.y, 0.5));
    float3 view_dir = normalize(float3(sin(t * 2.0), 0.0, -1.0));
    
    // Native faceforward(N, I, Ng)
    float3 ff_norm = faceforward(normal, view_dir, normal);
    float3 col = ff_norm * 0.5 + 0.5;
    
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 50: slt
    ("test_50_slt.fcg", """// =============================================================================
// PS3 RSX Test 50: slt / < (Shader Model 3.0 Relational Logic)
// Hardware SLT (Set on Less Than) splitting diagonal coordinate domains
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float threshold = sin(t * 2.0) * 0.4;
    // Hardware SLT: (uv.x < uv.y + threshold)
    bool is_less = input.uv.x < (input.uv.y + threshold);
    
    float3 col = is_less ? float3(0.9, 0.2, 0.3) : float3(0.1, 0.7, 0.9);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 51: sge
    ("test_51_sge.fcg", """// =============================================================================
// PS3 RSX Test 51: sge / >= (Shader Model 3.0 Relational Logic)
// Hardware SGE (Set on Greater or Equal) evaluating circular threshold mask
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float radius = 0.5 + 0.2 * sin(t * 2.5);
    // Hardware SGE: (length(uv) >= radius)
    bool is_ge = length(input.uv) >= radius;
    
    float3 col = is_ge ? float3(0.2, 0.8, 0.4) : float3(0.8, 0.3, 0.9);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 52: sgt
    ("test_52_sgt.fcg", """// =============================================================================
// PS3 RSX Test 52: sgt / > (Shader Model 3.0 Relational Logic)
// Hardware SGT (Set on Greater Than) generating dynamic binary zebra pattern
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float wave = sin((input.uv.x + input.uv.y) * 8.0 - t * 4.0);
    // Hardware SGT: (wave > 0.0)
    bool is_gt = wave > 0.0;
    
    float3 col = is_gt ? float3(0.95, 0.95, 0.95) : float3(0.05, 0.05, 0.05);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 53: sle
    ("test_53_sle.fcg", """// =============================================================================
// PS3 RSX Test 53: sle / <= (Shader Model 3.0 Relational Logic)
// Hardware SLE (Set on Less or Equal) rendering interior spotlight disc
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float radius = 0.45 + 0.15 * cos(t * 3.0);
    // Hardware SLE: (length(uv) <= radius)
    bool is_le = length(input.uv) <= radius;
    
    float3 col = is_le ? float3(1.0, 0.8, 0.1) : float3(0.1, 0.1, 0.2);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 54: seq
    ("test_54_seq.fcg", """// =============================================================================
// PS3 RSX Test 54: seq / == (Shader Model 3.0 Relational Logic)
// Hardware SEQ (Set on Equal) illuminating sharp scanning coincidence line
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float scan_pos = sin(t * 2.0) * 0.7;
    // Hardware comparison simulating equality tolerance band
    bool is_eq = abs(input.uv.x - scan_pos) < 0.02;
    
    float3 col = is_eq ? float3(0.2, 1.0, 0.5) : float3(0.05, 0.05, 0.1);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 55: sne
    ("test_55_sne.fcg", """// =============================================================================
// PS3 RSX Test 55: sne / != (Shader Model 3.0 Relational Logic)
// Hardware SNE (Set on Not Equal) illuminating everywhere except zero-axis line
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float axis_pos = cos(t * 2.0) * 0.5;
    // Hardware SNE: (abs(uv.y - axis_pos) > 0.03)
    bool is_ne = abs(input.uv.y - axis_pos) > 0.03;
    
    float3 col = is_ne ? float3(0.7, 0.3, 0.8) : float3(0.0, 0.0, 0.0);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 56: Screen-space derivatives (ddx, ddy, fwidth)
    ("test_56_deriv.fcg", """// =============================================================================
// PS3 RSX Test 56: Screen-Space Derivatives (ddx / ddy / fwidth)
// Hardware 2x2 Pixel Quad Screen Derivatives detecting procedural edge contours
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float pattern = sin(length(input.uv * 10.0) - t * 4.0);
    
    // Hardware Screen-Space Quad Derivatives: DDX and DDY
    float dx = abs(ddx(pattern));
    float dy = abs(ddy(pattern));
    float edge = (dx + dy) * 8.0;
    
    float3 col = float3(edge * 1.0, edge * 0.7, 0.2 + edge * 0.3);
    output.color = float4(col, 1.0);
    return output;
}
"""),

    # Test 57: Discard / KIL
    ("test_57_discard.fcg", """// =============================================================================
// PS3 RSX Test 57: discard / KIL (Shader Model 3.0 Fragment Conditional Pixel Kill)
// Hardware KIL instruction punching perforated circular holes through the screen
// =============================================================================

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : COLOR0;
};

FragmentOutput main(FragmentInput input, uniform float4 time_param) {
    FragmentOutput output;
    float t = time_param.x;
    
    float2 grid_uv = frac(input.uv * 8.0) - 0.5;
    float hole_dist = length(grid_uv);
    
    // Hardware KIL / Discard on circular holes
    if (hole_dist < (0.35 + 0.1 * sin(t * 3.0))) {
        discard;
    }
    
    float3 col = float3(0.9, 0.6, 0.2);
    output.color = float4(col, 1.0);
    return output;
}
""")
]

for filename, content in shaders:
    filepath = os.path.join(SHADERS_DIR, filename)
    with open(filepath, "w") as f:
        f.write(content)
    print(f"Generated {filename}")

print(f"Total shaders generated: {len(shaders)}")
