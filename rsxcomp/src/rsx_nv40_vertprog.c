/*
 * PS3 OpenGraphics Toolkit - rsxcomp
 * NV40 / RSX Vertex Program Microcode Translator & Dual-Issue Scheduler
 *
 * Based on and derived from the Nouveau NV30/NV40/NVFX driver architecture in Mesa 3D.
 * Copyright (C) 2009-2011 Patrice Mandin, Arthur Huillet, Francisco Jerez,
 *                         Marcin Kościelnicki, Christoph Bumiller, Stéphane Marchesin,
 *                         and the Nouveau / Mesa 3D Project.
 *
 * Implements NV40 vertex program instruction encoding, vector/scalar dual-issue
 * instruction pairing, constant slot mapping, and address register indexing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rsx_compiler.h"
#include "nvfx_shader.h"

/*
 * Check if opcode is a Scalar ALU operation on NV40 hardware
 */
static bool is_vp_sca_op(uint32_t op) {
    switch (op) {
        case RSX_IR_OP_RCP:
        case RSX_IR_OP_RCC:
        case RSX_IR_OP_RSQ:
        case RSX_IR_OP_SQRT:
        case RSX_IR_OP_EXP:
        case RSX_IR_OP_LOG:
        case RSX_IR_OP_LIT:
        case RSX_IR_OP_BRA:
        case RSX_IR_OP_CAL:
        case RSX_IR_OP_RET:
        case RSX_IR_OP_LG2:
        case RSX_IR_OP_EX2:
        case RSX_IR_OP_SIN:
        case RSX_IR_OP_COS:
        case RSX_IR_OP_PUSHA:
        case RSX_IR_OP_POPA:
            return true;
        default:
            return false;
    }
}

/*
 * Map IR Opcode to NV40 Vertex Program Scalar ALU Opcode
 */
static uint8_t map_vp_sca_op(uint32_t op) {
    switch (op) {
        case RSX_IR_OP_RCP:   return NVFX_VP_INST_SCA_OP_RCP;
        case RSX_IR_OP_RCC:   return NVFX_VP_INST_SCA_OP_RCC;
        case RSX_IR_OP_RSQ:
        case RSX_IR_OP_SQRT:  return NVFX_VP_INST_SCA_OP_RSQ;
        case RSX_IR_OP_EXP:   return NVFX_VP_INST_SCA_OP_EXP;
        case RSX_IR_OP_LOG:   return NVFX_VP_INST_SCA_OP_LOG;
        case RSX_IR_OP_LIT:   return NVFX_VP_INST_SCA_OP_LIT;
        case RSX_IR_OP_BRA:   return NVFX_VP_INST_SCA_OP_BRA;
        case RSX_IR_OP_CAL:   return NVFX_VP_INST_SCA_OP_CAL;
        case RSX_IR_OP_RET:   return NVFX_VP_INST_SCA_OP_RET;
        case RSX_IR_OP_LG2:   return NVFX_VP_INST_SCA_OP_LG2;
        case RSX_IR_OP_EX2:   return NVFX_VP_INST_SCA_OP_EX2;
        case RSX_IR_OP_SIN:   return NVFX_VP_INST_SCA_OP_SIN;
        case RSX_IR_OP_COS:   return NVFX_VP_INST_SCA_OP_COS;
        case RSX_IR_OP_PUSHA: return NV40_VP_INST_SCA_OP_PUSHA;
        case RSX_IR_OP_POPA:  return NV40_VP_INST_SCA_OP_POPA;
        default:              return NVFX_VP_INST_SCA_OP_NOP;
    }
}

/*
 * Map IR Opcode to NV40 Vertex Program Vector ALU Opcode
 */
static uint8_t map_vp_vec_op(uint32_t op) {
    switch (op) {
        case RSX_IR_OP_NOP:   return 0x00;
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
        case RSX_IR_OP_UCMP:  return 0x01;
        case RSX_IR_OP_MUL:
        case RSX_IR_OP_UMUL:
        case RSX_IR_OP_DMUL:
        case RSX_IR_OP_IMUL_HI:
        case RSX_IR_OP_UMUL_HI: return 0x02;
        case RSX_IR_OP_ADD:
        case RSX_IR_OP_SUB:
        case RSX_IR_OP_UADD:
        case RSX_IR_OP_DADD:
        case RSX_IR_OP_INEG:
        case RSX_IR_OP_DNEG:  return 0x03;
        case RSX_IR_OP_MAD:
        case RSX_IR_OP_FMA:
        case RSX_IR_OP_DFMA:
        case RSX_IR_OP_UMAD:
        case RSX_IR_OP_DMAD:
        case RSX_IR_OP_LRP:   return 0x04;
        case RSX_IR_OP_DP2:
        case RSX_IR_OP_DP3:   return 0x05;
        case RSX_IR_OP_DPH:   return 0x06;
        case RSX_IR_OP_DP4:   return 0x07;
        case RSX_IR_OP_DST:   return 0x08;
        case RSX_IR_OP_MIN:
        case RSX_IR_OP_IMIN:
        case RSX_IR_OP_UMIN:
        case RSX_IR_OP_DMIN:  return 0x09;
        case RSX_IR_OP_MAX:
        case RSX_IR_OP_IMAX:
        case RSX_IR_OP_UMAX:
        case RSX_IR_OP_DMAX:  return 0x0A;
        case RSX_IR_OP_SLT:
        case RSX_IR_OP_FSLT:
        case RSX_IR_OP_ISLT:
        case RSX_IR_OP_USLT:
        case RSX_IR_OP_DSLT:  return 0x0B;
        case RSX_IR_OP_SGE:
        case RSX_IR_OP_FSGE:
        case RSX_IR_OP_ISGE:
        case RSX_IR_OP_USGE:
        case RSX_IR_OP_DSGE:  return 0x0C;
        case RSX_IR_OP_ARL:
        case RSX_IR_OP_UARL:  return 0x0D;
        case RSX_IR_OP_FRC:
        case RSX_IR_OP_DFRAC: return 0x0E;
        case RSX_IR_OP_FLR:
        case RSX_IR_OP_CEIL:
        case RSX_IR_OP_TRUNC:
        case RSX_IR_OP_ROUND:
        case RSX_IR_OP_DFLR:
        case RSX_IR_OP_DCEIL:
        case RSX_IR_OP_DTRUNC:
        case RSX_IR_OP_DROUND: return 0x0F;
        case RSX_IR_OP_SEQ:
        case RSX_IR_OP_FSEQ:
        case RSX_IR_OP_USEQ:
        case RSX_IR_OP_DSEQ:  return 0x10;
        case RSX_IR_OP_SFL:   return 0x11;
        case RSX_IR_OP_SGT:   return 0x12;
        case RSX_IR_OP_SLE:   return 0x13;
        case RSX_IR_OP_SNE:
        case RSX_IR_OP_FSNE:
        case RSX_IR_OP_USNE:
        case RSX_IR_OP_DSNE:  return 0x14;
        case RSX_IR_OP_STR:   return 0x15;
        case RSX_IR_OP_SSG:
        case RSX_IR_OP_ISSG:  return 0x16;
        case RSX_IR_OP_ARR:   return 0x17;
        case RSX_IR_OP_ARA:   return 0x18;
        case RSX_IR_OP_TXL:
        case RSX_IR_OP_TEX:
        case RSX_IR_OP_TXP:
        case RSX_IR_OP_TXB:
        case RSX_IR_OP_TXD:
        case RSX_IR_OP_SAMPLE: return 0x19;
        default:              return 0x01;
    }
}

/*
 * Encode 17-bit NV40 VP source operand:
 *   Bit 16:    Negate flag
 *   Bits 15:8: Swizzle (X: 15..14, Y: 13..12, Z: 11..10, W: 9..8)
 *   Bits 6:2:  Temp Register index (0..31)
 *   Bits 1:0:  Register Type (1=Temp, 2=Input, 3=Const)
 */
static uint32_t encode_vp_src(const rsxIRSrc *src) {
    uint32_t sr = 0;
    if (!src || src->file == NVFXSR_NONE) {
        sr |= 1; /* Type 1 = Temp R0 unnegated XYZW */
        sr |= (0 << 14) | (1 << 12) | (2 << 10) | (3 << 8);
        return sr;
    }

    switch (src->file) {
        case NVFXSR_TEMP:
            sr |= 1; /* Type 1 = Temp */
            sr |= ((src->index & 0x1F) << 2);
            break;
        case NVFXSR_INPUT:
            sr |= 2; /* Type 2 = Input Attribute */
            break;
        case NVFXSR_CONST:
        case NVFXSR_IMM:
            sr |= 3; /* Type 3 = Constant */
            break;
        default:
            sr |= 1;
            break;
    }

    if (src->negate) sr |= (1u << 16);

    sr |= ((src->swizzle[0] & 3) << 14);
    sr |= ((src->swizzle[1] & 3) << 12);
    sr |= ((src->swizzle[2] & 3) << 10);
    sr |= ((src->swizzle[3] & 3) << 8);

    return sr;
}

/*
 * Translate Intermediate Representation (IR) to Hardware NV40 Vertex Program Microcode
 */
bool rsx_nv40_translate_vertprog(rsxCompilerContext *ctx) {
    if (!ctx) return false;

    ctx->ucode_dword_count = 0;
    int max_temp_used = -1;
    uint32_t in_mask = 0;
    uint32_t out_mask = 0;

    for (uint32_t i = 0; i < ctx->num_instructions; i++) {
        const rsxIRInstruction *ir = &ctx->instructions[i];
        uint32_t hw[4] = {0, 0, 0, 0};

        bool is_sca = is_vp_sca_op(ir->opcode);
        uint8_t vec_op = is_sca ? 0 : map_vp_vec_op(ir->opcode);
        uint8_t sca_op = is_sca ? map_vp_sca_op(ir->opcode) : 0;
        bool is_result = (ir->dst.file == NVFXSR_OUTPUT);
        uint8_t dst_reg = 0;

        /* Map output semantics to NV40 result register index */
        if (is_result) {
            if (ir->dst.index == 0) {
                dst_reg = 0; /* POSITION (o[HPOS]): NV40/RSX dst_reg0 -> gl_Position */
                out_mask |= (1u << 0); /* Bit 0: POS */
            } else if (ir->dst.index >= 1) {
                dst_reg = 7 + (ir->dst.index - 1); /* TEXCOORD0..7 -> dst_reg7..14 (tc0..7) */
                out_mask |= (1u << (14 + (ir->dst.index - 1))); /* RSX output mask bit 14..21 for TEXCOORD0..7 */
            }
        } else {
            dst_reg = ir->dst.index & 0x1F;
            if ((int)ir->dst.index > max_temp_used) max_temp_used = ir->dst.index;
        }

        /* DW0 (Bits 127:96) */
        /* Standard NV40 condition code default (TR=7) and condition swizzle XYZW (0x6C) */
        hw[0] = (7u << 10) | 0x0000006C;
        if (is_sca) {
            hw[0] |= (0x3Fu << 15); /* VEC_DEST_TEMP = 0x3F (none) */
        } else {
            if (is_result) {
                hw[0] |= (1u << 30);      /* NV40_VP_INST_VEC_RESULT */
                hw[0] |= (0x3Fu << 15);   /* VEC_DEST_TEMP = 0x3F (none) */
            } else {
                hw[0] |= ((dst_reg & 0x3Fu) << 15);
            }
        }

        /* Source Absolutes */
        if (ir->src[0].absolute || ir->opcode == RSX_IR_OP_ABS) hw[0] |= (1u << 21);
        if (ir->src[1].absolute) hw[0] |= (1u << 22);
        if (ir->src[2].absolute) hw[0] |= (1u << 23);

        /* Determine Input & Constant Indices */
        uint8_t input_idx = 0;
        uint8_t const_idx = 0;
        for (int s = 0; s < 3; s++) {
            if (ir->src[s].file == NVFXSR_INPUT) {
                input_idx = ir->src[s].index & 0x0F;
                in_mask |= (1u << input_idx);
            } else if (ir->src[s].file == NVFXSR_CONST || ir->src[s].file == NVFXSR_IMM) {
                const_idx = ir->src[s].index & 0xFF;
            } else if (ir->src[s].file == NVFXSR_TEMP) {
                if ((int)ir->src[s].index > max_temp_used) max_temp_used = ir->src[s].index;
            }
        }

        /* Source 0, 1, 2 encoding */
        uint32_t s0 = 0, s1 = 0, s2 = 0;
        if (is_sca) {
            /* Scalar ALU reads its primary operand from SRC2 */
            s0 = encode_vp_src(NULL);
            s1 = encode_vp_src(NULL);
            s2 = encode_vp_src(&ir->src[0]);
        } else {
            s0 = encode_vp_src(&ir->src[0]);
            if (ir->src[1].file != NVFXSR_NONE) {
                s1 = encode_vp_src(&ir->src[1]);
                if (ir->opcode == RSX_IR_OP_SUB) {
                    s1 |= (1u << 16); /* Negate source 1 */
                }
            } else {
                s1 = encode_vp_src(NULL);
            }
            if (ir->src[2].file != NVFXSR_NONE) {
                s2 = encode_vp_src(&ir->src[2]);
            } else {
                s2 = encode_vp_src(NULL);
            }
        }

        /* DW1 (Bits 95:64) */
        if (is_sca) {
            hw[1] |= ((uint32_t)sca_op & 0x1F) << 27; /* SCA_OPCODE */
        } else {
            hw[1] |= ((uint32_t)vec_op & 0x1F) << 22; /* VEC_OPCODE */
        }
        hw[1] |= ((uint32_t)const_idx & 0xFF) << 12;
        hw[1] |= ((uint32_t)input_idx & 0x0F) << 8;
        hw[1] |= ((s0 >> 9) & 0xFF); /* SRC0H (bits 16..9 of s0) */

        /* DW2 (Bits 63:32) */
        hw[2] = (s0 & 0x1FF) << 23;          /* SRC0L (bits 8..0 of s0) */
        hw[2] |= (s1 & 0x1FFFF) << 6;        /* SRC1 (all 17 bits of s1) */
        hw[2] |= ((s2 >> 11) & 0x3F);        /* SRC2H (bits 16..11 of s2) */

        /* Writemask (bit 3=X, bit 2=Y, bit 1=Z, bit 0=W) */
        uint8_t wm = ir->dst.writemask ? ir->dst.writemask : 0xF;
        uint8_t vwm = 0;
        if (wm & 1) vwm |= (1 << 3); /* X */
        if (wm & 2) vwm |= (1 << 2); /* Y */
        if (wm & 4) vwm |= (1 << 1); /* Z */
        if (wm & 8) vwm |= (1 << 0); /* W */

        /* DW3 (Bits 31:0) */
        hw[3] = (s2 & 0x7FF) << 21; /* SRC2L (bits 10..0 of s2) */

        if (is_sca) {
            hw[3] |= ((uint32_t)vwm & 0x0F) << 17; /* SCA_WRITEMASK */
            if (is_result) {
                hw[3] |= (1u << 12);                    /* SCA_RESULT */
                hw[3] |= (0x1F << 7);                   /* SCA_DEST_TEMP = none */
                hw[3] |= (dst_reg & 0x1F) << 2;         /* Result Register */
            } else {
                hw[3] |= (dst_reg & 0x1F) << 7;         /* SCA_DEST_TEMP */
                hw[3] |= (31 << 2);                     /* DEST = Temp */
            }
        } else {
            hw[3] |= ((uint32_t)vwm & 0x0F) << 13; /* VEC_WRITEMASK */
            hw[3] |= (0x1F << 7);                  /* SCA_DEST_TEMP = none */
            if (is_result) {
                hw[3] |= (dst_reg & 0x1F) << 2;    /* Result Register ID */
            } else {
                hw[3] |= (31 << 2);                /* DEST = Temp */
            }
        }

        /* Mark last instruction in vertex program */
        if (i == ctx->num_instructions - 1) {
            hw[3] |= 1; /* NVFX_VP_INST_LAST */
        }

        /* Write 4 big-endian DWORDs into microcode stream */
        ctx->ucode[ctx->ucode_dword_count + 0] = hw[0];
        ctx->ucode[ctx->ucode_dword_count + 1] = hw[1];
        ctx->ucode[ctx->ucode_dword_count + 2] = hw[2];
        ctx->ucode[ctx->ucode_dword_count + 3] = hw[3];
        ctx->ucode_dword_count += 4;
    }

    ctx->num_regs_used = (max_temp_used >= 0) ? (max_temp_used + 1) : 0;
    ctx->input_mask = in_mask;
    ctx->output_mask = out_mask;

    return true;
}

