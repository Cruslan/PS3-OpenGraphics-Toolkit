/*
 * PS3 OpenGraphics Toolkit - rsxcomp
 * RSX Vertex and Fragment Program Data Structures & Binary Types
 *
 * Defines structures compatible with PSL1GHT SDK (librsx) and Sony Cg binary format.
 * Copyright (C) PSL1GHT Project Contributors.
 */

#ifndef __RSX_PROGRAM_H__
#define __RSX_PROGRAM_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

#define PARAM_BOOL              0
#define PARAM_BOOL1             1
#define PARAM_BOOL2             2
#define PARAM_BOOL3             3
#define PARAM_BOOL4             4
#define PARAM_FLOAT             5
#define PARAM_FLOAT1            6
#define PARAM_FLOAT2            7
#define PARAM_FLOAT3            8
#define PARAM_FLOAT4            9
#define PARAM_FLOAT3x4          10
#define PARAM_FLOAT4x4          11
#define PARAM_FLOAT3x3          12
#define PARAM_FLOAT4x3          13
#define PARAM_SAMPLER1D         14
#define PARAM_SAMPLER2D         15
#define PARAM_SAMPLER3D         16
#define PARAM_SAMPLERCUBE       17
#define PARAM_SAMPLERRECT       18
#define PARAM_INT               19
#define PARAM_INT1              20
#define PARAM_INT2              21
#define PARAM_INT3              22
#define PARAM_INT4              23
#define PARAM_UNKNOWN           0xff

#define RSX_VP_MAGIC            0x5650  /* 'VP' */
#define RSX_FP_MAGIC            0x4650  /* 'FP' */

#pragma pack(push, 1)

typedef struct rsx_vp
{
    u16 magic;          /* 0x0101 */
    u16 _pad0;          /* 0 */

    u16 num_regs;       /* Number of temporary registers used */
    u16 num_attr;       /* Number of input attributes */
    u16 num_const;      /* Number of constants */
    u16 num_insn;       /* Number of vertex instructions (16 bytes each) */

    u32 attr_off;       /* Offset to rsxProgramAttrib array */
    u32 const_off;      /* Offset to rsxProgramConst array */
    u32 ucode_off;      /* Offset to microcode */

    u32 input_mask;     /* Mask of input attributes */
    u32 output_mask;    /* Mask of output attributes */

    u16 const_start;    /* Constant block start index */
    u16 insn_start;     /* Instruction start index */
} rsxVertexProgram;

typedef struct rsx_fp
{
    u16 magic;          /* 0x0201 */
    u16 _pad0;          /* 0 */

    u16 num_regs;       /* Number of temporary registers used */
    u16 num_attr;       /* Number of input attributes */
    u16 num_const;      /* Number of constants */
    u16 num_insn;       /* Number of fragment instructions */

    u32 attr_off;       /* Offset to rsxProgramAttrib array */
    u32 const_off;      /* Offset to rsxProgramConst array */
    u32 ucode_off;      /* Offset to microcode */

    u32 fp_control;     /* Fragment program control mask (e.g. register count) */

    u16 texcoords;      /* Bitmask of all used texture coords */
    u16 texcoord2D;     /* Bitmask of used 2D texture coords */
    u16 texcoord3D;     /* Bitmask of used 3D texture coords */
    u16 _pad1;          /* 0 */
} rsxFragmentProgram;

typedef struct rsx_const
{
    u32 name_off;       /* Offset to null-terminated name string */
    u32 index;          /* Constant index / register id / offset */
    u8 type;            /* Parameter type (PARAM_FLOAT, etc.) */
    u8 is_internal;     /* 1 = internal constant, 0 = named uniform */
    u8 count;           /* Element count (1..4) */
    u8 _pad0;           /* 0 */

    union {
        u32 u;
        f32 f;
    } values[4];
} rsxProgramConst;

typedef struct rsx_co_table
{
    u32 num;            /* Number of offset elements in table */
    u32 offset[];       /* Variable-length array of microcode offsets */
} rsxConstOffsetTable;

typedef struct rsx_attrib
{
    u32 name_off;       /* Offset to null-terminated name string */
    u32 index;          /* Attribute index */
    u8 type;            /* Attribute type */
    u8 _pad0[3];        /* 0 */
} rsxProgramAttrib;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* __RSX_PROGRAM_H__ */
