#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_OPCODES 16

typedef struct {
    const char *name;
    const char *shader_src;
    const char *expected_opcodes[MAX_OPCODES];
    int opcode_count;
} TestCase;

static const TestCase test_cases[] = {
    {
        .name = "Arithmetic_Basic",
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
        .name = "Vector_Math_Geometry",
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
        .name = "Optics_Reflection_Refraction",
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
        .name = "Trigonometry_Standard",
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
        .name = "Trigonometry_Inverse_Angular",
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
        .name = "Exponential_Power",
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
        .name = "Logarithmic_Functions",
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
        .name = "Math_Rounding_Floor_Frac",
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
        .name = "Clamping_Interpolation_Curves",
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
        .name = "Relational_Comparison_Logic",
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
        .name = "Matrix_Transform_Multiplication",
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
        .name = "Derivatives_ScreenSpace",
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
        .name = "Conditional_Discard_KIL",
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

    printf("=== PS3-OPENGRAPHICS-TOOLKIT INTRINSICS CONFORMANCE HARNESS ===\n");
    printf("Testing %d Comprehensive Shader Model 3.0 Feature Suites...\n\n", total_suites);

    char src_path[256];
    char fpo_path[256];
    char asm_path[256];
    char cmd[1024];

    snprintf(src_path, sizeof(src_path), "testsuite/tests/tmp_intrinsics_test.fcg");
    snprintf(fpo_path, sizeof(fpo_path), "testsuite/tests/tmp_intrinsics_test.fpo");
    snprintf(asm_path, sizeof(asm_path), "testsuite/tests/tmp_intrinsics_test.asm");

    for (int i = 0; i < total_suites; i++) {
        const TestCase *tc = &test_cases[i];

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
        snprintf(cmd, sizeof(cmd), "%s -f -i %s -o %s > /dev/null 2>&1", compiler_bin, src_path, fpo_path);
        int ret = system(cmd);
        if (ret != 0) {
            printf("[FAIL] %s: Compilation error (exit code %d)\n", tc->name, ret);
            failed++;
            unlink(src_path);
            continue;
        }

        /* 2. Disassemble binary microcode */
        snprintf(cmd, sizeof(cmd), "%s %s -o %s > /dev/null 2>&1", disasm_bin, fpo_path, asm_path);
        ret = system(cmd);
        if (ret != 0) {
            printf("[FAIL] %s: Disassembly error (exit code %d)\n", tc->name, ret);
            failed++;
            unlink(src_path);
            unlink(fpo_path);
            continue;
        }

        /* 3. Inspect disassembled assembly for expected hardware opcodes */
        char *asm_content = read_file_to_string(asm_path);
        if (!asm_content) {
            printf("[FAIL] %s: Failed to read disassembled output\n", tc->name);
            failed++;
            unlink(src_path);
            unlink(fpo_path);
            unlink(asm_path);
            continue;
        }

        int missing_count = 0;
        char missing_str[256] = "";
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
            char opcodes_str[256] = "";
            for (int op = 0; op < tc->opcode_count; op++) {
                if (op > 0) strcat(opcodes_str, ", ");
                strcat(opcodes_str, tc->expected_opcodes[op]);
            }
            printf("[PASS] %s: All %d opcodes verified (%s)\n", tc->name, tc->opcode_count, opcodes_str);
            passed++;
        }

        free(asm_content);
        unlink(src_path);
        unlink(fpo_path);
        unlink(asm_path);
    }

    printf("\n========================================================\n");
    printf("Total Suites: %d | Passed: %d | Failed: %d\n", total_suites, passed, failed);
    printf("========================================================\n");

    return (failed > 0) ? 1 : 0;
}
