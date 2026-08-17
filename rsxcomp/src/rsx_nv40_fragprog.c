/*
 * PS3 OpenGraphics Toolkit - rsxcomp
 * NV40 / RSX Fragment Program Microcode Translator & Register Allocator
 *
 * Based on and derived from the Nouveau NV30/NV40/NVFX driver architecture in Mesa 3D.
 * Copyright (C) 2009-2011 Patrice Mandin, Arthur Huillet, Francisco Jerez,
 *                         Marcin Kościelnicki, Christoph Bumiller, Stéphane Marchesin,
 *                         and the Nouveau / Mesa 3D Project.
 *
 * Implements NV40 fragment program instruction encoding (128-bit microcode words),
 * ALU source operand swizzling, condition code write masks, and texture sampler mapping.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rsx_compiler.h"
#include "nvfx_shader.h"

/*
 * Map RSX IR Opcode to Hardware NV40 / RSX Fragment Program Microcode Opcode
 */
static uint8_t map_fp_opcode(uint32_t op) {
    switch (op) {
        case RSX_IR_OP_NOP:   return NVFX_FP_OP_OPCODE_NOP;
        case RSX_IR_OP_MOV:
        case RSX_IR_OP_ABS:
        case RSX_IR_OP_I2F:
        case RSX_IR_OP_F2I:
        case RSX_IR_OP_U2F:
        case RSX_IR_OP_F2U:
        case RSX_IR_OP_F2D:
        case RSX_IR_OP_D2F:
        case RSX_IR_OP_NOT:
        case RSX_IR_OP_CMP:
        case RSX_IR_OP_UCMP:
        case RSX_IR_OP_SSG:
        case RSX_IR_OP_ISSG:  return NVFX_FP_OP_OPCODE_MOV;
        case RSX_IR_OP_MUL:
        case RSX_IR_OP_UMUL:
        case RSX_IR_OP_DMUL:
        case RSX_IR_OP_IMUL_HI:
        case RSX_IR_OP_UMUL_HI: return NVFX_FP_OP_OPCODE_MUL;
        case RSX_IR_OP_ADD:
        case RSX_IR_OP_SUB:
        case RSX_IR_OP_UADD:
        case RSX_IR_OP_DADD:
        case RSX_IR_OP_INEG:
        case RSX_IR_OP_DNEG:  return NVFX_FP_OP_OPCODE_ADD;
        case RSX_IR_OP_MAD:
        case RSX_IR_OP_FMA:
        case RSX_IR_OP_DFMA:
        case RSX_IR_OP_UMAD:
        case RSX_IR_OP_DMAD:
        case RSX_IR_OP_LRP:   return NVFX_FP_OP_OPCODE_MAD;
        case RSX_IR_OP_DIV:
        case RSX_IR_OP_IDIV:
        case RSX_IR_OP_UDIV:
        case RSX_IR_OP_DDIV:  return NVFX_FP_OP_OPCODE_MUL;
        case RSX_IR_OP_DP2:   return NVFX_FP_OP_OPCODE_DP3;
        case RSX_IR_OP_DP2A:  return NVFX_FP_OP_OPCODE_DP2A;
        case RSX_IR_OP_DP3:   return NVFX_FP_OP_OPCODE_DP3;
        case RSX_IR_OP_DP4:
        case RSX_IR_OP_DPH:   return NVFX_FP_OP_OPCODE_DP4;
        case RSX_IR_OP_DST:   return NVFX_FP_OP_OPCODE_DST;
        case RSX_IR_OP_MIN:
        case RSX_IR_OP_IMIN:
        case RSX_IR_OP_UMIN:
        case RSX_IR_OP_DMIN:  return NVFX_FP_OP_OPCODE_MIN;
        case RSX_IR_OP_MAX:
        case RSX_IR_OP_IMAX:
        case RSX_IR_OP_UMAX:
        case RSX_IR_OP_DMAX:  return NVFX_FP_OP_OPCODE_MAX;
        case RSX_IR_OP_SLT:
        case RSX_IR_OP_FSLT:
        case RSX_IR_OP_ISLT:
        case RSX_IR_OP_USLT:
        case RSX_IR_OP_DSLT:  return NVFX_FP_OP_OPCODE_SLT;
        case RSX_IR_OP_SGE:
        case RSX_IR_OP_FSGE:
        case RSX_IR_OP_ISGE:
        case RSX_IR_OP_USGE:
        case RSX_IR_OP_DSGE:  return NVFX_FP_OP_OPCODE_SGE;
        case RSX_IR_OP_SLE:   return NVFX_FP_OP_OPCODE_SLE;
        case RSX_IR_OP_SGT:   return NVFX_FP_OP_OPCODE_SGT;
        case RSX_IR_OP_SNE:
        case RSX_IR_OP_FSNE:
        case RSX_IR_OP_USNE:
        case RSX_IR_OP_DSNE:  return NVFX_FP_OP_OPCODE_SNE;
        case RSX_IR_OP_SEQ:
        case RSX_IR_OP_FSEQ:
        case RSX_IR_OP_USEQ:
        case RSX_IR_OP_DSEQ:  return NVFX_FP_OP_OPCODE_SEQ;
        case RSX_IR_OP_SFL:   return NVFX_FP_OP_OPCODE_SFL;
        case RSX_IR_OP_STR:   return NVFX_FP_OP_OPCODE_STR;
        case RSX_IR_OP_FRC:
        case RSX_IR_OP_DFRAC: return NVFX_FP_OP_OPCODE_FRC;
        case RSX_IR_OP_FLR:
        case RSX_IR_OP_CEIL:
        case RSX_IR_OP_TRUNC:
        case RSX_IR_OP_ROUND:
        case RSX_IR_OP_DFLR:
        case RSX_IR_OP_DCEIL:
        case RSX_IR_OP_DTRUNC:
        case RSX_IR_OP_DROUND: return NVFX_FP_OP_OPCODE_FLR;
        case RSX_IR_OP_KIL:
        case RSX_IR_OP_KILL_IF:
        case RSX_IR_OP_DEMOTE: return NVFX_FP_OP_OPCODE_KIL;
        case RSX_IR_OP_TEX:
        case RSX_IR_OP_TEX2:
        case RSX_IR_OP_SAMPLE:
        case RSX_IR_OP_SAMPLE_I:
        case RSX_IR_OP_SAMPLE_C:
        case RSX_IR_OP_SAMPLE_POS:
        case RSX_IR_OP_SAMPLE_INFO: return NVFX_FP_OP_OPCODE_TEX;
        case RSX_IR_OP_TXP:   return NVFX_FP_OP_OPCODE_TXP;
        case RSX_IR_OP_TXD:
        case RSX_IR_OP_SAMPLE_D: return NVFX_FP_OP_OPCODE_TXD;
        case RSX_IR_OP_TXB:
        case RSX_IR_OP_TXB2:
        case RSX_IR_OP_SAMPLE_B: return NVFX_FP_OP_OPCODE_TXB;
        case RSX_IR_OP_TXL:
        case RSX_IR_OP_TXL2:
        case RSX_IR_OP_SAMPLE_L:
        case RSX_IR_OP_TXF:
        case RSX_IR_OP_LOD:
        case RSX_IR_OP_LODQ:  return NVFX_FP_OP_OPCODE_TXL_NV40;
        case RSX_IR_OP_DDX:
        case RSX_IR_OP_DDX_FINE: return NVFX_FP_OP_OPCODE_DDX;
        case RSX_IR_OP_DDY:
        case RSX_IR_OP_DDY_FINE: return NVFX_FP_OP_OPCODE_DDY;
        case RSX_IR_OP_RCP:
        case RSX_IR_OP_DRCP:
        case RSX_IR_OP_RCC:   return NVFX_FP_OP_OPCODE_RCP;
        case RSX_IR_OP_RSQ:
        case RSX_IR_OP_DRSQ:
        case RSX_IR_OP_SQRT:
        case RSX_IR_OP_DSQRT: return NVFX_FP_OP_OPCODE_RSQ;
        case RSX_IR_OP_EX2:
        case RSX_IR_OP_EXP:   return NVFX_FP_OP_OPCODE_EX2;
        case RSX_IR_OP_LG2:
        case RSX_IR_OP_LOG:   return NVFX_FP_OP_OPCODE_LG2;
        case RSX_IR_OP_LIT:   return NVFX_FP_OP_OPCODE_LIT_NV30;
        case RSX_IR_OP_LITEX2: return NVFX_FP_OP_OPCODE_LITEX2_NV40;
        case RSX_IR_OP_COS:   return NVFX_FP_OP_OPCODE_COS;
        case RSX_IR_OP_SIN:   return NVFX_FP_OP_OPCODE_SIN;
        case RSX_IR_OP_POW:   return NVFX_FP_OP_OPCODE_POW_NV30;
        case RSX_IR_OP_RFL:   return NVFX_FP_OP_OPCODE_RFL_NV30;
        case RSX_IR_OP_PK4B:  return NVFX_FP_OP_OPCODE_PK4B;
        case RSX_IR_OP_UP4B:  return NVFX_FP_OP_OPCODE_UP4B;
        case RSX_IR_OP_PK2H:  return NVFX_FP_OP_OPCODE_PK2H;
        case RSX_IR_OP_UP2H:  return NVFX_FP_OP_OPCODE_UP2H;
        case RSX_IR_OP_PK4UB: return NVFX_FP_OP_OPCODE_PK4UB;
        case RSX_IR_OP_UP4UB: return NVFX_FP_OP_OPCODE_UP4UB;
        case RSX_IR_OP_PK2US: return NVFX_FP_OP_OPCODE_PK2US;
        case RSX_IR_OP_UP2US: return NVFX_FP_OP_OPCODE_UP2US;
        default:              return NVFX_FP_OP_OPCODE_MOV;
    }
}

/*
 * Encode 18-bit NV40 Fragment Program source operand:
 *   Bits 1..0:   Source Register Type (0=Temp, 1=Input, 2=Const/Imm)
 *   Bits 7..2:   Temp Register Index (0..63)
 *   Bit 8:       Source Half-Precision flag
 *   Bits 10..9:  Swizzle X (0=X, 1=Y, 2=Z, 3=W)
 *   Bits 12..11: Swizzle Y
 *   Bits 14..13: Swizzle Z
 *   Bits 16..15: Swizzle W
 *   Bit 17:      Negate
 */
static uint32_t encode_fp_src(const rsxIRSrc *src) {
    uint32_t sr = 0;
    if (!src || src->file == NVFXSR_NONE) {
        /* In NV40 hardware / Mesa, unused source operands map to NVFX_FP_REG_TYPE_INPUT */
        sr |= (NVFX_FP_REG_TYPE_INPUT << NVFX_FP_REG_TYPE_SHIFT);
        sr |= (0 << NVFX_FP_REG_SWZ_X_SHIFT);
        sr |= (1 << NVFX_FP_REG_SWZ_Y_SHIFT);
        sr |= (2 << NVFX_FP_REG_SWZ_Z_SHIFT);
        sr |= (3 << NVFX_FP_REG_SWZ_W_SHIFT);
        return sr;
    }

    switch (src->file) {
        case NVFXSR_INPUT:
            sr |= (NVFX_FP_REG_TYPE_INPUT << NVFX_FP_REG_TYPE_SHIFT);
            /* Register index for inputs is routed via HW0 INPUT_SRC bitfield */
            break;
        case NVFXSR_TEMP:
            sr |= (NVFX_FP_REG_TYPE_TEMP << NVFX_FP_REG_TYPE_SHIFT);
            sr |= ((src->index & 0x3F) << NVFX_FP_REG_SRC_SHIFT);
            break;
        case NVFXSR_CONST:
        case NVFXSR_IMM:
            sr |= (NVFX_FP_REG_TYPE_CONST << NVFX_FP_REG_TYPE_SHIFT);
            break;
        default:
            sr |= (NVFX_FP_REG_TYPE_TEMP << NVFX_FP_REG_TYPE_SHIFT);
            break;
    }

    if (src->negate) {
        sr |= NVFX_FP_REG_NEGATE;
    }

    sr |= ((src->swizzle[0] & 0x3) << NVFX_FP_REG_SWZ_X_SHIFT);
    sr |= ((src->swizzle[1] & 0x3) << NVFX_FP_REG_SWZ_Y_SHIFT);
    sr |= ((src->swizzle[2] & 0x3) << NVFX_FP_REG_SWZ_Z_SHIFT);
    sr |= ((src->swizzle[3] & 0x3) << NVFX_FP_REG_SWZ_W_SHIFT);

    return sr;
}

/*
 * Legalize IR for NV40 Fragment Pipeline:
 * Resolve hardware constraints where an instruction can reference at most
 * ONE constant/immediate register and at most ONE distinct input attribute.
 */
static void rsx_legalize_ir_fragprog(rsxCompilerContext *ctx) {
    if (!ctx || ctx->num_instructions == 0) return;

    int max_temp = 4;
    for (uint32_t i = 0; i < ctx->num_instructions; i++) {
        const rsxIRInstruction *ir = &ctx->instructions[i];
        if (ir->dst.file == NVFXSR_TEMP && ir->dst.index > max_temp) max_temp = ir->dst.index;
        if (ir->dst.file == NVFXSR_OUTPUT && ir->dst.index > max_temp) max_temp = ir->dst.index;
        for (int s = 0; s < 3; s++) {
            if (ir->src[s].file == NVFXSR_TEMP && ir->src[s].index > max_temp) max_temp = ir->src[s].index;
        }
    }
    int next_temp = max_temp + 1;

    rsxIRInstruction *new_insns = (rsxIRInstruction*)calloc(RSX_MAX_INSTRUCTIONS, sizeof(rsxIRInstruction));
    if (!new_insns) return;
    uint32_t new_count = 0;

    for (uint32_t i = 0; i < ctx->num_instructions; i++) {
        rsxIRInstruction ir = ctx->instructions[i];

        /* Check for multiple constant operands */
        int const_count = 0;
        for (int s = 0; s < 3; s++) {
            if (ir.src[s].file == NVFXSR_CONST || ir.src[s].file == NVFXSR_IMM) {
                const_count++;
                if (const_count > 1 && new_count < RSX_MAX_INSTRUCTIONS) {
                    int tmp_reg = next_temp++;
                    rsxIRInstruction *mov = &new_insns[new_count++];
                    memset(mov, 0, sizeof(*mov));
                    mov->opcode = RSX_IR_OP_MOV;
                    mov->dst.file = NVFXSR_TEMP;
                    mov->dst.index = tmp_reg;
                    mov->dst.writemask = 0xF;
                    mov->src[0] = ir.src[s];
                    mov->src[0].negate = false;
                    mov->src[0].absolute = false;
                    for (int k = 0; k < 4; k++) mov->src[0].swizzle[k] = k;

                    ir.src[s].file = NVFXSR_TEMP;
                    ir.src[s].index = tmp_reg;
                }
            }
        }

        /* Check for multiple input attributes */
        int first_input_idx = -1;
        for (int s = 0; s < 3; s++) {
            if (ir.src[s].file == NVFXSR_INPUT) {
                if (first_input_idx < 0) {
                    first_input_idx = ir.src[s].index;
                } else if (ir.src[s].index != first_input_idx && new_count < RSX_MAX_INSTRUCTIONS) {
                    int tmp_reg = next_temp++;
                    rsxIRInstruction *mov = &new_insns[new_count++];
                    memset(mov, 0, sizeof(*mov));
                    mov->opcode = RSX_IR_OP_MOV;
                    mov->dst.file = NVFXSR_TEMP;
                    mov->dst.index = tmp_reg;
                    mov->dst.writemask = 0xF;
                    mov->src[0] = ir.src[s];
                    mov->src[0].negate = false;
                    mov->src[0].absolute = false;
                    for (int k = 0; k < 4; k++) mov->src[0].swizzle[k] = k;

                    ir.src[s].file = NVFXSR_TEMP;
                    ir.src[s].index = tmp_reg;
                }
            }
        }

        if (new_count < RSX_MAX_INSTRUCTIONS) {
            new_insns[new_count++] = ir;
        }
    }

    memcpy(ctx->instructions, new_insns, new_count * sizeof(rsxIRInstruction));
    ctx->num_instructions = new_count;
    free(new_insns);
}

/*
 * Translate Intermediate Representation (IR) to Hardware NV40 / RSX Fragment Program Microcode
 */
bool rsx_nv40_translate_fragprog(rsxCompilerContext *ctx) {
    if (!ctx) return false;

    rsx_legalize_ir_fragprog(ctx);

    ctx->ucode_dword_count = 0;
    int max_temp_used = 4;
    uint16_t texcoord_mask = 0;
    uint16_t texcoord2d_mask = 0;
    uint16_t texcoord3d_mask = 0;
    bool uses_kil = false;

    for (uint32_t i = 0; i < ctx->num_instructions; i++) {
        const rsxIRInstruction *ir = &ctx->instructions[i];
        uint32_t hw[4] = {0, 0, 0, 0};
        bool has_inline_const = false;
        float inline_const_val[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        int const_idx = -1;

        uint8_t opcode = map_fp_opcode(ir->opcode);
        hw[0] |= (((uint32_t)opcode & 0x3F) << NVFX_FP_OP_OPCODE_SHIFT);

        if (ir->opcode == RSX_IR_OP_KIL || ir->opcode == RSX_IR_OP_KILL_IF || ir->opcode == RSX_IR_OP_DEMOTE) {
            uses_kil = true;
        }

        /* Destination register encoding */
        if (ir->dst.file == NVFXSR_OUTPUT) {
            if (ir->dst.index > max_temp_used) max_temp_used = ir->dst.index;
            if (ir->dst.index == 1) {
                /* Depth output register (R1 = DEPTH) */
                ctx->fp_control |= 0x0000000E;
                hw[0] |= ((uint32_t)(ir->dst.index & 0x3F) << NVFX_FP_OP_OUT_REG_SHIFT);
            } else {
                /* Full-precision color outputs: R0 (COLOR0), R2 (COLOR1), R3 (COLOR2), R4 (COLOR3) */
                hw[0] |= NVFX_FP_OP_PRECISION_FP32;
                hw[0] |= (((uint32_t)ir->dst.index & 0x3F) << NVFX_FP_OP_OUT_REG_SHIFT);
            }
        } else if (ir->dst.file == NVFXSR_TEMP) {
            hw[0] |= (((uint32_t)ir->dst.index & 0x3F) << NVFX_FP_OP_OUT_REG_SHIFT);
            if (ir->dst.index > max_temp_used) max_temp_used = ir->dst.index;
        } else {
            hw[0] |= NV40_FP_OP_OUT_NONE;
        }

        /* Writemask */
        uint8_t wm = ir->dst.writemask ? ir->dst.writemask : 0xF;
        if (wm & 1) hw[0] |= NVFX_FP_OP_OUT_X;
        if (wm & 2) hw[0] |= NVFX_FP_OP_OUT_Y;
        if (wm & 4) hw[0] |= NVFX_FP_OP_OUT_Z;
        if (wm & 8) hw[0] |= NVFX_FP_OP_OUT_W;

        if (ir->saturate) {
            hw[0] |= NVFX_FP_OP_OUT_SAT;
        }

        /* Check Texture Operations */
        if (opcode == NVFX_FP_OP_OPCODE_TEX || opcode == NVFX_FP_OP_OPCODE_TXP ||
            opcode == NVFX_FP_OP_OPCODE_TXB || opcode == NVFX_FP_OP_OPCODE_TXD ||
            opcode == NVFX_FP_OP_OPCODE_TXL_NV40) {
            hw[0] |= (((uint32_t)ir->tex_unit & 0x0F) << NVFX_FP_OP_TEX_UNIT_SHIFT);
            texcoord_mask |= (1 << ir->tex_unit);
            if (ir->tex_target == 2) {
                texcoord2d_mask |= (1 << ir->tex_unit);
            } else if (ir->tex_target == 3) {
                texcoord3d_mask |= (1 << ir->tex_unit);
            } else {
                texcoord2d_mask |= (1 << ir->tex_unit);
            }
        }

        /* Check inputs / attributes */
        for (int s = 0; s < 3; s++) {
            if (ir->src[s].file == NVFXSR_INPUT) {
                hw[0] |= (((uint32_t)ir->src[s].index & 0x0F) << NVFX_FP_OP_INPUT_SRC_SHIFT);
                if (ir->src[s].index >= NVFX_FP_OP_INPUT_SRC_TC0 && ir->src[s].index <= NVFX_FP_OP_INPUT_SRC_TC(7)) {
                    uint32_t tc_idx = ir->src[s].index - NVFX_FP_OP_INPUT_SRC_TC0;
                    texcoord_mask |= (1 << tc_idx);
                    texcoord2d_mask |= (1 << tc_idx);
                }
            } else if (ir->src[s].file == NVFXSR_TEMP) {
                if (ir->src[s].index > max_temp_used) max_temp_used = ir->src[s].index;
            } else if (ir->src[s].file == NVFXSR_CONST || ir->src[s].file == NVFXSR_IMM) {
                has_inline_const = true;
                const_idx = ir->src[s].index;
                if (const_idx >= 0 && const_idx < (int)ctx->num_constants) {
                    memcpy(inline_const_val, ctx->constants[const_idx].values, sizeof(inline_const_val));
                }
            }
        }

        if (ir->opcode == RSX_IR_OP_SLT || ir->opcode == RSX_IR_OP_FSLT ||
            ir->opcode == RSX_IR_OP_ISLT || ir->opcode == RSX_IR_OP_USLT ||
            ir->opcode == RSX_IR_OP_DSLT || ir->opcode == RSX_IR_OP_SGE ||
            ir->opcode == RSX_IR_OP_FSGE || ir->opcode == RSX_IR_OP_ISGE ||
            ir->opcode == RSX_IR_OP_USGE || ir->opcode == RSX_IR_OP_DSGE ||
            ir->opcode == RSX_IR_OP_SLE || ir->opcode == RSX_IR_OP_SGT ||
            ir->opcode == RSX_IR_OP_SNE || ir->opcode == RSX_IR_OP_FSNE ||
            ir->opcode == RSX_IR_OP_USNE || ir->opcode == RSX_IR_OP_DSNE ||
            ir->opcode == RSX_IR_OP_SEQ || ir->opcode == RSX_IR_OP_FSEQ ||
            ir->opcode == RSX_IR_OP_USEQ || ir->opcode == RSX_IR_OP_DSEQ ||
            ir->opcode == RSX_IR_OP_SFL || ir->opcode == RSX_IR_OP_STR) {
            hw[0] |= NVFX_FP_OP_COND_WRITE;
        }

        /* Encode Source 0, 1, 2 */
        hw[1] |= encode_fp_src(&ir->src[0]);
        uint32_t cond_test = NVFX_FP_OP_COND_TR;
        if (ir->opcode == RSX_IR_OP_KIL || ir->opcode == RSX_IR_OP_KILL_IF || ir->opcode == RSX_IR_OP_DEMOTE) {
            cond_test = ir->src[0].negate ? NVFX_FP_OP_COND_EQ : NVFX_FP_OP_COND_NE;
        }
        hw[1] |= (cond_test << NVFX_FP_OP_COND_SHIFT);
        hw[1] |= (((ir->src[0].swizzle[0] & 3) << NVFX_FP_OP_COND_SWZ_X_SHIFT) |
                  ((ir->src[0].swizzle[1] & 3) << NVFX_FP_OP_COND_SWZ_Y_SHIFT) |
                  ((ir->src[0].swizzle[2] & 3) << NVFX_FP_OP_COND_SWZ_Z_SHIFT) |
                  ((ir->src[0].swizzle[3] & 3) << NVFX_FP_OP_COND_SWZ_W_SHIFT));
        hw[2] |= encode_fp_src(&ir->src[1]);
        hw[3] |= encode_fp_src(&ir->src[2]);

        /* Source Absolutes: bits 29, 30, 31 in hw[1] for src0, src1, src2 */
        if (ir->src[0].absolute || ir->opcode == RSX_IR_OP_ABS) hw[1] |= (1u << 29);
        if (ir->src[1].absolute) hw[1] |= (1u << 30);
        if (ir->src[2].absolute) hw[1] |= (1u << 31);

        /* If SUB, negate source 1 */
        if (ir->opcode == RSX_IR_OP_SUB) {
            hw[2] |= NVFX_FP_REG_NEGATE;
        }

        /* Mark end of program */
        if (i == ctx->num_instructions - 1) {
            hw[0] |= NVFX_FP_OP_PROGRAM_END;
        }

        /* Write 4 instruction dwords with NV40 16-bit halfword swap */
        for (int k = 0; k < 4; k++) {
            ctx->ucode[ctx->ucode_dword_count + k] = ((hw[k] >> 16) & 0xFFFF) | ((hw[k] & 0xFFFF) << 16);
        }
        ctx->ucode_dword_count += 4;

        /* If instruction uses inline or uniform constants, append 4 constant dwords */
        if (has_inline_const) {
            uint32_t const_byte_offset = ctx->ucode_dword_count * 4;
            if (const_idx >= 0 && const_idx < (int)ctx->num_constants) {
                rsxCompilerConst *c = &ctx->constants[const_idx];
                if (c->patch_count < 64) {
                    c->patch_offsets[c->patch_count++] = const_byte_offset;
                }
            }

            uint32_t raw_floats[4];
            memcpy(raw_floats, inline_const_val, sizeof(raw_floats));
            for (int k = 0; k < 4; k++) {
                /*
                 * NV40 GPU fragment pipeline microcode fetches all 128-bit slots with 16-bit halfword swap.
                 * Embedded constant float dwords must have high and low 16-bit halves swapped so that
                 * both real RSX hardware, rsxSetFragmentProgramParameter, and emulator runtimes
                 * decode valid IEEE-754 single-precision floating point numbers.
                 */
                ctx->ucode[ctx->ucode_dword_count + k] = ((raw_floats[k] >> 16) & 0xFFFF) | ((raw_floats[k] & 0xFFFF) << 16);
            }
            ctx->ucode_dword_count += 4;
        }
    }

    ctx->num_regs_used = max_temp_used + 1;
    if (ctx->num_regs_used > 48) ctx->num_regs_used = 48;
    ctx->texcoords_mask |= texcoord_mask;
    ctx->texcoord2D_mask |= texcoord2d_mask;
    ctx->texcoord3D_mask |= texcoord3d_mask;
    ctx->fp_control |= 0x00000040;
    ctx->fp_control |= ((uint32_t)ctx->num_regs_used << NV40_3D_FP_CONTROL_TEMP_COUNT__SHIFT);
    if (uses_kil) {
        ctx->fp_control |= 0x00000080; /* NV40_3D_FP_CONTROL_KIL */
    }

    return true;
}

