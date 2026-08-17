/*
 * PS3 OpenGraphics Toolkit - rsxcomp
 * Offline SPIR-V Binary Parser & RSX Intermediate Representation (IR) Emitter
 *
 * Based on the SPIR-V Specification and Open Standard defined by the Khronos Group.
 * Copyright (C) Khronos Group Inc.
 *
 * Translates standard SPIR-V bytecodes, Type systems, OpDecorate uniform/attribute
 * bindings, OpVectorShuffle swizzles, and GLSL.std.450 extended mathematical instructions
 * directly into RSX compiler IR intermediate instructions for native NV40 emission.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rsx_compiler.h"
#include "nvfx_shader.h"

#define SPV_MAGIC_NUMBER   0x07230203
#define SPV_MAGIC_REVERSED 0x03022307

/* SPIR-V Opcode constants */
#define SpvOpName                   5
#define SpvOpMemberName             6
#define SpvOpExtInstImport          11
#define SpvOpExtInst                12
#define SpvOpTypeVoid               19
#define SpvOpTypeBool               20
#define SpvOpTypeInt                21
#define SpvOpTypeFloat              22
#define SpvOpTypeVector             23
#define SpvOpTypeMatrix             24
#define SpvOpTypeStruct             30
#define SpvOpTypePointer            32
#define SpvOpConstantTrue           41
#define SpvOpConstantFalse          42
#define SpvOpConstant               43
#define SpvOpConstantComposite      44
#define SpvOpConstantNull           46
#define SpvOpVariable               59
#define SpvOpLoad                   61
#define SpvOpStore                  62
#define SpvOpAccessChain            65
#define SpvOpDecorate               71
#define SpvOpMemberDecorate         72
#define SpvOpVectorShuffle          79
#define SpvOpCompositeConstruct     80
#define SpvOpCompositeExtract       81
#define SpvOpImageSampleImplicitLod 87
#define SpvOpFNegate                127
#define SpvOpFAdd                   129
#define SpvOpFSub                   131
#define SpvOpFMul                   133
#define SpvOpFDiv                   136
#define SpvOpDot                    148
#define SpvOpTranspose              84
#define SpvOpVectorTimesScalar      142
#define SpvOpVectorTimesMatrix      144
#define SpvOpMatrixTimesVector      145
#define SpvOpMatrixTimesMatrix      146
#define SpvOpAny                    154
#define SpvOpAll                    155
#define SpvOpLogicalEqual           164
#define SpvOpLogicalNotEqual        165
#define SpvOpLogicalOr              166
#define SpvOpLogicalAnd             167
#define SpvOpLogicalNot             168
#define SpvOpSelect                 169
#define SpvOpIEqual                 170
#define SpvOpINotEqual              171
#define SpvOpFOrdEqual              180
#define SpvOpFOrdNotEqual           182
#define SpvOpFUnordNotEqual         183
#define SpvOpFOrdLessThan           184
#define SpvOpFOrdGreaterThan        186
#define SpvOpFOrdLessThanEqual      188
#define SpvOpFOrdGreaterThanEqual   190
#define SpvOpDPdx                   207
#define SpvOpDPdy                   208
#define SpvOpFwidth                 209
#define SpvOpDPdxFine               210
#define SpvOpDPdyFine               211
#define SpvOpFwidthFine             212
#define SpvOpDPdxCoarse             213
#define SpvOpDPdyCoarse             214
#define SpvOpFwidthCoarse           215
#define SpvOpPhi                    245
#define SpvOpSelectionMerge         247
#define SpvOpLabel                  248
#define SpvOpBranch                 249
#define SpvOpBranchConditional      250
#define SpvOpKill                   252
#define SpvOpDemoteToHelperInvocationEXT 5380

/* GLSL.std.450 Extended Instruction IDs */
#define GLSLstd450Round       1
#define GLSLstd450RoundEven   2
#define GLSLstd450Trunc       3
#define GLSLstd450FAbs        4
#define GLSLstd450FSign       6
#define GLSLstd450Radians     11
#define GLSLstd450Degrees     12
#define GLSLstd450Sin         13
#define GLSLstd450Cos         14
#define GLSLstd450Tan         15
#define GLSLstd450Asin        16
#define GLSLstd450Acos        17
#define GLSLstd450Atan        18
#define GLSLstd450Atan2       25
#define GLSLstd450Pow         26
#define GLSLstd450Exp         27
#define GLSLstd450Log         28
#define GLSLstd450Exp2        29
#define GLSLstd450Log2        30
#define GLSLstd450Sqrt        31
#define GLSLstd450InverseSqrt 32
#define GLSLstd450Floor       8
#define GLSLstd450Ceil        9
#define GLSLstd450Fract       10
#define GLSLstd450FMin        37
#define GLSLstd450FMax        40
#define GLSLstd450FClamp      43
#define GLSLstd450FMix        46
#define GLSLstd450Step        48
#define GLSLstd450SmoothStep  49
#define GLSLstd450Length      66
#define GLSLstd450Distance    67
#define GLSLstd450Cross       68
#define GLSLstd450Normalize   69
#define GLSLstd450FaceForward 70
#define GLSLstd450Reflect     71
#define GLSLstd450Refract     72

typedef struct {
    uint8_t base_type;    /* 1 = float, 2 = int, 3 = bool, 4 = struct, 5 = pointer */
    uint8_t vec_size;     /* 1, 2, 3, 4 */
    uint32_t subtype_id;  /* pointee or component type */
} SpvTypeInfo;

typedef struct {
    uint32_t id;
    uint32_t type_id;
    uint8_t vec_size;
    uint8_t file;          /* NVFXSR_INPUT, NVFXSR_OUTPUT, NVFXSR_TEMP, NVFXSR_CONST, NVFXSR_IMM */
    int16_t index;
    uint8_t storage_class; /* 1 = Input, 2 = Uniform, 3 = Output, 7 = Function */
    char name[64];
    float const_val[4];
    uint32_t raw_int_val;
    uint32_t composite_ids[4];
    uint8_t composite_count;
    uint32_t use_count;
    bool is_live;
    bool is_permanent;
} SpvVar;

typedef struct {
    char member_names[32][64];
    uint32_t member_count;
} SpvStructInfo;

static int alloc_temp_reg(uint64_t *free_mask, int *max_used) {
    for (int i = 1; i < 48; i++) {
        if (*free_mask & (1ULL << i)) {
            *free_mask &= ~(1ULL << i);
            if (i > *max_used) *max_used = i;
            return i;
        }
    }
    return 1;
}

static inline void setup_src_swizzle(rsxIRSrc *src, const SpvVar *var) {
    if (!src) return;
    if (var && var->vec_size == 1) {
        for (int k = 0; k < 4; k++) src->swizzle[k] = 0; /* Broadcast scalar across xyzw */
    } else {
        for (int k = 0; k < 4; k++) src->swizzle[k] = k;
    }
}

static void release_var_if_dead(SpvVar *v, uint32_t current_inst, uint64_t *free_mask) {
    (void)current_inst;
    if (!v || v->is_permanent) return;
    if (v->use_count > 0) {
        v->use_count--;
    }
    if (v->use_count == 0 && v->file == NVFXSR_TEMP && v->is_live) {
        if (v->index >= 1 && v->index < 48) {
            *free_mask |= (1ULL << v->index);
        }
        v->is_live = false;
    }
}

static int get_or_add_const(rsxCompilerContext *ctx, float x, float y, float z, float w) {
    for (uint32_t i = 0; i < ctx->num_constants; i++) {
        rsxCompilerConst *c = &ctx->constants[i];
        if (c->values[0] == x && c->values[1] == y && c->values[2] == z && c->values[3] == w) {
            return (int)i;
        }
    }
    if (ctx->num_constants < RSX_MAX_CONSTANTS) {
        int idx = (int)ctx->num_constants++;
        rsxCompilerConst *c = &ctx->constants[idx];
        memset(c, 0, sizeof(*c));
        c->index = idx;
        c->type = PARAM_FLOAT4;
        c->is_internal = true;
        c->values[0] = x;
        c->values[1] = y;
        c->values[2] = z;
        c->values[3] = w;
        snprintf(c->name, sizeof(c->name), "imm%d", idx);
        return idx;
    }
    return 0;
}

static bool is_reachable_block(uint32_t start, uint32_t target, uint32_t stop, const uint32_t *succ1, const uint32_t *succ2, uint32_t bound) {
    if (start == 0 || target == 0 || start >= bound || target >= bound) return false;
    if (start == target) return true;
    if (start == stop) return false;

    uint8_t *visited = (uint8_t*)calloc(bound, sizeof(uint8_t));
    if (!visited) return false;
    uint32_t *queue = (uint32_t*)malloc(bound * sizeof(uint32_t));
    if (!queue) { free(visited); return false; }

    int q_head = 0, q_tail = 0;
    queue[q_tail++] = start;
    visited[start] = 1;
    bool found = false;

    while (q_head < q_tail) {
        uint32_t curr = queue[q_head++];
        if (curr == target) {
            found = true;
            break;
        }
        if (curr == stop) continue;

        uint32_t s1 = (curr < bound) ? succ1[curr] : 0;
        if (s1 && s1 < bound && !visited[s1]) {
            visited[s1] = 1;
            queue[q_tail++] = s1;
        }
        uint32_t s2 = (curr < bound) ? succ2[curr] : 0;
        if (s2 && s2 < bound && !visited[s2]) {
            visited[s2] = 1;
            queue[q_tail++] = s2;
        }
    }

    free(queue);
    free(visited);
    return found;
}

bool rsx_compiler_parse_spirv(rsxCompilerContext *ctx, const uint32_t *spv_words, size_t word_count) {
    if (!ctx || !spv_words || word_count < 5) return false;

    uint32_t magic = spv_words[0];
    bool swap_endian = (magic == SPV_MAGIC_REVERSED);
    if (magic != SPV_MAGIC_NUMBER && magic != SPV_MAGIC_REVERSED) {
        return false;
    }

    uint32_t bound = swap_endian ? __builtin_bswap32(spv_words[3]) : spv_words[3];
    SpvVar *vars = (SpvVar*)calloc(bound, sizeof(SpvVar));
    SpvTypeInfo *types = (SpvTypeInfo*)calloc(bound, sizeof(SpvTypeInfo));
    SpvStructInfo *structs = (SpvStructInfo*)calloc(bound, sizeof(SpvStructInfo));
    uint32_t *block_cond = (uint32_t*)calloc(bound, sizeof(uint32_t));
    bool *block_is_true = (bool*)calloc(bound, sizeof(bool));
    uint32_t *merge_cond = (uint32_t*)calloc(bound, sizeof(uint32_t));
    uint32_t *merge_true_b = (uint32_t*)calloc(bound, sizeof(uint32_t));
    uint32_t *merge_false_b = (uint32_t*)calloc(bound, sizeof(uint32_t));
    uint32_t *succ1 = (uint32_t*)calloc(bound, sizeof(uint32_t));
    uint32_t *succ2 = (uint32_t*)calloc(bound, sizeof(uint32_t));
    if (!vars || !types || !structs || !block_cond || !block_is_true || !merge_cond || !merge_true_b || !merge_false_b || !succ1 || !succ2) {
        if (vars) free(vars);
        if (types) free(types);
        if (structs) free(structs);
        if (block_cond) free(block_cond);
        if (block_is_true) free(block_is_true);
        if (merge_cond) free(merge_cond);
        if (merge_true_b) free(merge_true_b);
        if (merge_false_b) free(merge_false_b);
        if (succ1) free(succ1);
        if (succ2) free(succ2);
        return false;
    }

    /* PASS 1: Calculate reference count for each SSA ID and map block branch conditions */
    size_t idx = 5;
    uint32_t cur_block = 0;
    uint32_t pending_merge_block = 0;
    while (idx < word_count) {
        uint32_t word = swap_endian ? __builtin_bswap32(spv_words[idx]) : spv_words[idx];
        uint16_t word_len = (word >> 16) & 0xFFFF;
        uint16_t opcode = word & 0xFFFF;

        if (word_len == 0 || idx + word_len > word_count) break;
        const uint32_t *inst = &spv_words[idx];

        if (opcode == SpvOpLabel) {
            uint32_t label_id = (word_len >= 2) ? (swap_endian ? __builtin_bswap32(inst[1]) : inst[1]) : 0;
            cur_block = label_id;
        } else if (opcode == SpvOpBranch) {
            uint32_t target_b = (word_len >= 2) ? (swap_endian ? __builtin_bswap32(inst[1]) : inst[1]) : 0;
            if (cur_block < bound) succ1[cur_block] = target_b;
            if (target_b < bound && cur_block < bound && block_cond[target_b] == 0) {
                block_cond[target_b] = block_cond[cur_block];
                block_is_true[target_b] = block_is_true[cur_block];
            }
        } else if (opcode == SpvOpSelectionMerge) {
            uint32_t merge_b = (word_len >= 2) ? (swap_endian ? __builtin_bswap32(inst[1]) : inst[1]) : 0;
            pending_merge_block = merge_b;
        } else if (opcode == SpvOpBranchConditional) {
            uint32_t cond = (word_len >= 2) ? (swap_endian ? __builtin_bswap32(inst[1]) : inst[1]) : 0;
            uint32_t true_b = (word_len >= 3) ? (swap_endian ? __builtin_bswap32(inst[2]) : inst[2]) : 0;
            uint32_t false_b = (word_len >= 4) ? (swap_endian ? __builtin_bswap32(inst[3]) : inst[3]) : 0;
            if (cur_block < bound) {
                succ1[cur_block] = true_b;
                succ2[cur_block] = false_b;
            }
            if (pending_merge_block < bound && pending_merge_block > 0) {
                merge_cond[pending_merge_block] = cond;
                merge_true_b[pending_merge_block] = true_b;
                merge_false_b[pending_merge_block] = false_b;
                pending_merge_block = 0;
            }
            if (true_b < bound) { block_cond[true_b] = cond; block_is_true[true_b] = true; }
            if (false_b < bound) { block_cond[false_b] = cond; block_is_true[false_b] = false; }
            if (cond < bound) vars[cond].use_count++;
        } else if (opcode == SpvOpPhi) {
            uint32_t cond = (cur_block < bound && merge_cond[cur_block] != 0) ? merge_cond[cur_block] : ((cur_block < bound) ? block_cond[cur_block] : 0);
            if (cond < bound) vars[cond].use_count++;
            for (uint32_t w = 3; w < word_len; w += 2) {
                uint32_t ref_id = swap_endian ? __builtin_bswap32(inst[w]) : inst[w];
                if (ref_id < bound) vars[ref_id].use_count++;
            }
        } else if (opcode >= SpvOpFNegate && opcode <= SpvOpFOrdGreaterThanEqual) {
            for (uint32_t w = 3; w < word_len; w++) {
                uint32_t ref_id = swap_endian ? __builtin_bswap32(inst[w]) : inst[w];
                if (ref_id < bound) vars[ref_id].use_count++;
            }
        } else if (opcode == SpvOpTranspose || opcode == SpvOpAny || opcode == SpvOpAll) {
            for (uint32_t w = 3; w < word_len; w++) {
                uint32_t ref_id = swap_endian ? __builtin_bswap32(inst[w]) : inst[w];
                if (ref_id < bound) vars[ref_id].use_count++;
            }
        } else if (opcode == SpvOpStore) {
            uint32_t obj_id = (word_len >= 3) ? (swap_endian ? __builtin_bswap32(inst[2]) : inst[2]) : 0;
            if (obj_id < bound) vars[obj_id].use_count++;
        } else if (opcode == SpvOpCompositeConstruct) {
            for (uint32_t w = 3; w < word_len; w++) {
                uint32_t ref_id = swap_endian ? __builtin_bswap32(inst[w]) : inst[w];
                if (ref_id < bound) vars[ref_id].use_count++;
            }
        } else if (opcode == SpvOpCompositeExtract) {
            uint32_t comp_id = (word_len >= 4) ? (swap_endian ? __builtin_bswap32(inst[3]) : inst[3]) : 0;
            if (comp_id < bound) vars[comp_id].use_count++;
        } else if (opcode == SpvOpExtInst) {
            for (uint32_t w = 5; w < word_len; w++) {
                uint32_t ref_id = swap_endian ? __builtin_bswap32(inst[w]) : inst[w];
                if (ref_id < bound) vars[ref_id].use_count++;
            }
        } else if (opcode == SpvOpVectorShuffle) {
            uint32_t vec1_id = (word_len >= 4) ? (swap_endian ? __builtin_bswap32(inst[3]) : inst[3]) : 0;
            if (vec1_id < bound) vars[vec1_id].use_count++;
        } else if (opcode == SpvOpSelect) {
            for (uint32_t w = 3; w < word_len; w++) {
                uint32_t ref_id = swap_endian ? __builtin_bswap32(inst[w]) : inst[w];
                if (ref_id < bound) vars[ref_id].use_count++;
            }
        }

        idx += word_len;
    }

    /* PASS 2: Code Generation with Register Reuse Pool (R0 is reserved exclusively for final output color) */
    uint64_t free_temp_mask = ~0ULL & ~(1ULL << 0);
    int max_temp_used = 1;
    int next_input_reg = 0;
    int next_output_reg = 0;
    int next_const_reg = 0;

    uint32_t inst_counter = 0;
    cur_block = 0;
    idx = 5;
    while (idx < word_count) {
        uint32_t word = swap_endian ? __builtin_bswap32(spv_words[idx]) : spv_words[idx];
        uint16_t word_len = (word >> 16) & 0xFFFF;
        uint16_t opcode = word & 0xFFFF;

        if (word_len == 0 || idx + word_len > word_count) break;
        const uint32_t *inst = &spv_words[idx];

        switch (opcode) {
            case SpvOpLabel: {
                if (word_len >= 2) {
                    uint32_t label_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    cur_block = label_id;
                }
                break;
            }

            case SpvOpName: {
                if (word_len >= 3) {
                    uint32_t target_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    const char *name_str = (const char*)&inst[2];
                    if (target_id < bound) {
                        strncpy(vars[target_id].name, name_str, sizeof(vars[target_id].name) - 1);
                    }
                }
                break;
            }

            case SpvOpMemberName: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t member_idx = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    const char *name_str = (const char*)&inst[3];
                    if (type_id < bound && member_idx < 32) {
                        strncpy(structs[type_id].member_names[member_idx], name_str, sizeof(structs[type_id].member_names[member_idx]) - 1);
                        if (member_idx >= structs[type_id].member_count) {
                            structs[type_id].member_count = member_idx + 1;
                        }
                    }
                }
                break;
            }

            case SpvOpTypeFloat: {
                if (word_len >= 2) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    if (res_id < bound) {
                        types[res_id].base_type = 1;
                        types[res_id].vec_size = 1;
                    }
                }
                break;
            }

            case SpvOpTypeInt: {
                if (word_len >= 2) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    if (res_id < bound) {
                        types[res_id].base_type = 2;
                        types[res_id].vec_size = 1;
                    }
                }
                break;
            }

            case SpvOpTypeBool: {
                if (word_len >= 2) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    if (res_id < bound) {
                        types[res_id].base_type = 3;
                        types[res_id].vec_size = 1;
                    }
                }
                break;
            }

            case SpvOpTypeVector: {
                if (word_len >= 4) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t comp_type = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t num_comps = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    if (res_id < bound) {
                        types[res_id].base_type = (comp_type < bound) ? types[comp_type].base_type : 1;
                        types[res_id].vec_size = (num_comps > 4) ? 4 : (uint8_t)num_comps;
                        types[res_id].subtype_id = comp_type;
                    }
                }
                break;
            }

            case SpvOpTypeMatrix: {
                if (word_len >= 4) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t col_type = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t num_cols = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    if (res_id < bound) {
                        types[res_id].base_type = 6; /* matrix */
                        types[res_id].vec_size = (num_cols > 4) ? 4 : (uint8_t)num_cols;
                        types[res_id].subtype_id = col_type;
                    }
                }
                break;
            }

            case SpvOpTypePointer: {
                if (word_len >= 4) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t elem_type = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    if (res_id < bound) {
                        types[res_id].base_type = 5;
                        types[res_id].subtype_id = elem_type;
                        types[res_id].vec_size = (elem_type < bound && types[elem_type].vec_size) ? types[elem_type].vec_size : 1;
                    }
                }
                break;
            }

            case SpvOpTypeStruct: {
                if (word_len >= 2) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    if (res_id < bound) {
                        types[res_id].base_type = 4;
                    }
                }
                break;
            }

            case SpvOpConstantTrue: {
                if (word_len >= 3) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    if (res_id < bound) {
                        int c_idx = get_or_add_const(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 1;
                        vars[res_id].file = NVFXSR_IMM;
                        vars[res_id].index = c_idx;
                        vars[res_id].raw_int_val = 1;
                        for (int k = 0; k < 4; k++) vars[res_id].const_val[k] = 1.0f;
                    }
                }
                break;
            }

            case SpvOpConstantFalse:
            case SpvOpConstantNull: {
                if (word_len >= 3) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    if (res_id < bound) {
                        int c_idx = get_or_add_const(ctx, 0.0f, 0.0f, 0.0f, 0.0f);
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 1;
                        vars[res_id].file = NVFXSR_IMM;
                        vars[res_id].index = c_idx;
                        vars[res_id].raw_int_val = 0;
                        for (int k = 0; k < 4; k++) vars[res_id].const_val[k] = 0.0f;
                    }
                }
                break;
            }

            case SpvOpConstant: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t raw_val = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    float fval;
                    memcpy(&fval, &raw_val, sizeof(float));

                    if (res_id < bound) {
                        int c_idx = get_or_add_const(ctx, fval, fval, fval, fval);
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = 1;
                        vars[res_id].file = NVFXSR_IMM;
                        vars[res_id].index = c_idx;
                        vars[res_id].raw_int_val = raw_val;
                        for (int k = 0; k < 4; k++) vars[res_id].const_val[k] = fval;
                    }
                }
                break;
            }

            case SpvOpConstantComposite: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    if (res_id < bound) {
                        uint32_t num_comps = word_len - 3;
                        if (num_comps > 4) num_comps = 4;
                        float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                        for (uint32_t c_idx = 0; c_idx < num_comps; c_idx++) {
                            uint32_t elem_id = swap_endian ? __builtin_bswap32(inst[3 + c_idx]) : inst[3 + c_idx];
                            if (elem_id < bound) {
                                v[c_idx] = vars[elem_id].const_val[0];
                            }
                        }
                        int c_idx = get_or_add_const(ctx, v[0], v[1], v[2], v[3]);
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : (uint8_t)num_comps;
                        vars[res_id].file = NVFXSR_IMM;
                        vars[res_id].index = c_idx;
                        vars[res_id].composite_count = num_comps;
                        for (int k = 0; k < 4; k++) vars[res_id].const_val[k] = v[k];
                    }
                }
                break;
            }

            case SpvOpVariable: {
                if (word_len >= 4) {
                    uint32_t ptr_type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t storage = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];

                    if (res_id < bound) {
                        uint32_t elem_type_id = (ptr_type_id < bound) ? types[ptr_type_id].subtype_id : 0;
                        vars[res_id].type_id = elem_type_id;
                        vars[res_id].vec_size = (elem_type_id < bound && types[elem_type_id].vec_size) ? types[elem_type_id].vec_size : 4;
                        vars[res_id].storage_class = storage;
                        if (storage == 1) { /* Input */
                            vars[res_id].file = NVFXSR_INPUT;
                            if (ctx->type == RSX_PROGRAM_FRAGMENT) {
                                vars[res_id].index = NVFX_FP_OP_INPUT_SRC_TC(next_input_reg++);
                                ctx->texcoords_mask |= (1 << 0);
                                ctx->texcoord2D_mask |= (1 << 0);
                            } else {
                                vars[res_id].index = (next_input_reg == 0) ? 0 : (7 + next_input_reg);
                                next_input_reg++;
                            }
                            if (ctx->num_attributes < RSX_MAX_ATTRIBUTES) {
                                rsxCompilerAttrib *a = &ctx->attributes[ctx->num_attributes++];
                                a->index = vars[res_id].index;
                                a->type = PARAM_FLOAT4;
                                snprintf(a->name, sizeof(a->name), (a->index == 0) ? "input.pos" : "input.uv");
                            }
                        } else if (storage == 3) { /* Output */
                            vars[res_id].file = NVFXSR_OUTPUT;
                            vars[res_id].index = next_output_reg++;
                        } else if (storage == 2 || storage == 0) { /* Uniform Block */
                            vars[res_id].file = NVFXSR_CONST;
                            vars[res_id].index = next_const_reg++;
                        } else {
                            vars[res_id].file = NVFXSR_TEMP;
                            vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            vars[res_id].is_permanent = true;
                            vars[res_id].is_live = true;
                        }
                    }
                }
                break;
            }

            case SpvOpAccessChain: {
                if (word_len >= 5) {
                    uint32_t res_type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t base_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t index_id = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];

                    if (res_id < bound && base_id < bound) {
                        uint32_t elem_type_id = (res_type_id < bound) ? types[res_type_id].subtype_id : 0;
                        vars[res_id].type_id = elem_type_id;
                        vars[res_id].vec_size = (elem_type_id < bound && types[elem_type_id].vec_size) ? types[elem_type_id].vec_size : 4;

                        if (vars[base_id].file == NVFXSR_CONST || vars[base_id].storage_class == 2 || vars[base_id].storage_class == 0) {
                            /* Resolve member name using struct type ID from variable */
                            uint32_t struct_type = vars[base_id].type_id;
                            uint32_t m_idx = (index_id < bound) ? vars[index_id].raw_int_val : 0;
                            const char *m_name = (struct_type < bound && m_idx < 32) ? structs[struct_type].member_names[m_idx] : NULL;
                            if (!m_name || !*m_name) {
                                static const char *k_default_uniforms[] = {
                                    "cam_pos", "cam_fwd", "cam_right_scaled", "cam_up_scaled", "sphere_pos", "light_pos"
                                };
                                if (m_idx < 6) m_name = k_default_uniforms[m_idx];
                                else m_name = "param";
                            }

                            int found_c_idx = -1;
                            for (uint32_t c_i = 0; c_i < ctx->num_constants; c_i++) {
                                if (strcmp(ctx->constants[c_i].name, m_name) == 0 ||
                                    (strncmp(ctx->constants[c_i].name, m_name, strlen(m_name)) == 0 && ctx->constants[c_i].name[strlen(m_name)] == '[')) {
                                    found_c_idx = (int)c_i;
                                    break;
                                }
                            }

                            if (found_c_idx >= 0) {
                                vars[res_id].file = NVFXSR_CONST;
                                vars[res_id].index = found_c_idx;
                            } else {
                                bool is_mat = (elem_type_id < bound && types[elem_type_id].base_type == 6);
                                vars[res_id].file = NVFXSR_CONST;
                                vars[res_id].index = ctx->num_constants;
                                if (is_mat) {
                                    for (int col = 0; col < 4; col++) {
                                        if (ctx->num_constants < RSX_MAX_CONSTANTS) {
                                            rsxCompilerConst *c = &ctx->constants[ctx->num_constants++];
                                            c->index = ctx->num_constants - 1;
                                            c->type = PARAM_FLOAT4;
                                            c->is_internal = false;
                                            snprintf(c->name, sizeof(c->name), "%s[%d]", m_name, col);
                                        }
                                    }
                                } else {
                                    if (ctx->num_constants < RSX_MAX_CONSTANTS) {
                                        rsxCompilerConst *c = &ctx->constants[ctx->num_constants++];
                                        c->index = vars[res_id].index;
                                        c->type = (vars[res_id].vec_size == 3) ? PARAM_FLOAT3 : PARAM_FLOAT4;
                                        c->is_internal = false;
                                        snprintf(c->name, sizeof(c->name), "%s", m_name);
                                    }
                                }
                            }
                        } else {
                            vars[res_id].file = vars[base_id].file;
                            vars[res_id].index = vars[base_id].index;
                        }
                    }
                }
                break;
            }

            case SpvOpLoad: {
                if (word_len >= 4) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t ptr_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    if (res_id < bound && ptr_id < bound) {
                        vars[res_id] = vars[ptr_id];
                        vars[res_id].is_live = false;
                    }
                }
                break;
            }

            case SpvOpStore: {
                if (word_len >= 3) {
                    uint32_t ptr_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t obj_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];

                    if (ptr_id < bound && obj_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        uint32_t width = vars[ptr_id].vec_size ? vars[ptr_id].vec_size : (vars[obj_id].vec_size ? vars[obj_id].vec_size : 4);
                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->opcode = RSX_IR_OP_MOV;
                        ir->dst.file = vars[ptr_id].file;
                        ir->dst.index = vars[ptr_id].index;
                        ir->dst.writemask = (width >= 4) ? 0xFu : (uint8_t)((1u << width) - 1u);

                        ir->src[0].file = vars[obj_id].file;
                        ir->src[0].index = vars[obj_id].index;
                        for (int k = 0; k < 4; k++) ir->src[0].swizzle[k] = k;

                        inst_counter++;
                        release_var_if_dead(&vars[obj_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpVectorShuffle: {
                if (word_len >= 5) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t vec1_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t num_comps = word_len - 5;

                    if (res_id < bound && vec1_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : (uint8_t)num_comps;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->opcode = RSX_IR_OP_MOV;
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);

                        ir->src[0].file = vars[vec1_id].file;
                        ir->src[0].index = vars[vec1_id].index;
                        for (uint32_t k = 0; k < 4; k++) {
                            if (k < num_comps) {
                                uint32_t c = swap_endian ? __builtin_bswap32(inst[5 + k]) : inst[5 + k];
                                ir->src[0].swizzle[k] = (c < 4) ? (uint8_t)c : 0;
                            } else {
                                ir->src[0].swizzle[k] = (num_comps > 0) ? ir->src[0].swizzle[num_comps - 1] : 0;
                            }
                        }

                        inst_counter++;
                        release_var_if_dead(&vars[vec1_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpCompositeConstruct: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    if (res_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        uint32_t num_args = word_len - 3;
                        uint32_t dst_comp = 0;
                        for (uint32_t a = 0; a < num_args && dst_comp < 4; a++) {
                            uint32_t src_id = swap_endian ? __builtin_bswap32(inst[3 + a]) : inst[3 + a];
                            if (src_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                                uint32_t src_width = vars[src_id].vec_size ? vars[src_id].vec_size : 1;
                                if (src_width > (4 - dst_comp)) src_width = 4 - dst_comp;

                                rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                                memset(ir, 0, sizeof(*ir));
                                ir->opcode = RSX_IR_OP_MOV;
                                ir->dst.file = vars[res_id].file;
                                ir->dst.index = vars[res_id].index;
                                ir->dst.writemask = ((1u << src_width) - 1u) << dst_comp;

                                ir->src[0].file = vars[src_id].file;
                                ir->src[0].index = vars[src_id].index;
                                for (int k = 0; k < 4; k++) {
                                    if (k >= (int)dst_comp && k < (int)(dst_comp + src_width)) {
                                        ir->src[0].swizzle[k] = (uint8_t)(k - dst_comp);
                                    } else {
                                        ir->src[0].swizzle[k] = 0;
                                    }
                                }

                                dst_comp += src_width;
                                release_var_if_dead(&vars[src_id], inst_counter, &free_temp_mask);
                            }
                        }
                        inst_counter++;
                    }
                }
                break;
            }

            case SpvOpCompositeExtract: {
                if (word_len >= 5) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t comp_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t swz_idx = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];

                    if (res_id < bound && comp_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = 1;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->opcode = RSX_IR_OP_MOV;
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = 0xF;

                        ir->src[0].file = vars[comp_id].file;
                        ir->src[0].index = vars[comp_id].index;
                        uint8_t swz = (swz_idx < 4) ? (uint8_t)swz_idx : 0;
                        for (int k = 0; k < 4; k++) ir->src[0].swizzle[k] = swz;

                        inst_counter++;
                        release_var_if_dead(&vars[comp_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpFNegate: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t src_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];

                    if (res_id < bound && src_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->opcode = RSX_IR_OP_MOV;
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);

                        ir->src[0].file = vars[src_id].file;
                        ir->src[0].index = vars[src_id].index;
                        ir->src[0].negate = 1;
                        for (int k = 0; k < 4; k++) ir->src[0].swizzle[k] = k;

                        inst_counter++;
                        release_var_if_dead(&vars[src_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpFAdd:
            case SpvOpFSub:
            case SpvOpFMul:
            case SpvOpVectorTimesScalar:
            case SpvOpDot: {
                if (word_len >= 5) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t op1_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t op2_id = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];

                    if (res_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        if (opcode == SpvOpFAdd) ir->opcode = RSX_IR_OP_ADD;
                        else if (opcode == SpvOpFSub) ir->opcode = RSX_IR_OP_SUB;
                        else if (opcode == SpvOpFMul || opcode == SpvOpVectorTimesScalar) ir->opcode = RSX_IR_OP_MUL;
                        else if (opcode == SpvOpDot) {
                            uint32_t w = (op1_id < bound) ? vars[op1_id].vec_size : 3;
                            if (w == 4) ir->opcode = RSX_IR_OP_DP4;
                            else if (w == 2) ir->opcode = RSX_IR_OP_DP2;
                            else ir->opcode = RSX_IR_OP_DP3;
                        }

                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);

                        ir->src[0].file = vars[op1_id].file;
                        ir->src[0].index = vars[op1_id].index;
                        setup_src_swizzle(&ir->src[0], &vars[op1_id]);

                        ir->src[1].file = vars[op2_id].file;
                        ir->src[1].index = vars[op2_id].index;
                        if (opcode == SpvOpVectorTimesScalar) {
                            for (int k = 0; k < 4; k++) ir->src[1].swizzle[k] = 0;
                        } else {
                            setup_src_swizzle(&ir->src[1], &vars[op2_id]);
                        }

                        inst_counter++;
                        release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpFDiv: {
                if (word_len >= 5) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t op1_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t op2_id = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];

                    if (res_id < bound && ctx->num_instructions + 2 < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        /* rcp_reg = RCP(op2) */
                        int rcp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        rsxIRInstruction *ir_rcp = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_rcp, 0, sizeof(*ir_rcp));
                        ir_rcp->opcode = RSX_IR_OP_RCP;
                        ir_rcp->dst.file = NVFXSR_TEMP;
                        ir_rcp->dst.index = rcp_reg;
                        ir_rcp->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_rcp->src[0].file = vars[op2_id].file;
                        ir_rcp->src[0].index = vars[op2_id].index;
                        setup_src_swizzle(&ir_rcp->src[0], &vars[op2_id]);

                        /* res = op1 * rcp_reg */
                        rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_mul, 0, sizeof(*ir_mul));
                        ir_mul->opcode = RSX_IR_OP_MUL;
                        ir_mul->dst.file = vars[res_id].file;
                        ir_mul->dst.index = vars[res_id].index;
                        ir_mul->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_mul->src[0].file = vars[op1_id].file;
                        ir_mul->src[0].index = vars[op1_id].index;
                        setup_src_swizzle(&ir_mul->src[0], &vars[op1_id]);
                        ir_mul->src[1].file = NVFXSR_TEMP;
                        ir_mul->src[1].index = rcp_reg;
                        for (int k = 0; k < 4; k++) ir_mul->src[1].swizzle[k] = k;

                        if (rcp_reg >= 1 && rcp_reg < 48) free_temp_mask |= (1ULL << rcp_reg);

                        inst_counter++;
                        release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpVectorTimesMatrix:
            case SpvOpMatrixTimesVector: {
                if (word_len >= 5) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t vec_id = (opcode == SpvOpVectorTimesMatrix) ? (swap_endian ? __builtin_bswap32(inst[3]) : inst[3]) : (swap_endian ? __builtin_bswap32(inst[4]) : inst[4]);
                    uint32_t mat_id = (opcode == SpvOpVectorTimesMatrix) ? (swap_endian ? __builtin_bswap32(inst[4]) : inst[4]) : (swap_endian ? __builtin_bswap32(inst[3]) : inst[3]);

                    if (res_id < bound && vec_id < bound && mat_id < bound && ctx->num_instructions + 4 < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        /* Compute 4 DP4 dot products */
                        for (int r = 0; r < 4; r++) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_DP4;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (1u << r);

                            ir->src[0].file = vars[vec_id].file;
                            ir->src[0].index = vars[vec_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[vec_id]);

                            ir->src[1].file = vars[mat_id].file;
                            ir->src[1].index = vars[mat_id].index + r;
                            for (int k = 0; k < 4; k++) ir->src[1].swizzle[k] = k;
                        }

                        inst_counter++;
                        release_var_if_dead(&vars[vec_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[mat_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpExtInst: {
                if (word_len >= 5) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t ext_op = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];

                    if (res_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        uint32_t op1_id = (word_len >= 6) ? (swap_endian ? __builtin_bswap32(inst[5]) : inst[5]) : 0;
                        uint32_t op2_id = (word_len >= 7) ? (swap_endian ? __builtin_bswap32(inst[6]) : inst[6]) : 0;
                        uint32_t op3_id = (word_len >= 8) ? (swap_endian ? __builtin_bswap32(inst[7]) : inst[7]) : 0;

                        if (ext_op == GLSLstd450Normalize && op1_id < bound) {
                            /* v . v -> dot */
                            int dot_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_dot = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_dot, 0, sizeof(*ir_dot));
                            ir_dot->opcode = RSX_IR_OP_DP3;
                            ir_dot->dst.file = NVFXSR_TEMP;
                            ir_dot->dst.index = dot_reg;
                            ir_dot->dst.writemask = 0xF;
                            ir_dot->src[0].file = vars[op1_id].file;
                            ir_dot->src[0].index = vars[op1_id].index;
                            ir_dot->src[1].file = vars[op1_id].file;
                            ir_dot->src[1].index = vars[op1_id].index;
                            for (int k = 0; k < 4; k++) {
                                ir_dot->src[0].swizzle[k] = k;
                                ir_dot->src[1].swizzle[k] = k;
                            }

                            /* rsq(dot) -> rsq */
                            int rsq_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_rsq = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rsq, 0, sizeof(*ir_rsq));
                            ir_rsq->opcode = RSX_IR_OP_RSQ;
                            ir_rsq->dst.file = NVFXSR_TEMP;
                            ir_rsq->dst.index = rsq_reg;
                            ir_rsq->dst.writemask = 0xF;
                            ir_rsq->src[0].file = NVFXSR_TEMP;
                            ir_rsq->src[0].index = dot_reg;
                            for (int k = 0; k < 4; k++) ir_rsq->src[0].swizzle[k] = 0;

                            /* Free temporary dot register */
                            if (dot_reg >= 1 && dot_reg < 48) free_temp_mask |= (1ULL << dot_reg);

                            /* v * rsq -> res */
                            rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mul, 0, sizeof(*ir_mul));
                            ir_mul->opcode = RSX_IR_OP_MUL;
                            ir_mul->dst.file = vars[res_id].file;
                            ir_mul->dst.index = vars[res_id].index;
                            ir_mul->dst.writemask = 0xF;
                            ir_mul->src[0].file = vars[op1_id].file;
                            ir_mul->src[0].index = vars[op1_id].index;
                            ir_mul->src[1].file = NVFXSR_TEMP;
                            ir_mul->src[1].index = rsq_reg;
                            for (int k = 0; k < 4; k++) {
                                ir_mul->src[0].swizzle[k] = k;
                                ir_mul->src[1].swizzle[k] = 0;
                            }

                            /* Free temporary rsq register */
                            if (rsq_reg >= 1 && rsq_reg < 48) free_temp_mask |= (1ULL << rsq_reg);

                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450FAbs && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_MAX;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            ir->src[1].file = vars[op1_id].file;
                            ir->src[1].index = vars[op1_id].index;
                            ir->src[1].negate = true;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            setup_src_swizzle(&ir->src[1], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Floor && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_FLR;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Fract && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_FRC;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Pow && op1_id < bound && op2_id < bound) {
                            int tmp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_lg2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_lg2, 0, sizeof(*ir_lg2));
                            ir_lg2->opcode = RSX_IR_OP_LG2;
                            ir_lg2->dst.file = NVFXSR_TEMP;
                            ir_lg2->dst.index = tmp_reg;
                            ir_lg2->dst.writemask = 0xF;
                            ir_lg2->src[0].file = vars[op1_id].file;
                            ir_lg2->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_lg2->src[0], &vars[op1_id]);

                            rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mul, 0, sizeof(*ir_mul));
                            ir_mul->opcode = RSX_IR_OP_MUL;
                            ir_mul->dst.file = NVFXSR_TEMP;
                            ir_mul->dst.index = tmp_reg;
                            ir_mul->dst.writemask = 0xF;
                            ir_mul->src[0].file = NVFXSR_TEMP;
                            ir_mul->src[0].index = tmp_reg;
                            ir_mul->src[1].file = vars[op2_id].file;
                            ir_mul->src[1].index = vars[op2_id].index;
                            for (int k = 0; k < 4; k++) ir_mul->src[0].swizzle[k] = k;
                            setup_src_swizzle(&ir_mul->src[1], &vars[op2_id]);

                            rsxIRInstruction *ir_ex2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_ex2, 0, sizeof(*ir_ex2));
                            ir_ex2->opcode = RSX_IR_OP_EX2;
                            ir_ex2->dst.file = vars[res_id].file;
                            ir_ex2->dst.index = vars[res_id].index;
                            ir_ex2->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_ex2->src[0].file = NVFXSR_TEMP;
                            ir_ex2->src[0].index = tmp_reg;
                            for (int k = 0; k < 4; k++) ir_ex2->src[0].swizzle[k] = k;

                            if (tmp_reg >= 1 && tmp_reg < 48) free_temp_mask |= (1ULL << tmp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Sqrt && op1_id < bound) {
                            int tmp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_rsq = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rsq, 0, sizeof(*ir_rsq));
                            ir_rsq->opcode = RSX_IR_OP_RSQ;
                            ir_rsq->dst.file = NVFXSR_TEMP;
                            ir_rsq->dst.index = tmp_reg;
                            ir_rsq->dst.writemask = 0xF;
                            ir_rsq->src[0].file = vars[op1_id].file;
                            ir_rsq->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_rsq->src[0], &vars[op1_id]);

                            rsxIRInstruction *ir_rcp = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rcp, 0, sizeof(*ir_rcp));
                            ir_rcp->opcode = RSX_IR_OP_RCP;
                            ir_rcp->dst.file = vars[res_id].file;
                            ir_rcp->dst.index = vars[res_id].index;
                            ir_rcp->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_rcp->src[0].file = NVFXSR_TEMP;
                            ir_rcp->src[0].index = tmp_reg;
                            for (int k = 0; k < 4; k++) ir_rcp->src[0].swizzle[k] = k;

                            if (tmp_reg >= 1 && tmp_reg < 48) free_temp_mask |= (1ULL << tmp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450InverseSqrt && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_RSQ;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Cos && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_COS;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Sin && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_SIN;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450FMax && op1_id < bound && op2_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_MAX;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            ir->src[1].file = vars[op2_id].file;
                            ir->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op2_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450FMin && op1_id < bound && op2_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_MIN;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            ir->src[1].file = vars[op2_id].file;
                            ir->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op2_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450FClamp && op1_id < bound && op2_id < bound && op3_id < bound) {
                            int tmp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_max = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_max, 0, sizeof(*ir_max));
                            ir_max->opcode = RSX_IR_OP_MAX;
                            ir_max->dst.file = NVFXSR_TEMP;
                            ir_max->dst.index = tmp_reg;
                            ir_max->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_max->src[0].file = vars[op1_id].file;
                            ir_max->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_max->src[0], &vars[op1_id]);
                            ir_max->src[1].file = vars[op2_id].file;
                            ir_max->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_max->src[1], &vars[op2_id]);

                            rsxIRInstruction *ir_min = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_min, 0, sizeof(*ir_min));
                            ir_min->opcode = RSX_IR_OP_MIN;
                            ir_min->dst.file = vars[res_id].file;
                            ir_min->dst.index = vars[res_id].index;
                            ir_min->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_min->src[0].file = NVFXSR_TEMP;
                            ir_min->src[0].index = tmp_reg;
                            for (int k = 0; k < 4; k++) ir_min->src[0].swizzle[k] = k;
                            ir_min->src[1].file = vars[op3_id].file;
                            ir_min->src[1].index = vars[op3_id].index;
                            setup_src_swizzle(&ir_min->src[1], &vars[op3_id]);

                            if (tmp_reg >= 1 && tmp_reg < 48) free_temp_mask |= (1ULL << tmp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op3_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Step && op1_id < bound && op2_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_SGE;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op2_id].file;
                            ir->src[0].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op2_id]);
                            ir->src[1].file = vars[op1_id].file;
                            ir->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450FMix && op1_id < bound && op2_id < bound && op3_id < bound) {
                            int diff_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_sub = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_sub, 0, sizeof(*ir_sub));
                            ir_sub->opcode = RSX_IR_OP_ADD;
                            ir_sub->dst.file = NVFXSR_TEMP;
                            ir_sub->dst.index = diff_reg;
                            ir_sub->dst.writemask = 0xF;
                            ir_sub->src[0].file = vars[op2_id].file;
                            ir_sub->src[0].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_sub->src[0], &vars[op2_id]);

                            ir_sub->src[1].file = vars[op1_id].file;
                            ir_sub->src[1].index = vars[op1_id].index;
                            ir_sub->src[1].negate = true;
                            setup_src_swizzle(&ir_sub->src[1], &vars[op1_id]);

                            rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mad, 0, sizeof(*ir_mad));
                            ir_mad->opcode = RSX_IR_OP_MAD;
                            ir_mad->dst.file = vars[res_id].file;
                            ir_mad->dst.index = vars[res_id].index;
                            ir_mad->dst.writemask = 0xF;
                            ir_mad->src[0].file = vars[op3_id].file;
                            ir_mad->src[0].index = vars[op3_id].index;
                            setup_src_swizzle(&ir_mad->src[0], &vars[op3_id]);

                            ir_mad->src[1].file = NVFXSR_TEMP;
                            ir_mad->src[1].index = diff_reg;
                            for (int k = 0; k < 4; k++) ir_mad->src[1].swizzle[k] = k;

                            ir_mad->src[2].file = vars[op1_id].file;
                            ir_mad->src[2].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_mad->src[2], &vars[op1_id]);

                            if (diff_reg >= 1 && diff_reg < 48) free_temp_mask |= (1ULL << diff_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op3_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Length && op1_id < bound) {
                            int dot_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            if (vars[op1_id].vec_size == 2) {
                                int mul_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                                rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                                memset(ir_mul, 0, sizeof(*ir_mul));
                                ir_mul->opcode = RSX_IR_OP_MUL;
                                ir_mul->dst.file = NVFXSR_TEMP;
                                ir_mul->dst.index = mul_reg;
                                ir_mul->dst.writemask = 0x3;
                                ir_mul->src[0].file = vars[op1_id].file;
                                ir_mul->src[0].index = vars[op1_id].index;
                                setup_src_swizzle(&ir_mul->src[0], &vars[op1_id]);
                                ir_mul->src[1].file = vars[op1_id].file;
                                ir_mul->src[1].index = vars[op1_id].index;
                                setup_src_swizzle(&ir_mul->src[1], &vars[op1_id]);

                                rsxIRInstruction *ir_add = &ctx->instructions[ctx->num_instructions++];
                                memset(ir_add, 0, sizeof(*ir_add));
                                ir_add->opcode = RSX_IR_OP_ADD;
                                ir_add->dst.file = NVFXSR_TEMP;
                                ir_add->dst.index = dot_reg;
                                ir_add->dst.writemask = 0xF;
                                ir_add->src[0].file = NVFXSR_TEMP;
                                ir_add->src[0].index = mul_reg;
                                for (int k = 0; k < 4; k++) ir_add->src[0].swizzle[k] = 0;
                                ir_add->src[1].file = NVFXSR_TEMP;
                                ir_add->src[1].index = mul_reg;
                                for (int k = 0; k < 4; k++) ir_add->src[1].swizzle[k] = 1;
                                if (mul_reg >= 1 && mul_reg < 48) free_temp_mask |= (1ULL << mul_reg);
                            } else {
                                rsxIRInstruction *ir_dot = &ctx->instructions[ctx->num_instructions++];
                                memset(ir_dot, 0, sizeof(*ir_dot));
                                ir_dot->opcode = (vars[op1_id].vec_size >= 4) ? RSX_IR_OP_DP4 : RSX_IR_OP_DP3;
                                ir_dot->dst.file = NVFXSR_TEMP;
                                ir_dot->dst.index = dot_reg;
                                ir_dot->dst.writemask = 0xF;
                                ir_dot->src[0].file = vars[op1_id].file;
                                ir_dot->src[0].index = vars[op1_id].index;
                                ir_dot->src[1].file = vars[op1_id].file;
                                ir_dot->src[1].index = vars[op1_id].index;
                                for (int k = 0; k < 4; k++) {
                                    ir_dot->src[0].swizzle[k] = k;
                                    ir_dot->src[1].swizzle[k] = k;
                                }
                            }

                            int rsq_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_rsq = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rsq, 0, sizeof(*ir_rsq));
                            ir_rsq->opcode = RSX_IR_OP_RSQ;
                            ir_rsq->dst.file = NVFXSR_TEMP;
                            ir_rsq->dst.index = rsq_reg;
                            ir_rsq->dst.writemask = 0xF;
                            ir_rsq->src[0].file = NVFXSR_TEMP;
                            ir_rsq->src[0].index = dot_reg;
                            for (int k = 0; k < 4; k++) ir_rsq->src[0].swizzle[k] = 0;

                            if (dot_reg >= 1 && dot_reg < 48) free_temp_mask |= (1ULL << dot_reg);

                            rsxIRInstruction *ir_rcp = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rcp, 0, sizeof(*ir_rcp));
                            ir_rcp->opcode = RSX_IR_OP_RCP;
                            ir_rcp->dst.file = vars[res_id].file;
                            ir_rcp->dst.index = vars[res_id].index;
                            ir_rcp->dst.writemask = 0xF;
                            ir_rcp->src[0].file = NVFXSR_TEMP;
                            ir_rcp->src[0].index = rsq_reg;
                            for (int k = 0; k < 4; k++) ir_rcp->src[0].swizzle[k] = 0;

                            if (rsq_reg >= 1 && rsq_reg < 48) free_temp_mask |= (1ULL << rsq_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Cross && op1_id < bound && op2_id < bound) {
                            int tmp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mul, 0, sizeof(*ir_mul));
                            ir_mul->opcode = RSX_IR_OP_MUL;
                            ir_mul->dst.file = NVFXSR_TEMP;
                            ir_mul->dst.index = tmp_reg;
                            ir_mul->dst.writemask = 0x7;
                            ir_mul->src[0].file = vars[op1_id].file;
                            ir_mul->src[0].index = vars[op1_id].index;
                            ir_mul->src[0].swizzle[0] = 2; /* z */
                            ir_mul->src[0].swizzle[1] = 0; /* x */
                            ir_mul->src[0].swizzle[2] = 1; /* y */
                            ir_mul->src[0].swizzle[3] = 3;
                            ir_mul->src[1].file = vars[op2_id].file;
                            ir_mul->src[1].index = vars[op2_id].index;
                            ir_mul->src[1].swizzle[0] = 1; /* y */
                            ir_mul->src[1].swizzle[1] = 2; /* z */
                            ir_mul->src[1].swizzle[2] = 0; /* x */
                            ir_mul->src[1].swizzle[3] = 3;

                            rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mad, 0, sizeof(*ir_mad));
                            ir_mad->opcode = RSX_IR_OP_MAD;
                            ir_mad->dst.file = vars[res_id].file;
                            ir_mad->dst.index = vars[res_id].index;
                            ir_mad->dst.writemask = 0x7;
                            ir_mad->src[0].file = vars[op1_id].file;
                            ir_mad->src[0].index = vars[op1_id].index;
                            ir_mad->src[0].swizzle[0] = 1; /* y */
                            ir_mad->src[0].swizzle[1] = 2; /* z */
                            ir_mad->src[0].swizzle[2] = 0; /* x */
                            ir_mad->src[0].swizzle[3] = 3;
                            ir_mad->src[1].file = vars[op2_id].file;
                            ir_mad->src[1].index = vars[op2_id].index;
                            ir_mad->src[1].swizzle[0] = 2; /* z */
                            ir_mad->src[1].swizzle[1] = 0; /* x */
                            ir_mad->src[1].swizzle[2] = 1; /* y */
                            ir_mad->src[1].swizzle[3] = 3;
                            ir_mad->src[2].file = NVFXSR_TEMP;
                            ir_mad->src[2].index = tmp_reg;
                            ir_mad->src[2].negate = true;
                            for (int k = 0; k < 4; k++) ir_mad->src[2].swizzle[k] = k;

                            if (tmp_reg >= 1 && tmp_reg < 48) free_temp_mask |= (1ULL << tmp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Reflect && op1_id < bound && op2_id < bound) {
                            int dot_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_dot = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_dot, 0, sizeof(*ir_dot));
                            ir_dot->opcode = RSX_IR_OP_DP3;
                            ir_dot->dst.file = NVFXSR_TEMP;
                            ir_dot->dst.index = dot_reg;
                            ir_dot->dst.writemask = 0xF;
                            ir_dot->src[0].file = vars[op2_id].file;
                            ir_dot->src[0].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_dot->src[0], &vars[op2_id]);
                            ir_dot->src[1].file = vars[op1_id].file;
                            ir_dot->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_dot->src[1], &vars[op1_id]);

                            int dot2_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_add = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_add, 0, sizeof(*ir_add));
                            ir_add->opcode = RSX_IR_OP_ADD;
                            ir_add->dst.file = NVFXSR_TEMP;
                            ir_add->dst.index = dot2_reg;
                            ir_add->dst.writemask = 0xF;
                            ir_add->src[0].file = NVFXSR_TEMP;
                            ir_add->src[0].index = dot_reg;
                            ir_add->src[1].file = NVFXSR_TEMP;
                            ir_add->src[1].index = dot_reg;
                            for (int k = 0; k < 4; k++) {
                                ir_add->src[0].swizzle[k] = 0;
                                ir_add->src[1].swizzle[k] = 0;
                            }

                            if (dot_reg >= 1 && dot_reg < 48) free_temp_mask |= (1ULL << dot_reg);

                            rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mad, 0, sizeof(*ir_mad));
                            ir_mad->opcode = RSX_IR_OP_MAD;
                            ir_mad->dst.file = vars[res_id].file;
                            ir_mad->dst.index = vars[res_id].index;
                            ir_mad->dst.writemask = 0xF;
                            ir_mad->src[0].file = NVFXSR_TEMP;
                            ir_mad->src[0].index = dot2_reg;
                            ir_mad->src[0].negate = true;
                            for (int k = 0; k < 4; k++) {
                                ir_mad->src[0].swizzle[k] = 0;
                            }
                            ir_mad->src[1].file = vars[op2_id].file;
                            ir_mad->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_mad->src[1], &vars[op2_id]);
                            ir_mad->src[2].file = vars[op1_id].file;
                            ir_mad->src[2].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_mad->src[2], &vars[op1_id]);

                            if (dot2_reg >= 1 && dot2_reg < 48) free_temp_mask |= (1ULL << dot2_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Ceil && op1_id < bound) {
                            /* ceil(x) = -floor(-x) */
                            int tmp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_flr = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_flr, 0, sizeof(*ir_flr));
                            ir_flr->opcode = RSX_IR_OP_FLR;
                            ir_flr->dst.file = NVFXSR_TEMP;
                            ir_flr->dst.index = tmp_reg;
                            ir_flr->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_flr->src[0].file = vars[op1_id].file;
                            ir_flr->src[0].index = vars[op1_id].index;
                            ir_flr->src[0].negate = true;
                            setup_src_swizzle(&ir_flr->src[0], &vars[op1_id]);

                            rsxIRInstruction *ir_neg = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_neg, 0, sizeof(*ir_neg));
                            ir_neg->opcode = RSX_IR_OP_MOV;
                            ir_neg->dst.file = vars[res_id].file;
                            ir_neg->dst.index = vars[res_id].index;
                            ir_neg->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_neg->src[0].file = NVFXSR_TEMP;
                            ir_neg->src[0].index = tmp_reg;
                            ir_neg->src[0].negate = true;
                            for (int k = 0; k < 4; k++) ir_neg->src[0].swizzle[k] = k;

                            if (tmp_reg >= 1 && tmp_reg < 48) free_temp_mask |= (1ULL << tmp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if ((ext_op == GLSLstd450Round || ext_op == GLSLstd450RoundEven) && op1_id < bound) {
                            /* round(x) = floor(x + 0.5) */
                            int half_const = get_or_add_const(ctx, 0.5f, 0.5f, 0.5f, 0.5f);
                            int tmp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_add = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_add, 0, sizeof(*ir_add));
                            ir_add->opcode = RSX_IR_OP_ADD;
                            ir_add->dst.file = NVFXSR_TEMP;
                            ir_add->dst.index = tmp_reg;
                            ir_add->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_add->src[0].file = vars[op1_id].file;
                            ir_add->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_add->src[0], &vars[op1_id]);
                            ir_add->src[1].file = NVFXSR_CONST;
                            ir_add->src[1].index = half_const;
                            for (int k = 0; k < 4; k++) ir_add->src[1].swizzle[k] = 0;

                            rsxIRInstruction *ir_flr = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_flr, 0, sizeof(*ir_flr));
                            ir_flr->opcode = RSX_IR_OP_FLR;
                            ir_flr->dst.file = vars[res_id].file;
                            ir_flr->dst.index = vars[res_id].index;
                            ir_flr->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_flr->src[0].file = NVFXSR_TEMP;
                            ir_flr->src[0].index = tmp_reg;
                            for (int k = 0; k < 4; k++) ir_flr->src[0].swizzle[k] = k;

                            if (tmp_reg >= 1 && tmp_reg < 48) free_temp_mask |= (1ULL << tmp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Trunc && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_TRUNC;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450FSign && op1_id < bound) {
                            /* sign(x) = (x > 0.0) - (x < 0.0) */
                            int zero_const = get_or_add_const(ctx, 0.0f, 0.0f, 0.0f, 0.0f);
                            int tmp_pos = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int tmp_neg = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                            rsxIRInstruction *ir_sgt = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_sgt, 0, sizeof(*ir_sgt));
                            ir_sgt->opcode = RSX_IR_OP_SGT;
                            ir_sgt->dst.file = NVFXSR_TEMP;
                            ir_sgt->dst.index = tmp_pos;
                            ir_sgt->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_sgt->src[0].file = vars[op1_id].file;
                            ir_sgt->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_sgt->src[0], &vars[op1_id]);
                            ir_sgt->src[1].file = NVFXSR_CONST;
                            ir_sgt->src[1].index = zero_const;
                            for (int k = 0; k < 4; k++) ir_sgt->src[1].swizzle[k] = 0;

                            rsxIRInstruction *ir_slt = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_slt, 0, sizeof(*ir_slt));
                            ir_slt->opcode = RSX_IR_OP_SLT;
                            ir_slt->dst.file = NVFXSR_TEMP;
                            ir_slt->dst.index = tmp_neg;
                            ir_slt->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_slt->src[0].file = vars[op1_id].file;
                            ir_slt->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_slt->src[0], &vars[op1_id]);
                            ir_slt->src[1].file = NVFXSR_CONST;
                            ir_slt->src[1].index = zero_const;
                            for (int k = 0; k < 4; k++) ir_slt->src[1].swizzle[k] = 0;

                            rsxIRInstruction *ir_sub = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_sub, 0, sizeof(*ir_sub));
                            ir_sub->opcode = RSX_IR_OP_ADD;
                            ir_sub->dst.file = vars[res_id].file;
                            ir_sub->dst.index = vars[res_id].index;
                            ir_sub->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_sub->src[0].file = NVFXSR_TEMP;
                            ir_sub->src[0].index = tmp_pos;
                            for (int k = 0; k < 4; k++) ir_sub->src[0].swizzle[k] = k;
                            ir_sub->src[1].file = NVFXSR_TEMP;
                            ir_sub->src[1].index = tmp_neg;
                            ir_sub->src[1].negate = true;
                            for (int k = 0; k < 4; k++) ir_sub->src[1].swizzle[k] = k;

                            if (tmp_pos >= 1 && tmp_pos < 48) free_temp_mask |= (1ULL << tmp_pos);
                            if (tmp_neg >= 1 && tmp_neg < 48) free_temp_mask |= (1ULL << tmp_neg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Exp2 && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_EX2;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Exp && op1_id < bound) {
                            int log2e_const = get_or_add_const(ctx, 1.44269504f, 1.44269504f, 1.44269504f, 1.44269504f);
                            int tmp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mul, 0, sizeof(*ir_mul));
                            ir_mul->opcode = RSX_IR_OP_MUL;
                            ir_mul->dst.file = NVFXSR_TEMP;
                            ir_mul->dst.index = tmp_reg;
                            ir_mul->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mul->src[0].file = vars[op1_id].file;
                            ir_mul->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_mul->src[0], &vars[op1_id]);
                            ir_mul->src[1].file = NVFXSR_CONST;
                            ir_mul->src[1].index = log2e_const;
                            for (int k = 0; k < 4; k++) ir_mul->src[1].swizzle[k] = 0;

                            rsxIRInstruction *ir_ex2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_ex2, 0, sizeof(*ir_ex2));
                            ir_ex2->opcode = RSX_IR_OP_EX2;
                            ir_ex2->dst.file = vars[res_id].file;
                            ir_ex2->dst.index = vars[res_id].index;
                            ir_ex2->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_ex2->src[0].file = NVFXSR_TEMP;
                            ir_ex2->src[0].index = tmp_reg;
                            for (int k = 0; k < 4; k++) ir_ex2->src[0].swizzle[k] = k;

                            if (tmp_reg >= 1 && tmp_reg < 48) free_temp_mask |= (1ULL << tmp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Log2 && op1_id < bound) {
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_LG2;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Log && op1_id < bound) {
                            int ln2_const = get_or_add_const(ctx, 0.69314718f, 0.69314718f, 0.69314718f, 0.69314718f);
                            int tmp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            rsxIRInstruction *ir_lg2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_lg2, 0, sizeof(*ir_lg2));
                            ir_lg2->opcode = RSX_IR_OP_LG2;
                            ir_lg2->dst.file = NVFXSR_TEMP;
                            ir_lg2->dst.index = tmp_reg;
                            ir_lg2->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_lg2->src[0].file = vars[op1_id].file;
                            ir_lg2->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_lg2->src[0], &vars[op1_id]);

                            rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mul, 0, sizeof(*ir_mul));
                            ir_mul->opcode = RSX_IR_OP_MUL;
                            ir_mul->dst.file = vars[res_id].file;
                            ir_mul->dst.index = vars[res_id].index;
                            ir_mul->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mul->src[0].file = NVFXSR_TEMP;
                            ir_mul->src[0].index = tmp_reg;
                            for (int k = 0; k < 4; k++) ir_mul->src[0].swizzle[k] = k;
                            ir_mul->src[1].file = NVFXSR_CONST;
                            ir_mul->src[1].index = ln2_const;
                            for (int k = 0; k < 4; k++) ir_mul->src[1].swizzle[k] = 0;

                            if (tmp_reg >= 1 && tmp_reg < 48) free_temp_mask |= (1ULL << tmp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Radians && op1_id < bound) {
                            int rad_const = get_or_add_const(ctx, 0.0174532925f, 0.0174532925f, 0.0174532925f, 0.0174532925f);
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_MUL;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            ir->src[1].file = NVFXSR_CONST;
                            ir->src[1].index = rad_const;
                            for (int k = 0; k < 4; k++) ir->src[1].swizzle[k] = 0;
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Degrees && op1_id < bound) {
                            int deg_const = get_or_add_const(ctx, 57.2957795f, 57.2957795f, 57.2957795f, 57.2957795f);
                            rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                            memset(ir, 0, sizeof(*ir));
                            ir->opcode = RSX_IR_OP_MUL;
                            ir->dst.file = vars[res_id].file;
                            ir->dst.index = vars[res_id].index;
                            ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir->src[0].file = vars[op1_id].file;
                            ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            ir->src[1].file = NVFXSR_CONST;
                            ir->src[1].index = deg_const;
                            for (int k = 0; k < 4; k++) ir->src[1].swizzle[k] = 0;
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Tan && op1_id < bound) {
                            int sin_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int cos_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int rcp_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                            rsxIRInstruction *ir_sin = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_sin, 0, sizeof(*ir_sin));
                            ir_sin->opcode = RSX_IR_OP_SIN;
                            ir_sin->dst.file = NVFXSR_TEMP;
                            ir_sin->dst.index = sin_reg;
                            ir_sin->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_sin->src[0].file = vars[op1_id].file;
                            ir_sin->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_sin->src[0], &vars[op1_id]);

                            rsxIRInstruction *ir_cos = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_cos, 0, sizeof(*ir_cos));
                            ir_cos->opcode = RSX_IR_OP_COS;
                            ir_cos->dst.file = NVFXSR_TEMP;
                            ir_cos->dst.index = cos_reg;
                            ir_cos->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_cos->src[0].file = vars[op1_id].file;
                            ir_cos->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_cos->src[0], &vars[op1_id]);

                            rsxIRInstruction *ir_rcp = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rcp, 0, sizeof(*ir_rcp));
                            ir_rcp->opcode = RSX_IR_OP_RCP;
                            ir_rcp->dst.file = NVFXSR_TEMP;
                            ir_rcp->dst.index = rcp_reg;
                            ir_rcp->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_rcp->src[0].file = NVFXSR_TEMP;
                            ir_rcp->src[0].index = cos_reg;
                            for (int k = 0; k < 4; k++) ir_rcp->src[0].swizzle[k] = k;

                            rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mul, 0, sizeof(*ir_mul));
                            ir_mul->opcode = RSX_IR_OP_MUL;
                            ir_mul->dst.file = vars[res_id].file;
                            ir_mul->dst.index = vars[res_id].index;
                            ir_mul->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mul->src[0].file = NVFXSR_TEMP;
                            ir_mul->src[0].index = sin_reg;
                            for (int k = 0; k < 4; k++) ir_mul->src[0].swizzle[k] = k;
                            ir_mul->src[1].file = NVFXSR_TEMP;
                            ir_mul->src[1].index = rcp_reg;
                            for (int k = 0; k < 4; k++) ir_mul->src[1].swizzle[k] = k;

                            if (sin_reg >= 1 && sin_reg < 48) free_temp_mask |= (1ULL << sin_reg);
                            if (cos_reg >= 1 && cos_reg < 48) free_temp_mask |= (1ULL << cos_reg);
                            if (rcp_reg >= 1 && rcp_reg < 48) free_temp_mask |= (1ULL << rcp_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450SmoothStep && op1_id < bound && op2_id < bound && op3_id < bound) {
                            int diff_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int rcp_reg  = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int xsub_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int t_unclamped = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int t_max = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int t_clamped = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int t2_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int poly_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                            int zero_const = get_or_add_const(ctx, 0.0f, 0.0f, 0.0f, 0.0f);
                            int one_const  = get_or_add_const(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
                            int three_const = get_or_add_const(ctx, 3.0f, 3.0f, 3.0f, 3.0f);
                            int neg2_const = get_or_add_const(ctx, -2.0f, -2.0f, -2.0f, -2.0f);

                            rsxIRInstruction *ir_diff = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_diff, 0, sizeof(*ir_diff));
                            ir_diff->opcode = RSX_IR_OP_ADD;
                            ir_diff->dst.file = NVFXSR_TEMP;
                            ir_diff->dst.index = diff_reg;
                            ir_diff->dst.writemask = 0xF;
                            ir_diff->src[0].file = vars[op2_id].file;
                            ir_diff->src[0].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_diff->src[0], &vars[op2_id]);
                            ir_diff->src[1].file = vars[op1_id].file;
                            ir_diff->src[1].index = vars[op1_id].index;
                            ir_diff->src[1].negate = true;
                            setup_src_swizzle(&ir_diff->src[1], &vars[op1_id]);

                            rsxIRInstruction *ir_rcp = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rcp, 0, sizeof(*ir_rcp));
                            ir_rcp->opcode = RSX_IR_OP_RCP;
                            ir_rcp->dst.file = NVFXSR_TEMP;
                            ir_rcp->dst.index = rcp_reg;
                            ir_rcp->dst.writemask = 0xF;
                            ir_rcp->src[0].file = NVFXSR_TEMP;
                            ir_rcp->src[0].index = diff_reg;
                            for (int k = 0; k < 4; k++) ir_rcp->src[0].swizzle[k] = 0;

                            rsxIRInstruction *ir_xsub = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_xsub, 0, sizeof(*ir_xsub));
                            ir_xsub->opcode = RSX_IR_OP_ADD;
                            ir_xsub->dst.file = NVFXSR_TEMP;
                            ir_xsub->dst.index = xsub_reg;
                            ir_xsub->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_xsub->src[0].file = vars[op3_id].file;
                            ir_xsub->src[0].index = vars[op3_id].index;
                            setup_src_swizzle(&ir_xsub->src[0], &vars[op3_id]);
                            ir_xsub->src[1].file = vars[op1_id].file;
                            ir_xsub->src[1].index = vars[op1_id].index;
                            ir_xsub->src[1].negate = true;
                            setup_src_swizzle(&ir_xsub->src[1], &vars[op1_id]);

                            rsxIRInstruction *ir_mul1 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mul1, 0, sizeof(*ir_mul1));
                            ir_mul1->opcode = RSX_IR_OP_MUL;
                            ir_mul1->dst.file = NVFXSR_TEMP;
                            ir_mul1->dst.index = t_unclamped;
                            ir_mul1->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mul1->src[0].file = NVFXSR_TEMP;
                            ir_mul1->src[0].index = xsub_reg;
                            for (int k = 0; k < 4; k++) ir_mul1->src[0].swizzle[k] = k;
                            ir_mul1->src[1].file = NVFXSR_TEMP;
                            ir_mul1->src[1].index = rcp_reg;
                            for (int k = 0; k < 4; k++) ir_mul1->src[1].swizzle[k] = 0;

                            rsxIRInstruction *ir_max = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_max, 0, sizeof(*ir_max));
                            ir_max->opcode = RSX_IR_OP_MAX;
                            ir_max->dst.file = NVFXSR_TEMP;
                            ir_max->dst.index = t_max;
                            ir_max->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_max->src[0].file = NVFXSR_TEMP;
                            ir_max->src[0].index = t_unclamped;
                            for (int k = 0; k < 4; k++) ir_max->src[0].swizzle[k] = k;
                            ir_max->src[1].file = NVFXSR_CONST;
                            ir_max->src[1].index = zero_const;
                            for (int k = 0; k < 4; k++) ir_max->src[1].swizzle[k] = 0;

                            rsxIRInstruction *ir_min = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_min, 0, sizeof(*ir_min));
                            ir_min->opcode = RSX_IR_OP_MIN;
                            ir_min->dst.file = NVFXSR_TEMP;
                            ir_min->dst.index = t_clamped;
                            ir_min->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_min->src[0].file = NVFXSR_TEMP;
                            ir_min->src[0].index = t_max;
                            for (int k = 0; k < 4; k++) ir_min->src[0].swizzle[k] = k;
                            ir_min->src[1].file = NVFXSR_CONST;
                            ir_min->src[1].index = one_const;
                            for (int k = 0; k < 4; k++) ir_min->src[1].swizzle[k] = 0;

                            rsxIRInstruction *ir_mul2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mul2, 0, sizeof(*ir_mul2));
                            ir_mul2->opcode = RSX_IR_OP_MUL;
                            ir_mul2->dst.file = NVFXSR_TEMP;
                            ir_mul2->dst.index = t2_reg;
                            ir_mul2->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mul2->src[0].file = NVFXSR_TEMP;
                            ir_mul2->src[0].index = t_clamped;
                            for (int k = 0; k < 4; k++) ir_mul2->src[0].swizzle[k] = k;
                            ir_mul2->src[1].file = NVFXSR_TEMP;
                            ir_mul2->src[1].index = t_clamped;
                            for (int k = 0; k < 4; k++) ir_mul2->src[1].swizzle[k] = k;

                            rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mad, 0, sizeof(*ir_mad));
                            ir_mad->opcode = RSX_IR_OP_MAD;
                            ir_mad->dst.file = NVFXSR_TEMP;
                            ir_mad->dst.index = poly_reg;
                            ir_mad->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mad->src[0].file = NVFXSR_TEMP;
                            ir_mad->src[0].index = t_clamped;
                            for (int k = 0; k < 4; k++) ir_mad->src[0].swizzle[k] = k;
                            ir_mad->src[1].file = NVFXSR_CONST;
                            ir_mad->src[1].index = neg2_const;
                            for (int k = 0; k < 4; k++) ir_mad->src[1].swizzle[k] = 0;
                            ir_mad->src[2].file = NVFXSR_CONST;
                            ir_mad->src[2].index = three_const;
                            for (int k = 0; k < 4; k++) ir_mad->src[2].swizzle[k] = 0;

                            rsxIRInstruction *ir_res = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_res, 0, sizeof(*ir_res));
                            ir_res->opcode = RSX_IR_OP_MUL;
                            ir_res->dst.file = vars[res_id].file;
                            ir_res->dst.index = vars[res_id].index;
                            ir_res->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_res->src[0].file = NVFXSR_TEMP;
                            ir_res->src[0].index = t2_reg;
                            for (int k = 0; k < 4; k++) ir_res->src[0].swizzle[k] = k;
                            ir_res->src[1].file = NVFXSR_TEMP;
                            ir_res->src[1].index = poly_reg;
                            for (int k = 0; k < 4; k++) ir_res->src[1].swizzle[k] = k;

                            if (diff_reg >= 1 && diff_reg < 48) free_temp_mask |= (1ULL << diff_reg);
                            if (rcp_reg >= 1 && rcp_reg < 48) free_temp_mask |= (1ULL << rcp_reg);
                            if (xsub_reg >= 1 && xsub_reg < 48) free_temp_mask |= (1ULL << xsub_reg);
                            if (t_unclamped >= 1 && t_unclamped < 48) free_temp_mask |= (1ULL << t_unclamped);
                            if (t_max >= 1 && t_max < 48) free_temp_mask |= (1ULL << t_max);
                            if (t_clamped >= 1 && t_clamped < 48) free_temp_mask |= (1ULL << t_clamped);
                            if (t2_reg >= 1 && t2_reg < 48) free_temp_mask |= (1ULL << t2_reg);
                            if (poly_reg >= 1 && poly_reg < 48) free_temp_mask |= (1ULL << poly_reg);

                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op3_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Distance && op1_id < bound && op2_id < bound) {
                            int diff_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int dot_reg  = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int rsq_reg  = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                            rsxIRInstruction *ir_sub = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_sub, 0, sizeof(*ir_sub));
                            ir_sub->opcode = RSX_IR_OP_ADD;
                            ir_sub->dst.file = NVFXSR_TEMP;
                            ir_sub->dst.index = diff_reg;
                            ir_sub->dst.writemask = 0xF;
                            ir_sub->src[0].file = vars[op1_id].file;
                            ir_sub->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_sub->src[0], &vars[op1_id]);
                            ir_sub->src[1].file = vars[op2_id].file;
                            ir_sub->src[1].index = vars[op2_id].index;
                            ir_sub->src[1].negate = true;
                            setup_src_swizzle(&ir_sub->src[1], &vars[op2_id]);

                            if (vars[op1_id].vec_size == 2) {
                                int mul_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                                rsxIRInstruction *ir_mul = &ctx->instructions[ctx->num_instructions++];
                                memset(ir_mul, 0, sizeof(*ir_mul));
                                ir_mul->opcode = RSX_IR_OP_MUL;
                                ir_mul->dst.file = NVFXSR_TEMP;
                                ir_mul->dst.index = mul_reg;
                                ir_mul->dst.writemask = 0x3;
                                ir_mul->src[0].file = NVFXSR_TEMP;
                                ir_mul->src[0].index = diff_reg;
                                for (int k = 0; k < 4; k++) ir_mul->src[0].swizzle[k] = k;
                                ir_mul->src[1].file = NVFXSR_TEMP;
                                ir_mul->src[1].index = diff_reg;
                                for (int k = 0; k < 4; k++) ir_mul->src[1].swizzle[k] = k;

                                rsxIRInstruction *ir_add = &ctx->instructions[ctx->num_instructions++];
                                memset(ir_add, 0, sizeof(*ir_add));
                                ir_add->opcode = RSX_IR_OP_ADD;
                                ir_add->dst.file = NVFXSR_TEMP;
                                ir_add->dst.index = dot_reg;
                                ir_add->dst.writemask = 0xF;
                                ir_add->src[0].file = NVFXSR_TEMP;
                                ir_add->src[0].index = mul_reg;
                                for (int k = 0; k < 4; k++) ir_add->src[0].swizzle[k] = 0;
                                ir_add->src[1].file = NVFXSR_TEMP;
                                ir_add->src[1].index = mul_reg;
                                for (int k = 0; k < 4; k++) ir_add->src[1].swizzle[k] = 1;
                                if (mul_reg >= 1 && mul_reg < 48) free_temp_mask |= (1ULL << mul_reg);
                            } else {
                                rsxIRInstruction *ir_dot = &ctx->instructions[ctx->num_instructions++];
                                memset(ir_dot, 0, sizeof(*ir_dot));
                                ir_dot->opcode = (vars[op1_id].vec_size >= 4) ? RSX_IR_OP_DP4 : RSX_IR_OP_DP3;
                                ir_dot->dst.file = NVFXSR_TEMP;
                                ir_dot->dst.index = dot_reg;
                                ir_dot->dst.writemask = 0xF;
                                ir_dot->src[0].file = NVFXSR_TEMP;
                                ir_dot->src[0].index = diff_reg;
                                ir_dot->src[1].file = NVFXSR_TEMP;
                                ir_dot->src[1].index = diff_reg;
                                for (int k = 0; k < 4; k++) {
                                    ir_dot->src[0].swizzle[k] = k;
                                    ir_dot->src[1].swizzle[k] = k;
                                }
                            }

                            rsxIRInstruction *ir_rsq = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rsq, 0, sizeof(*ir_rsq));
                            ir_rsq->opcode = RSX_IR_OP_RSQ;
                            ir_rsq->dst.file = NVFXSR_TEMP;
                            ir_rsq->dst.index = rsq_reg;
                            ir_rsq->dst.writemask = 0xF;
                            ir_rsq->src[0].file = NVFXSR_TEMP;
                            ir_rsq->src[0].index = dot_reg;
                            for (int k = 0; k < 4; k++) ir_rsq->src[0].swizzle[k] = 0;

                            rsxIRInstruction *ir_rcp = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rcp, 0, sizeof(*ir_rcp));
                            ir_rcp->opcode = RSX_IR_OP_RCP;
                            ir_rcp->dst.file = vars[res_id].file;
                            ir_rcp->dst.index = vars[res_id].index;
                            ir_rcp->dst.writemask = 0xF;
                            ir_rcp->src[0].file = NVFXSR_TEMP;
                            ir_rcp->src[0].index = rsq_reg;
                            for (int k = 0; k < 4; k++) ir_rcp->src[0].swizzle[k] = 0;

                            if (diff_reg >= 1 && diff_reg < 48) free_temp_mask |= (1ULL << diff_reg);
                            if (dot_reg >= 1 && dot_reg < 48) free_temp_mask |= (1ULL << dot_reg);
                            if (rsq_reg >= 1 && rsq_reg < 48) free_temp_mask |= (1ULL << rsq_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450FaceForward && op1_id < bound && op2_id < bound && op3_id < bound) {
                            int dot_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                            rsxIRInstruction *ir_dot = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_dot, 0, sizeof(*ir_dot));
                            ir_dot->opcode = RSX_IR_OP_DP3;
                            ir_dot->dst.file = NVFXSR_TEMP;
                            ir_dot->dst.index = dot_reg;
                            ir_dot->dst.writemask = 0xF;
                            ir_dot->src[0].file = vars[op3_id].file;
                            ir_dot->src[0].index = vars[op3_id].index;
                            setup_src_swizzle(&ir_dot->src[0], &vars[op3_id]);
                            ir_dot->src[1].file = vars[op2_id].file;
                            ir_dot->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_dot->src[1], &vars[op2_id]);

                            rsxIRInstruction *ir_cmp = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_cmp, 0, sizeof(*ir_cmp));
                            ir_cmp->opcode = RSX_IR_OP_CMP;
                            ir_cmp->dst.file = vars[res_id].file;
                            ir_cmp->dst.index = vars[res_id].index;
                            ir_cmp->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_cmp->src[0].file = NVFXSR_TEMP;
                            ir_cmp->src[0].index = dot_reg;
                            for (int k = 0; k < 4; k++) ir_cmp->src[0].swizzle[k] = 0;
                            ir_cmp->src[1].file = vars[op1_id].file;
                            ir_cmp->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_cmp->src[1], &vars[op1_id]);
                            ir_cmp->src[2].file = vars[op1_id].file;
                            ir_cmp->src[2].index = vars[op1_id].index;
                            ir_cmp->src[2].negate = true;
                            setup_src_swizzle(&ir_cmp->src[2], &vars[op1_id]);

                            if (dot_reg >= 1 && dot_reg < 48) free_temp_mask |= (1ULL << dot_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op3_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Refract && op1_id < bound && op2_id < bound && op3_id < bound) {
                            int dot_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int k_reg   = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int rsq_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int sqrtk_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int coef_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int one_const = get_or_add_const(ctx, 1.0f, 1.0f, 1.0f, 1.0f);

                            rsxIRInstruction *ir_dot = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_dot, 0, sizeof(*ir_dot));
                            ir_dot->opcode = RSX_IR_OP_DP3;
                            ir_dot->dst.file = NVFXSR_TEMP;
                            ir_dot->dst.index = dot_reg;
                            ir_dot->dst.writemask = 0xF;
                            ir_dot->src[0].file = vars[op2_id].file;
                            ir_dot->src[0].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_dot->src[0], &vars[op2_id]);
                            ir_dot->src[1].file = vars[op1_id].file;
                            ir_dot->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_dot->src[1], &vars[op1_id]);

                            rsxIRInstruction *ir_k = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_k, 0, sizeof(*ir_k));
                            ir_k->opcode = RSX_IR_OP_MAD;
                            ir_k->dst.file = NVFXSR_TEMP;
                            ir_k->dst.index = k_reg;
                            ir_k->dst.writemask = 0xF;
                            ir_k->src[0].file = vars[op3_id].file;
                            ir_k->src[0].index = vars[op3_id].index;
                            setup_src_swizzle(&ir_k->src[0], &vars[op3_id]);
                            ir_k->src[1].file = vars[op3_id].file;
                            ir_k->src[1].index = vars[op3_id].index;
                            setup_src_swizzle(&ir_k->src[1], &vars[op3_id]);
                            ir_k->src[1].negate = true;
                            ir_k->src[2].file = NVFXSR_CONST;
                            ir_k->src[2].index = one_const;
                            for (int s = 0; s < 4; s++) ir_k->src[2].swizzle[s] = 0;

                            rsxIRInstruction *ir_rsq = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rsq, 0, sizeof(*ir_rsq));
                            ir_rsq->opcode = RSX_IR_OP_RSQ;
                            ir_rsq->dst.file = NVFXSR_TEMP;
                            ir_rsq->dst.index = rsq_reg;
                            ir_rsq->dst.writemask = 0xF;
                            ir_rsq->src[0].file = NVFXSR_TEMP;
                            ir_rsq->src[0].index = k_reg;
                            for (int s = 0; s < 4; s++) ir_rsq->src[0].swizzle[s] = 0;

                            rsxIRInstruction *ir_rcp = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rcp, 0, sizeof(*ir_rcp));
                            ir_rcp->opcode = RSX_IR_OP_RCP;
                            ir_rcp->dst.file = NVFXSR_TEMP;
                            ir_rcp->dst.index = sqrtk_reg;
                            ir_rcp->dst.writemask = 0xF;
                            ir_rcp->src[0].file = NVFXSR_TEMP;
                            ir_rcp->src[0].index = rsq_reg;
                            for (int s = 0; s < 4; s++) ir_rcp->src[0].swizzle[s] = 0;

                            rsxIRInstruction *ir_coef = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_coef, 0, sizeof(*ir_coef));
                            ir_coef->opcode = RSX_IR_OP_MAD;
                            ir_coef->dst.file = NVFXSR_TEMP;
                            ir_coef->dst.index = coef_reg;
                            ir_coef->dst.writemask = 0xF;
                            ir_coef->src[0].file = vars[op3_id].file;
                            ir_coef->src[0].index = vars[op3_id].index;
                            setup_src_swizzle(&ir_coef->src[0], &vars[op3_id]);
                            ir_coef->src[1].file = NVFXSR_TEMP;
                            ir_coef->src[1].index = dot_reg;
                            for (int s = 0; s < 4; s++) ir_coef->src[1].swizzle[s] = 0;
                            ir_coef->src[2].file = NVFXSR_TEMP;
                            ir_coef->src[2].index = sqrtk_reg;
                            for (int s = 0; s < 4; s++) ir_coef->src[2].swizzle[s] = 0;

                            rsxIRInstruction *ir_res = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_res, 0, sizeof(*ir_res));
                            ir_res->opcode = RSX_IR_OP_MAD;
                            ir_res->dst.file = vars[res_id].file;
                            ir_res->dst.index = vars[res_id].index;
                            ir_res->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_res->src[0].file = NVFXSR_TEMP;
                            ir_res->src[0].index = coef_reg;
                            ir_res->src[0].negate = true;
                            for (int s = 0; s < 4; s++) ir_res->src[0].swizzle[s] = 0;
                            ir_res->src[1].file = vars[op2_id].file;
                            ir_res->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_res->src[1], &vars[op2_id]);
                            ir_res->src[2].file = vars[op1_id].file;
                            ir_res->src[2].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_res->src[2], &vars[op1_id]);

                            if (dot_reg >= 1 && dot_reg < 48) free_temp_mask |= (1ULL << dot_reg);
                            if (k_reg >= 1 && k_reg < 48) free_temp_mask |= (1ULL << k_reg);
                            if (rsq_reg >= 1 && rsq_reg < 48) free_temp_mask |= (1ULL << rsq_reg);
                            if (sqrtk_reg >= 1 && sqrtk_reg < 48) free_temp_mask |= (1ULL << sqrtk_reg);
                            if (coef_reg >= 1 && coef_reg < 48) free_temp_mask |= (1ULL << coef_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op3_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Atan && op1_id < bound) {
                            int c1_const = get_or_add_const(ctx, 0.97239411f, 0.97239411f, 0.97239411f, 0.97239411f);
                            int c2_const = get_or_add_const(ctx, -0.19194795f, -0.19194795f, -0.19194795f, -0.19194795f);
                            int x2_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int poly_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                            rsxIRInstruction *ir_x2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_x2, 0, sizeof(*ir_x2));
                            ir_x2->opcode = RSX_IR_OP_MUL;
                            ir_x2->dst.file = NVFXSR_TEMP;
                            ir_x2->dst.index = x2_reg;
                            ir_x2->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_x2->src[0].file = vars[op1_id].file;
                            ir_x2->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_x2->src[0], &vars[op1_id]);
                            ir_x2->src[1].file = vars[op1_id].file;
                            ir_x2->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_x2->src[1], &vars[op1_id]);

                            rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mad, 0, sizeof(*ir_mad));
                            ir_mad->opcode = RSX_IR_OP_MAD;
                            ir_mad->dst.file = NVFXSR_TEMP;
                            ir_mad->dst.index = poly_reg;
                            ir_mad->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mad->src[0].file = NVFXSR_TEMP;
                            ir_mad->src[0].index = x2_reg;
                            for (int s = 0; s < 4; s++) ir_mad->src[0].swizzle[s] = s;
                            ir_mad->src[1].file = NVFXSR_CONST;
                            ir_mad->src[1].index = c2_const;
                            for (int s = 0; s < 4; s++) ir_mad->src[1].swizzle[s] = 0;
                            ir_mad->src[2].file = NVFXSR_CONST;
                            ir_mad->src[2].index = c1_const;
                            for (int s = 0; s < 4; s++) ir_mad->src[2].swizzle[s] = 0;

                            rsxIRInstruction *ir_res = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_res, 0, sizeof(*ir_res));
                            ir_res->opcode = RSX_IR_OP_MUL;
                            ir_res->dst.file = vars[res_id].file;
                            ir_res->dst.index = vars[res_id].index;
                            ir_res->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_res->src[0].file = vars[op1_id].file;
                            ir_res->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_res->src[0], &vars[op1_id]);
                            ir_res->src[1].file = NVFXSR_TEMP;
                            ir_res->src[1].index = poly_reg;
                            for (int s = 0; s < 4; s++) ir_res->src[1].swizzle[s] = s;

                            if (x2_reg >= 1 && x2_reg < 48) free_temp_mask |= (1ULL << x2_reg);
                            if (poly_reg >= 1 && poly_reg < 48) free_temp_mask |= (1ULL << poly_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Atan2 && op1_id < bound && op2_id < bound) {
                            int rcp_x = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int quot  = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int x2_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int poly_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int c1_const = get_or_add_const(ctx, 0.97239411f, 0.97239411f, 0.97239411f, 0.97239411f);
                            int c2_const = get_or_add_const(ctx, -0.19194795f, -0.19194795f, -0.19194795f, -0.19194795f);

                            rsxIRInstruction *ir_rcp = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_rcp, 0, sizeof(*ir_rcp));
                            ir_rcp->opcode = RSX_IR_OP_RCP;
                            ir_rcp->dst.file = NVFXSR_TEMP;
                            ir_rcp->dst.index = rcp_x;
                            ir_rcp->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_rcp->src[0].file = vars[op2_id].file;
                            ir_rcp->src[0].index = vars[op2_id].index;
                            setup_src_swizzle(&ir_rcp->src[0], &vars[op2_id]);

                            rsxIRInstruction *ir_quot = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_quot, 0, sizeof(*ir_quot));
                            ir_quot->opcode = RSX_IR_OP_MUL;
                            ir_quot->dst.file = NVFXSR_TEMP;
                            ir_quot->dst.index = quot;
                            ir_quot->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_quot->src[0].file = vars[op1_id].file;
                            ir_quot->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_quot->src[0], &vars[op1_id]);
                            ir_quot->src[1].file = NVFXSR_TEMP;
                            ir_quot->src[1].index = rcp_x;
                            for (int s = 0; s < 4; s++) ir_quot->src[1].swizzle[s] = s;

                            rsxIRInstruction *ir_x2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_x2, 0, sizeof(*ir_x2));
                            ir_x2->opcode = RSX_IR_OP_MUL;
                            ir_x2->dst.file = NVFXSR_TEMP;
                            ir_x2->dst.index = x2_reg;
                            ir_x2->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_x2->src[0].file = NVFXSR_TEMP;
                            ir_x2->src[0].index = quot;
                            for (int s = 0; s < 4; s++) ir_x2->src[0].swizzle[s] = s;
                            ir_x2->src[1].file = NVFXSR_TEMP;
                            ir_x2->src[1].index = quot;
                            for (int s = 0; s < 4; s++) ir_x2->src[1].swizzle[s] = s;

                            rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mad, 0, sizeof(*ir_mad));
                            ir_mad->opcode = RSX_IR_OP_MAD;
                            ir_mad->dst.file = NVFXSR_TEMP;
                            ir_mad->dst.index = poly_reg;
                            ir_mad->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mad->src[0].file = NVFXSR_TEMP;
                            ir_mad->src[0].index = x2_reg;
                            for (int s = 0; s < 4; s++) ir_mad->src[0].swizzle[s] = s;
                            ir_mad->src[1].file = NVFXSR_CONST;
                            ir_mad->src[1].index = c2_const;
                            for (int s = 0; s < 4; s++) ir_mad->src[1].swizzle[s] = 0;
                            ir_mad->src[2].file = NVFXSR_CONST;
                            ir_mad->src[2].index = c1_const;
                            for (int s = 0; s < 4; s++) ir_mad->src[2].swizzle[s] = 0;

                            rsxIRInstruction *ir_res = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_res, 0, sizeof(*ir_res));
                            ir_res->opcode = RSX_IR_OP_MUL;
                            ir_res->dst.file = vars[res_id].file;
                            ir_res->dst.index = vars[res_id].index;
                            ir_res->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_res->src[0].file = NVFXSR_TEMP;
                            ir_res->src[0].index = quot;
                            for (int s = 0; s < 4; s++) ir_res->src[0].swizzle[s] = s;
                            ir_res->src[1].file = NVFXSR_TEMP;
                            ir_res->src[1].index = poly_reg;
                            for (int s = 0; s < 4; s++) ir_res->src[1].swizzle[s] = s;

                            if (rcp_x >= 1 && rcp_x < 48) free_temp_mask |= (1ULL << rcp_x);
                            if (quot >= 1 && quot < 48) free_temp_mask |= (1ULL << quot);
                            if (x2_reg >= 1 && x2_reg < 48) free_temp_mask |= (1ULL << x2_reg);
                            if (poly_reg >= 1 && poly_reg < 48) free_temp_mask |= (1ULL << poly_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                            release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Asin && op1_id < bound) {
                            int c_const = get_or_add_const(ctx, 0.16666667f, 0.16666667f, 0.16666667f, 0.16666667f);
                            int one_const = get_or_add_const(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
                            int x2_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int poly_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                            rsxIRInstruction *ir_x2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_x2, 0, sizeof(*ir_x2));
                            ir_x2->opcode = RSX_IR_OP_MUL;
                            ir_x2->dst.file = NVFXSR_TEMP;
                            ir_x2->dst.index = x2_reg;
                            ir_x2->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_x2->src[0].file = vars[op1_id].file;
                            ir_x2->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_x2->src[0], &vars[op1_id]);
                            ir_x2->src[1].file = vars[op1_id].file;
                            ir_x2->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_x2->src[1], &vars[op1_id]);

                            rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mad, 0, sizeof(*ir_mad));
                            ir_mad->opcode = RSX_IR_OP_MAD;
                            ir_mad->dst.file = NVFXSR_TEMP;
                            ir_mad->dst.index = poly_reg;
                            ir_mad->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mad->src[0].file = NVFXSR_TEMP;
                            ir_mad->src[0].index = x2_reg;
                            for (int s = 0; s < 4; s++) ir_mad->src[0].swizzle[s] = s;
                            ir_mad->src[1].file = NVFXSR_CONST;
                            ir_mad->src[1].index = c_const;
                            for (int s = 0; s < 4; s++) ir_mad->src[1].swizzle[s] = 0;
                            ir_mad->src[2].file = NVFXSR_CONST;
                            ir_mad->src[2].index = one_const;
                            for (int s = 0; s < 4; s++) ir_mad->src[2].swizzle[s] = 0;

                            rsxIRInstruction *ir_res = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_res, 0, sizeof(*ir_res));
                            ir_res->opcode = RSX_IR_OP_MUL;
                            ir_res->dst.file = vars[res_id].file;
                            ir_res->dst.index = vars[res_id].index;
                            ir_res->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_res->src[0].file = vars[op1_id].file;
                            ir_res->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_res->src[0], &vars[op1_id]);
                            ir_res->src[1].file = NVFXSR_TEMP;
                            ir_res->src[1].index = poly_reg;
                            for (int s = 0; s < 4; s++) ir_res->src[1].swizzle[s] = s;

                            if (x2_reg >= 1 && x2_reg < 48) free_temp_mask |= (1ULL << x2_reg);
                            if (poly_reg >= 1 && poly_reg < 48) free_temp_mask |= (1ULL << poly_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        } else if (ext_op == GLSLstd450Acos && op1_id < bound) {
                            int c_const = get_or_add_const(ctx, 0.16666667f, 0.16666667f, 0.16666667f, 0.16666667f);
                            int one_const = get_or_add_const(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
                            int pi2_const = get_or_add_const(ctx, 1.57079632f, 1.57079632f, 1.57079632f, 1.57079632f);
                            int x2_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int poly_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                            int asin_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                            rsxIRInstruction *ir_x2 = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_x2, 0, sizeof(*ir_x2));
                            ir_x2->opcode = RSX_IR_OP_MUL;
                            ir_x2->dst.file = NVFXSR_TEMP;
                            ir_x2->dst.index = x2_reg;
                            ir_x2->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_x2->src[0].file = vars[op1_id].file;
                            ir_x2->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_x2->src[0], &vars[op1_id]);
                            ir_x2->src[1].file = vars[op1_id].file;
                            ir_x2->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_x2->src[1], &vars[op1_id]);

                            rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_mad, 0, sizeof(*ir_mad));
                            ir_mad->opcode = RSX_IR_OP_MAD;
                            ir_mad->dst.file = NVFXSR_TEMP;
                            ir_mad->dst.index = poly_reg;
                            ir_mad->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_mad->src[0].file = NVFXSR_TEMP;
                            ir_mad->src[0].index = x2_reg;
                            for (int s = 0; s < 4; s++) ir_mad->src[0].swizzle[s] = s;
                            ir_mad->src[1].file = NVFXSR_CONST;
                            ir_mad->src[1].index = c_const;
                            for (int s = 0; s < 4; s++) ir_mad->src[1].swizzle[s] = 0;
                            ir_mad->src[2].file = NVFXSR_CONST;
                            ir_mad->src[2].index = one_const;
                            for (int s = 0; s < 4; s++) ir_mad->src[2].swizzle[s] = 0;

                            rsxIRInstruction *ir_asin = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_asin, 0, sizeof(*ir_asin));
                            ir_asin->opcode = RSX_IR_OP_MUL;
                            ir_asin->dst.file = NVFXSR_TEMP;
                            ir_asin->dst.index = asin_reg;
                            ir_asin->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_asin->src[0].file = vars[op1_id].file;
                            ir_asin->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir_asin->src[0], &vars[op1_id]);
                            ir_asin->src[1].file = NVFXSR_TEMP;
                            ir_asin->src[1].index = poly_reg;
                            for (int s = 0; s < 4; s++) ir_asin->src[1].swizzle[s] = s;

                            rsxIRInstruction *ir_res = &ctx->instructions[ctx->num_instructions++];
                            memset(ir_res, 0, sizeof(*ir_res));
                            ir_res->opcode = RSX_IR_OP_ADD;
                            ir_res->dst.file = vars[res_id].file;
                            ir_res->dst.index = vars[res_id].index;
                            ir_res->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                            ir_res->src[0].file = NVFXSR_CONST;
                            ir_res->src[0].index = pi2_const;
                            for (int s = 0; s < 4; s++) ir_res->src[0].swizzle[s] = 0;
                            ir_res->src[1].file = NVFXSR_TEMP;
                            ir_res->src[1].index = asin_reg;
                            ir_res->src[1].negate = true;
                            for (int s = 0; s < 4; s++) ir_res->src[1].swizzle[s] = s;

                            if (x2_reg >= 1 && x2_reg < 48) free_temp_mask |= (1ULL << x2_reg);
                            if (poly_reg >= 1 && poly_reg < 48) free_temp_mask |= (1ULL << poly_reg);
                            if (asin_reg >= 1 && asin_reg < 48) free_temp_mask |= (1ULL << asin_reg);
                            inst_counter++;
                            release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        }
                    }
                }
                break;
            }

            case SpvOpFOrdLessThan:
            case SpvOpFOrdGreaterThan:
            case SpvOpFOrdLessThanEqual:
            case SpvOpFOrdGreaterThanEqual:
            case SpvOpFOrdEqual:
            case SpvOpFOrdNotEqual:
            case SpvOpFUnordNotEqual:
            case SpvOpIEqual:
            case SpvOpINotEqual: {
                if (word_len >= 5) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t op1_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t op2_id = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];

                    if (res_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 1;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);

                        if (opcode == SpvOpFOrdLessThan) {
                            ir->opcode = RSX_IR_OP_SLT;
                            ir->src[0].file = vars[op1_id].file; ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            ir->src[1].file = vars[op2_id].file; ir->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op2_id]);
                        } else if (opcode == SpvOpFOrdGreaterThan) {
                            ir->opcode = RSX_IR_OP_SLT;
                            ir->src[0].file = vars[op2_id].file; ir->src[0].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op2_id]);
                            ir->src[1].file = vars[op1_id].file; ir->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op1_id]);
                        } else if (opcode == SpvOpFOrdLessThanEqual) {
                            ir->opcode = RSX_IR_OP_SGE;
                            ir->src[0].file = vars[op2_id].file; ir->src[0].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op2_id]);
                            ir->src[1].file = vars[op1_id].file; ir->src[1].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op1_id]);
                        } else if (opcode == SpvOpFOrdGreaterThanEqual) {
                            ir->opcode = RSX_IR_OP_SGE;
                            ir->src[0].file = vars[op1_id].file; ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            ir->src[1].file = vars[op2_id].file; ir->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op2_id]);
                        } else if (opcode == SpvOpFOrdEqual || opcode == SpvOpIEqual) {
                            ir->opcode = RSX_IR_OP_SEQ;
                            ir->src[0].file = vars[op1_id].file; ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            ir->src[1].file = vars[op2_id].file; ir->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op2_id]);
                        } else if (opcode == SpvOpFOrdNotEqual || opcode == SpvOpFUnordNotEqual || opcode == SpvOpINotEqual) {
                            ir->opcode = RSX_IR_OP_SNE;
                            ir->src[0].file = vars[op1_id].file; ir->src[0].index = vars[op1_id].index;
                            setup_src_swizzle(&ir->src[0], &vars[op1_id]);
                            ir->src[1].file = vars[op2_id].file; ir->src[1].index = vars[op2_id].index;
                            setup_src_swizzle(&ir->src[1], &vars[op2_id]);
                        }

                        inst_counter++;
                        release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpTranspose: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t mat_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];

                    if (res_id < bound && mat_id < bound && ctx->num_instructions + 16 < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        int t0 = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        int t1 = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        int t2 = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        int t3 = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        int t_regs[4] = { t0, t1, t2, t3 };
                        vars[res_id].index = t0;
                        vars[res_id].is_live = true;

                        for (int r = 0; r < 4; r++) {
                            for (int c = 0; c < 4; c++) {
                                rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                                memset(ir, 0, sizeof(*ir));
                                ir->opcode = RSX_IR_OP_MOV;
                                ir->dst.file = NVFXSR_TEMP;
                                ir->dst.index = t_regs[r];
                                ir->dst.writemask = (1u << c);

                                ir->src[0].file = vars[mat_id].file;
                                ir->src[0].index = vars[mat_id].index + c;
                                for (int k = 0; k < 4; k++) ir->src[0].swizzle[k] = r;
                            }
                        }

                        inst_counter++;
                        release_var_if_dead(&vars[mat_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpAny:
            case SpvOpAll: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t vec_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];

                    if (res_id < bound && vec_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = 1;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        uint8_t in_size = vars[vec_id].vec_size ? vars[vec_id].vec_size : 2;
                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->opcode = (opcode == SpvOpAny) ? RSX_IR_OP_MAX : RSX_IR_OP_MIN;
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = 0x1;

                        ir->src[0].file = vars[vec_id].file;
                        ir->src[0].index = vars[vec_id].index;
                        ir->src[0].swizzle[0] = 0;
                        ir->src[0].swizzle[1] = 0;
                        ir->src[0].swizzle[2] = 0;
                        ir->src[0].swizzle[3] = 0;

                        ir->src[1].file = vars[vec_id].file;
                        ir->src[1].index = vars[vec_id].index;
                        ir->src[1].swizzle[0] = 1;
                        ir->src[1].swizzle[1] = 1;
                        ir->src[1].swizzle[2] = 1;
                        ir->src[1].swizzle[3] = 1;

                        if (in_size > 2) {
                            for (int c = 2; c < in_size && c < 4; c++) {
                                rsxIRInstruction *ir_c = &ctx->instructions[ctx->num_instructions++];
                                memset(ir_c, 0, sizeof(*ir_c));
                                ir_c->opcode = (opcode == SpvOpAny) ? RSX_IR_OP_MAX : RSX_IR_OP_MIN;
                                ir_c->dst.file = vars[res_id].file;
                                ir_c->dst.index = vars[res_id].index;
                                ir_c->dst.writemask = 0x1;
                                ir_c->src[0].file = vars[res_id].file;
                                ir_c->src[0].index = vars[res_id].index;
                                for (int k = 0; k < 4; k++) ir_c->src[0].swizzle[k] = 0;
                                ir_c->src[1].file = vars[vec_id].file;
                                ir_c->src[1].index = vars[vec_id].index;
                                for (int k = 0; k < 4; k++) ir_c->src[1].swizzle[k] = c;
                            }
                        }

                        inst_counter++;
                        release_var_if_dead(&vars[vec_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpLogicalAnd:
            case SpvOpLogicalOr:
            case SpvOpLogicalEqual:
            case SpvOpLogicalNotEqual: {
                if (word_len >= 5) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t op1_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t op2_id = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];

                    if (res_id < bound && op1_id < bound && op2_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 1;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);

                        if (opcode == SpvOpLogicalAnd) {
                            ir->opcode = RSX_IR_OP_MUL; /* 1.0 * 1.0 = 1.0, 1.0 * 0.0 = 0.0 */
                        } else if (opcode == SpvOpLogicalOr) {
                            ir->opcode = RSX_IR_OP_MAX; /* max(1.0, 0.0) = 1.0, max(0.0, 0.0) = 0.0 */
                        } else if (opcode == SpvOpLogicalEqual) {
                            ir->opcode = RSX_IR_OP_SEQ;
                        } else if (opcode == SpvOpLogicalNotEqual) {
                            ir->opcode = RSX_IR_OP_SNE;
                        }

                        ir->src[0].file = vars[op1_id].file;
                        ir->src[0].index = vars[op1_id].index;
                        setup_src_swizzle(&ir->src[0], &vars[op1_id]);

                        ir->src[1].file = vars[op2_id].file;
                        ir->src[1].index = vars[op2_id].index;
                        setup_src_swizzle(&ir->src[1], &vars[op2_id]);

                        inst_counter++;
                        release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[op2_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpLogicalNot: {
                if (word_len >= 4) {
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t op1_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];

                    if (res_id < bound && op1_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        /* not(x) = 1.0 - x */
                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->opcode = RSX_IR_OP_ADD;
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = 0xF;

                        ir->src[0].file = vars[op1_id].file;
                        ir->src[0].index = vars[op1_id].index;
                        ir->src[0].negate = true;

                        ir->src[1].file = NVFXSR_CONST;
                        ir->src[1].index = get_or_add_const(ctx, 1.0f, 1.0f, 1.0f, 1.0f);

                        for (int k = 0; k < 4; k++) {
                            ir->src[0].swizzle[k] = k;
                            ir->src[1].swizzle[k] = 0;
                        }

                        inst_counter++;
                        release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpSelect: {
                if (word_len >= 6) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t cond_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t true_id = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];
                    uint32_t false_id = swap_endian ? __builtin_bswap32(inst[5]) : inst[5];

                    if (res_id < bound && ctx->num_instructions + 2 < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : ((true_id < bound && vars[true_id].vec_size) ? vars[true_id].vec_size : 4);
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        /* diff = true_val - false_val */
                        int diff_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        rsxIRInstruction *ir_sub = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_sub, 0, sizeof(*ir_sub));
                        ir_sub->opcode = RSX_IR_OP_SUB;
                        ir_sub->dst.file = NVFXSR_TEMP;
                        ir_sub->dst.index = diff_reg;
                        ir_sub->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_sub->src[0].file = vars[true_id].file;
                        ir_sub->src[0].index = vars[true_id].index;
                        setup_src_swizzle(&ir_sub->src[0], &vars[true_id]);
                        ir_sub->src[1].file = vars[false_id].file;
                        ir_sub->src[1].index = vars[false_id].index;
                        setup_src_swizzle(&ir_sub->src[1], &vars[false_id]);

                        /* res = cond * diff + false_val */
                        rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_mad, 0, sizeof(*ir_mad));
                        ir_mad->opcode = RSX_IR_OP_MAD;
                        ir_mad->dst.file = vars[res_id].file;
                        ir_mad->dst.index = vars[res_id].index;
                        ir_mad->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_mad->src[0].file = vars[cond_id].file;
                        ir_mad->src[0].index = vars[cond_id].index;
                        setup_src_swizzle(&ir_mad->src[0], &vars[cond_id]);
                        ir_mad->src[1].file = NVFXSR_TEMP;
                        ir_mad->src[1].index = diff_reg;
                        for (int k = 0; k < 4; k++) ir_mad->src[1].swizzle[k] = k;
                        ir_mad->src[2].file = vars[false_id].file;
                        ir_mad->src[2].index = vars[false_id].index;
                        setup_src_swizzle(&ir_mad->src[2], &vars[false_id]);

                        /* Free temporary diff register */
                        if (diff_reg >= 0 && diff_reg < 48) free_temp_mask |= (1ULL << diff_reg);

                        inst_counter++;
                        release_var_if_dead(&vars[cond_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[true_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[false_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpPhi: {
                if (word_len >= 7) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t val1_id = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];
                    uint32_t b1 = swap_endian ? __builtin_bswap32(inst[4]) : inst[4];
                    uint32_t val2_id = swap_endian ? __builtin_bswap32(inst[5]) : inst[5];
                    uint32_t b2 = swap_endian ? __builtin_bswap32(inst[6]) : inst[6];

                    uint32_t cond_id = 0;
                    uint32_t true_id = val1_id;
                    uint32_t false_id = val2_id;

                    if (cur_block < bound && merge_cond[cur_block] != 0) {
                        cond_id = merge_cond[cur_block];
                        uint32_t true_b = merge_true_b[cur_block];
                        bool b1_is_true = is_reachable_block(true_b, b1, cur_block, succ1, succ2, bound);
                        if (b1_is_true) {
                            true_id = val1_id;
                            false_id = val2_id;
                        } else {
                            true_id = val2_id;
                            false_id = val1_id;
                        }
                    } else if (b1 < bound && block_cond[b1] != 0) {
                        cond_id = block_cond[b1];
                        if (!block_is_true[b1]) {
                            true_id = val2_id;
                            false_id = val1_id;
                        }
                    } else if (b2 < bound && block_cond[b2] != 0) {
                        cond_id = block_cond[b2];
                        if (block_is_true[b2]) {
                            true_id = val2_id;
                            false_id = val1_id;
                        }
                    }

                    if (cond_id != 0 && res_id < bound && ctx->num_instructions + 2 < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : ((true_id < bound && vars[true_id].vec_size) ? vars[true_id].vec_size : 4);
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        /* diff = true_val - false_val */
                        int diff_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        rsxIRInstruction *ir_sub = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_sub, 0, sizeof(*ir_sub));
                        ir_sub->opcode = RSX_IR_OP_SUB;
                        ir_sub->dst.file = NVFXSR_TEMP;
                        ir_sub->dst.index = diff_reg;
                        ir_sub->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_sub->src[0].file = vars[true_id].file;
                        ir_sub->src[0].index = vars[true_id].index;
                        setup_src_swizzle(&ir_sub->src[0], &vars[true_id]);
                        ir_sub->src[1].file = vars[false_id].file;
                        ir_sub->src[1].index = vars[false_id].index;
                        setup_src_swizzle(&ir_sub->src[1], &vars[false_id]);

                        /* res = cond * diff + false_val */
                        rsxIRInstruction *ir_mad = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_mad, 0, sizeof(*ir_mad));
                        ir_mad->opcode = RSX_IR_OP_MAD;
                        ir_mad->dst.file = vars[res_id].file;
                        ir_mad->dst.index = vars[res_id].index;
                        ir_mad->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_mad->src[0].file = vars[cond_id].file;
                        ir_mad->src[0].index = vars[cond_id].index;
                        setup_src_swizzle(&ir_mad->src[0], &vars[cond_id]);
                        ir_mad->src[1].file = NVFXSR_TEMP;
                        ir_mad->src[1].index = diff_reg;
                        for (int k = 0; k < 4; k++) ir_mad->src[1].swizzle[k] = k;
                        ir_mad->src[2].file = vars[false_id].file;
                        ir_mad->src[2].index = vars[false_id].index;
                        setup_src_swizzle(&ir_mad->src[2], &vars[false_id]);

                        if (diff_reg >= 1 && diff_reg < 48) free_temp_mask |= (1ULL << diff_reg);

                        inst_counter++;
                        release_var_if_dead(&vars[cond_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[true_id], inst_counter, &free_temp_mask);
                        release_var_if_dead(&vars[false_id], inst_counter, &free_temp_mask);
                    } else if (res_id < bound && val1_id < bound) {
                        vars[res_id] = vars[val1_id];
                    }
                }
                break;
            }

            case SpvOpDPdx:
            case SpvOpDPdxFine:
            case SpvOpDPdxCoarse: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id  = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t op1_id  = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];

                    if (res_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->opcode = RSX_IR_OP_DDX;
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir->src[0].file = vars[op1_id].file;
                        ir->src[0].index = vars[op1_id].index;
                        setup_src_swizzle(&ir->src[0], &vars[op1_id]);

                        inst_counter++;
                        release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpDPdy:
            case SpvOpDPdyFine:
            case SpvOpDPdyCoarse: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id  = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t op1_id  = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];

                    if (res_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                        memset(ir, 0, sizeof(*ir));
                        ir->opcode = RSX_IR_OP_DDY;
                        ir->dst.file = vars[res_id].file;
                        ir->dst.index = vars[res_id].index;
                        ir->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir->src[0].file = vars[op1_id].file;
                        ir->src[0].index = vars[op1_id].index;
                        setup_src_swizzle(&ir->src[0], &vars[op1_id]);

                        inst_counter++;
                        release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpFwidth:
            case SpvOpFwidthFine:
            case SpvOpFwidthCoarse: {
                if (word_len >= 4) {
                    uint32_t type_id = swap_endian ? __builtin_bswap32(inst[1]) : inst[1];
                    uint32_t res_id  = swap_endian ? __builtin_bswap32(inst[2]) : inst[2];
                    uint32_t op1_id  = swap_endian ? __builtin_bswap32(inst[3]) : inst[3];

                    if (res_id < bound && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                        vars[res_id].type_id = type_id;
                        vars[res_id].vec_size = (type_id < bound && types[type_id].vec_size) ? types[type_id].vec_size : 4;
                        vars[res_id].file = NVFXSR_TEMP;
                        vars[res_id].index = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        vars[res_id].is_live = true;

                        int dx_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);
                        int dy_reg = alloc_temp_reg(&free_temp_mask, &max_temp_used);

                        rsxIRInstruction *ir_dx = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_dx, 0, sizeof(*ir_dx));
                        ir_dx->opcode = RSX_IR_OP_DDX;
                        ir_dx->dst.file = NVFXSR_TEMP;
                        ir_dx->dst.index = dx_reg;
                        ir_dx->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_dx->src[0].file = vars[op1_id].file;
                        ir_dx->src[0].index = vars[op1_id].index;
                        setup_src_swizzle(&ir_dx->src[0], &vars[op1_id]);

                        rsxIRInstruction *ir_dy = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_dy, 0, sizeof(*ir_dy));
                        ir_dy->opcode = RSX_IR_OP_DDY;
                        ir_dy->dst.file = NVFXSR_TEMP;
                        ir_dy->dst.index = dy_reg;
                        ir_dy->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_dy->src[0].file = vars[op1_id].file;
                        ir_dy->src[0].index = vars[op1_id].index;
                        setup_src_swizzle(&ir_dy->src[0], &vars[op1_id]);

                        rsxIRInstruction *ir_add = &ctx->instructions[ctx->num_instructions++];
                        memset(ir_add, 0, sizeof(*ir_add));
                        ir_add->opcode = RSX_IR_OP_ADD;
                        ir_add->dst.file = vars[res_id].file;
                        ir_add->dst.index = vars[res_id].index;
                        ir_add->dst.writemask = (vars[res_id].vec_size >= 4) ? 0xFu : (uint8_t)((1u << vars[res_id].vec_size) - 1u);
                        ir_add->src[0].file = NVFXSR_TEMP;
                        ir_add->src[0].index = dx_reg;
                        ir_add->src[0].absolute = true;
                        for (int k = 0; k < 4; k++) ir_add->src[0].swizzle[k] = k;
                        ir_add->src[1].file = NVFXSR_TEMP;
                        ir_add->src[1].index = dy_reg;
                        ir_add->src[1].absolute = true;
                        for (int k = 0; k < 4; k++) ir_add->src[1].swizzle[k] = k;

                        if (dx_reg >= 1 && dx_reg < 48) free_temp_mask |= (1ULL << dx_reg);
                        if (dy_reg >= 1 && dy_reg < 48) free_temp_mask |= (1ULL << dy_reg);
                        inst_counter++;
                        release_var_if_dead(&vars[op1_id], inst_counter, &free_temp_mask);
                    }
                }
                break;
            }

            case SpvOpKill:
            case SpvOpDemoteToHelperInvocationEXT: {
                if (ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                    rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                    memset(ir, 0, sizeof(*ir));
                    ir->opcode = RSX_IR_OP_KIL;
                    ir->dst.file = NVFXSR_NONE;
                    if (cur_block < bound && block_cond[cur_block] != 0 && block_cond[cur_block] < bound) {
                        uint32_t c_id = block_cond[cur_block];
                        ir->src[0].file = vars[c_id].file;
                        ir->src[0].index = vars[c_id].index;
                        setup_src_swizzle(&ir->src[0], &vars[c_id]);
                        if (!block_is_true[cur_block]) {
                            ir->src[0].negate = true;
                        }
                    } else {
                        ir->src[0].file = NVFXSR_TEMP;
                        ir->src[0].index = 0;
                        for (int k = 0; k < 4; k++) ir->src[0].swizzle[k] = 0;
                    }
                    inst_counter++;
                }
                break;
            }

            default:
                break;
        }

        idx += word_len;
    }

    ctx->num_regs_used = max_temp_used + 1;
    free(vars);
    free(types);
    free(structs);
    free(block_cond);
    free(block_is_true);
    free(merge_cond);
    free(merge_true_b);
    free(merge_false_b);
    free(succ1);
    free(succ2);
    return (ctx->num_instructions > 0);
}
