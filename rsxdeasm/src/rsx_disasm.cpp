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

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <array>
#include <bit>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std::string_view_literals;

// Standard fixed-width integer aliases
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using f32 = float;

// Big-Endian swapping utilities
static inline u16 bswap16(u16 val) { return (val << 8) | (val >> 8); }
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

// Format Identification Constants
constexpr u32 CG_PROFILE_SCE_VP_RSX = 7003u; // Official Sony Cg Vertex Program
constexpr u32 CG_PROFILE_SCE_FP_RSX = 7004u; // Official Sony Cg Fragment Program

constexpr u16 PSL1GHT_RSX_VP_MAGIC = 0x5650; // 'VP' in Big-Endian (0x56='V', 0x50='P')
constexpr u16 PSL1GHT_RSX_FP_MAGIC = 0x4650; // 'FP' in Big-Endian (0x46='F', 0x50='P')

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

static const std::string rsx_fp_op_names[] =
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

static const std::string rsx_vp_sca_op_names[] =
{
    "NOP", "MOV", "RCP", "RCC", "RSQ", "EXP", "LOG",
    "LIT", "BRA", "BRI", "CAL", "CLI", "RET", "LG2",
    "EX2", "SIN", "COS", "BRB", "CLB", "PSH", "POP"
};

static const std::string rsx_vp_vec_op_names[] =
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

// Helper for PSL1GHT parameter types
static std::string GetPsl1ghtParamTypeName(u8 type)
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
    default: return "unknown_type(" + std::to_string(type) + ")";
    }
}

// String token replacement engine
static std::string ReplaceTokens(std::string text, const std::vector<std::pair<std::string, std::function<std::string()>>>& repls)
{
    for (const auto& [token, func] : repls)
    {
        size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos)
        {
            std::string replacement = func();
            text.replace(pos, token.length(), replacement);
            pos += replacement.length();
        }
    }
    return text;
}

// Unified Standalone Cg / RSX Binary Disassembler Engine
class CgBinaryDisasmEngine
{
    std::vector<u8> m_buffer;
    std::string m_arb_shader;
    std::string m_dst_reg_name;

    // Fragment Program state
    OPDEST dst{};
    SRC0 src0{};
    SRC1 src1{};
    SRC2 src2{};
    u32 m_offset = 0;
    u32 m_opcode = 0;
    u32 m_step = 0;
    u32 m_size = 0;
    std::vector<u32> m_end_offsets;
    std::vector<u32> m_else_offsets;
    std::vector<u32> m_loop_end_offsets;

    // Vertex Program state
    D0 d0{};
    D1 d1{};
    D2 d2{};
    D3 d3{};
    VP_SRC src[3]{};
    u32 m_sca_opcode = 0;
    u32 m_vec_opcode = 0;
    static constexpr size_t m_max_instr_count = 512;
    size_t m_instr_count = 0;
    std::vector<u32> m_data;

public:
    explicit CgBinaryDisasmEngine(std::vector<u8> buffer)
        : m_buffer(std::move(buffer))
    {
    }

    const std::string& GetArbShader() const { return m_arb_shader; }

    template<typename T>
    T& GetCgRef(u32 offset)
    {
        return *reinterpret_cast<T*>(&m_buffer[offset]);
    }

    static std::string GetCgParamType(u32 type)
    {
        switch (type)
        {
        case 1045: return "float";
        case 1046:
        case 1047:
        case 1048: return "float" + std::to_string(type - 1044);
        case 1064: return "float4x4";
        case 1066: return "sampler2D";
        case 1069: return "samplerCUBE";
        case 1091: return "float1";
        default: return "!UnkCgType(" + std::to_string(type) + ")";
        }
    }

    std::string GetCgParamName(u32 offset) const
    {
        if (offset >= m_buffer.size()) return "";
        return reinterpret_cast<const char*>(&m_buffer[offset]);
    }

    std::string GetCgParamSemantic(u32 offset) const
    {
        if (offset >= m_buffer.size()) return "";
        return reinterpret_cast<const char*>(&m_buffer[offset]);
    }

    std::string GetCgParamValue(u32 offset, u32 end_offset) const
    {
        if (offset == 0 || offset >= m_buffer.size() || end_offset <= offset) return "";
        std::string offsets = "offsets:";
        u32 num = 0;
        offset += 6;
        while (offset < end_offset && offset + 1 < m_buffer.size())
        {
            u32 val = (static_cast<u32>(m_buffer[offset]) << 8) | static_cast<u32>(m_buffer[offset + 1]);
            offsets += " " + std::to_string(val) + ",";
            offset += 4;
            num++;
        }
        if (num > 4 || num == 0) return "";
        offsets.pop_back();
        return "num " + std::to_string(num) + " " + offsets;
    }

    void ConvertCgProgramToLE(CgBinaryProgram& prog)
    {
        const u32 be_profile = bswap32(prog.profile);

        auto swap_be32 = [&](u32 start_offset, size_t size_bytes)
        {
            if (start_offset + size_bytes > m_buffer.size()) return;
            auto* start = reinterpret_cast<u32*>(m_buffer.data() + start_offset);
            auto* end   = reinterpret_cast<u32*>(m_buffer.data() + start_offset + size_bytes);
            for (auto* data = start; data < end; ++data)
            {
                *data = bswap32(*data);
            }
        };

        // 1. Swap the main header
        swap_be32(0, sizeof(CgBinaryProgram));

        // 2. Swap parameter descriptors
        if (prog.parameterArray < m_buffer.size())
        {
            swap_be32(prog.parameterArray, sizeof(CgBinaryParameter) * prog.parameterCount);
        }

        // 3. Swap the raw microcode payload
        if (prog.ucode < m_buffer.size())
        {
            swap_be32(prog.ucode, m_buffer.size() - prog.ucode);
        }

        // 4. Swap domain specific header
        if (be_profile == CG_PROFILE_SCE_FP_RSX)
        {
            if (prog.program + sizeof(CgBinaryFragmentProgram) <= m_buffer.size())
            {
                auto& fprog = GetCgRef<CgBinaryFragmentProgram>(prog.program);
                fprog.instructionCount   = bswap32(fprog.instructionCount);
                fprog.attributeInputMask = bswap32(fprog.attributeInputMask);
                fprog.partialTexType     = bswap32(fprog.partialTexType);
                fprog.texCoordsInputMask = bswap16(fprog.texCoordsInputMask);
                fprog.texCoords2D        = bswap16(fprog.texCoords2D);
                fprog.texCoordsCentroid  = bswap16(fprog.texCoordsCentroid);
            }
        }
        else
        {
            if (prog.program + sizeof(CgBinaryVertexProgram) <= m_buffer.size())
            {
                swap_be32(prog.program, sizeof(CgBinaryVertexProgram));
            }
        }
    }

    bool BuildDisassembly()
    {
        if (m_buffer.size() < sizeof(u32))
        {
            m_arb_shader = "# Error: File too small\n";
            return false;
        }

        const u16 magic_u16 = *reinterpret_cast<const u16*>(m_buffer.data());
        const u16 magic_be  = bswap16(magic_u16);
        const u32 magic_u32 = *reinterpret_cast<const u32*>(m_buffer.data());
        const u32 magic_u32_be = bswap32(magic_u32);

        // Check for PSL1GHT / Homebrew binary format
        if (magic_u16 == PSL1GHT_RSX_FP_MAGIC || magic_be == PSL1GHT_RSX_FP_MAGIC)
        {
            return BuildPsl1ghtFPDisassembly();
        }
        else if (magic_u16 == PSL1GHT_RSX_VP_MAGIC || magic_be == PSL1GHT_RSX_VP_MAGIC)
        {
            return BuildPsl1ghtVPDisassembly();
        }
        // Check for Official Sony Cg binary format
        else if (magic_u32 == CG_PROFILE_SCE_FP_RSX || magic_u32_be == CG_PROFILE_SCE_FP_RSX ||
                 magic_u32 == CG_PROFILE_SCE_VP_RSX || magic_u32_be == CG_PROFILE_SCE_VP_RSX)
        {
            return BuildOfficialCgDisassembly();
        }
        else
        {
            char buf[256];
            snprintf(buf, sizeof(buf), "# Error: Unknown shader format (magic: 0x%04X, u32: 0x%08X / %u)\n",
                magic_be, magic_u32_be, magic_u32_be);
            m_arb_shader = buf;
            return false;
        }
    }

private:
    bool BuildOfficialCgDisassembly()
    {
        if (m_buffer.size() < sizeof(CgBinaryProgram))
        {
            m_arb_shader = "# Error: Buffer too small for CgBinaryProgram header\n";
            return false;
        }

        auto& prog = GetCgRef<CgBinaryProgram>(0);
        const u32 raw_profile_be = bswap32(prog.profile);

        if (raw_profile_be == CG_PROFILE_SCE_VP_RSX || raw_profile_be == CG_PROFILE_SCE_FP_RSX)
        {
            ConvertCgProgramToLE(prog);
        }

        if (prog.profile == CG_PROFILE_SCE_FP_RSX)
        {
            if (prog.program + sizeof(CgBinaryFragmentProgram) > m_buffer.size())
            {
                m_arb_shader = "# Error: Truncated Fragment Program header\n";
                return false;
            }

            const auto& fprog = GetCgRef<CgBinaryFragmentProgram>(prog.program);
            char header_buf[512];
            snprintf(header_buf, sizeof(header_buf),
                "# binaryFormatRevision 0x%x\n"
                "# profile sce_fp_rsx\n"
                "# parameterCount %u\n"
                "# instructionCount %u\n"
                "# attributeInputMask 0x%x\n"
                "# registerCount %u\n\n",
                prog.binaryFormatRevision,
                prog.parameterCount,
                fprog.instructionCount,
                fprog.attributeInputMask,
                static_cast<u32>(fprog.registerCount));
            m_arb_shader += header_buf;

            u32 offset = prog.parameterArray;
            for (u32 i = 0; i < prog.parameterCount; i++)
            {
                if (offset + sizeof(CgBinaryParameter) > m_buffer.size()) break;
                const auto& fparam = GetCgRef<CgBinaryParameter>(offset);

                std::string param_type     = GetCgParamType(fparam.type) + " ";
                std::string param_name     = GetCgParamName(fparam.name) + " ";
                std::string param_semantic = GetCgParamSemantic(fparam.semantic) + " ";
                std::string param_const    = GetCgParamValue(fparam.embeddedConst, fparam.name);

                char param_buf[512];
                snprintf(param_buf, sizeof(param_buf), "#%u %s%s%s%s\n",
                    i, param_type.c_str(), param_name.c_str(), param_semantic.c_str(), param_const.c_str());
                m_arb_shader += param_buf;

                offset += static_cast<u32>(sizeof(CgBinaryParameter));
            }

            m_arb_shader += "\n";
            m_offset = prog.ucode;
            TaskFP();
            return true;
        }
        else if (prog.profile == CG_PROFILE_SCE_VP_RSX)
        {
            if (prog.program + sizeof(CgBinaryVertexProgram) > m_buffer.size())
            {
                m_arb_shader = "# Error: Truncated Vertex Program header\n";
                return false;
            }

            const auto& vprog = GetCgRef<CgBinaryVertexProgram>(prog.program);
            char header_buf[512];
            snprintf(header_buf, sizeof(header_buf),
                "# binaryFormatRevision 0x%x\n"
                "# profile sce_vp_rsx\n"
                "# parameterCount %u\n"
                "# instructionCount %u\n"
                "# registerCount %u\n"
                "# attributeInputMask 0x%x\n"
                "# attributeOutputMask 0x%x\n\n",
                prog.binaryFormatRevision,
                prog.parameterCount,
                vprog.instructionCount,
                vprog.registerCount,
                vprog.attributeInputMask,
                vprog.attributeOutputMask);
            m_arb_shader += header_buf;

            u32 offset = prog.parameterArray;
            for (u32 i = 0; i < prog.parameterCount; i++)
            {
                if (offset + sizeof(CgBinaryParameter) > m_buffer.size()) break;
                const auto& vparam = GetCgRef<CgBinaryParameter>(offset);

                std::string param_type     = GetCgParamType(vparam.type) + " ";
                std::string param_name     = GetCgParamName(vparam.name) + " ";
                std::string param_semantic = GetCgParamSemantic(vparam.semantic) + " ";
                std::string param_const    = GetCgParamValue(vparam.embeddedConst, vparam.name);

                char param_buf[512];
                snprintf(param_buf, sizeof(param_buf), "#%u %s%s%s%s\n",
                    i, param_type.c_str(), param_name.c_str(), param_semantic.c_str(), param_const.c_str());
                m_arb_shader += param_buf;

                offset += static_cast<u32>(sizeof(CgBinaryParameter));
            }

            m_arb_shader += "\n";
            m_offset = prog.ucode;
            if (m_offset > m_buffer.size())
            {
                m_arb_shader += "# Error: Invalid microcode offset\n";
                return false;
            }

            const u32 ucode_size = (prog.ucodeSize != 0 && m_offset + prog.ucodeSize <= m_buffer.size())
                ? prog.ucodeSize : static_cast<u32>(m_buffer.size() - m_offset);

            const auto* vdata = reinterpret_cast<const u32*>(&m_buffer[m_offset]);
            m_data.resize(ucode_size / sizeof(u32));
            std::memcpy(m_data.data(), vdata, m_data.size() * sizeof(u32));
            TaskVP();
            return true;
        }

        return false;
    }

    bool BuildPsl1ghtFPDisassembly()
    {
        if (m_buffer.size() < sizeof(Psl1ghtFragmentProgramHeader))
        {
            m_arb_shader = "# Error: Truncated PSL1GHT Fragment Program header\n";
            return false;
        }

        const auto* raw_hdr = reinterpret_cast<const Psl1ghtFragmentProgramHeader*>(m_buffer.data());
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

        char header_buf[512];
        snprintf(header_buf, sizeof(header_buf),
            "# profile sce_fp_rsx (PSL1GHT / Homebrew)\n"
            "# instructionCount %u\n"
            "# registerCount %u\n"
            "# attributeCount %u\n"
            "# constantCount %u\n"
            "# fp_control 0x%08X\n"
            "# texcoords 0x%04X (tex2D: 0x%04X, tex3D: 0x%04X)\n\n",
            num_insn, num_regs, num_attr, num_const, fp_control, texcoords, texcoord2D, texcoord3D);
        m_arb_shader += header_buf;

        // Print input attributes
        if (num_attr > 0 && attr_off < m_buffer.size())
        {
            m_arb_shader += "# Attributes:\n";
            u32 cur_attr = attr_off;
            for (u32 i = 0; i < num_attr && cur_attr + sizeof(Psl1ghtProgramAttrib) <= m_buffer.size(); ++i)
            {
                const auto* attr = reinterpret_cast<const Psl1ghtProgramAttrib*>(&m_buffer[cur_attr]);
                const u32 name_off = is_be ? bswap32(attr->name_off) : attr->name_off;
                const u32 index    = is_be ? bswap32(attr->index)    : attr->index;
                const std::string name = (name_off < m_buffer.size()) ? reinterpret_cast<const char*>(&m_buffer[name_off]) : "attr" + std::to_string(i);

                char attr_line[256];
                snprintf(attr_line, sizeof(attr_line), "#   [%u] name: %s (type: %s, index: %u)\n",
                    i, name.c_str(), GetPsl1ghtParamTypeName(attr->type).c_str(), index);
                m_arb_shader += attr_line;
                cur_attr += static_cast<u32>(sizeof(Psl1ghtProgramAttrib));
            }
            m_arb_shader += "\n";
        }

        // Print named constants
        if (num_const > 0 && const_off < m_buffer.size())
        {
            m_arb_shader += "# Constants / Uniforms:\n";
            u32 cur_const = const_off;
            for (u32 i = 0; i < num_const && cur_const + sizeof(Psl1ghtProgramConst) <= m_buffer.size(); ++i)
            {
                const auto* c = reinterpret_cast<const Psl1ghtProgramConst*>(&m_buffer[cur_const]);
                const u32 name_off = is_be ? bswap32(c->name_off) : c->name_off;
                const u32 index    = is_be ? bswap32(c->index)    : c->index;
                const std::string name = (name_off < m_buffer.size()) ? reinterpret_cast<const char*>(&m_buffer[name_off]) : "c" + std::to_string(i);

                char const_line[256];
                snprintf(const_line, sizeof(const_line), "#   [%u] name: %s (type: %s, index/offset: %u)\n",
                    i, name.c_str(), GetPsl1ghtParamTypeName(c->type).c_str(), index);
                m_arb_shader += const_line;
                cur_const += static_cast<u32>(sizeof(Psl1ghtProgramConst));
            }
            m_arb_shader += "\n";
        }

        m_offset = ucode_off;
        if (is_be && m_offset < m_buffer.size())
        {
            auto* start = reinterpret_cast<u32*>(m_buffer.data() + m_offset);
            auto* end   = reinterpret_cast<u32*>(m_buffer.data() + m_buffer.size());
            for (auto* ptr = start; ptr < end; ++ptr)
            {
                *ptr = bswap32(*ptr);
            }
        }

        TaskFP();
        return true;
    }

    bool BuildPsl1ghtVPDisassembly()
    {
        if (m_buffer.size() < sizeof(Psl1ghtVertexProgramHeader))
        {
            m_arb_shader = "# Error: Truncated PSL1GHT Vertex Program header\n";
            return false;
        }

        const auto* raw_hdr = reinterpret_cast<const Psl1ghtVertexProgramHeader*>(m_buffer.data());
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

        char header_buf[512];
        snprintf(header_buf, sizeof(header_buf),
            "# profile sce_vp_rsx (PSL1GHT / Homebrew)\n"
            "# instructionCount %u\n"
            "# registerCount %u\n"
            "# attributeCount %u\n"
            "# constantCount %u\n"
            "# attributeInputMask 0x%08X\n"
            "# attributeOutputMask 0x%08X\n\n",
            num_insn, num_regs, num_attr, num_const, input_mask, output_mask);
        m_arb_shader += header_buf;

        // Print input attributes
        if (num_attr > 0 && attr_off < m_buffer.size())
        {
            m_arb_shader += "# Attributes:\n";
            u32 cur_attr = attr_off;
            for (u32 i = 0; i < num_attr && cur_attr + sizeof(Psl1ghtProgramAttrib) <= m_buffer.size(); ++i)
            {
                const auto* attr = reinterpret_cast<const Psl1ghtProgramAttrib*>(&m_buffer[cur_attr]);
                const u32 name_off = is_be ? bswap32(attr->name_off) : attr->name_off;
                const u32 index    = is_be ? bswap32(attr->index)    : attr->index;
                const std::string name = (name_off < m_buffer.size()) ? reinterpret_cast<const char*>(&m_buffer[name_off]) : "attr" + std::to_string(i);

                char attr_line[256];
                snprintf(attr_line, sizeof(attr_line), "#   [%u] name: %s (type: %s, index: %u)\n",
                    i, name.c_str(), GetPsl1ghtParamTypeName(attr->type).c_str(), index);
                m_arb_shader += attr_line;
                cur_attr += static_cast<u32>(sizeof(Psl1ghtProgramAttrib));
            }
            m_arb_shader += "\n";
        }

        // Print named constants
        if (num_const > 0 && const_off < m_buffer.size())
        {
            m_arb_shader += "# Constants / Uniforms:\n";
            u32 cur_const = const_off;
            for (u32 i = 0; i < num_const && cur_const + sizeof(Psl1ghtProgramConst) <= m_buffer.size(); ++i)
            {
                const auto* c = reinterpret_cast<const Psl1ghtProgramConst*>(&m_buffer[cur_const]);
                const u32 name_off = is_be ? bswap32(c->name_off) : c->name_off;
                const u32 index    = is_be ? bswap32(c->index)    : c->index;
                const std::string name = (name_off < m_buffer.size()) ? reinterpret_cast<const char*>(&m_buffer[name_off]) : "c" + std::to_string(i);

                char const_line[256];
                snprintf(const_line, sizeof(const_line), "#   [%u] name: %s (type: %s, index: %u)\n",
                    i, name.c_str(), GetPsl1ghtParamTypeName(c->type).c_str(), index);
                m_arb_shader += const_line;
                cur_const += static_cast<u32>(sizeof(Psl1ghtProgramConst));
            }
            m_arb_shader += "\n";
        }

        m_offset = ucode_off;
        if (m_offset > m_buffer.size())
        {
            m_arb_shader += "# Error: Invalid microcode offset\n";
            return false;
        }

        const u32 ucode_size = (num_insn > 0 && m_offset + num_insn * 16 <= m_buffer.size())
            ? static_cast<u32>(num_insn * 16) : static_cast<u32>(m_buffer.size() - m_offset);

        m_data.resize(ucode_size / sizeof(u32));
        const auto* raw_vdata = reinterpret_cast<const u32*>(&m_buffer[m_offset]);
        for (size_t i = 0; i < m_data.size(); ++i)
        {
            m_data[i] = is_be ? bswap32(raw_vdata[i]) : raw_vdata[i];
        }

        TaskVP();
        return true;
    }

public:
    // ==========================================
    // Fragment Program Disassembly Implementation
    // ==========================================
    std::string GetMask() const
    {
        std::string ret;
        ret.reserve(5);
        static constexpr std::string_view dst_mask = "xyzw";
        ret += '.';
        if (dst.mask_x) ret += dst_mask[0];
        if (dst.mask_y) ret += dst_mask[1];
        if (dst.mask_z) ret += dst_mask[2];
        if (dst.mask_w) ret += dst_mask[3];
        return (ret == "."sv || ret == ".xyzw"sv) ? "" : ret;
    }

    std::string AddRegDisAsm(u32 index, int fp16) const
    {
        return (fp16 ? 'H' : 'R') + std::to_string(index);
    }

    std::string AddConstDisAsm()
    {
        if (m_offset + m_size + 4 * sizeof(u32) + 4 * sizeof(u32) > m_buffer.size())
        {
            return "{0, 0, 0, 0}";
        }

        const auto* data = reinterpret_cast<const u32*>(&m_buffer[m_offset + m_size + 4 * sizeof(u32)]);
        m_step = 2 * 4 * sizeof(u32);

        const u32 x = GetData(data[0]);
        const u32 y = GetData(data[1]);
        const u32 z = GetData(data[2]);
        const u32 w = GetData(data[3]);

        char buf[256];
        snprintf(buf, sizeof(buf), "{0x%08x(%g), 0x%08x(%g), 0x%08x(%g), 0x%08x(%g)}",
            x, std::bit_cast<f32>(x),
            y, std::bit_cast<f32>(y),
            z, std::bit_cast<f32>(z),
            w, std::bit_cast<f32>(w));
        return std::string(buf);
    }

    std::string AddTexDisAsm() const
    {
        return "TEX" + std::to_string(dst.tex_num);
    }

    std::string GetCondDisAsm() const
    {
        static constexpr std::string_view f = "xyzw";
        std::string swizzle;
        swizzle.reserve(5);
        swizzle += '.';
        swizzle += f[src0.cond_swizzle_x];
        swizzle += f[src0.cond_swizzle_y];
        swizzle += f[src0.cond_swizzle_z];
        swizzle += f[src0.cond_swizzle_w];

        if (swizzle == ".xxxx"sv) swizzle = ".x";
        else if (swizzle == ".yyyy"sv) swizzle = ".y";
        else if (swizzle == ".zzzz"sv) swizzle = ".z";
        else if (swizzle == ".wwww"sv) swizzle = ".w";
        else if (swizzle == ".xyzw"sv) swizzle.clear();

        std::string cond;
        if (src0.exec_if_gr && src0.exec_if_eq)      cond = "GE";
        else if (src0.exec_if_lt && src0.exec_if_eq) cond = "LE";
        else if (src0.exec_if_gr && src0.exec_if_lt) cond = "NE";
        else if (src0.exec_if_gr)                    cond = "GT";
        else if (src0.exec_if_lt)                    cond = "LT";
        else if (src0.exec_if_eq)                    cond = "FL";
        else                                         cond = "TR";

        return cond + swizzle;
    }

    template<typename T>
    std::string GetSrcDisAsm(T src)
    {
        std::string ret;
        switch (src.reg_type)
        {
        case 0: // Temporary register
            ret += AddRegDisAsm(src.tmp_reg_index, src.fp16);
            break;
        case 1: // Input attribute register
        {
            static const std::string reg_table[] =
            {
                "WPOS", "COL0", "COL1", "FOGC", "TEX0",
                "TEX1", "TEX2", "TEX3", "TEX4", "TEX5",
                "TEX6", "TEX7", "TEX8", "TEX9", "SSA"
            };

            if (dst.src_attr_reg_num == 0)
            {
                ret += reg_table[0];
            }
            else if (dst.src_attr_reg_num < std::size(reg_table))
            {
                const std::string perspective_correction = src2.perspective_corr ? "g" : "f";
                ret += perspective_correction + "[" + reg_table[dst.src_attr_reg_num] + "]";
            }
            else
            {
                ret += "v[" + std::to_string(dst.src_attr_reg_num) + "]";
            }
            break;
        }
        case 2: // Constant vector
            ret += AddConstDisAsm();
            break;
        default:
            ret += "UNK_REG";
            break;
        }

        static constexpr std::string_view f = "xyzw";
        std::string swizzle;
        swizzle.reserve(5);
        swizzle += '.';
        swizzle += f[src.swizzle_x];
        swizzle += f[src.swizzle_y];
        swizzle += f[src.swizzle_z];
        swizzle += f[src.swizzle_w];

        if (swizzle == ".xxxx"sv) swizzle = ".x";
        else if (swizzle == ".yyyy"sv) swizzle = ".y";
        else if (swizzle == ".zzzz"sv) swizzle = ".z";
        else if (swizzle == ".wwww"sv) swizzle = ".w";

        if (swizzle != ".xyzw"sv)
        {
            ret += swizzle;
        }

        if (src.neg) ret = "-" + ret;
        if (src.abs) ret = "|" + ret + "|";

        return ret;
    }

    std::string FormatDisAsm(const std::string& code)
    {
        const std::vector<std::pair<std::string, std::function<std::string()>>> repl_list =
        {
            { "$$",    []() -> std::string { return "$"; } },
            { "$0",    [this]{ return GetSrcDisAsm<SRC0>(src0); } },
            { "$1",    [this]{ return GetSrcDisAsm<SRC1>(src1); } },
            { "$2",    [this]{ return GetSrcDisAsm<SRC2>(src2); } },
            { "$t",    [this]{ return AddTexDisAsm(); } },
            { "$m",    [this]{ return GetMask(); } },
            { "$cond", [this]{ return GetCondDisAsm(); } },
            { "$c",    [this]{ return AddConstDisAsm(); } },
        };
        return ReplaceTokens(code, repl_list);
    }

    void AddCodeAsm(const std::string& code)
    {
        if (m_opcode >= std::size(rsx_fp_op_names)) return;

        std::string op_name;
        if (dst.dest_reg == 63)
        {
            m_dst_reg_name = "RC" + GetMask() + ", ";
            op_name = rsx_fp_op_names[m_opcode] + "XC";
        }
        else
        {
            m_dst_reg_name = (dst.fp16 ? "H" : "R") + std::to_string(dst.dest_reg) + GetMask() + ", ";
            op_name = rsx_fp_op_names[m_opcode] + (dst.fp16 ? "H" : "R");
        }

        switch (m_opcode)
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
            m_dst_reg_name.clear();
            op_name = rsx_fp_op_names[m_opcode] + (dst.fp16 ? "H" : "R");
            break;
        default: break;
        }

        m_arb_shader += (op_name + " " + m_dst_reg_name + FormatDisAsm(code) + ";\n");
    }

    void TaskFP()
    {
        m_size = 0;
        if (m_offset >= m_buffer.size()) return;

        const auto* data = reinterpret_cast<const u32*>(&m_buffer[m_offset]);
        const size_t total_words = (m_buffer.size() - m_offset) / sizeof(u32);
        size_t current_word_idx = 0;

        while (current_word_idx + 4 <= total_words)
        {
            for (auto found = std::find(m_end_offsets.begin(), m_end_offsets.end(), m_size);
                found != m_end_offsets.end();
                found = std::find(m_end_offsets.begin(), m_end_offsets.end(), m_size))
            {
                m_end_offsets.erase(found);
                m_arb_shader += "ENDIF;\n";
            }

            for (auto found = std::find(m_loop_end_offsets.begin(), m_loop_end_offsets.end(), m_size);
                found != m_loop_end_offsets.end();
                found = std::find(m_loop_end_offsets.begin(), m_loop_end_offsets.end(), m_size))
            {
                m_loop_end_offsets.erase(found);
                m_arb_shader += "ENDLOOP;\n";
            }

            for (auto found = std::find(m_else_offsets.begin(), m_else_offsets.end(), m_size);
                found != m_else_offsets.end();
                found = std::find(m_else_offsets.begin(), m_else_offsets.end(), m_size))
            {
                m_else_offsets.erase(found);
                m_arb_shader += "ELSE;\n";
            }

            dst.HEX  = GetData(data[current_word_idx + 0]);
            src0.HEX = GetData(data[current_word_idx + 1]);
            src1.HEX = GetData(data[current_word_idx + 2]);
            src2.HEX = GetData(data[current_word_idx + 3]);

            m_step = 4 * sizeof(u32);
            m_opcode = dst.opcode | (src1.opcode_hi << 6);

            auto SCT = [&]() -> bool
            {
                switch (m_opcode)
                {
                case RSX_FP_OPCODE_ADD: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DIV: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DIVSQ: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DP2: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DP3: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DP4: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DP2A: AddCodeAsm("$0, $1, $2"); break;
                case RSX_FP_OPCODE_MAD: AddCodeAsm("$0, $1, $2"); break;
                case RSX_FP_OPCODE_MAX: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_MIN: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_MOV: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_MUL: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_RCP: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_RSQ: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_SEQ: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SFL: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SGE: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SGT: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SLE: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SLT: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SNE: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_STR: AddCodeAsm("$0, $1"); break;
                default: return false;
                }
                return true;
            };

            auto SCB = [&]() -> bool
            {
                switch (m_opcode)
                {
                case RSX_FP_OPCODE_ADD: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_COS: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_DP2: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DP3: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DP4: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_DP2A: AddCodeAsm("$0, $1, $2"); break;
                case RSX_FP_OPCODE_DST: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_REFL: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_EX2: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_FLR: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_FRC: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_LIT: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_LIF: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_LRP: AddCodeAsm("# WARNING"); break;
                case RSX_FP_OPCODE_LG2: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_MAD: AddCodeAsm("$0, $1, $2"); break;
                case RSX_FP_OPCODE_MAX: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_MIN: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_MOV: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_MUL: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_PK2: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_PK4: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_PK16: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_PKB: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_PKG: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_SEQ: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SFL: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SGE: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SGT: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SIN: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_SLE: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SLT: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_SNE: AddCodeAsm("$0, $1"); break;
                case RSX_FP_OPCODE_STR: AddCodeAsm("$0, $1"); break;
                default: return false;
                }
                return true;
            };

            auto TEX_SRB = [&]() -> bool
            {
                switch (m_opcode)
                {
                case RSX_FP_OPCODE_DDX: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_DDY: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_NRM: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_BEM: AddCodeAsm("# WARNING"); break;
                case RSX_FP_OPCODE_TEX: AddCodeAsm("$0, $t"); break;
                case RSX_FP_OPCODE_TEXBEM: AddCodeAsm("# WARNING"); break;
                case RSX_FP_OPCODE_TXP: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_TXPBEM: AddCodeAsm("# WARNING"); break;
                case RSX_FP_OPCODE_TXD: AddCodeAsm("$0, $1, $t"); break;
                case RSX_FP_OPCODE_TXB: AddCodeAsm("$0, $t"); break;
                case RSX_FP_OPCODE_TXL: AddCodeAsm("$0, $t"); break;
                case RSX_FP_OPCODE_UP2: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_UP4: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_UP16: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_UPB: AddCodeAsm("$0"); break;
                case RSX_FP_OPCODE_UPG: AddCodeAsm("$0"); break;
                default: return false;
                }
                return true;
            };

            auto SIP = [&]() -> bool
            {
                switch (m_opcode)
                {
                case RSX_FP_OPCODE_BRK: AddCodeAsm("$cond"); break;
                case RSX_FP_OPCODE_CAL: AddCodeAsm("$cond"); break;
                case RSX_FP_OPCODE_FENCT: AddCodeAsm(""); break;
                case RSX_FP_OPCODE_FENCB: AddCodeAsm(""); break;
                case RSX_FP_OPCODE_IFE:
                    m_else_offsets.push_back(src1.else_offset << 2);
                    m_end_offsets.push_back(src2.end_offset << 2);
                    AddCodeAsm("($cond)");
                    break;
                case RSX_FP_OPCODE_LOOP:
                    if (src0.exec_if_eq || src0.exec_if_gr || src0.exec_if_lt)
                    {
                        m_loop_end_offsets.push_back(src2.end_offset << 2);
                    }
                    AddCodeAsm("{ " + std::to_string(src1.rep_count) + ", " +
                                      std::to_string(src1.init_counter) + ", " +
                                      std::to_string(src1.increment) + " }");
                    break;
                case RSX_FP_OPCODE_REP:
                    if (!src0.exec_if_eq && !src0.exec_if_gr && !src0.exec_if_lt)
                    {
                        m_arb_shader += "# RSX_FP_OPCODE_REP_1\n";
                    }
                    else
                    {
                        m_end_offsets.push_back(src2.end_offset << 2);
                        m_arb_shader += "# RSX_FP_OPCODE_REP_2\n";
                    }
                    break;
                case RSX_FP_OPCODE_RET: AddCodeAsm("$cond"); break;
                default: return false;
                }
                return true;
            };

            switch (m_opcode)
            {
            case RSX_FP_OPCODE_NOP: AddCodeAsm(""); break;
            case RSX_FP_OPCODE_KIL: AddCodeAsm("$cond"); break;
            default:
                if (!SIP() && !SCT() && !TEX_SRB() && !SCB())
                {
                    m_arb_shader += "# Unknown FP opcode: 0x" + std::to_string(m_opcode) + "\n";
                }
                break;
            }

            m_size += m_step;

            if (dst.end)
            {
                if (!m_arb_shader.empty() && m_arb_shader.back() == '\n') m_arb_shader.pop_back();
                m_arb_shader += " # last instruction\nEND\n";
                break;
            }

            current_word_idx += (m_step / sizeof(u32));
        }
    }

    // ========================================
    // Vertex Program Disassembly Implementation
    // ========================================
    void AddScaCodeDisasm(const std::string& code = "")
    {
        if (m_sca_opcode < std::size(rsx_vp_sca_op_names))
        {
            m_arb_shader += rsx_vp_sca_op_names[m_sca_opcode] + code + " ";
        }
    }

    void AddVecCodeDisasm(const std::string& code = "")
    {
        if (m_vec_opcode < std::size(rsx_vp_vec_op_names))
        {
            m_arb_shader += rsx_vp_vec_op_names[m_vec_opcode] + code + " ";
        }
    }

    std::string GetMaskDisasm(bool is_sca) const
    {
        std::string ret;
        ret.reserve(5);
        ret += '.';
        if (is_sca)
        {
            if (d3.sca_writemask_x) ret += "x";
            if (d3.sca_writemask_y) ret += "y";
            if (d3.sca_writemask_z) ret += "z";
            if (d3.sca_writemask_w) ret += "w";
        }
        else
        {
            if (d3.vec_writemask_x) ret += "x";
            if (d3.vec_writemask_y) ret += "y";
            if (d3.vec_writemask_z) ret += "z";
            if (d3.vec_writemask_w) ret += "w";
        }
        return (ret == "."sv || ret == ".xyzw"sv) ? "" : ret;
    }

    std::string GetDSTDisasm(bool is_sca) const
    {
        std::string ret;
        const std::string mask = GetMaskDisasm(is_sca);

        static constexpr std::array<std::string_view, 22> output_names =
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

        const u32 target_dst = (is_sca && d3.sca_dst_tmp != 0x3f) ? 0x1f : d3.dst;
        switch (target_dst)
        {
        case 0x1f:
            ret += (is_sca ? "R" + std::to_string(d3.sca_dst_tmp) : "R" + std::to_string(d0.dst_tmp)) + mask;
            break;
        default:
            if (d3.dst < output_names.size())
            {
                ret += std::string(output_names[d3.dst]) + mask;
            }
            else
            {
                ret += "(bad out index) o[" + std::to_string(d3.dst) + "]";
            }
            if (d0.dst_tmp != 0x3f)
            {
                ret += " R" + std::to_string(d0.dst_tmp) + mask;
            }
            break;
        }
        return ret;
    }

    std::string AddAddrMaskDisasm() const
    {
        static constexpr std::string_view f = "xyzw";
        return std::string(".") + f[d0.addr_swz];
    }

    std::string AddAddrRegDisasm() const
    {
        return "A" + std::to_string(d0.addr_reg_sel_1) + AddAddrMaskDisasm();
    }

    u32 GetAddrDisasm() const
    {
        return (d2.iaddrh << 3) | d3.iaddrl;
    }

    std::string GetSRCDisasm(u32 n) const
    {
        std::string ret;
        static constexpr std::array<std::string_view, 16> reg_table =
        {
            "in_pos", "in_weight", "in_normal",
            "in_diff_color", "in_spec_color",
            "in_fog", "in_point_size", "in_7",
            "in_tc0", "in_tc1", "in_tc2", "in_tc3",
            "in_tc4", "in_tc5", "in_tc6", "in_tc7"
        };

        const VP_SRC& s = src[n];
        switch (s.reg_type)
        {
        case 1: // Temporary register
            ret += "R" + std::to_string(s.tmp_src);
            break;
        case 2: // Input attribute
            if (d1.input_src < reg_table.size())
            {
                ret += reg_table[d1.input_src];
            }
            else
            {
                ret += "v[" + std::to_string(d1.input_src) + "]";
            }
            break;
        case 3: // Constant register
            ret += "c[" + (d3.index_const ? AddAddrRegDisasm() + " + " : "") + std::to_string(d1.const_src) + "]";
            break;
        default:
            ret += "UNK_SRC";
            break;
        }

        static constexpr std::string_view f = "xyzw";
        std::string swizzle;
        swizzle.reserve(5);
        swizzle += '.';
        swizzle += f[s.swz_x];
        swizzle += f[s.swz_y];
        swizzle += f[s.swz_z];
        swizzle += f[s.swz_w];

        if (swizzle == ".xxxx") swizzle = ".x";
        else if (swizzle == ".yyyy") swizzle = ".y";
        else if (swizzle == ".zzzz") swizzle = ".z";
        else if (swizzle == ".wwww") swizzle = ".w";

        if (swizzle != ".xyzw"sv)
        {
            ret += swizzle;
        }

        bool abs = false;
        switch (n)
        {
        default:
        case 0: abs = d0.src0_abs; break;
        case 1: abs = d0.src1_abs; break;
        case 2: abs = d0.src2_abs; break;
        }

        if (abs) ret = "|" + ret + "|";
        if (s.neg) ret = "-" + ret;

        return ret;
    }

    std::string GetCondDisasm() const
    {
        enum { lt = 0x1, eq = 0x2, gt = 0x4 };
        if (d0.cond == 0) return "false";
        if (d0.cond == (lt | gt | eq)) return "true";

        static const char* cond_string_table[(lt | gt | eq) + 1] =
        {
            "ERROR", "LT", "EQ", "LE", "GT", "NE", "GE", "ERROR"
        };

        static constexpr std::string_view f = "xyzw";
        std::string swizzle;
        swizzle.reserve(5);
        swizzle += '.';
        swizzle += f[d0.mask_x];
        swizzle += f[d0.mask_y];
        swizzle += f[d0.mask_z];
        swizzle += f[d0.mask_w];

        if (swizzle == ".xxxx") swizzle = ".x";
        else if (swizzle == ".yyyy") swizzle = ".y";
        else if (swizzle == ".zzzz") swizzle = ".z";
        else if (swizzle == ".wwww") swizzle = ".w";
        else if (swizzle == ".xyzw"sv) swizzle.clear();

        return "(" + std::string(cond_string_table[d0.cond]) + swizzle + ")";
    }

    std::string FormatDisasm(const std::string& code) const
    {
        const std::vector<std::pair<std::string, std::function<std::string()>>> repl_list =
        {
            { "$$",      []() -> std::string { return "$"; } },
            { "$0",      [this]{ return GetSRCDisasm(0); } },
            { "$1",      [this]{ return GetSRCDisasm(1); } },
            { "$2",      [this]{ return GetSRCDisasm(2); } },
            { "$s",      [this]{ return GetSRCDisasm(2); } },
            { "$am",     [this]{ return AddAddrMaskDisasm(); } },
            { "$a",      [this]{ return AddAddrRegDisasm(); } },
            { "$t",      []{ return "TEX0"; } },
            { "$fa",     [this]{ return std::to_string(GetAddrDisasm()); } },
            { "$ifcond ", [this]
                {
                    std::string cond = GetCondDisasm();
                    return (cond == "true") ? "" : (cond + " ");
                }
            },
            { "$cond",   [this]{ return GetCondDisasm(); } },
        };
        return ReplaceTokens(code, repl_list);
    }

    void AddCodeDisasm(const std::string& code)
    {
        m_arb_shader += FormatDisasm(code) + "\n";
    }

    void AddCodeCondDisasm(const std::string& dst_str, const std::string& src_str)
    {
        enum { lt = 0x1, eq = 0x2, gt = 0x4 };
        if (!d0.cond_test_enable || d0.cond == (lt | gt | eq))
        {
            AddCodeDisasm(dst_str + ", " + src_str + ";");
            return;
        }
        if (d0.cond == 0)
        {
            AddCodeDisasm("# " + dst_str + ", " + src_str + ";");
            return;
        }

        static const char* cond_string_table[(lt | gt | eq) + 1] =
        {
            "ERROR", "LT", "EQ", "LE", "GT", "NE", "GE", "ERROR"
        };

        static constexpr std::string_view f = "xyzw";
        std::string swizzle;
        swizzle.reserve(5);
        swizzle += '.';
        swizzle += f[d0.mask_x];
        swizzle += f[d0.mask_y];
        swizzle += f[d0.mask_z];
        swizzle += f[d0.mask_w];

        if (swizzle == ".xxxx") swizzle = ".x";
        else if (swizzle == ".yyyy") swizzle = ".y";
        else if (swizzle == ".zzzz") swizzle = ".z";
        else if (swizzle == ".wwww") swizzle = ".w";
        else if (swizzle == ".xyzw"sv) swizzle.clear();

        const std::string cond = std::string(cond_string_table[d0.cond]) + swizzle;
        AddCodeDisasm(dst_str + "(" + cond + ") , " + src_str + ";");
    }

    void SetDSTDisasm(bool is_sca, const std::string& value)
    {
        is_sca ? AddScaCodeDisasm() : AddVecCodeDisasm();
        if (d0.cond == 0) return;

        if (d0.staturate)
        {
            if (!m_arb_shader.empty() && m_arb_shader.back() == ' ') m_arb_shader.pop_back();
            m_arb_shader += "_sat ";
        }

        std::string dest;
        if (d0.cond_update_enable_0 && d0.cond_update_enable_1)
        {
            if (!m_arb_shader.empty() && m_arb_shader.back() == ' ') m_arb_shader.pop_back();
            m_arb_shader += "C ";
            dest = "RC" + GetMaskDisasm(is_sca);
        }
        else if (d3.dst != 0x1f || (is_sca ? d3.sca_dst_tmp != 0x3f : d0.dst_tmp != 0x3f))
        {
            dest = GetDSTDisasm(is_sca);
        }

        AddCodeCondDisasm(FormatDisasm(dest), value);
    }

    void SetDSTVecDisasm(const std::string& code) { SetDSTDisasm(false, code); }
    void SetDSTScaDisasm(const std::string& code) { SetDSTDisasm(true, code); }

    void TaskVP()
    {
        m_instr_count = 0;
        bool is_has_BRA = false;

        for (u32 i = 1; m_instr_count < m_max_instr_count && i < m_data.size(); m_instr_count++)
        {
            if (is_has_BRA)
            {
                d3.HEX = m_data[i];
                i += 4;
            }
            else
            {
                d1.HEX = m_data[i++];
                m_sca_opcode = d1.sca_opcode;
                switch (d1.sca_opcode)
                {
                case 0x08: // BRA
                    is_has_BRA = true;
                    if (i < m_data.size()) d3.HEX = m_data[++i];
                    i += 4;
                    AddScaCodeDisasm("# WARNING");
                    break;
                case 0x09: // BRI
                    if (i < m_data.size()) d2.HEX = m_data[i++];
                    if (i < m_data.size()) d3.HEX = m_data[i];
                    i += 2;
                    AddScaCodeDisasm("$ifcond # WARNING");
                    break;
                default:
                    if (i < m_data.size()) d3.HEX = m_data[++i];
                    i += 2;
                    break;
                }
            }

            if (d3.end)
            {
                m_instr_count++;
                break;
            }
        }

        for (u32 i = 0; i < m_instr_count && (i * 4 + 3) < m_data.size(); ++i)
        {
            d0.HEX = m_data[i * 4 + 0];
            d1.HEX = m_data[i * 4 + 1];
            d2.HEX = m_data[i * 4 + 2];
            d3.HEX = m_data[i * 4 + 3];

            src[0].src0l = d2.src0l;
            src[0].src0h = d1.src0h;
            src[1].src1  = d2.src1;
            src[2].src2l = d3.src2l;
            src[2].src2h = d2.src2h;

            m_sca_opcode = d1.sca_opcode;
            switch (d1.sca_opcode)
            {
            case RSX_SCA_OPCODE_NOP: break;
            case RSX_SCA_OPCODE_MOV: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_RCP: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_RCC: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_RSQ: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_EXP: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_LOG: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_LIT: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_BRA: AddScaCodeDisasm("BRA # WARNING"); break;
            case RSX_SCA_OPCODE_BRI: AddCodeDisasm("$ifcond # WARNING"); break;
            case RSX_SCA_OPCODE_CAL: AddCodeDisasm("$ifcond $f# WARNING"); break;
            case RSX_SCA_OPCODE_CLI: AddCodeDisasm("$ifcond $f # WARNING"); break;
            case RSX_SCA_OPCODE_RET: AddCodeDisasm("$ifcond # WARNING"); break;
            case RSX_SCA_OPCODE_LG2: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_EX2: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_SIN: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_COS: SetDSTScaDisasm("$s"); break;
            case RSX_SCA_OPCODE_BRB: SetDSTScaDisasm("# WARNING Boolean constant"); break;
            case RSX_SCA_OPCODE_CLB: SetDSTScaDisasm("# WARNING Boolean constant"); break;
            case RSX_SCA_OPCODE_PSH: SetDSTScaDisasm(""); break;
            case RSX_SCA_OPCODE_POP: SetDSTScaDisasm(""); break;
            default:
                m_arb_shader += "# Unknown VP sca_opcode: 0x" + std::to_string(d1.sca_opcode) + "\n";
                break;
            }

            m_vec_opcode = d1.vec_opcode;
            switch (d1.vec_opcode)
            {
            case RSX_VEC_OPCODE_NOP: break;
            case RSX_VEC_OPCODE_MOV: SetDSTVecDisasm("$0"); break;
            case RSX_VEC_OPCODE_MUL: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_ADD: SetDSTVecDisasm("$0, $2"); break;
            case RSX_VEC_OPCODE_MAD: SetDSTVecDisasm("$0, $1, $2"); break;
            case RSX_VEC_OPCODE_DP3: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_DPH: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_DP4: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_DST: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_MIN: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_MAX: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_SLT: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_SGE: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_ARL: AddCodeDisasm("ARL, $a, $0"); break;
            case RSX_VEC_OPCODE_FRC: SetDSTVecDisasm("$0"); break;
            case RSX_VEC_OPCODE_FLR: SetDSTVecDisasm("$0"); break;
            case RSX_VEC_OPCODE_SEQ: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_SFL: SetDSTVecDisasm("$0"); break;
            case RSX_VEC_OPCODE_SGT: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_SLE: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_SNE: SetDSTVecDisasm("$0, $1"); break;
            case RSX_VEC_OPCODE_STR: SetDSTVecDisasm("$0"); break;
            case RSX_VEC_OPCODE_SSG: SetDSTVecDisasm("$0"); break;
            case RSX_VEC_OPCODE_TXL: SetDSTVecDisasm("$t, $0"); break;
            default:
                m_arb_shader += "# Unknown VP vec_opcode: 0x" + std::to_string(d1.vec_opcode) + "\n";
                break;
            }
        }

        m_arb_shader += "END\n";
    }
};

static void PrintUsage(const char* prog_name)
{
    std::cout << "Sony PlayStation 3 RSX Cg / PSL1GHT Binary Shader Disassembler (VPO/FPO -> ASM)\n"
              << "Usage: " << prog_name << " [options] <input_file> [output_file]\n\n"
              << "Options:\n"
              << "  -o, --output <file>  Specify output assembly file path\n"
              << "  -s, --stdout         Print disassembly directly to standard output\n"
              << "  -h, --help           Display this help message and exit\n"
              << "  -v, --version        Show version information\n\n"
              << "Supported input formats:\n"
              << "  *.vpo  (SCE RSX Vertex Program Object / PSL1GHT 'VP')\n"
              << "  *.fpo  (SCE RSX Fragment Program Object / PSL1GHT 'FP')\n"
              << "  *.bin  (Raw binary shader dumps)\n\n"
              << "Examples:\n"
              << "  " << prog_name << " shader.vpo -o shader.asm\n"
              << "  " << prog_name << " fragment.fpo --stdout\n"
              << "  " << prog_name << " rsxrt.fpo rsxrt.asm\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string input_path;
    std::string output_path;
    bool write_stdout = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--version")
        {
            std::cout << "cg_disasm 1.1.0 (Sony Cg & PSL1GHT Universal RSX Engine)\n";
            return 0;
        }
        else if (arg == "-s" || arg == "--stdout")
        {
            write_stdout = true;
        }
        else if (arg == "-o" || arg == "--output")
        {
            if (i + 1 < argc)
            {
                output_path = argv[++i];
            }
            else
            {
                std::cerr << "Error: Option " << arg << " requires a file argument.\n";
                return 1;
            }
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
        else
        {
            if (input_path.empty())
            {
                input_path = arg;
            }
            else if (output_path.empty())
            {
                output_path = arg;
            }
            else
            {
                std::cerr << "Error: Unexpected positional argument: " << arg << "\n";
                return 1;
            }
        }
    }

    if (input_path.empty())
    {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    std::ifstream file(input_path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Error: Failed to open input file: " << input_path << "\n";
        return 1;
    }

    const std::streamsize file_size = file.tellg();
    if (file_size <= 0)
    {
        std::cerr << "Error: Input file is empty: " << input_path << "\n";
        return 1;
    }

    file.seekg(0, std::ios::beg);
    std::vector<u8> buffer(static_cast<size_t>(file_size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size))
    {
        std::cerr << "Error: Failed to read file data: " << input_path << "\n";
        return 1;
    }
    file.close();

    CgBinaryDisasmEngine engine(std::move(buffer));
    if (!engine.BuildDisassembly())
    {
        std::cerr << "Disassembly warning or partial decoding for " << input_path << "\n";
    }

    const std::string& asm_output = engine.GetArbShader();

    if (write_stdout)
    {
        std::cout << asm_output;
        return 0;
    }

    if (output_path.empty())
    {
        fs::path in_p(input_path);
        in_p.replace_extension(".asm");
        output_path = in_p.string();
    }

    std::ofstream out_file(output_path);
    if (!out_file.is_open())
    {
        std::cerr << "Error: Failed to open output file for writing: " << output_path << "\n";
        return 1;
    }

    out_file << asm_output;
    out_file.close();

    std::cout << "Successfully disassembled " << input_path << " -> " << output_path << "\n";
    return 0;
}
