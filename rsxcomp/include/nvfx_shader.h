/*
 * PS3 OpenGraphics Toolkit - rsxcomp
 * NV40 / NVFX Hardware Shader Register and Instruction Definitions
 *
 * Based on and derived from Mesa 3D Graphics Library (Gallium3D Nouveau nvfx driver).
 * Copyright (C) 2009-2011 Patrice Mandin, Arthur Huillet, Francisco Jerez,
 *                         Marcin Kościelnicki, Christoph Bumiller, Stéphane Marchesin,
 *                         and the Nouveau / Mesa 3D Project.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

#ifndef __NVFX_SHADER_H__
#define __NVFX_SHADER_H__

#include <stdint.h>
#include <stdbool.h>

#define NVFX_SWZ_IDENTITY ((3 << 6) | (2 << 4) | (1 << 2) | (0 << 0))
#define NVFX_SWZ(x, y, z, w) (((w) << 6) | ((z) << 4) | ((y) << 2) | ((x) << 0))

#define NVFX_SWZ_X 0
#define NVFX_SWZ_Y 1
#define NVFX_SWZ_Z 2
#define NVFX_SWZ_W 3

/* Register Types */
#define NVFXSR_NONE   0
#define NVFXSR_OUTPUT 1
#define NVFXSR_INPUT  2
#define NVFXSR_TEMP   3
#define NVFXSR_IMM    4
#define NVFXSR_CONST  5
#define NVFXSR_RELOCATED 6
#define NVFXSR_SAMPLER 7

struct nvfx_reg {
    uint8_t type;
    uint8_t is_fp16;
    int16_t index;
};

struct nvfx_src {
    struct nvfx_reg reg;
    uint8_t swz[4];
    uint8_t negate;
    uint8_t abs;
};

/* =========================================================================
 * NV40 Vertex Program Definitions
 * ========================================================================= */

#define NV40_VP_INST_SCA_OP_NOP    0x00
#define NV40_VP_INST_SCA_OP_MOV    0x01
#define NV40_VP_INST_SCA_OP_RCP    0x02
#define NV40_VP_INST_SCA_OP_RCC    0x03
#define NV40_VP_INST_SCA_OP_RSQ    0x04
#define NV40_VP_INST_SCA_OP_EXP    0x05
#define NV40_VP_INST_SCA_OP_LOG    0x06
#define NV40_VP_INST_SCA_OP_LIT    0x07
#define NV40_VP_INST_SCA_OP_BRA    0x09
#define NV40_VP_INST_SCA_OP_CAL    0x0B
#define NV40_VP_INST_SCA_OP_RET    0x0C
#define NV40_VP_INST_SCA_OP_LG2    0x0D
#define NV40_VP_INST_SCA_OP_EX2    0x0E
#define NV40_VP_INST_SCA_OP_SIN    0x0F
#define NV40_VP_INST_SCA_OP_COS    0x10
#define NV40_VP_INST_SCA_OP_PUSHA  0x13
#define NV40_VP_INST_SCA_OP_POPA   0x14

#define NVFX_VP_INST_SCA_OP_NOP    NV40_VP_INST_SCA_OP_NOP
#define NVFX_VP_INST_SCA_OP_MOV    NV40_VP_INST_SCA_OP_MOV
#define NVFX_VP_INST_SCA_OP_RCP    NV40_VP_INST_SCA_OP_RCP
#define NVFX_VP_INST_SCA_OP_RCC    NV40_VP_INST_SCA_OP_RCC
#define NVFX_VP_INST_SCA_OP_RSQ    NV40_VP_INST_SCA_OP_RSQ
#define NVFX_VP_INST_SCA_OP_EXP    NV40_VP_INST_SCA_OP_EXP
#define NVFX_VP_INST_SCA_OP_LOG    NV40_VP_INST_SCA_OP_LOG
#define NVFX_VP_INST_SCA_OP_LIT    NV40_VP_INST_SCA_OP_LIT
#define NVFX_VP_INST_SCA_OP_BRA    NV40_VP_INST_SCA_OP_BRA
#define NVFX_VP_INST_SCA_OP_CAL    NV40_VP_INST_SCA_OP_CAL
#define NVFX_VP_INST_SCA_OP_RET    NV40_VP_INST_SCA_OP_RET
#define NVFX_VP_INST_SCA_OP_LG2    NV40_VP_INST_SCA_OP_LG2
#define NVFX_VP_INST_SCA_OP_EX2    NV40_VP_INST_SCA_OP_EX2
#define NVFX_VP_INST_SCA_OP_SIN    NV40_VP_INST_SCA_OP_SIN
#define NVFX_VP_INST_SCA_OP_COS    NV40_VP_INST_SCA_OP_COS

#define NV40_VP_INST_VEC_OP_NOP    0x00
#define NV40_VP_INST_VEC_OP_MOV    0x01
#define NV40_VP_INST_VEC_OP_MUL    0x02
#define NV40_VP_INST_VEC_OP_ADD    0x03
#define NV40_VP_INST_VEC_OP_MAD    0x04
#define NV40_VP_INST_VEC_OP_DP3    0x05
#define NV40_VP_INST_VEC_OP_DPH    0x06
#define NV40_VP_INST_VEC_OP_DP4    0x07
#define NV40_VP_INST_VEC_OP_DST    0x08
#define NV40_VP_INST_VEC_OP_MIN    0x09
#define NV40_VP_INST_VEC_OP_MAX    0x0A
#define NV40_VP_INST_VEC_OP_SLT    0x0B
#define NV40_VP_INST_VEC_OP_SGE    0x0C
#define NV40_VP_INST_VEC_OP_ARL    0x0D
#define NV40_VP_INST_VEC_OP_FRC    0x0E
#define NV40_VP_INST_VEC_OP_FLR    0x0F
#define NV40_VP_INST_VEC_OP_SEQ    0x10
#define NV40_VP_INST_VEC_OP_SFL    0x11
#define NV40_VP_INST_VEC_OP_SGT    0x12
#define NV40_VP_INST_VEC_OP_SLE    0x13
#define NV40_VP_INST_VEC_OP_SNE    0x14
#define NV40_VP_INST_VEC_OP_STR    0x15
#define NV40_VP_INST_VEC_OP_SSG    0x16
#define NV40_VP_INST_VEC_OP_ARR    0x17
#define NV40_VP_INST_VEC_OP_ARA    0x18
#define NV40_VP_INST_VEC_OP_TXL    0x19

/* NV40 VP Instruction Bitfields */
#define NV40_VP_INST_SRC_TYPE_TEMP  0x01
#define NV40_VP_INST_SRC_TYPE_INPUT 0x02
#define NV40_VP_INST_SRC_TYPE_CONST 0x03

#define NV40_VP_INST_DEST_MASK_X    (1 << 20)
#define NV40_VP_INST_DEST_MASK_Y    (1 << 21)
#define NV40_VP_INST_DEST_MASK_Z    (1 << 22)
#define NV40_VP_INST_DEST_MASK_W    (1 << 23)
#define NV40_VP_INST_DEST_MASK_ALL  (0xF << 20)

#define NV40_VP_INST_LAST           (1 << 0)

/* =========================================================================
 * NV40 Fragment Program Standard Mesa/Hardware Opcodes
 * ========================================================================= */

#define NVFX_FP_OP_OPCODE_NOP   0x00
#define NVFX_FP_OP_OPCODE_MOV   0x01
#define NVFX_FP_OP_OPCODE_MUL   0x02
#define NVFX_FP_OP_OPCODE_ADD   0x03
#define NVFX_FP_OP_OPCODE_MAD   0x04
#define NVFX_FP_OP_OPCODE_DP3   0x05
#define NVFX_FP_OP_OPCODE_DP4   0x06
#define NVFX_FP_OP_OPCODE_DST   0x07
#define NVFX_FP_OP_OPCODE_MIN   0x08
#define NVFX_FP_OP_OPCODE_MAX   0x09
#define NVFX_FP_OP_OPCODE_SLT   0x0A
#define NVFX_FP_OP_OPCODE_SGE   0x0B
#define NVFX_FP_OP_OPCODE_SLE   0x0C
#define NVFX_FP_OP_OPCODE_SGT   0x0D
#define NVFX_FP_OP_OPCODE_SNE   0x0E
#define NVFX_FP_OP_OPCODE_SEQ   0x0F
#define NVFX_FP_OP_OPCODE_FRC   0x10
#define NVFX_FP_OP_OPCODE_FLR   0x11
#define NVFX_FP_OP_OPCODE_KIL   0x12
#define NVFX_FP_OP_OPCODE_PK4B  0x13
#define NVFX_FP_OP_OPCODE_UP4B  0x14
#define NVFX_FP_OP_OPCODE_DDX   0x15
#define NVFX_FP_OP_OPCODE_DDY   0x16
#define NVFX_FP_OP_OPCODE_TEX   0x17
#define NVFX_FP_OP_OPCODE_TXP   0x18
#define NVFX_FP_OP_OPCODE_TXD   0x19
#define NVFX_FP_OP_OPCODE_RCP   0x1A
#define NVFX_FP_OP_OPCODE_RSQ   0x1B
#define NVFX_FP_OP_OPCODE_EX2   0x1C
#define NVFX_FP_OP_OPCODE_LG2   0x1D
#define NVFX_FP_OP_OPCODE_STR   0x20
#define NVFX_FP_OP_OPCODE_SFL   0x21
#define NVFX_FP_OP_OPCODE_COS   0x22
#define NVFX_FP_OP_OPCODE_SIN   0x23
#define NVFX_FP_OP_OPCODE_PK2H  0x24
#define NVFX_FP_OP_OPCODE_UP2H  0x25
#define NVFX_FP_OP_OPCODE_PK4UB 0x27
#define NVFX_FP_OP_OPCODE_UP4UB 0x28
#define NVFX_FP_OP_OPCODE_PK2US 0x29
#define NVFX_FP_OP_OPCODE_UP2US 0x2A
#define NVFX_FP_OP_OPCODE_DP2A  0x2E
#define NVFX_FP_OP_OPCODE_TXL   0x2F
#define NVFX_FP_OP_OPCODE_TXL_NV40 0x2F
#define NVFX_FP_OP_OPCODE_TXB   0x31
#define NVFX_FP_OP_OPCODE_DIV   0x3A
#define NVFX_FP_OP_OPCODE_LIT_NV30 0x1E
#define NVFX_FP_OP_OPCODE_LRP_NV30 0x1F
#define NVFX_FP_OP_OPCODE_POW_NV30 0x26
#define NVFX_FP_OP_OPCODE_RFL_NV30 0x36
#define NVFX_FP_OP_OPCODE_LITEX2_NV40 0x3C

#define NVFX_FP_REG_TYPE_TEMP  0
#define NVFX_FP_REG_TYPE_INPUT 1
#define NVFX_FP_REG_TYPE_CONST 2

#define NVFX_FP_REG_TYPE_SHIFT 0
#define NVFX_FP_REG_TYPE_MASK  0x03
#define NVFX_FP_REG_SRC_SHIFT  2
#define NVFX_FP_REG_SRC_MASK   0x3F
#define NVFX_FP_REG_SRC_HALF   (1 << 8)

#define NVFX_FP_REG_SWZ_X_SHIFT 9
#define NVFX_FP_REG_SWZ_Y_SHIFT 11
#define NVFX_FP_REG_SWZ_Z_SHIFT 13
#define NVFX_FP_REG_SWZ_W_SHIFT 15
#define NVFX_FP_REG_NEGATE     (1 << 17)

#define NVFX_FP_OP_PROGRAM_END    (1 << 0)
#define NVFX_FP_OP_OUT_REG_SHIFT  1
#define NVFX_FP_OP_OUT_REG_MASK   0x3F
#define NVFX_FP_OP_OUT_REG_HALF   (1 << 7)
#define NVFX_FP_OP_COND_WRITE     (1 << 8)
#define NVFX_FP_OP_OUT_WRITEMASK_X (1 << 9)
#define NVFX_FP_OP_OUT_WRITEMASK_Y (1 << 10)
#define NVFX_FP_OP_OUT_WRITEMASK_Z (1 << 11)
#define NVFX_FP_OP_OUT_WRITEMASK_W (1 << 12)
#define NVFX_FP_OP_OUT_WRITEMASK_ALL (0xF << 9)
#define NVFX_FP_OP_OUT_X NVFX_FP_OP_OUT_WRITEMASK_X
#define NVFX_FP_OP_OUT_Y NVFX_FP_OP_OUT_WRITEMASK_Y
#define NVFX_FP_OP_OUT_Z NVFX_FP_OP_OUT_WRITEMASK_Z
#define NVFX_FP_OP_OUT_W NVFX_FP_OP_OUT_WRITEMASK_W
#define NVFX_FP_OP_INPUT_SRC_SHIFT 13
#define NVFX_FP_OP_INPUT_SRC_MASK  0x0F
#define NVFX_FP_OP_INPUT_SRC_POSITION  0x0
#define NVFX_FP_OP_INPUT_SRC_COL0      0x1
#define NVFX_FP_OP_INPUT_SRC_COL1      0x2
#define NVFX_FP_OP_INPUT_SRC_FOGC      0x3
#define NVFX_FP_OP_INPUT_SRC_TC0       0x4
#define NVFX_FP_OP_INPUT_SRC_TC(n)     (0x4 + (n))
#define NVFX_FP_OP_TEX_UNIT_SHIFT 17
#define NVFX_FP_OP_TEX_UNIT_MASK  0x0F
#define NVFX_FP_OP_PRECISION_FP32 (0 << 22)
#define NVFX_FP_OP_PRECISION_FP16 (1 << 22)
#define NVFX_FP_OP_OPCODE_SHIFT   24
#define NVFX_FP_OP_OPCODE_MASK    0x3F
#define NV40_FP_OP_OUT_NONE       (1 << 30)
#define NVFX_FP_OP_OUT_SAT        (1u << 31)
#define NV40_3D_FP_CONTROL_TEMP_COUNT__SHIFT 24

#define NVFX_FP_OP_COND_SHIFT          18
#define NVFX_FP_OP_COND_MASK           (0x07 << 18)
#define NVFX_FP_OP_COND_FL             0
#define NVFX_FP_OP_COND_LT             1
#define NVFX_FP_OP_COND_EQ             2
#define NVFX_FP_OP_COND_LE             3
#define NVFX_FP_OP_COND_GT             4
#define NVFX_FP_OP_COND_NE             5
#define NVFX_FP_OP_COND_GE             6
#define NVFX_FP_OP_COND_TR             7

#define NVFX_FP_OP_COND_SWZ_X_SHIFT    21
#define NVFX_FP_OP_COND_SWZ_Y_SHIFT    23
#define NVFX_FP_OP_COND_SWZ_Z_SHIFT    25
#define NVFX_FP_OP_COND_SWZ_W_SHIFT    27

#endif /* __NVFX_SHADER_H__ */
