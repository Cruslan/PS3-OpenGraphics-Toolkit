/*
 * PS3 OpenGraphics Toolkit - rsxcomp
 * RSX Binary Packaging & PSL1GHT Big-Endian Container Serializer
 *
 * Implements binary container packaging compatible with PSL1GHT librsx and Sony/NVIDIA
 * Cg binary formats. Cites the PSL1GHT open-source SDK project and homebrew community.
 * Copyright (C) PSL1GHT Project Contributors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rsx_compiler.h"
#include "rsx_program.h"

/* Cross-platform Big-Endian byte swap helpers */
static inline uint16_t swap_be16(uint16_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (uint16_t)(((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8));
#else
    return x;
#endif
}

static inline uint32_t swap_be32(uint32_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((x & 0x000000FFu) << 24) |
            ((x & 0x0000FF00u) << 8)  |
            ((x & 0x00FF0000u) >> 8)  |
            ((x & 0xFF000000u) >> 24));
#else
    return x;
#endif
}

static inline float swap_be_float(float f) {
    union { float f; uint32_t u; } val;
    val.f = f;
    val.u = swap_be32(val.u);
    return val.f;
}

#define ALIGN_VAL(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

uint8_t* rsx_package_program(const rsxCompilerContext *ctx, size_t *out_size) {
    if (!ctx || !out_size) return NULL;

    /* Count named uniform constants (exclude internal compiler immediates) */
    uint32_t num_named_consts = 0;
    uint32_t named_const_indices[RSX_MAX_CONSTANTS];
    for (uint32_t i = 0; i < ctx->num_constants; i++) {
        if (!ctx->constants[i].is_internal) {
            named_const_indices[num_named_consts++] = i;
        }
    }

    size_t header_size = (ctx->type == RSX_PROGRAM_VERTEX) ? sizeof(rsxVertexProgram) : sizeof(rsxFragmentProgram);
    size_t attrib_table_size = ctx->num_attributes * sizeof(rsxProgramAttrib);
    size_t const_table_size = num_named_consts * sizeof(rsxProgramConst);
    size_t ucode_size = ctx->ucode_dword_count * sizeof(uint32_t);

    /* Compute string table size for attributes and named constants */
    size_t string_table_size = 0;
    for (uint32_t i = 0; i < ctx->num_attributes; i++) {
        string_table_size += strlen(ctx->attributes[i].name) + 1;
    }
    for (uint32_t k = 0; k < num_named_consts; k++) {
        uint32_t idx = named_const_indices[k];
        string_table_size += strlen(ctx->constants[idx].name) + 1;
    }

    /* Compute size for rsxConstOffsetTable elements (Fragment Programs only) */
    size_t offset_tables_size = 0;
    if (ctx->type == RSX_PROGRAM_FRAGMENT) {
        for (uint32_t k = 0; k < num_named_consts; k++) {
            uint32_t idx = named_const_indices[k];
            offset_tables_size += ALIGN_VAL(sizeof(uint32_t) + (ctx->constants[idx].patch_count * sizeof(uint32_t)), 4);
        }
    }

    size_t est_total = header_size + attrib_table_size + offset_tables_size + const_table_size + string_table_size + ucode_size + 256;
    uint8_t *buffer = (uint8_t*)calloc(1, est_total);
    if (!buffer) return NULL;

    uint32_t current_offset = header_size;
    uint32_t attr_offset = ALIGN_VAL(current_offset, 4);
    current_offset = attr_offset + attrib_table_size;

    /* For Fragment Programs, write rsxConstOffsetTable before rsxProgramConst */
    uint32_t *const_table_offsets = NULL;
    if (ctx->type == RSX_PROGRAM_FRAGMENT && num_named_consts > 0) {
        const_table_offsets = (uint32_t*)malloc(num_named_consts * sizeof(uint32_t));
        for (uint32_t k = 0; k < num_named_consts; k++) {
            uint32_t idx = named_const_indices[k];
            const rsxCompilerConst *c = &ctx->constants[idx];
            current_offset = ALIGN_VAL(current_offset, 4);
            const_table_offsets[k] = current_offset;

            rsxConstOffsetTable *tbl = (rsxConstOffsetTable*)(buffer + current_offset);
            tbl->num = swap_be32(c->patch_count);
            for (uint32_t p = 0; p < c->patch_count; p++) {
                tbl->offset[p] = swap_be32(c->patch_offsets[p]);
            }
            current_offset += sizeof(uint32_t) + (c->patch_count * sizeof(uint32_t));
        }
    }

    uint32_t const_offset = ALIGN_VAL(current_offset, 4);
    current_offset = const_offset + const_table_size;

    uint32_t str_offset = ALIGN_VAL(current_offset, 4);
    current_offset = str_offset + string_table_size;

    /* Microcode in PSL1GHT is strictly 16-byte aligned */
    uint32_t ucode_offset = ALIGN_VAL(current_offset, 16);
    current_offset = ucode_offset + ucode_size;

    size_t total_size = current_offset;

    /* Write Header */
    if (ctx->type == RSX_PROGRAM_VERTEX) {
        rsxVertexProgram *vp = (rsxVertexProgram*)buffer;
        vp->magic = swap_be16(RSX_VP_MAGIC);
        vp->_pad0 = 0;
        vp->num_regs = swap_be16(ctx->num_regs_used);
        vp->num_attr = swap_be16(ctx->num_attributes);
        vp->num_const = swap_be16(num_named_consts);
        vp->num_insn = swap_be16(ctx->num_instructions);
        vp->attr_off = swap_be32(attr_offset);
        vp->const_off = swap_be32(const_offset);
        vp->ucode_off = swap_be32(ucode_offset);
        vp->input_mask = swap_be32(ctx->input_mask);
        vp->output_mask = swap_be32(ctx->output_mask);
        vp->const_start = swap_be16(0);
        vp->insn_start = swap_be16(0);
    } else {
        rsxFragmentProgram *fp = (rsxFragmentProgram*)buffer;
        fp->magic = swap_be16(RSX_FP_MAGIC);
        fp->_pad0 = 0;
        fp->num_regs = swap_be16(ctx->num_regs_used);
        fp->num_attr = swap_be16(ctx->num_attributes);
        fp->num_const = swap_be16(num_named_consts);
        fp->num_insn = swap_be16((uint16_t)(ctx->ucode_dword_count / 4));
        fp->attr_off = swap_be32(attr_offset);
        fp->const_off = swap_be32(const_offset);
        fp->ucode_off = swap_be32(ucode_offset);
        fp->fp_control = swap_be32(ctx->fp_control);
        fp->texcoords = swap_be16(ctx->texcoords_mask);
        fp->texcoord2D = swap_be16(ctx->texcoord2D_mask);
        fp->texcoord3D = swap_be16(ctx->texcoord3D_mask);
        fp->_pad1 = 0;
    }

    /* Write Attributes Table */
    uint32_t cur_str = str_offset;
    if (ctx->num_attributes > 0) {
        rsxProgramAttrib *attribs = (rsxProgramAttrib*)(buffer + attr_offset);
        for (uint32_t i = 0; i < ctx->num_attributes; i++) {
            size_t name_len = strlen(ctx->attributes[i].name) + 1;
            memcpy(buffer + cur_str, ctx->attributes[i].name, name_len);

            attribs[i].name_off = swap_be32(cur_str);
            attribs[i].index = swap_be32(ctx->attributes[i].index);
            attribs[i].type = ctx->attributes[i].type;
            cur_str += name_len;
        }
    }

    /* Write Constants Table (Named Uniforms only) */
    if (num_named_consts > 0) {
        rsxProgramConst *consts = (rsxProgramConst*)(buffer + const_offset);
        for (uint32_t k = 0; k < num_named_consts; k++) {
            uint32_t idx = named_const_indices[k];
            const rsxCompilerConst *c = &ctx->constants[idx];
            size_t name_len = strlen(c->name) + 1;
            memcpy(buffer + cur_str, c->name, name_len);

            consts[k].name_off = swap_be32(cur_str);
            if (ctx->type == RSX_PROGRAM_FRAGMENT && const_table_offsets) {
                consts[k].index = swap_be32(const_table_offsets[k]);
            } else {
                consts[k].index = swap_be32(c->index);
            }
            consts[k].type = c->type ? c->type : PARAM_FLOAT4;
            consts[k].is_internal = 0;
            consts[k].count = 1;
            for (int j = 0; j < 4; j++) {
                consts[k].values[j].f = swap_be_float(c->values[j]);
            }
            cur_str += name_len;
        }
    }

    if (const_table_offsets) {
        free(const_table_offsets);
    }

    /* Write Microcode */
    uint32_t *be_ucode = (uint32_t*)(buffer + ucode_offset);
    for (uint32_t i = 0; i < ctx->ucode_dword_count; i++) {
        be_ucode[i] = swap_be32(ctx->ucode[i]);
    }

    *out_size = total_size;
    return buffer;
}

bool rsx_write_output_file(const rsxCompilerContext *ctx, const char *filepath, rsxOutputFormat fmt, const char *symbol_name) {
    if (!ctx || !filepath) return false;

    if (fmt == RSX_OUTPUT_BIN) {
        /* Raw microcode binary */
        FILE *f = fopen(filepath, "wb");
        if (!f) return false;
        for (uint32_t i = 0; i < ctx->ucode_dword_count; i++) {
            uint32_t val = swap_be32(ctx->ucode[i]);
            fwrite(&val, sizeof(val), 1, f);
        }
        fclose(f);
        return true;
    }

    size_t pkg_size = 0;
    uint8_t *pkg = rsx_package_program(ctx, &pkg_size);
    if (!pkg) return false;

    if (fmt == RSX_OUTPUT_HEADER) {
        /* C/C++ Header Array */
        FILE *f = fopen(filepath, "w");
        if (!f) {
            free(pkg);
            return false;
        }

        char sym[128];
        if (symbol_name && *symbol_name) {
            strncpy(sym, symbol_name, sizeof(sym) - 1);
            sym[sizeof(sym) - 1] = '\0';
        } else {
            snprintf(sym, sizeof(sym), (ctx->type == RSX_PROGRAM_VERTEX) ? "rsx_vertex_program" : "rsx_fragment_program");
        }

        fprintf(f, "/* Autogenerated by PS3 RSX Offline Shader Compiler */\n");
        fprintf(f, "#ifndef __%s_H__\n#define __%s_H__\n\n", sym, sym);
        fprintf(f, "#include <stdint.h>\n\n");
        fprintf(f, "static const uint8_t %s_data[%zu] __attribute__((aligned(16))) = {\n", sym, pkg_size);
        for (size_t i = 0; i < pkg_size; i++) {
            fprintf(f, "0x%02X%s", pkg[i], (i + 1 == pkg_size) ? "" : ", ");
            if ((i + 1) % 16 == 0) fprintf(f, "\n    ");
        }
        fprintf(f, "\n};\n\n");
        fprintf(f, "static const void *%s = (const void*)%s_data;\n\n", sym, sym);
        fprintf(f, "#endif /* __%s_H__ */\n", sym);
        fclose(f);
        free(pkg);
        return true;
    }

    /* Default .vpo / .fpo binary */
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        free(pkg);
        return false;
    }
    fwrite(pkg, 1, pkg_size, f);
    fclose(f);
    free(pkg);
    return true;
}
