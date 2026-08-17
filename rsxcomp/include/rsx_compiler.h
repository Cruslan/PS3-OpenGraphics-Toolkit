/*
 * PS3 OpenGraphics Toolkit - rsxcomp
 * Main Compiler Context, Intermediate Representation (IR), and Public Interface
 *
 * Designed as an open-source alternative to the proprietary NVIDIA Cg compiler for PS3.
 * Incorporates concepts from Mesa 3D (Gallium3D Nouveau nvfx driver), Khronos SPIR-V,
 * and PSL1GHT SDK runtime specifications.
 */

#ifndef __RSX_COMPILER_H__
#define __RSX_COMPILER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "rsx_program.h"
#include "nvfx_shader.h"

#define RSX_MAX_INSTRUCTIONS 4096
#define RSX_MAX_CONSTANTS    468
#define RSX_MAX_ATTRIBUTES   16
#define RSX_MAX_TEMPS        48
#define RSX_MAX_SAMPLERS     16

typedef enum {
    RSX_IR_OP_NOP = 0,
    RSX_IR_OP_MOV,
    RSX_IR_OP_ADD,
    RSX_IR_OP_SUB,
    RSX_IR_OP_MUL,
    RSX_IR_OP_MAD,
    RSX_IR_OP_DIV,
    RSX_IR_OP_DP2,
    RSX_IR_OP_DP2A,
    RSX_IR_OP_DP3,
    RSX_IR_OP_DP4,
    RSX_IR_OP_DPH,
    RSX_IR_OP_DST,
    RSX_IR_OP_MIN,
    RSX_IR_OP_MAX,
    RSX_IR_OP_SLT,
    RSX_IR_OP_SGE,
    RSX_IR_OP_SLE,
    RSX_IR_OP_SGT,
    RSX_IR_OP_SNE,
    RSX_IR_OP_SEQ,
    RSX_IR_OP_SFL,
    RSX_IR_OP_STR,
    RSX_IR_OP_SSG,
    RSX_IR_OP_FRC,
    RSX_IR_OP_FLR,
    RSX_IR_OP_CEIL,
    RSX_IR_OP_TRUNC,
    RSX_IR_OP_ROUND,
    RSX_IR_OP_ABS,
    RSX_IR_OP_CMP,
    RSX_IR_OP_UCMP,
    RSX_IR_OP_LRP,
    RSX_IR_OP_FMA,
    RSX_IR_OP_SQRT,
    RSX_IR_OP_LDEXP,
    RSX_IR_OP_KIL,
    RSX_IR_OP_KILL_IF,
    RSX_IR_OP_DEMOTE,
    RSX_IR_OP_TEX,
    RSX_IR_OP_TXP,
    RSX_IR_OP_TXD,
    RSX_IR_OP_TXB,
    RSX_IR_OP_TXL,
    RSX_IR_OP_TXF,
    RSX_IR_OP_TXQ,
    RSX_IR_OP_TXQS,
    RSX_IR_OP_TEX2,
    RSX_IR_OP_TXB2,
    RSX_IR_OP_TXL2,
    RSX_IR_OP_SAMPLE,
    RSX_IR_OP_SAMPLE_I,
    RSX_IR_OP_SAMPLE_I_MS,
    RSX_IR_OP_SAMPLE_B,
    RSX_IR_OP_SAMPLE_C,
    RSX_IR_OP_SAMPLE_C_LZ,
    RSX_IR_OP_SAMPLE_D,
    RSX_IR_OP_SAMPLE_L,
    RSX_IR_OP_GATHER4,
    RSX_IR_OP_SVIEWINFO,
    RSX_IR_OP_SAMPLE_POS,
    RSX_IR_OP_SAMPLE_INFO,
    RSX_IR_OP_DDX,
    RSX_IR_OP_DDY,
    RSX_IR_OP_DDX_FINE,
    RSX_IR_OP_DDY_FINE,
    RSX_IR_OP_RCP,
    RSX_IR_OP_RCC,
    RSX_IR_OP_RSQ,
    RSX_IR_OP_EX2,
    RSX_IR_OP_LG2,
    RSX_IR_OP_EXP,
    RSX_IR_OP_LOG,
    RSX_IR_OP_LIT,
    RSX_IR_OP_LITEX2,
    RSX_IR_OP_SIN,
    RSX_IR_OP_COS,
    RSX_IR_OP_POW,
    RSX_IR_OP_ARL,
    RSX_IR_OP_ARR,
    RSX_IR_OP_ARA,
    RSX_IR_OP_UARL,
    RSX_IR_OP_PK4B,
    RSX_IR_OP_UP4B,
    RSX_IR_OP_PK2H,
    RSX_IR_OP_UP2H,
    RSX_IR_OP_PK4UB,
    RSX_IR_OP_UP4UB,
    RSX_IR_OP_PK2US,
    RSX_IR_OP_UP2US,
    RSX_IR_OP_PUSHA,
    RSX_IR_OP_POPA,
    RSX_IR_OP_BRA,
    RSX_IR_OP_CAL,
    RSX_IR_OP_RET,
    RSX_IR_OP_IF,
    RSX_IR_OP_UIF,
    RSX_IR_OP_ELSE,
    RSX_IR_OP_ENDIF,
    RSX_IR_OP_BGNLOOP,
    RSX_IR_OP_ENDLOOP,
    RSX_IR_OP_BGNSUB,
    RSX_IR_OP_ENDSUB,
    RSX_IR_OP_BRK,
    RSX_IR_OP_CONT,
    RSX_IR_OP_NOT,
    RSX_IR_OP_AND,
    RSX_IR_OP_OR,
    RSX_IR_OP_XOR,
    RSX_IR_OP_SHL,
    RSX_IR_OP_ISHR,
    RSX_IR_OP_USHR,
    RSX_IR_OP_MOD,
    RSX_IR_OP_UMOD,
    RSX_IR_OP_FSEQ,
    RSX_IR_OP_FSGE,
    RSX_IR_OP_FSLT,
    RSX_IR_OP_FSNE,
    RSX_IR_OP_F2I,
    RSX_IR_OP_I2F,
    RSX_IR_OP_F2U,
    RSX_IR_OP_U2F,
    RSX_IR_OP_INEG,
    RSX_IR_OP_IABS,
    RSX_IR_OP_ISSG,
    RSX_IR_OP_IMIN,
    RSX_IR_OP_IMAX,
    RSX_IR_OP_UMIN,
    RSX_IR_OP_UMAX,
    RSX_IR_OP_UADD,
    RSX_IR_OP_UDIV,
    RSX_IR_OP_IDIV,
    RSX_IR_OP_UMUL,
    RSX_IR_OP_UMAD,
    RSX_IR_OP_USEQ,
    RSX_IR_OP_USGE,
    RSX_IR_OP_USLT,
    RSX_IR_OP_USNE,
    RSX_IR_OP_ISGE,
    RSX_IR_OP_ISLT,
    RSX_IR_OP_SWITCH,
    RSX_IR_OP_CASE,
    RSX_IR_OP_DEFAULT,
    RSX_IR_OP_ENDSWITCH,
    RSX_IR_OP_LOAD,
    RSX_IR_OP_STORE,
    RSX_IR_OP_BARRIER,
    RSX_IR_OP_MEMBAR,
    RSX_IR_OP_EMIT,
    RSX_IR_OP_ENDPRIM,
    RSX_IR_OP_FBFETCH,
    RSX_IR_OP_RFL,
    RSX_IR_OP_LOD,
    RSX_IR_OP_LODQ,
    RSX_IR_OP_TG4,
    RSX_IR_OP_IBFE,
    RSX_IR_OP_UBFE,
    RSX_IR_OP_BFI,
    RSX_IR_OP_BREV,
    RSX_IR_OP_POPC,
    RSX_IR_OP_LSB,
    RSX_IR_OP_IMSB,
    RSX_IR_OP_UMSB,
    RSX_IR_OP_BALLOT,
    RSX_IR_OP_READ_INVOC,
    RSX_IR_OP_READ_FIRST,
    RSX_IR_OP_READ_HELPER,
    RSX_IR_OP_CLOCK,
    RSX_IR_OP_IMG2HND,
    RSX_IR_OP_SAMP2HND,
    RSX_IR_OP_ATOMUADD,
    RSX_IR_OP_ATOMXCHG,
    RSX_IR_OP_ATOMCAS,
    RSX_IR_OP_ATOMAND,
    RSX_IR_OP_ATOMOR,
    RSX_IR_OP_ATOMXOR,
    RSX_IR_OP_ATOMUMIN,
    RSX_IR_OP_ATOMUMAX,
    RSX_IR_OP_ATOMIMIN,
    RSX_IR_OP_ATOMIMAX,
    RSX_IR_OP_ATOMFADD,
    RSX_IR_OP_ATOMINC_WRAP,
    RSX_IR_OP_ATOMDEC_WRAP,
    RSX_IR_OP_IMUL_HI,
    RSX_IR_OP_UMUL_HI,
    RSX_IR_OP_INTERP_CENTROID,
    RSX_IR_OP_INTERP_SAMPLE,
    RSX_IR_OP_INTERP_OFFSET,
    RSX_IR_OP_F2D,
    RSX_IR_OP_D2F,
    RSX_IR_OP_DABS,
    RSX_IR_OP_DNEG,
    RSX_IR_OP_DADD,
    RSX_IR_OP_DMUL,
    RSX_IR_OP_DMAX,
    RSX_IR_OP_DMIN,
    RSX_IR_OP_DSLT,
    RSX_IR_OP_DSGE,
    RSX_IR_OP_DSEQ,
    RSX_IR_OP_DSNE,
    RSX_IR_OP_DRCP,
    RSX_IR_OP_DSQRT,
    RSX_IR_OP_DMAD,
    RSX_IR_OP_DFRAC,
    RSX_IR_OP_DLDEXP,
    RSX_IR_OP_D2I,
    RSX_IR_OP_I2D,
    RSX_IR_OP_D2U,
    RSX_IR_OP_U2D,
    RSX_IR_OP_DRSQ,
    RSX_IR_OP_DTRUNC,
    RSX_IR_OP_DCEIL,
    RSX_IR_OP_DFLR,
    RSX_IR_OP_DROUND,
    RSX_IR_OP_DSSG,
    RSX_IR_OP_DDIV,
    RSX_IR_OP_DFMA,
    RSX_IR_OP_F2U64,
    RSX_IR_OP_F2I64,
    RSX_IR_OP_D2U64,
    RSX_IR_OP_D2I64,
    RSX_IR_OP_U2I64,
    RSX_IR_OP_I2I64,
    RSX_IR_OP_U642D,
    RSX_IR_OP_I642D,
    RSX_IR_OP_U642F,
    RSX_IR_OP_I642F,
    RSX_IR_OP_U64SEQ,
    RSX_IR_OP_U64SNE,
    RSX_IR_OP_I64SLT,
    RSX_IR_OP_U64SLT,
    RSX_IR_OP_I64SGE,
    RSX_IR_OP_U64SGE,
    RSX_IR_OP_I64MIN,
    RSX_IR_OP_U64MIN,
    RSX_IR_OP_I64MAX,
    RSX_IR_OP_U64MAX,
    RSX_IR_OP_I64ABS,
    RSX_IR_OP_I64SSG,
    RSX_IR_OP_I64NEG,
    RSX_IR_OP_U64ADD,
    RSX_IR_OP_U64MUL,
    RSX_IR_OP_U64SHL,
    RSX_IR_OP_I64SHR,
    RSX_IR_OP_U64SHR,
    RSX_IR_OP_I64DIV,
    RSX_IR_OP_U64DIV,
    RSX_IR_OP_I64MOD,
    RSX_IR_OP_U64MOD,
    RSX_IR_OP_VOTE_ANY,
    RSX_IR_OP_VOTE_ALL,
    RSX_IR_OP_VOTE_EQ
} rsxIROpcode;

typedef enum {
    RSX_PROGRAM_VERTEX = 0,
    RSX_PROGRAM_FRAGMENT = 1
} rsxProgramType;

typedef enum {
    RSX_OUTPUT_VPO = 0,   /* PSL1GHT vertex program binary (.vpo) */
    RSX_OUTPUT_FPO = 1,   /* PSL1GHT fragment program binary (.fpo) */
    RSX_OUTPUT_BIN = 2,   /* Raw microcode binary (.bin) */
    RSX_OUTPUT_HEADER = 3 /* C/C++ header array (.h) */
} rsxOutputFormat;

/* Internal Intermediate Representation (IR) Register */
typedef struct {
    uint8_t file;       /* NVFXSR_TEMP, NVFXSR_INPUT, NVFXSR_CONST, NVFXSR_IMM, NVFXSR_OUTPUT */
    int16_t index;      /* Register or binding index */
    uint8_t swizzle[4]; /* Component swizzle (X, Y, Z, W) */
    bool negate;        /* Negate sign */
    bool absolute;      /* Absolute value */
} rsxIRSrc;

typedef struct {
    uint8_t file;       /* NVFXSR_TEMP, NVFXSR_OUTPUT */
    int16_t index;      /* Register index */
    uint8_t writemask;  /* Bitmask (1=X, 2=Y, 4=Z, 8=W) */
} rsxIRDst;

typedef struct {
    uint32_t opcode;    /* Generic opcode (MOV, ADD, MUL, MAD, DP3, DP4, TEX, etc.) */
    rsxIRDst dst;
    rsxIRSrc src[3];
    uint8_t tex_unit;   /* For texture ops */
    uint8_t tex_target; /* 1D, 2D, 3D, CUBE, RECT */
    bool saturate;      /* Clamp to [0, 1] */
} rsxIRInstruction;

/* Constant entry */
typedef struct {
    char name[64];
    int index;
    uint8_t type;       /* PARAM_FLOAT4, etc. */
    bool is_internal;
    float values[4];
    uint32_t patch_offsets[64];
    uint32_t patch_count;
} rsxCompilerConst;

/* Attribute entry */
typedef struct {
    char name[64];
    int index;
    uint8_t type;
} rsxCompilerAttrib;

/* Compiler Context */
typedef struct {
    rsxProgramType type;
    
    /* IR Instructions */
    rsxIRInstruction instructions[RSX_MAX_INSTRUCTIONS];
    uint32_t num_instructions;
    
    /* Constants & Immediates */
    rsxCompilerConst constants[RSX_MAX_CONSTANTS];
    uint32_t num_constants;
    
    /* Input Attributes */
    rsxCompilerAttrib attributes[RSX_MAX_ATTRIBUTES];
    uint32_t num_attributes;

    /* Output Declarations */
    uint32_t output_mask;
    uint32_t input_mask;
    
    /* Texture coordinates and samplers */
    uint16_t texcoords_mask;
    uint16_t texcoord2D_mask;
    uint16_t texcoord3D_mask;
    
    /* Microcode output buffer (each instruction is 4 dwords / 16 bytes) */
    uint32_t ucode[RSX_MAX_INSTRUCTIONS * 8];
    uint32_t ucode_dword_count;
    
    /* Register allocation metrics */
    uint16_t num_regs_used;
    uint32_t fp_control;
} rsxCompilerContext;

/* Core Compiler API */
void rsx_compiler_init(rsxCompilerContext *ctx, rsxProgramType type);
bool rsx_compiler_translate(rsxCompilerContext *ctx);
bool rsx_compiler_parse_tgsi(rsxCompilerContext *ctx, const char *tgsi_source);
bool rsx_compiler_parse_spirv(rsxCompilerContext *ctx, const uint32_t *spv_words, size_t word_count);

/* Binary Package API */
uint8_t* rsx_package_program(const rsxCompilerContext *ctx, size_t *out_size);
bool rsx_write_output_file(const rsxCompilerContext *ctx, const char *filepath, rsxOutputFormat fmt, const char *symbol_name);

#endif /* __RSX_COMPILER_H__ */
