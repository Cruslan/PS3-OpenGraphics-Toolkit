#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_OPCODES 24

typedef struct {
    const char *name;
    bool is_vertex;
    bool is_tgsi;
    const char *shader_src;
    const char *expected_opcodes[MAX_OPCODES];
    int opcode_count;
} TestCase;

static const TestCase test_cases[] = {
    /* =========================================================================
     * FRAGMENT SHADER INTRINSICS & MATH CONFORMANCE SUITES (1..13)
     * ========================================================================= */
    {
        .name = "Fragment_Arithmetic_Basic",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 c1, uniform float4 c2) {\n"
            "    Out o;\n"
            "    float4 sum = c1 + c2;\n"
            "    float4 diff = c1 - c2;\n"
            "    float4 prod = c1 * c2;\n"
            "    float4 quot = c1 / c2;\n"
            "    float4 neg = -c1;\n"
            "    o.col = sum + diff + prod + quot + neg;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "ADDR", "MULR", "RCPR" },
        .opcode_count = 3
    },
    {
        .name = "Fragment_Vector_Math_Geometry",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float3 v1, uniform float3 v2, uniform float4 v4a, uniform float4 v4b) {\n"
            "    Out o;\n"
            "    float d3 = dot(v1, v2);\n"
            "    float d4 = dot(v4a, v4b);\n"
            "    float3 cr = cross(v1, v2);\n"
            "    float len = length(v1);\n"
            "    float3 n = normalize(v1);\n"
            "    o.col = float4(cr + n, d3 + d4 + len);\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "DP3R", "DP4R", "RSQR", "RCPR", "MULR", "MADR" },
        .opcode_count = 6
    },
    {
        .name = "Fragment_Optics_Reflection_Refraction",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float3 inc, uniform float3 norm) {\n"
            "    Out o;\n"
            "    float3 refl = reflect(inc, norm);\n"
            "    float3 refr = refract(inc, norm, 0.75);\n"
            "    float3 ff = faceforward(norm, inc, norm);\n"
            "    o.col = float4(refl + refr + ff, 1.0);\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "DP3R", "MADR", "RSQR", "RCPR" },
        .opcode_count = 4
    },
    {
        .name = "Fragment_Trigonometry_Standard",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a) {\n"
            "    Out o;\n"
            "    float4 s = sin(a);\n"
            "    float4 c = cos(a);\n"
            "    float4 t = tan(a);\n"
            "    o.col = s + c + t;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "SINR", "COSR", "RCPR", "MULR", "ADDR" },
        .opcode_count = 5
    },
    {
        .name = "Fragment_Trigonometry_Inverse_Angular",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a, uniform float4 b) {\n"
            "    Out o;\n"
            "    float4 as_val = asin(a);\n"
            "    float4 ac_val = acos(a);\n"
            "    float4 at_val = atan(a);\n"
            "    float4 at2_val = atan2(a, b);\n"
            "    float4 rad = radians(a);\n"
            "    float4 deg = degrees(a);\n"
            "    o.col = as_val + ac_val + at_val + at2_val + rad + deg;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "MADR", "MULR", "ADDR", "RCPR" },
        .opcode_count = 4
    },
    {
        .name = "Fragment_Exponential_Power",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a, uniform float4 b) {\n"
            "    Out o;\n"
            "    float4 e = exp(a);\n"
            "    float4 e2 = exp2(a);\n"
            "    float4 p = pow(abs(a), b);\n"
            "    float4 sq = sqrt(abs(a));\n"
            "    float4 rsq = rsqrt(abs(a) + 0.001);\n"
            "    o.col = e + e2 + p + sq + rsq;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "EX2R", "LG2R", "RSQR", "RCPR", "MULR" },
        .opcode_count = 5
    },
    {
        .name = "Fragment_Logarithmic_Functions",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a) {\n"
            "    Out o;\n"
            "    float4 l = log(abs(a));\n"
            "    float4 l2 = log2(abs(a));\n"
            "    float4 l10 = log10(abs(a));\n"
            "    o.col = l + l2 + l10;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "LG2R", "MULR", "ADDR" },
        .opcode_count = 3
    },
    {
        .name = "Fragment_Math_Rounding_Floor_Frac",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a) {\n"
            "    Out o;\n"
            "    float4 ab = abs(a);\n"
            "    float4 sgn = sign(a);\n"
            "    float4 fl = floor(a);\n"
            "    float4 cl = ceil(a);\n"
            "    float4 rd = round(a);\n"
            "    float4 tr = trunc(a);\n"
            "    float4 fr = frac(a);\n"
            "    o.col = ab + sgn + fl + cl + rd + tr + fr;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "MAXR", "FLRR", "FRCR", "SGTR", "SLTR", "ADDR" },
        .opcode_count = 6
    },
    {
        .name = "Fragment_Clamping_Interpolation_Curves",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a, uniform float4 b, uniform float4 w) {\n"
            "    Out o;\n"
            "    float4 mn = min(a, b);\n"
            "    float4 mx = max(a, b);\n"
            "    float4 cl = clamp(a, 0.0, 1.0);\n"
            "    float4 sat = saturate(a);\n"
            "    float4 l = lerp(a, b, w);\n"
            "    float4 st = step(a, b);\n"
            "    float4 sm = smoothstep(0.0, 1.0, a);\n"
            "    o.col = mn + mx + cl + sat + l + st + sm;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "MINR", "MAXR", "MADR", "SGER", "RCPR", "MULR" },
        .opcode_count = 6
    },
    {
        .name = "Fragment_Relational_Comparison_Logic",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a, uniform float4 b) {\n"
            "    Out o;\n"
            "    bool4 lt = a < b;\n"
            "    bool4 gt = a > b;\n"
            "    bool4 eq = a == b;\n"
            "    float4 res = (a.x < b.x) ? a : b;\n"
            "    float4 any_v = any(a) ? 1.0 : 0.0;\n"
            "    float4 all_v = all(b) ? 1.0 : 0.0;\n"
            "    o.col = res + any_v + all_v;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "SLTR", "MADR" },
        .opcode_count = 2
    },
    {
        .name = "Fragment_Matrix_Transform_Multiplication",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4x4 m, uniform float4 v) {\n"
            "    Out o;\n"
            "    float4 t1 = mul(m, v);\n"
            "    float4 t2 = mul(v, m);\n"
            "    float4 t3 = mul(transpose(m), v);\n"
            "    o.col = t1 + t2 + t3;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "DP4R", "ADDR" },
        .opcode_count = 2
    },
    {
        .name = "Fragment_Derivatives_ScreenSpace",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a) {\n"
            "    Out o;\n"
            "    float4 dx = ddx(a);\n"
            "    float4 dy = ddy(a);\n"
            "    float4 fw = fwidth(a);\n"
            "    o.col = dx + dy + fw;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "DDXR", "DDYR", "ADDR" },
        .opcode_count = 3
    },
    {
        .name = "Fragment_Conditional_Discard_KIL",
        .is_vertex = false,
        .is_tgsi = false,
        .shader_src =
            "struct In { float2 uv : TEXCOORD0; };\n"
            "struct Out { float4 col : COLOR0; };\n"
            "Out main(In i, uniform float4 a) {\n"
            "    Out o;\n"
            "    if (a.x < 0.0) discard;\n"
            "    o.col = a;\n"
            "    return o;\n"
            "}\n",
        .expected_opcodes = { "KIL", "MOVR" },
        .opcode_count = 2
    },

    /* =========================================================================
     * VERTEX SHADER INTRINSICS & MATH CONFORMANCE SUITES (14..22)
     * ========================================================================= */
    {
        .name = "Vertex_TGSI_Basic_Transform",
        .is_vertex = true,
        .is_tgsi = true,
        .shader_src =
            "VERT\n"
            "DCL IN[0], POSITION\n"
            "DCL IN[1], COLOR\n"
            "DCL CONST[0], ModelViewProj0\n"
            "DCL CONST[1], ModelViewProj1\n"
            "DCL CONST[2], ModelViewProj2\n"
            "DCL CONST[3], ModelViewProj3\n"
            "DCL OUT[0], POSITION\n"
            "DCL OUT[1], COLOR\n"
            "DP4 OUT[0].x, IN[0], CONST[0]\n"
            "DP4 OUT[0].y, IN[0], CONST[1]\n"
            "DP4 OUT[0].z, IN[0], CONST[2]\n"
            "DP4 OUT[0].w, IN[0], CONST[3]\n"
            "MOV OUT[1], IN[1]\n"
            "END\n",
        .expected_opcodes = { "DP4", "MOV" },
        .opcode_count = 2
    },
    {
        .name = "Vertex_TGSI_ALU_Opcodes",
        .is_vertex = true,
        .is_tgsi = true,
        .shader_src =
            "VERT\n"
            "DCL IN[0], POSITION\n"
            "DCL IN[1], COLOR\n"
            "DCL CONST[0], modelViewProj[0]\n"
            "DCL CONST[1], modelViewProj[1]\n"
            "DCL CONST[2], modelViewProj[2]\n"
            "DCL CONST[3], modelViewProj[3]\n"
            "DCL OUT[0], POSITION\n"
            "DCL OUT[1], GENERIC[0]\n"
            "DP4 OUT[0].x, IN[0], CONST[0]\n"
            "DP4 OUT[0].y, IN[0], CONST[1]\n"
            "DP4 OUT[0].z, IN[0], CONST[2]\n"
            "DP4 OUT[0].w, IN[0], CONST[3]\n"
            "ABS TEMP[0], IN[1]\n"
            "MIN TEMP[1], TEMP[0], IN[1]\n"
            "MAX TEMP[2], TEMP[1], TEMP[0]\n"
            "SLT TEMP[3], TEMP[2], IN[0]\n"
            "SGE TEMP[4], TEMP[3], IN[1]\n"
            "SEQ TEMP[5], TEMP[4], TEMP[0]\n"
            "SNE TEMP[6], TEMP[5], TEMP[1]\n"
            "FRC TEMP[7], TEMP[6]\n"
            "FLR TEMP[8], TEMP[7]\n"
            "MAD OUT[1], TEMP[8], IN[1], TEMP[0]\n"
            "END\n",
        .expected_opcodes = { "DP4", "MIN", "MAX", "SLT", "SGE", "SEQ", "SNE", "FRC", "FLR", "MAD" },
        .opcode_count = 10
    },
    {
        .name = "Vertex_TGSI_Extended_ALU_Opcodes",
        .is_vertex = true,
        .is_tgsi = true,
        .shader_src =
            "VERT\n"
            "DCL IN[0], POSITION\n"
            "DCL IN[1], NORMAL\n"
            "DCL IN[2], COLOR\n"
            "DCL CONST[0], matrix[0]\n"
            "DCL CONST[1], matrix[1]\n"
            "DCL CONST[2], matrix[2]\n"
            "DCL CONST[3], matrix[3]\n"
            "DCL CONST[4], params\n"
            "IMM[0] FLT32 { 0.5, 2.0, 1.0, 0.0 }\n"
            "DCL OUT[0], POSITION\n"
            "DCL OUT[1], COLOR\n"
            "DP4 OUT[0].x, IN[0], CONST[0]\n"
            "DP4 OUT[0].y, IN[0], CONST[1]\n"
            "DP4 OUT[0].z, IN[0], CONST[2]\n"
            "DP4 OUT[0].w, IN[0], CONST[3]\n"
            "RCP TEMP[0].x, IN[1].x\n"
            "RSQ TEMP[1].x, IN[1].y\n"
            "EX2 TEMP[2].x, IN[1].z\n"
            "LG2 TEMP[3].x, IN[1].w\n"
            "SIN TEMP[4].x, IN[2].x\n"
            "COS TEMP[5].x, IN[2].y\n"
            "LIT TEMP[6], IN[1]\n"
            "FMA TEMP[7], TEMP[0], TEMP[1], TEMP[2]\n"
            "SQRT TEMP[8], TEMP[3]\n"
            "ROUND TEMP[9], TEMP[4]\n"
            "CEIL TEMP[10], TEMP[5]\n"
            "TRUNC TEMP[11], TEMP[6]\n"
            "UADD TEMP[12], TEMP[7], TEMP[8]\n"
            "UMUL TEMP[13], TEMP[9], TEMP[10]\n"
            "UMAD TEMP[14], TEMP[11], TEMP[12], TEMP[13]\n"
            "UCMP TEMP[15], TEMP[14], TEMP[7], TEMP[8]\n"
            "FSLT TEMP[16], TEMP[15], IMM[0]\n"
            "FSGE TEMP[17], TEMP[16], CONST[4]\n"
            "MAD OUT[1], TEMP[17], IN[2], IMM[0]\n"
            "END\n",
        .expected_opcodes = { "DP4", "RCP", "RSQ", "EX2", "LG2", "SIN", "COS", "LIT", "MAD", "RSQ", "FLR", "ADD", "MUL", "SLT", "SGE" },
        .opcode_count = 15
    },
    {
        .name = "Vertex_TGSI_Comprehensive_Features",
        .is_vertex = true,
        .is_tgsi = true,
        .shader_src =
            "VERT\n"
            "DCL IN[0], POSITION\n"
            "DCL IN[1], NORMAL\n"
            "DCL IN[2], COLOR\n"
            "DCL CONST[0..3]\n"
            "IMM[0] FLT32 { 1.0, 0.0, 0.0, 1.0 }\n"
            "0: DP4 OUT[0].x, IN[0], CONST[0]\n"
            "1: DP4 OUT[0].y, IN[0], CONST[1]\n"
            "2: DP4 OUT[0].z, IN[0], CONST[2]\n"
            "3: DP4 OUT[0].w, IN[0], CONST[3]\n"
            "4: MOV_SAT TEMP[0], (0.5, 0.5, 0.5, 1.0)\n"
            "5: ADD_SAT TEMP[1], TEMP[0], IN[2]\n"
            "6: MUL_SAT TEMP[2], TEMP[1], CONST[0]\n"
            "7: MAD_SAT TEMP[3], TEMP[2], IN[1], (0.1, 0.2, 0.3, 1.0)\n"
            "8: RCP TEMP[4].x, IN[1].x\n"
            "9: RSQ TEMP[5].x, IN[1].y\n"
            "10: EX2 TEMP[6].x, IN[1].z\n"
            "11: LG2 TEMP[7].x, IN[1].w\n"
            "12: SIN TEMP[8].x, IN[2].x\n"
            "13: COS TEMP[9].x, IN[2].y\n"
            "14: LIT TEMP[10], IN[1]\n"
            "15: DP3 TEMP[11], IN[1], IN[1]\n"
            "16: DP2 TEMP[12], IN[1], IN[2]\n"
            "17: MIN TEMP[13], TEMP[11], TEMP[12]\n"
            "18: MAX TEMP[14], TEMP[13], IMM[0]\n"
            "19: SLT TEMP[15], TEMP[14], (0.8, 0.8, 0.8, 0.8)\n"
            "20: SGE TEMP[16], TEMP[15], CONST[1]\n"
            "21: SEQ TEMP[17], TEMP[16], TEMP[0]\n"
            "22: FRC TEMP[18], TEMP[3]\n"
            "23: FLR TEMP[19], TEMP[18]\n"
            "24: MAD OUT[1], TEMP[19], IN[2], IMM[0]\n"
            "END\n",
        .expected_opcodes = { "DP4", "MOV", "ADD", "MUL", "MAD", "RCP", "RSQ", "EX2", "LG2", "SIN", "COS", "LIT", "DP3", "MIN", "MAX", "SLT", "SGE", "SEQ", "FRC", "FLR" },
        .opcode_count = 20
    },
    {
        .name = "Vertex_Cg_Passthrough_Quad",
        .is_vertex = true,
        .is_tgsi = false,
        .shader_src =
            "struct VertexInput {\n"
            "    float4 pos : POSITION;\n"
            "    float2 uv  : TEXCOORD0;\n"
            "};\n"
            "struct VertexOutput {\n"
            "    float4 pos : POSITION;\n"
            "    float2 uv  : TEXCOORD0;\n"
            "};\n"
            "VertexOutput main(VertexInput input) {\n"
            "    VertexOutput output;\n"
            "    output.pos = input.pos;\n"
            "    output.uv = input.uv;\n"
            "    return output;\n"
            "}\n",
        .expected_opcodes = { "MOV" },
        .opcode_count = 1
    },
    {
        .name = "Vertex_Cg_Transform_Lighting",
        .is_vertex = true,
        .is_tgsi = false,
        .shader_src =
            "struct VertexInput {\n"
            "    float4 pos : POSITION;\n"
            "    float3 normal : NORMAL;\n"
            "    float4 color : COLOR0;\n"
            "};\n"
            "struct VertexOutput {\n"
            "    float4 pos : POSITION;\n"
            "    float4 color : COLOR0;\n"
            "    float2 uv : TEXCOORD0;\n"
            "};\n"
            "VertexOutput main(VertexInput input, uniform float4x4 mvp, uniform float4 light_dir) {\n"
            "    VertexOutput output;\n"
            "    output.pos = mul(mvp, input.pos);\n"
            "    float diff = max(0.0, dot(input.normal, light_dir.xyz));\n"
            "    output.color = input.color * diff + float4(0.2, 0.2, 0.2, 1.0);\n"
            "    output.uv = input.pos.xy;\n"
            "    return output;\n"
            "}\n",
        .expected_opcodes = { "DP4", "DP3", "MAX", "MUL", "ADD", "MOV" },
        .opcode_count = 6
    },
    {
        .name = "Vertex_Cg_Wave_Deformation",
        .is_vertex = true,
        .is_tgsi = false,
        .shader_src =
            "struct VertexInput {\n"
            "    float4 pos : POSITION;\n"
            "    float3 normal : NORMAL;\n"
            "    float2 uv : TEXCOORD0;\n"
            "};\n"
            "struct VertexOutput {\n"
            "    float4 pos : POSITION;\n"
            "    float4 color : COLOR0;\n"
            "    float2 uv : TEXCOORD0;\n"
            "};\n"
            "VertexOutput main(VertexInput input, uniform float4x4 mvp, uniform float4 time_param) {\n"
            "    VertexOutput output;\n"
            "    float t = time_param.x;\n"
            "    float wave = sin(input.pos.x * 4.0 + t) * cos(input.pos.z * 4.0 + t) * 0.25;\n"
            "    float4 displaced_pos = input.pos;\n"
            "    displaced_pos.y += wave;\n"
            "    output.pos = mul(mvp, displaced_pos);\n"
            "    output.color = float4(wave * 2.0 + 0.5, 0.5 - wave * 2.0, 1.0, 1.0);\n"
            "    output.uv = input.uv;\n"
            "    return output;\n"
            "}\n",
        .expected_opcodes = { "DP4", "SIN", "COS", "MUL", "ADD", "MOV" },
        .opcode_count = 6
    },
    {
        .name = "Vertex_Cg_Specular_BlinnPhong",
        .is_vertex = true,
        .is_tgsi = false,
        .shader_src =
            "struct VertexInput {\n"
            "    float4 pos : POSITION;\n"
            "    float3 normal : NORMAL;\n"
            "};\n"
            "struct VertexOutput {\n"
            "    float4 pos : POSITION;\n"
            "    float4 color : COLOR0;\n"
            "};\n"
            "VertexOutput main(VertexInput input, uniform float4x4 mvp, uniform float4 light_dir, uniform float4 eye_pos) {\n"
            "    VertexOutput output;\n"
            "    output.pos = mul(mvp, input.pos);\n"
            "    float3 n = normalize(input.normal);\n"
            "    float3 l = normalize(light_dir.xyz);\n"
            "    float3 v = normalize(eye_pos.xyz - input.pos.xyz);\n"
            "    float3 h = normalize(l + v);\n"
            "    float n_dot_l = max(0.0, dot(n, l));\n"
            "    float n_dot_h = max(0.0, dot(n, h));\n"
            "    float spec = pow(n_dot_h, 16.0);\n"
            "    output.color = float4(float3(0.2, 0.2, 0.2) + float3(0.8, 0.8, 0.8) * n_dot_l + float3(1.0, 1.0, 1.0) * spec, 1.0);\n"
            "    return output;\n"
            "}\n",
        .expected_opcodes = { "DP4", "DP3", "RSQ", "MUL", "ADD", "MAX", "LG2", "EX2", "MOV" },
        .opcode_count = 9
    },
    {
        .name = "Vertex_Cg_Twist_Morph",
        .is_vertex = true,
        .is_tgsi = false,
        .shader_src =
            "struct VertexInput {\n"
            "    float4 pos : POSITION;\n"
            "};\n"
            "struct VertexOutput {\n"
            "    float4 pos : POSITION;\n"
            "    float4 color : COLOR0;\n"
            "};\n"
            "VertexOutput main(VertexInput input, uniform float4x4 mvp, uniform float4 twist_param) {\n"
            "    VertexOutput output;\n"
            "    float angle = input.pos.y * twist_param.x;\n"
            "    float cos_a = cos(angle);\n"
            "    float sin_a = sin(angle);\n"
            "    float4 twisted_pos = input.pos;\n"
            "    twisted_pos.x = input.pos.x * cos_a - input.pos.z * sin_a;\n"
            "    twisted_pos.z = input.pos.x * sin_a + input.pos.z * cos_a;\n"
            "    output.pos = mul(mvp, twisted_pos);\n"
            "    output.color = float4(cos_a * 0.5 + 0.5, sin_a * 0.5 + 0.5, 0.8, 1.0);\n"
            "    return output;\n"
            "}\n",
        .expected_opcodes = { "DP4", "COS", "SIN", "MUL", "ADD", "MOV" },
        .opcode_count = 6
    }
};

static char *read_file_to_string(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read_bytes = fread(buf, 1, sz, f);
    buf[read_bytes] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char *compiler_bin = (access("build/rsxcomp", X_OK) == 0) ? "build/rsxcomp" : ((access("rsxcomp/rsxcomp", X_OK) == 0) ? "rsxcomp/rsxcomp" : ((access("./rsxcomp", X_OK) == 0) ? "./rsxcomp" : "./ps3-shader-compiler"));
    const char *disasm_bin = (access("build/rsxdeasm", X_OK) == 0) ? "build/rsxdeasm" : ((access("rsxdeasm/rsxdeasm", X_OK) == 0) ? "rsxdeasm/rsxdeasm" : ((access("./rsxdeasm", X_OK) == 0) ? "./rsxdeasm" : "./ps3-shader-disasm"));

    int total_suites = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed = 0;
    int failed = 0;

    printf("=== PS3-OPENGRAPHICS-TOOLKIT SHADER CONFORMANCE HARNESS ===\n");
    printf("Executing %d Comprehensive Fragment & Vertex Shader Model 3.0 Test Suites...\n\n", total_suites);

    char src_path[256];
    char bin_path[256];
    char asm_path[256];
    char cmd[1024];

    for (int i = 0; i < total_suites; i++) {
        const TestCase *tc = &test_cases[i];

        if (tc->is_vertex) {
            if (tc->is_tgsi) {
                snprintf(src_path, sizeof(src_path), "testsuite/tests/tmp_vert_test_%d.vert", i);
            } else {
                snprintf(src_path, sizeof(src_path), "testsuite/tests/tmp_vert_test_%d.vcg", i);
            }
            snprintf(bin_path, sizeof(bin_path), "testsuite/tests/tmp_vert_test_%d.vpo", i);
            snprintf(asm_path, sizeof(asm_path), "testsuite/tests/tmp_vert_test_%d.asm", i);
        } else {
            snprintf(src_path, sizeof(src_path), "testsuite/tests/tmp_frag_test_%d.fcg", i);
            snprintf(bin_path, sizeof(bin_path), "testsuite/tests/tmp_frag_test_%d.fpo", i);
            snprintf(asm_path, sizeof(asm_path), "testsuite/tests/tmp_frag_test_%d.asm", i);
        }

        /* Write shader source to temporary file */
        FILE *sf = fopen(src_path, "w");
        if (!sf) {
            fprintf(stderr, "[FAIL] %s: Failed to create temporary source file\n", tc->name);
            failed++;
            continue;
        }
        fputs(tc->shader_src, sf);
        fclose(sf);

        /* 1. Compile shader */
        if (tc->is_vertex) {
            snprintf(cmd, sizeof(cmd), "%s -v -i %s -o %s > /dev/null 2>&1", compiler_bin, src_path, bin_path);
        } else {
            snprintf(cmd, sizeof(cmd), "%s -f -i %s -o %s > /dev/null 2>&1", compiler_bin, src_path, bin_path);
        }
        int ret = system(cmd);
        if (ret != 0) {
            printf("[FAIL] %s: Compilation error (exit code %d)\n", tc->name, ret);
            failed++;
            unlink(src_path);
            continue;
        }

        /* 2. Disassemble binary microcode */
        snprintf(cmd, sizeof(cmd), "%s %s -o %s > /dev/null 2>&1", disasm_bin, bin_path, asm_path);
        ret = system(cmd);
        if (ret != 0) {
            printf("[FAIL] %s: Disassembly error (exit code %d)\n", tc->name, ret);
            failed++;
            unlink(src_path);
            unlink(bin_path);
            continue;
        }

        /* 3. Inspect disassembled assembly for expected hardware opcodes */
        char *asm_content = read_file_to_string(asm_path);
        if (!asm_content) {
            printf("[FAIL] %s: Failed to read disassembled output\n", tc->name);
            failed++;
            unlink(src_path);
            unlink(bin_path);
            unlink(asm_path);
            continue;
        }

        int missing_count = 0;
        char missing_str[512] = "";
        for (int op = 0; op < tc->opcode_count; op++) {
            if (!strstr(asm_content, tc->expected_opcodes[op])) {
                if (missing_count > 0) strcat(missing_str, ", ");
                strcat(missing_str, tc->expected_opcodes[op]);
                missing_count++;
            }
        }

        if (missing_count > 0) {
            printf("[FAIL] %s: Missing expected hardware opcodes: %s\n", tc->name, missing_str);
            failed++;
        } else {
            char opcodes_str[512] = "";
            for (int op = 0; op < tc->opcode_count; op++) {
                if (op > 0) strcat(opcodes_str, ", ");
                strcat(opcodes_str, tc->expected_opcodes[op]);
            }
            printf("[PASS] %s (%s): All %d opcodes verified (%s)\n",
                   tc->name, tc->is_vertex ? "Vertex" : "Fragment", tc->opcode_count, opcodes_str);
            passed++;
        }

        free(asm_content);
        unlink(src_path);
        unlink(bin_path);
        unlink(asm_path);
    }

    printf("\n========================================================\n");
    printf("Total Suites: %d | Passed: %d | Failed: %d\n", total_suites, passed, failed);
    printf("========================================================\n");

    return (failed > 0) ? 1 : 0;
}
