/*
 * PS3 OpenGraphics Toolkit - rsxdeasm (RSX Microcode Decompiler & Disassembler)
 *
 * A modern, open-source decompiler and disassembler for PlayStation 3 RSX (NV40/NV47)
 * vertex and fragment shader microcode (.vpo, .fpo, .bin, Cg binary containers).
 *
 * Licensed under the GNU General Public License Version 2 (GPLv2).
 *
 * Credits & Acknowledgments:
 * - RPCS3 PlayStation 3 Emulator Project: Nekotekina, kd-11, eladash, scribblemaniac,
 *   and the RPCS3 contributors for foundational reverse-engineering of the RSX Reality
 *   Synthesizer microcode instruction formats, vector/scalar dual-issue decoding tables,
 *   condition code bitmasks, texture sampling layouts, and Cg binary structures.
 *   (https://github.com/RPCS3/rpcs3 - Licensed under GPLv2)
 *
 * - Mesa 3D Graphics Library & Nouveau Project: For NV30/NV40/NVFX Gallium3D driver
 *   microcode register layout specifications.
 *
 * - PSL1GHT SDK Project: For homebrew RSX binary format container specifications.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

// Standard fixed-width integer aliases
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef float    f32;

// Big-Endian swapping utilities
static inline u16 bswap16(u16 val) {
    return (val << 8) | (val >> 8);
}

static inline u32 bswap32(u32 val) {
    return ((val & 0x000000FFu) << 24) |
           ((val & 0x0000FF00u) << 8)  |
           ((val & 0x00FF0000u) >> 8)  |
           ((val & 0xFF000000u) >> 24);
}

// RSX Fragment Program microcode word swizzler (NV40 GPU pipeline hardware layout)
// The 32-bit words in RSX FP microcode have high and low 16-bit halves swapped
static inline u32 GetData(u32 d) {
    return (d << 16) | (d >> 16);
}

// Safe union-based IEEE-754 bitcast from uint32 to float
static inline f32 u32_to_f32(u32 u) {
    union { u32 u; f32 f; } conv;
    conv.u = u;
    return conv.f;
}

// Format Identification Constants
#define CG_PROFILE_SCE_VP_RSX 7003u // Official Sony Cg Vertex Program
#define CG_PROFILE_SCE_FP_RSX 7004u // Official Sony Cg Fragment Program

#define PSL1GHT_RSX_VP_MAGIC 0x5650 // 'VP' in Big-Endian (0x56='V', 0x50='P')
#define PSL1GHT_RSX_FP_MAGIC 0x4650 // 'FP' in Big-Endian (0x46='F', 0x50='P')

// ============================================================================
// 1. Official Sony SDK / libgcm / NVIDIA Cg Binary Program Headers
// ============================================================================

struct CgBinaryProgram
{
    u32 profile;              // 7003 for sce_vp_rsx, 7004 for sce_fp_rsx
    u32 binaryFormatRevision; // Binary layout revision (usually 6)
    u32 totalSize;            // Total size of binary struct including microcode
    u32 parameterCount;       // Number of uniform / attribute parameters
    u32 parameterArray;       // Byte offset to parameter array
    u32 program;              // Byte offset to domain specific header (VP or FP)
    u32 ucodeSize;            // Size in bytes of raw microcode
    u32 ucode;                // Byte offset to raw microcode
};

struct CgBinaryParameter
{
    u32 type;                 // Cg parameter type ID
    u32 res;                  // Resource binding type
    u32 var;                  // Variability
    s32 resIndex;             // Resource index
    u32 name;                 // Byte offset to ASCII name string
    u32 defaultValue;         // Byte offset to default float values
    u32 embeddedConst;        // Byte offset to embedded constant table
    u32 semantic;             // Byte offset to ASCII semantic string
    u32 direction;            // Parameter direction (in/out/inout)
    s32 paramno;              // Parameter index (-1 for globals)
    s32 isReferenced;         // Non-zero if referenced in code
    s32 isShared;             // Non-zero if shared parameter
};

struct CgBinaryVertexProgram
{
    u32 instructionCount;     // Number of 128-bit VP instruction slots
    u32 instructionSlot;      // Load address slot
    u32 registerCount;        // Number of temporary R registers required
    u32 attributeInputMask;   // Input vertex attributes bitmask
    u32 attributeOutputMask;  // Output vertex attributes bitmask
    u32 userClipMask;         // User clip plane enable bitmask
};

struct CgBinaryFragmentProgram
{
    u32 instructionCount;     // Number of 128-bit FP instruction slots
    u32 attributeInputMask;   // Input fragment attributes bitmask
    u32 partialTexType;       // Texture coordinate partial precision mask
    u16 texCoordsInputMask;   // Texture coordinates input mask
    u16 texCoords2D;          // 2D texture coordinates mask
    u16 texCoordsCentroid;    // Centroid texture coordinates mask
    u8  registerCount;        // Temporary registers count (R0-R31)
    u8  outputFromH0;         // 1 if output from H0 (half), 0 if from R0 (float)
    u8  depthReplace;         // 1 if fragment depth is written
    u8  pixelKill;            // 1 if program uses kill / discard operations
};

// ============================================================================
// 2. Open-Source Homebrew / PSL1GHT / PS3-Shader-Compiler Binary Headers
// ============================================================================

#pragma pack(push, 1)
struct Psl1ghtVertexProgramHeader
{
    u16 magic;          // 0x5650 'VP'
    u16 _pad0;          // 0
    u16 num_regs;       // Number of temporary registers used
    u16 num_attr;       // Number of input attributes
    u16 num_const;      // Number of constants
    u16 num_insn;       // Number of vertex instructions
    u32 attr_off;       // Offset to rsxProgramAttrib array
    u32 const_off;      // Offset to rsxProgramConst array
    u32 ucode_off;      // Offset to microcode
    u32 input_mask;     // Mask of input attributes
    u32 output_mask;    // Mask of output attributes
    u16 const_start;    // Constant block start index
    u16 insn_start;     // Instruction start index
};

struct Psl1ghtFragmentProgramHeader
{
    u16 magic;          // 0x4650 'FP'
    u16 _pad0;          // 0
    u16 num_regs;       // Number of temporary registers used
    u16 num_attr;       // Number of input attributes
    u16 num_const;      // Number of constants
    u16 num_insn;       // Number of fragment instructions
    u32 attr_off;       // Offset to rsxProgramAttrib array
    u32 const_off;      // Offset to rsxProgramConst array
    u32 ucode_off;      // Offset to microcode
    u32 fp_control;     // Fragment program control mask
    u16 texcoords;      // Bitmask of all used texture coords
    u16 texcoord2D;     // Bitmask of used 2D texture coords
    u16 texcoord3D;     // Bitmask of used 3D texture coords
    u16 _pad1;          // 0
};

struct Psl1ghtProgramConst
{
    u32 name_off;       // Offset to null-terminated name string
    u32 index;          // Constant index / register id / offset
    u8 type;            // Parameter type
    u8 is_internal;     // 1 = internal constant, 0 = named uniform
    u8 count;           // Element count (1..4)
    u8 _pad0;           // 0
    union {
        u32 u;
        f32 f;
    } values[4];
};

struct Psl1ghtProgramAttrib
{
    u32 name_off;       // Offset to null-terminated name string
    u32 index;          // Attribute index
    u8 type;            // Attribute type
    u8 _pad0[3];        // 0
};
#pragma pack(pop)

// Fragment Program Opcode Definitions
enum rsx_fp_opcode
{
    RSX_FP_OPCODE_NOP = 0,
    RSX_FP_OPCODE_MOV = 1,
    RSX_FP_OPCODE_MUL = 2,
    RSX_FP_OPCODE_ADD = 3,
    RSX_FP_OPCODE_MAD = 4,
    RSX_FP_OPCODE_DP3 = 5,
    RSX_FP_OPCODE_DP4 = 6,
    RSX_FP_OPCODE_DST = 7,
    RSX_FP_OPCODE_MIN = 8,
    RSX_FP_OPCODE_MAX = 9,
    RSX_FP_OPCODE_SLT = 10,
    RSX_FP_OPCODE_SGE = 11,
    RSX_FP_OPCODE_SLE = 12,
    RSX_FP_OPCODE_SGT = 13,
    RSX_FP_OPCODE_SNE = 14,
    RSX_FP_OPCODE_SEQ = 15,
    RSX_FP_OPCODE_FRC = 16,
    RSX_FP_OPCODE_FLR = 17,
    RSX_FP_OPCODE_KIL = 18,
    RSX_FP_OPCODE_PK4 = 19,
    RSX_FP_OPCODE_UP4 = 20,
    RSX_FP_OPCODE_DDX = 21,
    RSX_FP_OPCODE_DDY = 22,
    RSX_FP_OPCODE_TEX = 23,
    RSX_FP_OPCODE_TXP = 24,
    RSX_FP_OPCODE_TXD = 25,
    RSX_FP_OPCODE_RCP = 26,
    RSX_FP_OPCODE_RSQ = 27,
    RSX_FP_OPCODE_EX2 = 28,
    RSX_FP_OPCODE_LG2 = 29,
    RSX_FP_OPCODE_LIT = 30,
    RSX_FP_OPCODE_LRP = 31,
    RSX_FP_OPCODE_STR = 32,
    RSX_FP_OPCODE_SFL = 33,
    RSX_FP_OPCODE_COS = 34,
    RSX_FP_OPCODE_SIN = 35,
    RSX_FP_OPCODE_PK2 = 36,
    RSX_FP_OPCODE_UP2 = 37,
    RSX_FP_OPCODE_POW = 38,
    RSX_FP_OPCODE_PKB = 39,
    RSX_FP_OPCODE_UPB = 40,
    RSX_FP_OPCODE_PK16 = 41,
    RSX_FP_OPCODE_UP16 = 42,
    RSX_FP_OPCODE_BEM = 43,
    RSX_FP_OPCODE_PKG = 44,
    RSX_FP_OPCODE_UPG = 45,
    RSX_FP_OPCODE_DP2A = 46,
    RSX_FP_OPCODE_TXL = 47,
    RSX_FP_OPCODE_TXB = 49,
    RSX_FP_OPCODE_TEXBEM = 51,
    RSX_FP_OPCODE_TXPBEM = 52,
    RSX_FP_OPCODE_BEMLUM = 53,
    RSX_FP_OPCODE_REFL = 54,
    RSX_FP_OPCODE_TIMESWTEX = 55,
    RSX_FP_OPCODE_DP2 = 56,
    RSX_FP_OPCODE_NRM = 57,
    RSX_FP_OPCODE_DIV = 58,
    RSX_FP_OPCODE_DIVSQ = 59,
    RSX_FP_OPCODE_LIF = 60,
    RSX_FP_OPCODE_FENCT = 61,
    RSX_FP_OPCODE_FENCB = 62,
    RSX_FP_OPCODE_BRK = 64,
    RSX_FP_OPCODE_CAL = 65,
    RSX_FP_OPCODE_IFE = 66,
    RSX_FP_OPCODE_LOOP = 67,
    RSX_FP_OPCODE_REP = 68,
    RSX_FP_OPCODE_RET = 69
};

static const char* rsx_fp_op_names[] =
{
    "NOP", "MOV", "MUL", "ADD", "MAD", "DP3", "DP4",
    "DST", "MIN", "MAX", "SLT", "SGE", "SLE", "SGT",
    "SNE", "SEQ", "FRC", "FLR", "KIL", "PK4", "UP4",
    "DDX", "DDY", "TEX", "TXP", "TXD", "RCP", "RSQ",
    "EX2", "LG2", "LIT", "LRP", "STR", "SFL", "COS",
    "SIN", "PK2", "UP2", "POW", "PKB", "UPB", "PK16",
    "UP16", "BEM", "PKG", "UPG", "DP2A", "TXL", "NULL",
    "TXB", "NULL", "TEXBEM", "TXPBEM", "BEMLUM", "REFL", "TIMESWTEX",
    "DP2", "NRM", "DIV", "DIVSQ", "LIF", "FENCT", "FENCB",
    "NULL", "BRK", "CAL", "IFE", "LOOP", "REP", "RET"
};

// Fragment Program Hardware Bitfield Unions
union OPDEST
{
    u32 HEX;
    struct
    {
        u32 end              : 1; // Bit 0: Set to 1 on program termination
        u32 dest_reg         : 6; // Destination register number
        u32 fp16             : 1; // 1 = Half-precision H register, 0 = Full-precision R register
        u32 set_cond         : 1; // Update Condition Code registers
        u32 mask_x           : 1; // Write mask component X
        u32 mask_y           : 1; // Write mask component Y
        u32 mask_z           : 1; // Write mask component Z
        u32 mask_w           : 1; // Write mask component W
        u32 src_attr_reg_num : 4; // Source attribute register index (WPOS, COL0, TEX0-TEX9, etc.)
        u32 tex_num          : 4; // Texture sampler unit index
        u32 exp_tex          : 1; // Bias / scale expansion flag (_bx2)
        u32 prec             : 2; // Output precision mode
        u32 opcode           : 6; // Opcode low 6 bits
        u32 no_dest          : 1; // No destination register flag
        u32 saturate         : 1; // Output clamping [0.0, 1.0] (_sat)
    };
};

union SRC0
{
    u32 HEX;
    struct
    {
        u32 reg_type           : 2; // 0: Temp, 1: Input, 2: Const
        u32 tmp_reg_index      : 6; // Register index (0-63)
        u32 fp16               : 1; // 1 = Half-precision
        u32 swizzle_x          : 2; // X component swizzle
        u32 swizzle_y          : 2; // Y component swizzle
        u32 swizzle_z          : 2; // Z component swizzle
        u32 swizzle_w          : 2; // W component swizzle
        u32 neg                : 1; // Negate operand (-)
        u32 exec_if_lt         : 1; // Condition: execute if less than
        u32 exec_if_eq         : 1; // Condition: execute if equal
        u32 exec_if_gr         : 1; // Condition: execute if greater than
        u32 cond_swizzle_x     : 2; // Condition code X swizzle
        u32 cond_swizzle_y     : 2; // Condition code Y swizzle
        u32 cond_swizzle_z     : 2; // Condition code Z swizzle
        u32 cond_swizzle_w     : 2; // Condition code W swizzle
        u32 abs                : 1; // Absolute value (|x|)
        u32 cond_mod_reg_index : 1; // Condition modifier register
        u32 cond_reg_index     : 1; // Condition register index
    };
};

union SRC1
{
    u32 HEX;
    struct
    {
        u32 reg_type      : 2;
        u32 tmp_reg_index : 6;
        u32 fp16          : 1;
        u32 swizzle_x     : 2;
        u32 swizzle_y     : 2;
        u32 swizzle_z     : 2;
        u32 swizzle_w     : 2;
        u32 neg           : 1;
        u32 abs           : 1;
        u32 src0_prec_mod : 3;
        u32 src1_prec_mod : 3;
        u32 src2_prec_mod : 3;
        u32 scale         : 3;
        u32 opcode_hi     : 1; // Opcode high bit (bit 6)
    };
    struct
    {
        u32 else_offset   : 31;
        u32               : 1;
    };
    struct
    {
        u32               : 2;
        u32 rep_count     : 8;
        u32 init_counter  : 8;
        u32               : 1;
        u32 increment     : 8;
    };
};

union SRC2
{
    u32 HEX;
    u32 end_offset;
    struct
    {
        u32 reg_type         : 2;
        u32 tmp_reg_index    : 6;
        u32 fp16             : 1;
        u32 swizzle_x        : 2;
        u32 swizzle_y        : 2;
        u32 swizzle_z        : 2;
        u32 swizzle_w        : 2;
        u32 neg              : 1;
        u32 abs              : 1;
        u32 addr_reg         : 11;
        u32 use_index_reg    : 1;
        u32 perspective_corr : 1;
    };
};

// Vertex Program Opcode Definitions
enum sca_opcode
{
    RSX_SCA_OPCODE_NOP = 0x00,
    RSX_SCA_OPCODE_MOV = 0x01,
    RSX_SCA_OPCODE_RCP = 0x02,
    RSX_SCA_OPCODE_RCC = 0x03,
    RSX_SCA_OPCODE_RSQ = 0x04,
    RSX_SCA_OPCODE_EXP = 0x05,
    RSX_SCA_OPCODE_LOG = 0x06,
    RSX_SCA_OPCODE_LIT = 0x07,
    RSX_SCA_OPCODE_BRA = 0x08,
    RSX_SCA_OPCODE_BRI = 0x09,
    RSX_SCA_OPCODE_CAL = 0x0a,
    RSX_SCA_OPCODE_CLI = 0x0b,
    RSX_SCA_OPCODE_RET = 0x0c,
    RSX_SCA_OPCODE_LG2 = 0x0d,
    RSX_SCA_OPCODE_EX2 = 0x0e,
    RSX_SCA_OPCODE_SIN = 0x0f,
    RSX_SCA_OPCODE_COS = 0x10,
    RSX_SCA_OPCODE_BRB = 0x11,
    RSX_SCA_OPCODE_CLB = 0x12,
    RSX_SCA_OPCODE_PSH = 0x13,
    RSX_SCA_OPCODE_POP = 0x14,
};

enum vec_opcode
{
    RSX_VEC_OPCODE_NOP = 0x00,
    RSX_VEC_OPCODE_MOV = 0x01,
    RSX_VEC_OPCODE_MUL = 0x02,
    RSX_VEC_OPCODE_ADD = 0x03,
    RSX_VEC_OPCODE_MAD = 0x04,
    RSX_VEC_OPCODE_DP3 = 0x05,
    RSX_VEC_OPCODE_DPH = 0x06,
    RSX_VEC_OPCODE_DP4 = 0x07,
    RSX_VEC_OPCODE_DST = 0x08,
    RSX_VEC_OPCODE_MIN = 0x09,
    RSX_VEC_OPCODE_MAX = 0x0a,
    RSX_VEC_OPCODE_SLT = 0x0b,
    RSX_VEC_OPCODE_SGE = 0x0c,
    RSX_VEC_OPCODE_ARL = 0x0d,
    RSX_VEC_OPCODE_FRC = 0x0e,
    RSX_VEC_OPCODE_FLR = 0x0f,
    RSX_VEC_OPCODE_SEQ = 0x10,
    RSX_VEC_OPCODE_SFL = 0x11,
    RSX_VEC_OPCODE_SGT = 0x12,
    RSX_VEC_OPCODE_SLE = 0x13,
    RSX_VEC_OPCODE_SNE = 0x14,
    RSX_VEC_OPCODE_STR = 0x15,
    RSX_VEC_OPCODE_SSG = 0x16,
    RSX_VEC_OPCODE_TXL = 0x19,
};

static const char* rsx_vp_sca_op_names[] =
{
    "NOP", "MOV", "RCP", "RCC", "RSQ", "EXP", "LOG",
    "LIT", "BRA", "BRI", "CAL", "CLI", "RET", "LG2",
    "EX2", "SIN", "COS", "BRB", "CLB", "PSH", "POP"
};

static const char* rsx_vp_vec_op_names[] =
{
    "NOP", "MOV", "MUL", "ADD", "MAD", "DP3", "DPH", "DP4",
    "DST", "MIN", "MAX", "SLT", "SGE", "ARL", "FRC", "FLR",
    "SEQ", "SFL", "SGT", "SLE", "SNE", "STR", "SSG", "NULL", "NULL", "TXL"
};

// Vertex Program Hardware Bitfield Unions
union D0
{
    u32 HEX;
    struct
    {
        u32 addr_swz             : 2;
        u32 mask_w               : 2;
        u32 mask_z               : 2;
        u32 mask_y               : 2;
        u32 mask_x               : 2;
        u32 cond                 : 3;
        u32 cond_test_enable     : 1;
        u32 cond_update_enable_0 : 1;
        u32 dst_tmp              : 6;
        u32 src0_abs             : 1;
        u32 src1_abs             : 1;
        u32 src2_abs             : 1;
        u32 addr_reg_sel_1       : 1;
        u32 cond_reg_sel_1       : 1;
        u32 staturate            : 1;
        u32 index_input          : 1;
        u32                      : 1;
        u32 cond_update_enable_1 : 1;
        u32 vec_result           : 1;
        u32                      : 1;
    };
    struct
    {
        u32         : 23;
        u32 iaddrh2 : 1;
        u32         : 8;
    };
};

union D1
{
    u32 HEX;
    struct
    {
        u32 src0h      : 8;
        u32 input_src  : 4;
        u32 const_src  : 10;
        u32 vec_opcode : 5;
        u32 sca_opcode : 5;
    };
};

union D2
{
    u32 HEX;
    struct
    {
        u32 src2h  : 6;
        u32 src1   : 17;
        u32 src0l  : 9;
    };
    struct
    {
        u32 iaddrh : 6;
        u32        : 26;
    };
    struct
    {
        u32         : 8;
        u32 tex_num : 2;
        u32         : 22;
    };
};

union D3
{
    u32 HEX;
    struct
    {
        u32 end             : 1;
        u32 index_const     : 1;
        u32 dst             : 5;
        u32 sca_dst_tmp     : 6;
        u32 vec_writemask_w : 1;
        u32 vec_writemask_z : 1;
        u32 vec_writemask_y : 1;
        u32 vec_writemask_x : 1;
        u32 sca_writemask_w : 1;
        u32 sca_writemask_z : 1;
        u32 sca_writemask_y : 1;
        u32 sca_writemask_x : 1;
        u32 src2l           : 11;
    };
    struct
    {
        u32                 : 23;
        u32 branch_index    : 5;
        u32 brb_cond_true   : 1;
        u32 iaddrl          : 3;
    };
};

union VP_SRC
{
    union
    {
        u32 HEX;
        struct { u32 src0l : 9; u32 src0h : 8; };
        struct { u32 src1  : 17; };
        struct { u32 src2l : 11; u32 src2h : 6; };
    };
    struct
    {
        u32 reg_type : 2;
        u32 tmp_src  : 6;
        u32 swz_w    : 2;
        u32 swz_z    : 2;
        u32 swz_y    : 2;
        u32 swz_x    : 2;
        u32 neg      : 1;
    };
};

// ============================================================================
// 3. Dynamic String Buffer (str_buf_t) and Vector Structures
// ============================================================================

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} str_buf_t;

static void sb_init(str_buf_t *sb, size_t initial_cap) {
    if (initial_cap < 64) initial_cap = 64;
    sb->data = (char *)malloc(initial_cap);
    if (sb->data) {
        sb->data[0] = '\0';
        sb->len = 0;
        sb->cap = initial_cap;
    } else {
        sb->len = 0;
        sb->cap = 0;
    }
}

static void sb_ensure(str_buf_t *sb, size_t needed) {
    if (sb->len + needed + 1 > sb->cap) {
        size_t new_cap = sb->cap ? (sb->cap * 2) : 128;
        while (new_cap < sb->len + needed + 1) {
            new_cap *= 2;
        }
        char *new_data = (char *)realloc(sb->data, new_cap);
        if (new_data) {
            sb->data = new_data;
            sb->cap = new_cap;
        }
    }
}

static void sb_append_str(str_buf_t *sb, const char *str) {
    if (!str || !*str) return;
    size_t slen = strlen(str);
    sb_ensure(sb, slen);
    if (sb->data) {
        memcpy(sb->data + sb->len, str, slen);
        sb->len += slen;
        sb->data[sb->len] = '\0';
    }
}

static void sb_append_char(str_buf_t *sb, char c) {
    sb_ensure(sb, 1);
    if (sb->data) {
        sb->data[sb->len++] = c;
        sb->data[sb->len] = '\0';
    }
}

static void sb_append_fmt(str_buf_t *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed > 0) {
        sb_ensure(sb, (size_t)needed);
        if (sb->data) {
            vsnprintf(sb->data + sb->len, needed + 1, fmt, args);
            sb->len += (size_t)needed;
        }
    }
    va_end(args);
}

static void sb_pop_back(str_buf_t *sb) {
    if (sb && sb->len > 0) {
        sb->len--;
        sb->data[sb->len] = '\0';
    }
}

static void sb_free(str_buf_t *sb) {
    if (sb->data) {
        free(sb->data);
        sb->data = NULL;
    }
    sb->len = 0;
    sb->cap = 0;
}

typedef struct {
    u32 *data;
    size_t count;
    size_t cap;
} u32_vec_t;

static void u32_vec_init(u32_vec_t *vec) {
    vec->data = NULL;
    vec->count = 0;
    vec->cap = 0;
}

static void u32_vec_push(u32_vec_t *vec, u32 val) {
    if (vec->count >= vec->cap) {
        size_t new_cap = vec->cap ? (vec->cap * 2) : 16;
        u32 *new_data = (u32 *)realloc(vec->data, new_cap * sizeof(u32));
        if (new_data) {
            vec->data = new_data;
            vec->cap = new_cap;
        }
    }
    if (vec->data) {
        vec->data[vec->count++] = val;
    }
}

static bool u32_vec_remove_first(u32_vec_t *vec, u32 val) {
    for (size_t i = 0; i < vec->count; ++i) {
        if (vec->data[i] == val) {
            for (size_t j = i; j + 1 < vec->count; ++j) {
                vec->data[j] = vec->data[j + 1];
            }
            vec->count--;
            return true;
        }
    }
    return false;
}

static void u32_vec_free(u32_vec_t *vec) {
    if (vec->data) {
        free(vec->data);
        vec->data = NULL;
    }
    vec->count = 0;
    vec->cap = 0;
}

// Helper for PSL1GHT parameter types
static const char* GetPsl1ghtParamTypeName(u8 type, char *buf, size_t buf_sz)
{
    switch (type)
    {
    case 0: return "bool";
    case 1: return "bool1";
    case 2: return "bool2";
    case 3: return "bool3";
    case 4: return "bool4";
    case 5: return "float";
    case 6: return "float1";
    case 7: return "float2";
    case 8: return "float3";
    case 9: return "float4";
    case 10: return "float3x4";
    case 11: return "float4x4";
    case 12: return "float3x3";
    case 13: return "float4x3";
    case 14: return "sampler1D";
    case 15: return "sampler2D";
    case 16: return "sampler3D";
    case 17: return "samplerCUBE";
    case 18: return "samplerRECT";
    default:
        snprintf(buf, buf_sz, "unknown_type(%u)", (unsigned)type);
        return buf;
    }
}

// ============================================================================
// 4. Unified Disassembler Engine State & Forward Declarations
// ============================================================================

typedef struct {
    u8 *buffer;
    size_t buffer_size;
    str_buf_t arb_shader;
    char dst_reg_name[64];

    // Fragment Program state
    union OPDEST dst;
    union SRC0 src0;
    union SRC1 src1;
    union SRC2 src2;
    u32 offset;
    u32 opcode;
    u32 step;
    u32 size;
    u32_vec_t end_offsets;
    u32_vec_t else_offsets;
    u32_vec_t loop_end_offsets;

    // Vertex Program state
    union D0 d0;
    union D1 d1;
    union D2 d2;
    union D3 d3;
    union VP_SRC src[3];
    u32 sca_opcode;
    u32 vec_opcode;
    size_t instr_count;
    u32 *vp_data;
    size_t vp_data_count;
} disasm_engine_t;

static void disasm_engine_init(disasm_engine_t *e, u8 *buf, size_t buf_sz) {
    memset(e, 0, sizeof(*e));
    e->buffer = buf;
    e->buffer_size = buf_sz;
    sb_init(&e->arb_shader, 4096);
    u32_vec_init(&e->end_offsets);
    u32_vec_init(&e->else_offsets);
    u32_vec_init(&e->loop_end_offsets);
}

static void disasm_engine_free(disasm_engine_t *e) {
    sb_free(&e->arb_shader);
    u32_vec_free(&e->end_offsets);
    u32_vec_free(&e->else_offsets);
    u32_vec_free(&e->loop_end_offsets);
    if (e->vp_data) {
        free(e->vp_data);
        e->vp_data = NULL;
    }
    e->vp_data_count = 0;
}

static const char* GetCgParamType(u32 type, char *buf, size_t buf_sz)
{
    switch (type)
    {
    case 1045: return "float";
    case 1046:
    case 1047:
    case 1048:
        snprintf(buf, buf_sz, "float%u", type - 1044);
        return buf;
    case 1064: return "float4x4";
    case 1066: return "sampler2D";
    case 1069: return "samplerCUBE";
    case 1091: return "float1";
    default:
        snprintf(buf, buf_sz, "!UnkCgType(%u)", type);
        return buf;
    }
}

static const char* GetCgParamName(const disasm_engine_t *e, u32 offset)
{
    if (offset >= e->buffer_size) return "";
    return (const char *)&e->buffer[offset];
}

static const char* GetCgParamSemantic(const disasm_engine_t *e, u32 offset)
{
    if (offset >= e->buffer_size) return "";
    return (const char *)&e->buffer[offset];
}

static void GetCgParamValue(const disasm_engine_t *e, u32 offset, u32 end_offset, char *out, size_t out_sz)
{
    out[0] = '\0';
    if (offset == 0 || offset >= e->buffer_size || end_offset <= offset) return;

    str_buf_t offsets;
    sb_init(&offsets, 128);
    sb_append_str(&offsets, "offsets:");
    u32 num = 0;
    offset += 6;
    while (offset < end_offset && offset + 1 < e->buffer_size)
    {
        u32 val = ((u32)e->buffer[offset] << 8) | (u32)e->buffer[offset + 1];
        sb_append_fmt(&offsets, " %u,", val);
        offset += 4;
        num++;
    }
    if (num > 4 || num == 0)
    {
        sb_free(&offsets);
        return;
    }
    sb_pop_back(&offsets); // remove trailing ','
    snprintf(out, out_sz, "num %u %s", num, offsets.data);
    sb_free(&offsets);
}

static void SwapBe32Range(u8 *buf, size_t buf_sz, u32 start_offset, size_t size_bytes)
{
    if ((size_t)start_offset + size_bytes > buf_sz) return;
    u32 *start = (u32 *)(buf + start_offset);
    u32 *end   = (u32 *)(buf + start_offset + size_bytes);
    for (u32 *ptr = start; ptr < end; ++ptr)
    {
        *ptr = bswap32(*ptr);
    }
}

static void ConvertCgProgramToLE(disasm_engine_t *e, struct CgBinaryProgram *prog)
{
    const u32 be_profile = bswap32(prog->profile);

    // 1. Swap the main header
    SwapBe32Range(e->buffer, e->buffer_size, 0, sizeof(struct CgBinaryProgram));

    // 2. Swap parameter descriptors
    if (prog->parameterArray < e->buffer_size)
    {
        SwapBe32Range(e->buffer, e->buffer_size, prog->parameterArray,
                      sizeof(struct CgBinaryParameter) * prog->parameterCount);
    }

    // 3. Swap the raw microcode payload
    if (prog->ucode < e->buffer_size)
    {
        SwapBe32Range(e->buffer, e->buffer_size, prog->ucode, e->buffer_size - prog->ucode);
    }

    // 4. Swap domain specific header
    if (be_profile == CG_PROFILE_SCE_FP_RSX)
    {
        if (prog->program + sizeof(struct CgBinaryFragmentProgram) <= e->buffer_size)
        {
            struct CgBinaryFragmentProgram *fprog =
                (struct CgBinaryFragmentProgram *)(e->buffer + prog->program);
            fprog->instructionCount   = bswap32(fprog->instructionCount);
            fprog->attributeInputMask = bswap32(fprog->attributeInputMask);
            fprog->partialTexType     = bswap32(fprog->partialTexType);
            fprog->texCoordsInputMask = bswap16(fprog->texCoordsInputMask);
            fprog->texCoords2D        = bswap16(fprog->texCoords2D);
            fprog->texCoordsCentroid  = bswap16(fprog->texCoordsCentroid);
        }
    }
    else
    {
        if (prog->program + sizeof(struct CgBinaryVertexProgram) <= e->buffer_size)
        {
            SwapBe32Range(e->buffer, e->buffer_size, prog->program, sizeof(struct CgBinaryVertexProgram));
        }
    }
}

// ============================================================================
// 5. Fragment Program Decoding Implementation
// ============================================================================

static void FP_GetMask(const disasm_engine_t *e, char *out, size_t out_sz)
{
    char buf[8];
    int idx = 0;
    buf[idx++] = '.';
    if (e->dst.mask_x) buf[idx++] = 'x';
    if (e->dst.mask_y) buf[idx++] = 'y';
    if (e->dst.mask_z) buf[idx++] = 'z';
    if (e->dst.mask_w) buf[idx++] = 'w';
    buf[idx] = '\0';

    if (strcmp(buf, ".") == 0 || strcmp(buf, ".xyzw") == 0) {
        out[0] = '\0';
    } else {
        snprintf(out, out_sz, "%s", buf);
    }
}

static void FP_AddConstDisAsm(disasm_engine_t *e, char *out, size_t out_sz)
{
    if (e->offset + e->size + 4 * sizeof(u32) + 4 * sizeof(u32) > e->buffer_size)
    {
        snprintf(out, out_sz, "{0, 0, 0, 0}");
        return;
    }

    const u32 *data = (const u32 *)(e->buffer + e->offset + e->size + 4 * sizeof(u32));
    e->step = 2 * 4 * sizeof(u32);

    const u32 x = GetData(data[0]);
    const u32 y = GetData(data[1]);
    const u32 z = GetData(data[2]);
    const u32 w = GetData(data[3]);

    snprintf(out, out_sz, "{0x%08x(%g), 0x%08x(%g), 0x%08x(%g), 0x%08x(%g)}",
        x, (double)u32_to_f32(x),
        y, (double)u32_to_f32(y),
        z, (double)u32_to_f32(z),
        w, (double)u32_to_f32(w));
}

static void FP_GetCondDisAsm(const disasm_engine_t *e, char *out, size_t out_sz)
{
    static const char f[] = "xyzw";
    char swizzle[8];
    snprintf(swizzle, sizeof(swizzle), ".%c%c%c%c",
        f[e->src0.cond_swizzle_x],
        f[e->src0.cond_swizzle_y],
        f[e->src0.cond_swizzle_z],
        f[e->src0.cond_swizzle_w]);

    if (strcmp(swizzle, ".xxxx") == 0) strcpy(swizzle, ".x");
    else if (strcmp(swizzle, ".yyyy") == 0) strcpy(swizzle, ".y");
    else if (strcmp(swizzle, ".zzzz") == 0) strcpy(swizzle, ".z");
    else if (strcmp(swizzle, ".wwww") == 0) strcpy(swizzle, ".w");
    else if (strcmp(swizzle, ".xyzw") == 0) swizzle[0] = '\0';

    const char *cond = "TR";
    if (e->src0.exec_if_gr && e->src0.exec_if_eq)      cond = "GE";
    else if (e->src0.exec_if_lt && e->src0.exec_if_eq) cond = "LE";
    else if (e->src0.exec_if_gr && e->src0.exec_if_lt) cond = "NE";
    else if (e->src0.exec_if_gr)                       cond = "GT";
    else if (e->src0.exec_if_lt)                       cond = "LT";
    else if (e->src0.exec_if_eq)                       cond = "FL";
    else                                               cond = "TR";

    snprintf(out, out_sz, "%s%s", cond, swizzle);
}

static void FP_GetSrcDisAsm(disasm_engine_t *e, u32 src_num, char *out, size_t out_sz)
{
    u32 reg_type = 0;
    u32 tmp_reg_index = 0;
    u32 fp16 = 0;
    u32 swizzle_x = 0, swizzle_y = 1, swizzle_z = 2, swizzle_w = 3;
    u32 neg = 0, abs_val = 0;

    if (src_num == 0)
    {
        reg_type = e->src0.reg_type;
        tmp_reg_index = e->src0.tmp_reg_index;
        fp16 = e->src0.fp16;
        swizzle_x = e->src0.swizzle_x;
        swizzle_y = e->src0.swizzle_y;
        swizzle_z = e->src0.swizzle_z;
        swizzle_w = e->src0.swizzle_w;
        neg = e->src0.neg;
        abs_val = e->src0.abs;
    }
    else if (src_num == 1)
    {
        reg_type = e->src1.reg_type;
        tmp_reg_index = e->src1.tmp_reg_index;
        fp16 = e->src1.fp16;
        swizzle_x = e->src1.swizzle_x;
        swizzle_y = e->src1.swizzle_y;
        swizzle_z = e->src1.swizzle_z;
        swizzle_w = e->src1.swizzle_w;
        neg = e->src1.neg;
        abs_val = e->src1.abs;
    }
    else
    {
        reg_type = e->src2.reg_type;
        tmp_reg_index = e->src2.tmp_reg_index;
        fp16 = e->src2.fp16;
        swizzle_x = e->src2.swizzle_x;
        swizzle_y = e->src2.swizzle_y;
        swizzle_z = e->src2.swizzle_z;
        swizzle_w = e->src2.swizzle_w;
        neg = e->src2.neg;
        abs_val = e->src2.abs;
    }

    char base[256];
    base[0] = '\0';

    switch (reg_type)
    {
    case 0: // Temporary register
        snprintf(base, sizeof(base), "%c%u", fp16 ? 'H' : 'R', tmp_reg_index);
        break;
    case 1: // Input attribute register
    {
        static const char* const reg_table[] =
        {
            "WPOS", "COL0", "COL1", "FOGC", "TEX0",
            "TEX1", "TEX2", "TEX3", "TEX4", "TEX5",
            "TEX6", "TEX7", "TEX8", "TEX9", "SSA"
        };
        const size_t num_regs = sizeof(reg_table) / sizeof(reg_table[0]);

        if (e->dst.src_attr_reg_num == 0)
        {
            snprintf(base, sizeof(base), "%s", reg_table[0]);
        }
        else if (e->dst.src_attr_reg_num < num_regs)
        {
            const char* perspective_correction = e->src2.perspective_corr ? "g" : "f";
            snprintf(base, sizeof(base), "%s[%s]", perspective_correction, reg_table[e->dst.src_attr_reg_num]);
        }
        else
        {
            snprintf(base, sizeof(base), "v[%u]", e->dst.src_attr_reg_num);
        }
        break;
    }
    case 2: // Constant vector
        FP_AddConstDisAsm(e, base, sizeof(base));
        break;
    default:
        snprintf(base, sizeof(base), "UNK_REG");
        break;
    }

    static const char f[] = "xyzw";
    char swizzle[8];
    snprintf(swizzle, sizeof(swizzle), ".%c%c%c%c",
        f[swizzle_x], f[swizzle_y], f[swizzle_z], f[swizzle_w]);

    if (strcmp(swizzle, ".xxxx") == 0) strcpy(swizzle, ".x");
    else if (strcmp(swizzle, ".yyyy") == 0) strcpy(swizzle, ".y");
    else if (strcmp(swizzle, ".zzzz") == 0) strcpy(swizzle, ".z");
    else if (strcmp(swizzle, ".wwww") == 0) strcpy(swizzle, ".w");

    char full[300];
    if (strcmp(swizzle, ".xyzw") != 0) {
        snprintf(full, sizeof(full), "%s%s", base, swizzle);
    } else {
        snprintf(full, sizeof(full), "%s", base);
    }

    char res[320];
    if (abs_val && neg) {
        snprintf(res, sizeof(res), "-|%s|", full);
    } else if (abs_val) {
        snprintf(res, sizeof(res), "|%s|", full);
    } else if (neg) {
        snprintf(res, sizeof(res), "-%s", full);
    } else {
        snprintf(res, sizeof(res), "%s", full);
    }

    snprintf(out, out_sz, "%s", res);
}

static void FP_FormatDisAsm(disasm_engine_t *e, const char *code, char *out, size_t out_sz)
{
    str_buf_t sb;
    sb_init(&sb, 256);

    const char *p = code;
    while (*p)
    {
        if (*p == '$')
        {
            p++;
            if (*p == '$')
            {
                sb_append_char(&sb, '$');
                p++;
            }
            else if (*p == '0')
            {
                char tmp[512];
                FP_GetSrcDisAsm(e, 0, tmp, sizeof(tmp));
                sb_append_str(&sb, tmp);
                p++;
            }
            else if (*p == '1')
            {
                char tmp[512];
                FP_GetSrcDisAsm(e, 1, tmp, sizeof(tmp));
                sb_append_str(&sb, tmp);
                p++;
            }
            else if (*p == '2')
            {
                char tmp[512];
                FP_GetSrcDisAsm(e, 2, tmp, sizeof(tmp));
                sb_append_str(&sb, tmp);
                p++;
            }
            else if (*p == 't')
            {
                sb_append_fmt(&sb, "TEX%u", e->dst.tex_num);
                p++;
            }
            else if (*p == 'm')
            {
                char mask[16];
                FP_GetMask(e, mask, sizeof(mask));
                sb_append_str(&sb, mask);
                p++;
            }
            else if (strncmp(p, "cond", 4) == 0)
            {
                char cond[32];
                FP_GetCondDisAsm(e, cond, sizeof(cond));
                sb_append_str(&sb, cond);
                p += 4;
            }
            else if (*p == 'c')
            {
                char cbuf[256];
                FP_AddConstDisAsm(e, cbuf, sizeof(cbuf));
                sb_append_str(&sb, cbuf);
                p++;
            }
            else
            {
                sb_append_char(&sb, '$');
            }
        }
        else
        {
            sb_append_char(&sb, *p);
            p++;
        }
    }

    snprintf(out, out_sz, "%s", sb.data ? sb.data : "");
    sb_free(&sb);
}

static void FP_AddCodeAsm(disasm_engine_t *e, const char *code)
{
    const size_t num_op_names = sizeof(rsx_fp_op_names) / sizeof(rsx_fp_op_names[0]);
    if (e->opcode >= num_op_names) return;

    char mask[16];
    FP_GetMask(e, mask, sizeof(mask));

    char op_name[64];
    if (e->dst.dest_reg == 63)
    {
        snprintf(e->dst_reg_name, sizeof(e->dst_reg_name), "RC%s, ", mask);
        snprintf(op_name, sizeof(op_name), "%sXC", rsx_fp_op_names[e->opcode]);
    }
    else
    {
        snprintf(e->dst_reg_name, sizeof(e->dst_reg_name), "%c%u%s, ",
            e->dst.fp16 ? 'H' : 'R', e->dst.dest_reg, mask);
        snprintf(op_name, sizeof(op_name), "%s%c",
            rsx_fp_op_names[e->opcode], e->dst.fp16 ? 'H' : 'R');
    }

    switch (e->opcode)
    {
    case RSX_FP_OPCODE_BRK:
    case RSX_FP_OPCODE_CAL:
    case RSX_FP_OPCODE_FENCT:
    case RSX_FP_OPCODE_FENCB:
    case RSX_FP_OPCODE_IFE:
    case RSX_FP_OPCODE_KIL:
    case RSX_FP_OPCODE_LOOP:
    case RSX_FP_OPCODE_NOP:
    case RSX_FP_OPCODE_REP:
    case RSX_FP_OPCODE_RET:
        e->dst_reg_name[0] = '\0';
        snprintf(op_name, sizeof(op_name), "%s%c",
            rsx_fp_op_names[e->opcode], e->dst.fp16 ? 'H' : 'R');
        break;
    default:
        break;
    }

    char formatted[512];
    FP_FormatDisAsm(e, code, formatted, sizeof(formatted));

    sb_append_fmt(&e->arb_shader, "%s %s%s;\n", op_name, e->dst_reg_name, formatted);
}

static bool FP_SCT(disasm_engine_t *e)
{
    switch (e->opcode)
    {
    case RSX_FP_OPCODE_ADD: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DIV: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DIVSQ: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DP2: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DP3: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DP4: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DP2A: FP_AddCodeAsm(e, "$0, $1, $2"); break;
    case RSX_FP_OPCODE_MAD: FP_AddCodeAsm(e, "$0, $1, $2"); break;
    case RSX_FP_OPCODE_MAX: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_MIN: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_MOV: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_MUL: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_RCP: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_RSQ: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_SEQ: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SFL: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SGE: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SGT: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SLE: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SLT: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SNE: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_STR: FP_AddCodeAsm(e, "$0, $1"); break;
    default: return false;
    }
    return true;
}

static bool FP_SCB(disasm_engine_t *e)
{
    switch (e->opcode)
    {
    case RSX_FP_OPCODE_ADD: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_COS: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_DP2: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DP3: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DP4: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_DP2A: FP_AddCodeAsm(e, "$0, $1, $2"); break;
    case RSX_FP_OPCODE_DST: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_REFL: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_EX2: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_FLR: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_FRC: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_LIT: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_LIF: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_LRP: FP_AddCodeAsm(e, "# WARNING"); break;
    case RSX_FP_OPCODE_LG2: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_MAD: FP_AddCodeAsm(e, "$0, $1, $2"); break;
    case RSX_FP_OPCODE_MAX: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_MIN: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_MOV: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_MUL: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_PK2: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_PK4: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_PK16: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_PKB: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_PKG: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_SEQ: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SFL: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_SGE: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SGT: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SIN: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_SLE: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SLT: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_SNE: FP_AddCodeAsm(e, "$0, $1"); break;
    case RSX_FP_OPCODE_STR: FP_AddCodeAsm(e, "$0"); break;
    default: return false;
    }
    return true;
}

static bool FP_TEX_SRB(disasm_engine_t *e)
{
    switch (e->opcode)
    {
    case RSX_FP_OPCODE_DDX: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_DDY: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_NRM: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_BEM: FP_AddCodeAsm(e, "# WARNING"); break;
    case RSX_FP_OPCODE_TEX: FP_AddCodeAsm(e, "$0, $t"); break;
    case RSX_FP_OPCODE_TEXBEM: FP_AddCodeAsm(e, "# WARNING"); break;
    case RSX_FP_OPCODE_TXP: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_TXPBEM: FP_AddCodeAsm(e, "# WARNING"); break;
    case RSX_FP_OPCODE_TXD: FP_AddCodeAsm(e, "$0, $1, $t"); break;
    case RSX_FP_OPCODE_TXB: FP_AddCodeAsm(e, "$0, $t"); break;
    case RSX_FP_OPCODE_TXL: FP_AddCodeAsm(e, "$0, $t"); break;
    case RSX_FP_OPCODE_UP2: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_UP4: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_UP16: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_UPB: FP_AddCodeAsm(e, "$0"); break;
    case RSX_FP_OPCODE_UPG: FP_AddCodeAsm(e, "$0"); break;
    default: return false;
    }
    return true;
}

static bool FP_SIP(disasm_engine_t *e)
{
    switch (e->opcode)
    {
    case RSX_FP_OPCODE_BRK: FP_AddCodeAsm(e, "$cond"); break;
    case RSX_FP_OPCODE_CAL: FP_AddCodeAsm(e, "$cond"); break;
    case RSX_FP_OPCODE_FENCT: FP_AddCodeAsm(e, ""); break;
    case RSX_FP_OPCODE_FENCB: FP_AddCodeAsm(e, ""); break;
    case RSX_FP_OPCODE_IFE:
        u32_vec_push(&e->else_offsets, e->src1.else_offset << 2);
        u32_vec_push(&e->end_offsets, e->src2.end_offset << 2);
        FP_AddCodeAsm(e, "($cond)");
        break;
    case RSX_FP_OPCODE_LOOP:
        if (e->src0.exec_if_eq || e->src0.exec_if_gr || e->src0.exec_if_lt)
        {
            u32_vec_push(&e->loop_end_offsets, e->src2.end_offset << 2);
        }
        {
            char loop_buf[128];
            snprintf(loop_buf, sizeof(loop_buf), "{ %u, %u, %u }",
                e->src1.rep_count, e->src1.init_counter, e->src1.increment);
            FP_AddCodeAsm(e, loop_buf);
        }
        break;
    case RSX_FP_OPCODE_REP:
        if (!e->src0.exec_if_eq && !e->src0.exec_if_gr && !e->src0.exec_if_lt)
        {
            sb_append_str(&e->arb_shader, "# RSX_FP_OPCODE_REP_1\n");
        }
        else
        {
            u32_vec_push(&e->end_offsets, e->src2.end_offset << 2);
            sb_append_str(&e->arb_shader, "# RSX_FP_OPCODE_REP_2\n");
        }
        break;
    case RSX_FP_OPCODE_RET: FP_AddCodeAsm(e, "$cond"); break;
    default: return false;
    }
    return true;
}

static void TaskFP(disasm_engine_t *e)
{
    e->size = 0;
    if (e->offset >= e->buffer_size) return;

    const u32 *data = (const u32 *)(e->buffer + e->offset);
    const size_t total_words = (e->buffer_size - e->offset) / sizeof(u32);
    size_t current_word_idx = 0;

    while (current_word_idx + 4 <= total_words)
    {
        while (u32_vec_remove_first(&e->end_offsets, e->size))
        {
            sb_append_str(&e->arb_shader, "ENDIF;\n");
        }
        while (u32_vec_remove_first(&e->loop_end_offsets, e->size))
        {
            sb_append_str(&e->arb_shader, "ENDLOOP;\n");
        }
        while (u32_vec_remove_first(&e->else_offsets, e->size))
        {
            sb_append_str(&e->arb_shader, "ELSE;\n");
        }

        e->dst.HEX  = GetData(data[current_word_idx + 0]);
        e->src0.HEX = GetData(data[current_word_idx + 1]);
        e->src1.HEX = GetData(data[current_word_idx + 2]);
        e->src2.HEX = GetData(data[current_word_idx + 3]);

        e->step = 4 * sizeof(u32);
        e->opcode = e->dst.opcode | (e->src1.opcode_hi << 6);

        switch (e->opcode)
        {
        case RSX_FP_OPCODE_NOP: FP_AddCodeAsm(e, ""); break;
        case RSX_FP_OPCODE_KIL: FP_AddCodeAsm(e, "$cond"); break;
        default:
            if (!FP_SIP(e) && !FP_SCT(e) && !FP_TEX_SRB(e) && !FP_SCB(e))
            {
                sb_append_fmt(&e->arb_shader, "# Unknown FP opcode: 0x%x\n", e->opcode);
            }
            break;
        }

        e->size += e->step;

        if (e->dst.end)
        {
            if (e->arb_shader.len > 0 && e->arb_shader.data[e->arb_shader.len - 1] == '\n')
            {
                sb_pop_back(&e->arb_shader);
            }
            sb_append_str(&e->arb_shader, " # last instruction\nEND\n");
            break;
        }

        current_word_idx += (e->step / sizeof(u32));
    }
}

// ============================================================================
// 6. Vertex Program Decoding Implementation
// ============================================================================

static void VP_GetMaskDisasm(const disasm_engine_t *e, bool is_sca, char *out, size_t out_sz)
{
    char buf[8];
    int idx = 0;
    buf[idx++] = '.';

    if (is_sca)
    {
        if (e->d3.sca_writemask_x) buf[idx++] = 'x';
        if (e->d3.sca_writemask_y) buf[idx++] = 'y';
        if (e->d3.sca_writemask_z) buf[idx++] = 'z';
        if (e->d3.sca_writemask_w) buf[idx++] = 'w';
    }
    else
    {
        if (e->d3.vec_writemask_x) buf[idx++] = 'x';
        if (e->d3.vec_writemask_y) buf[idx++] = 'y';
        if (e->d3.vec_writemask_z) buf[idx++] = 'z';
        if (e->d3.vec_writemask_w) buf[idx++] = 'w';
    }
    buf[idx] = '\0';

    if (strcmp(buf, ".") == 0 || strcmp(buf, ".xyzw") == 0) {
        out[0] = '\0';
    } else {
        snprintf(out, out_sz, "%s", buf);
    }
}

static void VP_GetDSTDisasm(const disasm_engine_t *e, bool is_sca, char *out, size_t out_sz)
{
    char mask[16];
    VP_GetMaskDisasm(e, is_sca, mask, sizeof(mask));

    static const char* const output_names[] =
    {
        "out_diffuse_color",
        "out_specular_color",
        "out_back_diffuse_color",
        "out_back_specular_color",
        "out_fog",
        "out_point_size",
        "out_clip_distance[0]",
        "out_clip_distance[1]",
        "out_clip_distance[2]",
        "out_clip_distance[3]",
        "out_clip_distance[4]",
        "out_clip_distance[5]",
        "out_tc8",
        "out_tc9",
        "out_tc0",
        "out_tc1",
        "out_tc2",
        "out_tc3",
        "out_tc4",
        "out_tc5",
        "out_tc6",
        "out_tc7"
    };
    const size_t num_outputs = sizeof(output_names) / sizeof(output_names[0]);

    const u32 target_dst = (is_sca && e->d3.sca_dst_tmp != 0x3f) ? 0x1f : e->d3.dst;
    if (target_dst == 0x1f)
    {
        if (is_sca) {
            snprintf(out, out_sz, "R%u%s", e->d3.sca_dst_tmp, mask);
        } else {
            snprintf(out, out_sz, "R%u%s", e->d0.dst_tmp, mask);
        }
    }
    else
    {
        char tmp[256];
        if (e->d3.dst < num_outputs)
        {
            snprintf(tmp, sizeof(tmp), "%s%s", output_names[e->d3.dst], mask);
        }
        else
        {
            snprintf(tmp, sizeof(tmp), "(bad out index) o[%u]", e->d3.dst);
        }

        if (e->d0.dst_tmp != 0x3f)
        {
            snprintf(out, out_sz, "%s R%u%s", tmp, e->d0.dst_tmp, mask);
        }
        else
        {
            snprintf(out, out_sz, "%s", tmp);
        }
    }
}

static void VP_AddAddrMaskDisasm(const disasm_engine_t *e, char *out, size_t out_sz)
{
    static const char f[] = "xyzw";
    snprintf(out, out_sz, ".%c", f[e->d0.addr_swz]);
}

static void VP_AddAddrRegDisasm(const disasm_engine_t *e, char *out, size_t out_sz)
{
    char mask[8];
    VP_AddAddrMaskDisasm(e, mask, sizeof(mask));
    snprintf(out, out_sz, "A%u%s", e->d0.addr_reg_sel_1, mask);
}

static inline u32 VP_GetAddrDisasm(const disasm_engine_t *e)
{
    return (e->d2.iaddrh << 3) | e->d3.iaddrl;
}

static void VP_GetSRCDisasm(const disasm_engine_t *e, u32 n, char *out, size_t out_sz)
{
    static const char* const reg_table[] =
    {
        "in_pos", "in_weight", "in_normal",
        "in_diff_color", "in_spec_color",
        "in_fog", "in_point_size", "in_7",
        "in_tc0", "in_tc1", "in_tc2", "in_tc3",
        "in_tc4", "in_tc5", "in_tc6", "in_tc7"
    };
    const size_t num_regs = sizeof(reg_table) / sizeof(reg_table[0]);

    const union VP_SRC *s = &e->src[n];
    char base[256];
    base[0] = '\0';

    switch (s->reg_type)
    {
    case 1: // Temporary register
        snprintf(base, sizeof(base), "R%u", s->tmp_src);
        break;
    case 2: // Input attribute
        if (e->d1.input_src < num_regs)
        {
            snprintf(base, sizeof(base), "%s", reg_table[e->d1.input_src]);
        }
        else
        {
            snprintf(base, sizeof(base), "v[%u]", e->d1.input_src);
        }
        break;
    case 3: // Constant register
        if (e->d3.index_const)
        {
            char addr_reg[32];
            VP_AddAddrRegDisasm(e, addr_reg, sizeof(addr_reg));
            snprintf(base, sizeof(base), "c[%s + %u]", addr_reg, e->d1.const_src);
        }
        else
        {
            snprintf(base, sizeof(base), "c[%u]", e->d1.const_src);
        }
        break;
    default:
        snprintf(base, sizeof(base), "UNK_SRC");
        break;
    }

    static const char f[] = "xyzw";
    char swizzle[8];
    snprintf(swizzle, sizeof(swizzle), ".%c%c%c%c",
        f[s->swz_x], f[s->swz_y], f[s->swz_z], f[s->swz_w]);

    if (strcmp(swizzle, ".xxxx") == 0) strcpy(swizzle, ".x");
    else if (strcmp(swizzle, ".yyyy") == 0) strcpy(swizzle, ".y");
    else if (strcmp(swizzle, ".zzzz") == 0) strcpy(swizzle, ".z");
    else if (strcmp(swizzle, ".wwww") == 0) strcpy(swizzle, ".w");

    char full[300];
    if (strcmp(swizzle, ".xyzw") != 0) {
        snprintf(full, sizeof(full), "%s%s", base, swizzle);
    } else {
        snprintf(full, sizeof(full), "%s", base);
    }

    bool abs_flag = false;
    switch (n)
    {
    default:
    case 0: abs_flag = e->d0.src0_abs; break;
    case 1: abs_flag = e->d0.src1_abs; break;
    case 2: abs_flag = e->d0.src2_abs; break;
    }

    char res[320];
    if (abs_flag && s->neg) {
        snprintf(res, sizeof(res), "-|%s|", full);
    } else if (abs_flag) {
        snprintf(res, sizeof(res), "|%s|", full);
    } else if (s->neg) {
        snprintf(res, sizeof(res), "-%s", full);
    } else {
        snprintf(res, sizeof(res), "%s", full);
    }

    snprintf(out, out_sz, "%s", res);
}

static void VP_GetCondDisasm(const disasm_engine_t *e, char *out, size_t out_sz)
{
    enum { lt = 0x1, eq = 0x2, gt = 0x4 };
    if (e->d0.cond == 0)
    {
        snprintf(out, out_sz, "false");
        return;
    }
    if (e->d0.cond == (lt | gt | eq))
    {
        snprintf(out, out_sz, "true");
        return;
    }

    static const char* const cond_string_table[(lt | gt | eq) + 1] =
    {
        "ERROR", "LT", "EQ", "LE", "GT", "NE", "GE", "ERROR"
    };

    static const char f[] = "xyzw";
    char swizzle[8];
    snprintf(swizzle, sizeof(swizzle), ".%c%c%c%c",
        f[e->d0.mask_x],
        f[e->d0.mask_y],
        f[e->d0.mask_z],
        f[e->d0.mask_w]);

    if (strcmp(swizzle, ".xxxx") == 0) strcpy(swizzle, ".x");
    else if (strcmp(swizzle, ".yyyy") == 0) strcpy(swizzle, ".y");
    else if (strcmp(swizzle, ".zzzz") == 0) strcpy(swizzle, ".z");
    else if (strcmp(swizzle, ".wwww") == 0) strcpy(swizzle, ".w");
    else if (strcmp(swizzle, ".xyzw") == 0) swizzle[0] = '\0';

    snprintf(out, out_sz, "(%s%s)", cond_string_table[e->d0.cond], swizzle);
}

static void VP_FormatDisasm(const disasm_engine_t *e, const char *code, char *out, size_t out_sz)
{
    str_buf_t sb;
    sb_init(&sb, 256);

    const char *p = code;
    while (*p)
    {
        if (*p == '$')
        {
            p++;
            if (*p == '$')
            {
                sb_append_char(&sb, '$');
                p++;
            }
            else if (*p == '0')
            {
                char tmp[512];
                VP_GetSRCDisasm(e, 0, tmp, sizeof(tmp));
                sb_append_str(&sb, tmp);
                p++;
            }
            else if (*p == '1')
            {
                char tmp[512];
                VP_GetSRCDisasm(e, 1, tmp, sizeof(tmp));
                sb_append_str(&sb, tmp);
                p++;
            }
            else if (*p == '2' || *p == 's')
            {
                char tmp[512];
                VP_GetSRCDisasm(e, 2, tmp, sizeof(tmp));
                sb_append_str(&sb, tmp);
                p++;
            }
            else if (strncmp(p, "am", 2) == 0)
            {
                char tmp[32];
                VP_AddAddrMaskDisasm(e, tmp, sizeof(tmp));
                sb_append_str(&sb, tmp);
                p += 2;
            }
            else if (*p == 'a')
            {
                char tmp[32];
                VP_AddAddrRegDisasm(e, tmp, sizeof(tmp));
                sb_append_str(&sb, tmp);
                p++;
            }
            else if (*p == 't')
            {
                sb_append_str(&sb, "TEX0");
                p++;
            }
            else if (strncmp(p, "fa", 2) == 0)
            {
                sb_append_fmt(&sb, "%u", VP_GetAddrDisasm(e));
                p += 2;
            }
            else if (strncmp(p, "ifcond ", 7) == 0)
            {
                char cond[32];
                VP_GetCondDisasm(e, cond, sizeof(cond));
                if (strcmp(cond, "true") != 0)
                {
                    sb_append_fmt(&sb, "%s ", cond);
                }
                p += 7;
            }
            else if (strncmp(p, "cond", 4) == 0)
            {
                char cond[32];
                VP_GetCondDisasm(e, cond, sizeof(cond));
                sb_append_str(&sb, cond);
                p += 4;
            }
            else
            {
                sb_append_char(&sb, '$');
            }
        }
        else
        {
            sb_append_char(&sb, *p);
            p++;
        }
    }

    snprintf(out, out_sz, "%s", sb.data ? sb.data : "");
    sb_free(&sb);
}

static void VP_AddCodeDisasm(disasm_engine_t *e, const char *code)
{
    char formatted[1024];
    VP_FormatDisasm(e, code, formatted, sizeof(formatted));
    sb_append_fmt(&e->arb_shader, "%s\n", formatted);
}

static void VP_AddCodeCondDisasm(disasm_engine_t *e, const char *dst_str, const char *src_str)
{
    enum { lt = 0x1, eq = 0x2, gt = 0x4 };
    if (!e->d0.cond_test_enable || e->d0.cond == (lt | gt | eq))
    {
        char line[1024];
        snprintf(line, sizeof(line), "%s, %s;", dst_str, src_str);
        VP_AddCodeDisasm(e, line);
        return;
    }
    if (e->d0.cond == 0)
    {
        char line[1024];
        snprintf(line, sizeof(line), "# %s, %s;", dst_str, src_str);
        VP_AddCodeDisasm(e, line);
        return;
    }

    static const char* const cond_string_table[(lt | gt | eq) + 1] =
    {
        "ERROR", "LT", "EQ", "LE", "GT", "NE", "GE", "ERROR"
    };

    static const char f[] = "xyzw";
    char swizzle[8];
    snprintf(swizzle, sizeof(swizzle), ".%c%c%c%c",
        f[e->d0.mask_x],
        f[e->d0.mask_y],
        f[e->d0.mask_z],
        f[e->d0.mask_w]);

    if (strcmp(swizzle, ".xxxx") == 0) strcpy(swizzle, ".x");
    else if (strcmp(swizzle, ".yyyy") == 0) strcpy(swizzle, ".y");
    else if (strcmp(swizzle, ".zzzz") == 0) strcpy(swizzle, ".z");
    else if (strcmp(swizzle, ".wwww") == 0) strcpy(swizzle, ".w");
    else if (strcmp(swizzle, ".xyzw") == 0) swizzle[0] = '\0';

    char line[1024];
    snprintf(line, sizeof(line), "%s(%s%s) , %s;", dst_str, cond_string_table[e->d0.cond], swizzle, src_str);
    VP_AddCodeDisasm(e, line);
}

static void VP_SetDSTDisasm(disasm_engine_t *e, bool is_sca, const char *value)
{
    if (is_sca)
    {
        const size_t num_sca = sizeof(rsx_vp_sca_op_names) / sizeof(rsx_vp_sca_op_names[0]);
        if (e->sca_opcode < num_sca)
        {
            sb_append_fmt(&e->arb_shader, "%s ", rsx_vp_sca_op_names[e->sca_opcode]);
        }
    }
    else
    {
        const size_t num_vec = sizeof(rsx_vp_vec_op_names) / sizeof(rsx_vp_vec_op_names[0]);
        if (e->vec_opcode < num_vec)
        {
            sb_append_fmt(&e->arb_shader, "%s ", rsx_vp_vec_op_names[e->vec_opcode]);
        }
    }

    if (e->d0.cond == 0) return;

    if (e->d0.staturate)
    {
        if (e->arb_shader.len > 0 && e->arb_shader.data[e->arb_shader.len - 1] == ' ')
        {
            sb_pop_back(&e->arb_shader);
        }
        sb_append_str(&e->arb_shader, "_sat ");
    }

    char dest[512];
    dest[0] = '\0';
    char mask[16];
    VP_GetMaskDisasm(e, is_sca, mask, sizeof(mask));

    if (e->d0.cond_update_enable_0 && e->d0.cond_update_enable_1)
    {
        if (e->arb_shader.len > 0 && e->arb_shader.data[e->arb_shader.len - 1] == ' ')
        {
            sb_pop_back(&e->arb_shader);
        }
        sb_append_str(&e->arb_shader, "C ");
        snprintf(dest, sizeof(dest), "RC%s", mask);
    }
    else if (e->d3.dst != 0x1f || (is_sca ? e->d3.sca_dst_tmp != 0x3f : e->d0.dst_tmp != 0x3f))
    {
        VP_GetDSTDisasm(e, is_sca, dest, sizeof(dest));
    }

    char formatted_dest[512];
    VP_FormatDisasm(e, dest, formatted_dest, sizeof(formatted_dest));
    VP_AddCodeCondDisasm(e, formatted_dest, value);
}

static void TaskVP(disasm_engine_t *e)
{
    e->instr_count = 0;
    bool is_has_BRA = false;

    const size_t max_instr_count = 512;
    for (u32 i = 1; e->instr_count < max_instr_count && i < e->vp_data_count; e->instr_count++)
    {
        if (is_has_BRA)
        {
            e->d3.HEX = e->vp_data[i];
            i += 4;
        }
        else
        {
            e->d1.HEX = e->vp_data[i++];
            e->sca_opcode = e->d1.sca_opcode;
            switch (e->d1.sca_opcode)
            {
            case 0x08: // BRA
                is_has_BRA = true;
                if (i < e->vp_data_count) e->d3.HEX = e->vp_data[++i];
                i += 4;
                if (e->sca_opcode < sizeof(rsx_vp_sca_op_names) / sizeof(rsx_vp_sca_op_names[0]))
                {
                    sb_append_fmt(&e->arb_shader, "%s# WARNING ", rsx_vp_sca_op_names[e->sca_opcode]);
                }
                break;
            case 0x09: // BRI
                if (i < e->vp_data_count) e->d2.HEX = e->vp_data[i++];
                if (i < e->vp_data_count) e->d3.HEX = e->vp_data[i];
                i += 2;
                if (e->sca_opcode < sizeof(rsx_vp_sca_op_names) / sizeof(rsx_vp_sca_op_names[0]))
                {
                    sb_append_fmt(&e->arb_shader, "%s$ifcond # WARNING ", rsx_vp_sca_op_names[e->sca_opcode]);
                }
                break;
            default:
                if (i < e->vp_data_count) e->d3.HEX = e->vp_data[++i];
                i += 2;
                break;
            }
        }

        if (e->d3.end)
        {
            e->instr_count++;
            break;
        }
    }

    for (u32 i = 0; i < e->instr_count && (i * 4 + 3) < e->vp_data_count; ++i)
    {
        e->d0.HEX = e->vp_data[i * 4 + 0];
        e->d1.HEX = e->vp_data[i * 4 + 1];
        e->d2.HEX = e->vp_data[i * 4 + 2];
        e->d3.HEX = e->vp_data[i * 4 + 3];

        e->src[0].src0l = e->d2.src0l;
        e->src[0].src0h = e->d1.src0h;
        e->src[1].src1  = e->d2.src1;
        e->src[2].src2l = e->d3.src2l;
        e->src[2].src2h = e->d2.src2h;

        e->sca_opcode = e->d1.sca_opcode;
        switch (e->d1.sca_opcode)
        {
        case RSX_SCA_OPCODE_NOP: break;
        case RSX_SCA_OPCODE_MOV: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_RCP: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_RCC: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_RSQ: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_EXP: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_LOG: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_LIT: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_BRA:
            if (e->sca_opcode < sizeof(rsx_vp_sca_op_names) / sizeof(rsx_vp_sca_op_names[0])) {
                sb_append_fmt(&e->arb_shader, "%sBRA # WARNING ", rsx_vp_sca_op_names[e->sca_opcode]);
            }
            break;
        case RSX_SCA_OPCODE_BRI: VP_AddCodeDisasm(e, "$ifcond # WARNING"); break;
        case RSX_SCA_OPCODE_CAL: VP_AddCodeDisasm(e, "$ifcond $f# WARNING"); break;
        case RSX_SCA_OPCODE_CLI: VP_AddCodeDisasm(e, "$ifcond $f # WARNING"); break;
        case RSX_SCA_OPCODE_RET: VP_AddCodeDisasm(e, "$ifcond # WARNING"); break;
        case RSX_SCA_OPCODE_LG2: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_EX2: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_SIN: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_COS: VP_SetDSTDisasm(e, true, "$s"); break;
        case RSX_SCA_OPCODE_BRB: VP_SetDSTDisasm(e, true, "# WARNING Boolean constant"); break;
        case RSX_SCA_OPCODE_CLB: VP_SetDSTDisasm(e, true, "# WARNING Boolean constant"); break;
        case RSX_SCA_OPCODE_PSH: VP_SetDSTDisasm(e, true, ""); break;
        case RSX_SCA_OPCODE_POP: VP_SetDSTDisasm(e, true, ""); break;
        default:
            sb_append_fmt(&e->arb_shader, "# Unknown VP sca_opcode: 0x%x\n", e->d1.sca_opcode);
            break;
        }

        e->vec_opcode = e->d1.vec_opcode;
        switch (e->d1.vec_opcode)
        {
        case RSX_VEC_OPCODE_NOP: break;
        case RSX_VEC_OPCODE_MOV: VP_SetDSTDisasm(e, false, "$0"); break;
        case RSX_VEC_OPCODE_MUL: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_ADD: VP_SetDSTDisasm(e, false, "$0, $2"); break;
        case RSX_VEC_OPCODE_MAD: VP_SetDSTDisasm(e, false, "$0, $1, $2"); break;
        case RSX_VEC_OPCODE_DP3: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_DPH: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_DP4: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_DST: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_MIN: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_MAX: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_SLT: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_SGE: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_ARL: VP_AddCodeDisasm(e, "ARL, $a, $0"); break;
        case RSX_VEC_OPCODE_FRC: VP_SetDSTDisasm(e, false, "$0"); break;
        case RSX_VEC_OPCODE_FLR: VP_SetDSTDisasm(e, false, "$0"); break;
        case RSX_VEC_OPCODE_SEQ: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_SFL: VP_SetDSTDisasm(e, false, "$0"); break;
        case RSX_VEC_OPCODE_SGT: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_SLE: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_SNE: VP_SetDSTDisasm(e, false, "$0, $1"); break;
        case RSX_VEC_OPCODE_STR: VP_SetDSTDisasm(e, false, "$0"); break;
        case RSX_VEC_OPCODE_SSG: VP_SetDSTDisasm(e, false, "$0"); break;
        case RSX_VEC_OPCODE_TXL: VP_SetDSTDisasm(e, false, "$t, $0"); break;
        default:
            sb_append_fmt(&e->arb_shader, "# Unknown VP vec_opcode: 0x%x\n", e->d1.vec_opcode);
            break;
        }
    }

    sb_append_str(&e->arb_shader, "END\n");
}

// ============================================================================
// 7. Binary Container Parsing (Sony Cg, PSL1GHT VP/FP)
// ============================================================================

static bool BuildOfficialCgDisassembly(disasm_engine_t *e)
{
    if (e->buffer_size < sizeof(struct CgBinaryProgram))
    {
        sb_append_str(&e->arb_shader, "# Error: Buffer too small for CgBinaryProgram header\n");
        return false;
    }

    struct CgBinaryProgram *prog = (struct CgBinaryProgram *)e->buffer;
    const u32 raw_profile_be = bswap32(prog->profile);

    if (raw_profile_be == CG_PROFILE_SCE_VP_RSX || raw_profile_be == CG_PROFILE_SCE_FP_RSX)
    {
        ConvertCgProgramToLE(e, prog);
    }

    if (prog->profile == CG_PROFILE_SCE_FP_RSX)
    {
        if (prog->program + sizeof(struct CgBinaryFragmentProgram) > e->buffer_size)
        {
            sb_append_str(&e->arb_shader, "# Error: Truncated Fragment Program header\n");
            return false;
        }

        const struct CgBinaryFragmentProgram *fprog =
            (const struct CgBinaryFragmentProgram *)(e->buffer + prog->program);

        sb_append_fmt(&e->arb_shader,
            "# binaryFormatRevision 0x%x\n"
            "# profile sce_fp_rsx\n"
            "# parameterCount %u\n"
            "# instructionCount %u\n"
            "# attributeInputMask 0x%x\n"
            "# registerCount %u\n\n",
            prog->binaryFormatRevision,
            prog->parameterCount,
            fprog->instructionCount,
            fprog->attributeInputMask,
            (unsigned)fprog->registerCount);

        u32 offset = prog->parameterArray;
        for (u32 i = 0; i < prog->parameterCount; i++)
        {
            if (offset + sizeof(struct CgBinaryParameter) > e->buffer_size) break;
            const struct CgBinaryParameter *fparam =
                (const struct CgBinaryParameter *)(e->buffer + offset);

            char type_buf[64];
            char val_buf[256];
            GetCgParamValue(e, fparam->embeddedConst, fparam->name, val_buf, sizeof(val_buf));

            sb_append_fmt(&e->arb_shader, "#%u %s %s %s %s\n",
                i,
                GetCgParamType(fparam->type, type_buf, sizeof(type_buf)),
                GetCgParamName(e, fparam->name),
                GetCgParamSemantic(e, fparam->semantic),
                val_buf);

            offset += (u32)sizeof(struct CgBinaryParameter);
        }

        sb_append_str(&e->arb_shader, "\n");
        e->offset = prog->ucode;
        TaskFP(e);
        return true;
    }
    else if (prog->profile == CG_PROFILE_SCE_VP_RSX)
    {
        if (prog->program + sizeof(struct CgBinaryVertexProgram) > e->buffer_size)
        {
            sb_append_str(&e->arb_shader, "# Error: Truncated Vertex Program header\n");
            return false;
        }

        const struct CgBinaryVertexProgram *vprog =
            (const struct CgBinaryVertexProgram *)(e->buffer + prog->program);

        sb_append_fmt(&e->arb_shader,
            "# binaryFormatRevision 0x%x\n"
            "# profile sce_vp_rsx\n"
            "# parameterCount %u\n"
            "# instructionCount %u\n"
            "# registerCount %u\n"
            "# attributeInputMask 0x%x\n"
            "# attributeOutputMask 0x%x\n\n",
            prog->binaryFormatRevision,
            prog->parameterCount,
            vprog->instructionCount,
            vprog->registerCount,
            vprog->attributeInputMask,
            vprog->attributeOutputMask);

        u32 offset = prog->parameterArray;
        for (u32 i = 0; i < prog->parameterCount; i++)
        {
            if (offset + sizeof(struct CgBinaryParameter) > e->buffer_size) break;
            const struct CgBinaryParameter *vparam =
                (const struct CgBinaryParameter *)(e->buffer + offset);

            char type_buf[64];
            char val_buf[256];
            GetCgParamValue(e, vparam->embeddedConst, vparam->name, val_buf, sizeof(val_buf));

            sb_append_fmt(&e->arb_shader, "#%u %s %s %s %s\n",
                i,
                GetCgParamType(vparam->type, type_buf, sizeof(type_buf)),
                GetCgParamName(e, vparam->name),
                GetCgParamSemantic(e, vparam->semantic),
                val_buf);

            offset += (u32)sizeof(struct CgBinaryParameter);
        }

        sb_append_str(&e->arb_shader, "\n");
        e->offset = prog->ucode;
        if (e->offset > e->buffer_size)
        {
            sb_append_str(&e->arb_shader, "# Error: Invalid microcode offset\n");
            return false;
        }

        const u32 ucode_size = (prog->ucodeSize != 0 && e->offset + prog->ucodeSize <= e->buffer_size)
            ? prog->ucodeSize : (u32)(e->buffer_size - e->offset);

        e->vp_data_count = ucode_size / sizeof(u32);
        e->vp_data = (u32 *)malloc(e->vp_data_count * sizeof(u32));
        if (e->vp_data)
        {
            memcpy(e->vp_data, e->buffer + e->offset, e->vp_data_count * sizeof(u32));
            TaskVP(e);
            return true;
        }
        return false;
    }

    return false;
}

static bool BuildPsl1ghtFPDisassembly(disasm_engine_t *e)
{
    if (e->buffer_size < sizeof(struct Psl1ghtFragmentProgramHeader))
    {
        sb_append_str(&e->arb_shader, "# Error: Truncated PSL1GHT Fragment Program header\n");
        return false;
    }

    const struct Psl1ghtFragmentProgramHeader *raw_hdr =
        (const struct Psl1ghtFragmentProgramHeader *)e->buffer;
    const bool is_be = (raw_hdr->magic == bswap16(PSL1GHT_RSX_FP_MAGIC));

    const u16 num_regs   = is_be ? bswap16(raw_hdr->num_regs)   : raw_hdr->num_regs;
    const u16 num_attr   = is_be ? bswap16(raw_hdr->num_attr)   : raw_hdr->num_attr;
    const u16 num_const  = is_be ? bswap16(raw_hdr->num_const)  : raw_hdr->num_const;
    const u16 num_insn   = is_be ? bswap16(raw_hdr->num_insn)   : raw_hdr->num_insn;
    const u32 attr_off   = is_be ? bswap32(raw_hdr->attr_off)   : raw_hdr->attr_off;
    const u32 const_off  = is_be ? bswap32(raw_hdr->const_off)  : raw_hdr->const_off;
    const u32 ucode_off  = is_be ? bswap32(raw_hdr->ucode_off)  : raw_hdr->ucode_off;
    const u32 fp_control = is_be ? bswap32(raw_hdr->fp_control) : raw_hdr->fp_control;
    const u16 texcoords  = is_be ? bswap16(raw_hdr->texcoords)  : raw_hdr->texcoords;
    const u16 texcoord2D = is_be ? bswap16(raw_hdr->texcoord2D) : raw_hdr->texcoord2D;
    const u16 texcoord3D = is_be ? bswap16(raw_hdr->texcoord3D) : raw_hdr->texcoord3D;

    sb_append_fmt(&e->arb_shader,
        "# profile sce_fp_rsx (PSL1GHT / Homebrew)\n"
        "# instructionCount %u\n"
        "# registerCount %u\n"
        "# attributeCount %u\n"
        "# constantCount %u\n"
        "# fp_control 0x%08X\n"
        "# texcoords 0x%04X (tex2D: 0x%04X, tex3D: 0x%04X)\n\n",
        num_insn, num_regs, num_attr, num_const, fp_control, texcoords, texcoord2D, texcoord3D);

    // Print input attributes
    if (num_attr > 0 && attr_off < e->buffer_size)
    {
        sb_append_str(&e->arb_shader, "# Attributes:\n");
        u32 cur_attr = attr_off;
        for (u32 i = 0; i < num_attr && cur_attr + sizeof(struct Psl1ghtProgramAttrib) <= e->buffer_size; ++i)
        {
            const struct Psl1ghtProgramAttrib *attr =
                (const struct Psl1ghtProgramAttrib *)&e->buffer[cur_attr];
            const u32 name_off = is_be ? bswap32(attr->name_off) : attr->name_off;
            const u32 index    = is_be ? bswap32(attr->index)    : attr->index;
            const char* name = (name_off < e->buffer_size) ? (const char*)&e->buffer[name_off] : "attr";

            char type_buf[64];
            sb_append_fmt(&e->arb_shader, "#   [%u] name: %s (type: %s, index: %u)\n",
                i, name, GetPsl1ghtParamTypeName(attr->type, type_buf, sizeof(type_buf)), index);
            cur_attr += (u32)sizeof(struct Psl1ghtProgramAttrib);
        }
        sb_append_str(&e->arb_shader, "\n");
    }

    // Print named constants
    if (num_const > 0 && const_off < e->buffer_size)
    {
        sb_append_str(&e->arb_shader, "# Constants / Uniforms:\n");
        u32 cur_const = const_off;
        for (u32 i = 0; i < num_const && cur_const + sizeof(struct Psl1ghtProgramConst) <= e->buffer_size; ++i)
        {
            const struct Psl1ghtProgramConst *c =
                (const struct Psl1ghtProgramConst *)&e->buffer[cur_const];
            const u32 name_off = is_be ? bswap32(c->name_off) : c->name_off;
            const u32 index    = is_be ? bswap32(c->index)    : c->index;
            const char* name = (name_off < e->buffer_size) ? (const char*)&e->buffer[name_off] : "c";

            char type_buf[64];
            sb_append_fmt(&e->arb_shader, "#   [%u] name: %s (type: %s, index/offset: %u)\n",
                i, name, GetPsl1ghtParamTypeName(c->type, type_buf, sizeof(type_buf)), index);
            cur_const += (u32)sizeof(struct Psl1ghtProgramConst);
        }
        sb_append_str(&e->arb_shader, "\n");
    }

    e->offset = ucode_off;
    if (is_be && e->offset < e->buffer_size)
    {
        SwapBe32Range(e->buffer, e->buffer_size, e->offset, e->buffer_size - e->offset);
    }

    TaskFP(e);
    return true;
}

static bool BuildPsl1ghtVPDisassembly(disasm_engine_t *e)
{
    if (e->buffer_size < sizeof(struct Psl1ghtVertexProgramHeader))
    {
        sb_append_str(&e->arb_shader, "# Error: Truncated PSL1GHT Vertex Program header\n");
        return false;
    }

    const struct Psl1ghtVertexProgramHeader *raw_hdr =
        (const struct Psl1ghtVertexProgramHeader *)e->buffer;
    const bool is_be = (raw_hdr->magic == bswap16(PSL1GHT_RSX_VP_MAGIC));

    const u16 num_regs    = is_be ? bswap16(raw_hdr->num_regs)    : raw_hdr->num_regs;
    const u16 num_attr    = is_be ? bswap16(raw_hdr->num_attr)    : raw_hdr->num_attr;
    const u16 num_const   = is_be ? bswap16(raw_hdr->num_const)   : raw_hdr->num_const;
    const u16 num_insn    = is_be ? bswap16(raw_hdr->num_insn)    : raw_hdr->num_insn;
    const u32 attr_off    = is_be ? bswap32(raw_hdr->attr_off)    : raw_hdr->attr_off;
    const u32 const_off   = is_be ? bswap32(raw_hdr->const_off)   : raw_hdr->const_off;
    const u32 ucode_off   = is_be ? bswap32(raw_hdr->ucode_off)   : raw_hdr->ucode_off;
    const u32 input_mask  = is_be ? bswap32(raw_hdr->input_mask)  : raw_hdr->input_mask;
    const u32 output_mask = is_be ? bswap32(raw_hdr->output_mask) : raw_hdr->output_mask;

    sb_append_fmt(&e->arb_shader,
        "# profile sce_vp_rsx (PSL1GHT / Homebrew)\n"
        "# instructionCount %u\n"
        "# registerCount %u\n"
        "# attributeCount %u\n"
        "# constantCount %u\n"
        "# attributeInputMask 0x%08X\n"
        "# attributeOutputMask 0x%08X\n\n",
        num_insn, num_regs, num_attr, num_const, input_mask, output_mask);

    // Print input attributes
    if (num_attr > 0 && attr_off < e->buffer_size)
    {
        sb_append_str(&e->arb_shader, "# Attributes:\n");
        u32 cur_attr = attr_off;
        for (u32 i = 0; i < num_attr && cur_attr + sizeof(struct Psl1ghtProgramAttrib) <= e->buffer_size; ++i)
        {
            const struct Psl1ghtProgramAttrib *attr =
                (const struct Psl1ghtProgramAttrib *)&e->buffer[cur_attr];
            const u32 name_off = is_be ? bswap32(attr->name_off) : attr->name_off;
            const u32 index    = is_be ? bswap32(attr->index)    : attr->index;
            const char* name = (name_off < e->buffer_size) ? (const char*)&e->buffer[name_off] : "attr";

            char type_buf[64];
            sb_append_fmt(&e->arb_shader, "#   [%u] name: %s (type: %s, index: %u)\n",
                i, name, GetPsl1ghtParamTypeName(attr->type, type_buf, sizeof(type_buf)), index);
            cur_attr += (u32)sizeof(struct Psl1ghtProgramAttrib);
        }
        sb_append_str(&e->arb_shader, "\n");
    }

    // Print named constants
    if (num_const > 0 && const_off < e->buffer_size)
    {
        sb_append_str(&e->arb_shader, "# Constants / Uniforms:\n");
        u32 cur_const = const_off;
        for (u32 i = 0; i < num_const && cur_const + sizeof(struct Psl1ghtProgramConst) <= e->buffer_size; ++i)
        {
            const struct Psl1ghtProgramConst *c =
                (const struct Psl1ghtProgramConst *)&e->buffer[cur_const];
            const u32 name_off = is_be ? bswap32(c->name_off) : c->name_off;
            const u32 index    = is_be ? bswap32(c->index)    : c->index;
            const char* name = (name_off < e->buffer_size) ? (const char*)&e->buffer[name_off] : "c";

            char type_buf[64];
            sb_append_fmt(&e->arb_shader, "#   [%u] name: %s (type: %s, index/offset: %u)\n",
                i, name, GetPsl1ghtParamTypeName(c->type, type_buf, sizeof(type_buf)), index);
            cur_const += (u32)sizeof(struct Psl1ghtProgramConst);
        }
        sb_append_str(&e->arb_shader, "\n");
    }

    e->offset = ucode_off;
    if (e->offset > e->buffer_size)
    {
        sb_append_str(&e->arb_shader, "# Error: Invalid microcode offset\n");
        return false;
    }

    const u32 ucode_size = (num_insn > 0 && e->offset + num_insn * 16 <= e->buffer_size)
        ? (u32)(num_insn * 16) : (u32)(e->buffer_size - e->offset);

    e->vp_data_count = ucode_size / sizeof(u32);
    e->vp_data = (u32 *)malloc(e->vp_data_count * sizeof(u32));
    if (!e->vp_data) return false;

    const u32 *raw_vdata = (const u32 *)(e->buffer + e->offset);
    for (size_t i = 0; i < e->vp_data_count; ++i)
    {
        e->vp_data[i] = is_be ? bswap32(raw_vdata[i]) : raw_vdata[i];
    }

    TaskVP(e);
    return true;
}

static bool BuildDisassembly(disasm_engine_t *e)
{
    if (e->buffer_size < sizeof(u32))
    {
        sb_append_str(&e->arb_shader, "# Error: File too small\n");
        return false;
    }

    const u16 magic_u16 = *(const u16 *)e->buffer;
    const u16 magic_be  = bswap16(magic_u16);
    const u32 magic_u32 = *(const u32 *)e->buffer;
    const u32 magic_u32_be = bswap32(magic_u32);

    // Check for PSL1GHT / Homebrew binary format
    if (magic_u16 == PSL1GHT_RSX_FP_MAGIC || magic_be == PSL1GHT_RSX_FP_MAGIC)
    {
        return BuildPsl1ghtFPDisassembly(e);
    }
    else if (magic_u16 == PSL1GHT_RSX_VP_MAGIC || magic_be == PSL1GHT_RSX_VP_MAGIC)
    {
        return BuildPsl1ghtVPDisassembly(e);
    }
    // Check for Official Sony Cg binary format
    else if (magic_u32 == CG_PROFILE_SCE_FP_RSX || magic_u32_be == CG_PROFILE_SCE_FP_RSX ||
             magic_u32 == CG_PROFILE_SCE_VP_RSX || magic_u32_be == CG_PROFILE_SCE_VP_RSX)
    {
        return BuildOfficialCgDisassembly(e);
    }
    else
    {
        sb_append_fmt(&e->arb_shader,
            "# Error: Unknown shader format (magic: 0x%04X, u32: 0x%08X / %u)\n",
            magic_be, magic_u32_be, magic_u32_be);
        return false;
    }
}

// ============================================================================
// 8. Command Line Interface & Main Entry Point
// ============================================================================

static void PrintUsage(const char* prog_name)
{
    printf("Sony PlayStation 3 RSX Cg / PSL1GHT Binary Shader Disassembler (VPO/FPO -> ASM)\n"
           "Usage: %s [options] <input_file> [output_file]\n\n"
           "Options:\n"
           "  -o, --output <file>  Specify output assembly file path\n"
           "  -s, --stdout         Print disassembly directly to standard output\n"
           "  -h, --help           Display this help message and exit\n"
           "  -v, --version        Show version information\n\n"
           "Supported input formats:\n"
           "  *.vpo  (SCE RSX Vertex Program Object / PSL1GHT 'VP')\n"
           "  *.fpo  (SCE RSX Fragment Program Object / PSL1GHT 'FP')\n"
           "  *.bin  (Raw binary shader dumps)\n\n"
           "Examples:\n"
           "  %s shader.vpo -o shader.asm\n"
           "  %s fragment.fpo --stdout\n"
           "  %s rsxrt.fpo rsxrt.asm\n",
           prog_name, prog_name, prog_name, prog_name);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    const char* input_path = NULL;
    char output_path[1024];
    output_path[0] = '\0';
    bool write_stdout = false;

    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
        {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0)
        {
            printf("cg_disasm 1.1.0 (Sony Cg & PSL1GHT Universal RSX Engine)\n");
            return 0;
        }
        else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--stdout") == 0)
        {
            write_stdout = true;
        }
        else if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0)
        {
            if (i + 1 < argc)
            {
                snprintf(output_path, sizeof(output_path), "%s", argv[++i]);
            }
            else
            {
                fprintf(stderr, "Error: Option %s requires a file argument.\n", arg);
                return 1;
            }
        }
        else if (arg[0] == '-')
        {
            fprintf(stderr, "Error: Unknown option: %s\n", arg);
            PrintUsage(argv[0]);
            return 1;
        }
        else
        {
            if (!input_path)
            {
                input_path = arg;
            }
            else if (output_path[0] == '\0')
            {
                snprintf(output_path, sizeof(output_path), "%s", arg);
            }
            else
            {
                fprintf(stderr, "Error: Unexpected positional argument: %s\n", arg);
                return 1;
            }
        }
    }

    if (!input_path)
    {
        fprintf(stderr, "Error: No input file specified.\n");
        return 1;
    }

    FILE* fp = fopen(input_path, "rb");
    if (!fp)
    {
        fprintf(stderr, "Error: Failed to open input file: %s\n", input_path);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size <= 0)
    {
        fprintf(stderr, "Error: Input file is empty: %s\n", input_path);
        fclose(fp);
        return 1;
    }
    fseek(fp, 0, SEEK_SET);

    u8 *buffer = (u8 *)malloc((size_t)file_size);
    if (!buffer)
    {
        fprintf(stderr, "Error: Out of memory reading %s (%ld bytes)\n", input_path, file_size);
        fclose(fp);
        return 1;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)file_size, fp);
    fclose(fp);

    if (read_bytes != (size_t)file_size)
    {
        fprintf(stderr, "Error: Failed to read file data: %s\n", input_path);
        free(buffer);
        return 1;
    }

    disasm_engine_t engine;
    disasm_engine_init(&engine, buffer, (size_t)file_size);

    if (!BuildDisassembly(&engine))
    {
        fprintf(stderr, "Disassembly warning or partial decoding for %s\n", input_path);
    }

    const char* asm_output = engine.arb_shader.data ? engine.arb_shader.data : "";

    if (write_stdout)
    {
        fputs(asm_output, stdout);
        disasm_engine_free(&engine);
        free(buffer);
        return 0;
    }

    if (output_path[0] == '\0')
    {
        snprintf(output_path, sizeof(output_path), "%s", input_path);
        char *dot = strrchr(output_path, '.');
        char *slash = strrchr(output_path, '/');
        if (dot && (!slash || dot > slash))
        {
            snprintf(dot, sizeof(output_path) - (size_t)(dot - output_path), ".asm");
        }
        else
        {
            strncat(output_path, ".asm", sizeof(output_path) - strlen(output_path) - 1);
        }
    }

    FILE* out_fp = fopen(output_path, "w");
    if (!out_fp)
    {
        fprintf(stderr, "Error: Failed to open output file for writing: %s\n", output_path);
        disasm_engine_free(&engine);
        free(buffer);
        return 1;
    }

    fputs(asm_output, out_fp);
    fclose(out_fp);

    printf("Successfully disassembled %s -> %s\n", input_path, output_path);

    disasm_engine_free(&engine);
    free(buffer);
    return 0;
}
