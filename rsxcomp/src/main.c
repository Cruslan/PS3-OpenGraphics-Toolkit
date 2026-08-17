/*
 * PS3 OpenGraphics Toolkit - rsxcomp (Offline RSX Shader Compiler)
 *
 * A modern, open-source offline shader compiler converting SPIR-V / TGSI
 * representations into native PlayStation 3 RSX (NV40/NV47) microcode.
 *
 * Credits & Acknowledgments:
 * - Mesa 3D Graphics Library & Nouveau Project: NV30/NV40/NVFX Gallium3D compiler architecture.
 * - Khronos Group: SPIR-V open standard specification and opcode mappings.
 * - PSL1GHT Project: Open-source PlayStation 3 SDK and librsx runtime structures.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "rsx_compiler.h"

extern bool rsx_nv40_translate_vertprog(rsxCompilerContext *ctx);
extern bool rsx_nv40_translate_fragprog(rsxCompilerContext *ctx);

void rsx_compiler_init(rsxCompilerContext *ctx, rsxProgramType type) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->type = type;
}

bool rsx_compiler_translate(rsxCompilerContext *ctx) {
    if (!ctx) return false;
    if (ctx->type == RSX_PROGRAM_VERTEX) {
        return rsx_nv40_translate_vertprog(ctx);
    } else {
        return rsx_nv40_translate_fragprog(ctx);
    }
}

static void print_usage(const char *prog) {
    fprintf(stderr, "PS3 RSX Offline Shader Compiler (FOSS alternative to nvidia-cg-toolkit)\n");
    fprintf(stderr, "Usage: %s [options] -i <input_shader> -o <output_binary>\n\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -v, --vertex          Compile as Vertex Program (.vpo)\n");
    fprintf(stderr, "  -f, --fragment        Compile as Fragment Program (.fpo)\n");
    fprintf(stderr, "  -i, --input <file>    Path to input shader source (TGSI / IR)\n");
    fprintf(stderr, "  -o, --output <file>   Path to output binary file\n");
    fprintf(stderr, "  -c, --header <file>   Generate C/C++ header array (.h)\n");
    fprintf(stderr, "  -s, --symbol <name>   Symbol name for C header array\n");
    fprintf(stderr, "  -b, --raw-bin         Output raw microcode binary without PSL1GHT wrapper\n");
    fprintf(stderr, "  -d, --dump            Dump generated microcode dwords to stdout\n");
    fprintf(stderr, "  -h, --help            Show this help message\n");
}

int main(int argc, char **argv) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    const char *header_file = NULL;
    const char *symbol_name = NULL;
    rsxProgramType prog_type = RSX_PROGRAM_VERTEX;
    bool type_specified = false;
    bool raw_bin = false;
    bool dump_disasm = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--vertex")) {
            prog_type = RSX_PROGRAM_VERTEX;
            type_specified = true;
        } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--fragment")) {
            prog_type = RSX_PROGRAM_FRAGMENT;
            type_specified = true;
        } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input")) {
            if (++i < argc) input_file = argv[i];
        } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if (++i < argc) output_file = argv[i];
        } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--header")) {
            if (++i < argc) header_file = argv[i];
        } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--symbol")) {
            if (++i < argc) symbol_name = argv[i];
        } else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--raw-bin")) {
            raw_bin = true;
        } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--dump")) {
            dump_disasm = true;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            /* Positional argument: first is input, second is output */
            if (!input_file) {
                input_file = argv[i];
            } else if (!output_file) {
                output_file = argv[i];
            } else {
                fprintf(stderr, "Error: Unexpected extra argument '%s'\n", argv[i]);
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!input_file) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    /* Auto-detect shader type from file extension if not explicitly given */
    if (!type_specified) {
        if (strstr(input_file, ".vert") || strstr(input_file, ".vp") || strstr(input_file, ".vcg")) {
            prog_type = RSX_PROGRAM_VERTEX;
        } else if (strstr(input_file, ".frag") || strstr(input_file, ".fp") || strstr(input_file, ".fcg")) {
            prog_type = RSX_PROGRAM_FRAGMENT;
        }
    }

    FILE *f = fopen(input_file, "rb");
    if (!f) {
        fprintf(stderr, "Error: Unable to open input file '%s'\n", input_file);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = (char*)malloc(size + 1);
    if (!source) {
        fclose(f);
        fprintf(stderr, "Error: Out of memory reading '%s'\n", input_file);
        return 1;
    }

    size_t read_bytes = fread(source, 1, size, f);
    source[read_bytes] = '\0';
    fclose(f);



    rsxCompilerContext ctx;
    rsx_compiler_init(&ctx, prog_type);
    bool parsed = false;

    /* Check if input is a Cg / HLSL file (.vcg, .fcg, .hlsl) for SPIR-V translation */
    if (strstr(input_file, ".vcg") || strstr(input_file, ".fcg") || strstr(input_file, ".hlsl")) {
        char tmp_spv[256];
        snprintf(tmp_spv, sizeof(tmp_spv), "/tmp/ps3_shader_%d.spv", getpid());

        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "glslangValidator -D -V -e main -S %s \"%s\" -o \"%s\" > /dev/null 2>&1",
                 (prog_type == RSX_PROGRAM_VERTEX) ? "vert" : "frag", input_file, tmp_spv);

        int ret = system(cmd);
        if (ret == 0) {
            FILE *spvf = fopen(tmp_spv, "rb");
            if (spvf) {
                fseek(spvf, 0, SEEK_END);
                long spv_sz = ftell(spvf);
                fseek(spvf, 0, SEEK_SET);

                if (spv_sz >= 20) {
                    uint32_t *spv_words = (uint32_t*)malloc(spv_sz);
                    if (spv_words) {
                        size_t r = fread(spv_words, 1, spv_sz, spvf);
                        if (r == (size_t)spv_sz) {
                            parsed = rsx_compiler_parse_spirv(&ctx, spv_words, spv_sz / 4);
                        }
                        free(spv_words);
                    }
                }
                fclose(spvf);
                remove(tmp_spv);
            }
        }
    }

    /* If not parsed via SPIR-V frontend, try TGSI text parser */
    if (!parsed) {
        parsed = rsx_compiler_parse_tgsi(&ctx, source);
    }
    free(source);

    if (!parsed) {
        fprintf(stderr, "Error: Failed to parse shader source from '%s'\n", input_file);
        return 1;
    }

    if (!rsx_compiler_translate(&ctx)) {
        fprintf(stderr, "Error: Failed to translate shader instructions into RSX microcode\n");
        return 1;
    }

    printf("[PS3-Shader-Compiler] Compiled %s successfully!\n", (ctx.type == RSX_PROGRAM_VERTEX) ? "Vertex Program" : "Fragment Program");
    printf("  Instructions: %u\n", ctx.num_instructions);
    printf("  Microcode Words: %u (size: %zu bytes)\n", ctx.ucode_dword_count, ctx.ucode_dword_count * sizeof(uint32_t));
    printf("  Registers: %u\n", ctx.num_regs_used);
    printf("  Attributes: %u\n", ctx.num_attributes);
    printf("  Constants: %u\n", ctx.num_constants);

    if (dump_disasm) {
        printf("\n--- RSX Microcode Disassembly Dump ---\n");
        for (uint32_t i = 0; i < ctx.ucode_dword_count; i += 4) {
            printf("[%03u] 0x%08X 0x%08X 0x%08X 0x%08X\n",
                   i >> 2,
                   ctx.ucode[i + 0],
                   ctx.ucode[i + 1],
                   ctx.ucode[i + 2],
                   ctx.ucode[i + 3]);
        }
        printf("--------------------------------------\n\n");
    }

    if (output_file) {
        rsxOutputFormat fmt = raw_bin ? RSX_OUTPUT_BIN : ((ctx.type == RSX_PROGRAM_VERTEX) ? RSX_OUTPUT_VPO : RSX_OUTPUT_FPO);
        if (!rsx_write_output_file(&ctx, output_file, fmt, symbol_name)) {
            fprintf(stderr, "Error: Failed to write binary output file '%s'\n", output_file);
            return 1;
        }
        printf("  Written binary payload to: %s\n", output_file);
    }

    if (header_file) {
        if (!rsx_write_output_file(&ctx, header_file, RSX_OUTPUT_HEADER, symbol_name)) {
            fprintf(stderr, "Error: Failed to write C header file '%s'\n", header_file);
            return 1;
        }
        printf("  Written C header array to: %s\n", header_file);
    }

    return 0;
}
