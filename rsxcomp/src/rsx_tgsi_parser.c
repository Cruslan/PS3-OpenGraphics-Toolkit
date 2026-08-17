/*
 * PS3 OpenGraphics Toolkit - rsxcomp
 * Gallium3D TGSI (Tungsten Graphics Shader Infrastructure) Parser
 *
 * Based on the Mesa 3D Graphics Library Gallium3D TGSI specification.
 * Copyright (C) Mesa 3D / Tungsten Graphics / VMware, Inc.
 *
 * Parses textual and intermediate TGSI shader assembly instructions, registers,
 * swizzles, and declarations into RSX compiler IR intermediate instructions.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "rsx_compiler.h"
#include "nvfx_shader.h"

/*
 * Strip leading and trailing whitespace from string in-place
 */
static char* trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/*
 * Parse instruction opcode string into RSX IR Opcode, stripping _SAT / _PRECISE suffixes
 */
static uint32_t parse_opcode_with_modifiers(const char *tok, bool *out_saturate) {
    if (!tok || *tok == '\0') return 0;
    if (out_saturate) *out_saturate = false;

    char buf[64];
    strncpy(buf, tok, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Check for _SAT or _PRECISE suffixes (e.g. MOV_SAT, ADD_SAT) */
    size_t len = strlen(buf);
    if (len > 4 && !strcasecmp(buf + len - 4, "_SAT")) {
        if (out_saturate) *out_saturate = true;
        buf[len - 4] = '\0';
    } else if (len > 8 && !strcasecmp(buf + len - 8, "_PRECISE")) {
        buf[len - 8] = '\0';
        size_t sublen = strlen(buf);
        if (sublen > 4 && !strcasecmp(buf + sublen - 4, "_SAT")) {
            if (out_saturate) *out_saturate = true;
            buf[sublen - 4] = '\0';
        }
    }

    /* TGSI and NVFX Opcode Dictionary */
    if (!strcasecmp(buf, "MOV")) return RSX_IR_OP_MOV;
    if (!strcasecmp(buf, "MUL")) return RSX_IR_OP_MUL;
    if (!strcasecmp(buf, "ADD")) return RSX_IR_OP_ADD;
    if (!strcasecmp(buf, "SUB")) return RSX_IR_OP_SUB;
    if (!strcasecmp(buf, "MAD")) return RSX_IR_OP_MAD;
    if (!strcasecmp(buf, "DIV")) return RSX_IR_OP_DIV;
    if (!strcasecmp(buf, "DP2")) return RSX_IR_OP_DP2;
    if (!strcasecmp(buf, "DP2A")) return RSX_IR_OP_DP2A;
    if (!strcasecmp(buf, "DP3")) return RSX_IR_OP_DP3;
    if (!strcasecmp(buf, "DP4")) return RSX_IR_OP_DP4;
    if (!strcasecmp(buf, "DPH")) return RSX_IR_OP_DPH;
    if (!strcasecmp(buf, "DST")) return RSX_IR_OP_DST;
    if (!strcasecmp(buf, "MIN")) return RSX_IR_OP_MIN;
    if (!strcasecmp(buf, "MAX")) return RSX_IR_OP_MAX;
    if (!strcasecmp(buf, "SLT")) return RSX_IR_OP_SLT;
    if (!strcasecmp(buf, "SGE")) return RSX_IR_OP_SGE;
    if (!strcasecmp(buf, "SLE")) return RSX_IR_OP_SLE;
    if (!strcasecmp(buf, "SGT")) return RSX_IR_OP_SGT;
    if (!strcasecmp(buf, "SNE")) return RSX_IR_OP_SNE;
    if (!strcasecmp(buf, "SEQ")) return RSX_IR_OP_SEQ;
    if (!strcasecmp(buf, "SFL")) return RSX_IR_OP_SFL;
    if (!strcasecmp(buf, "STR")) return RSX_IR_OP_STR;
    if (!strcasecmp(buf, "SSG") || !strcasecmp(buf, "SGN")) return RSX_IR_OP_SSG;
    if (!strcasecmp(buf, "FRC")) return RSX_IR_OP_FRC;
    if (!strcasecmp(buf, "FLR")) return RSX_IR_OP_FLR;
    if (!strcasecmp(buf, "CEIL")) return RSX_IR_OP_CEIL;
    if (!strcasecmp(buf, "TRUNC")) return RSX_IR_OP_TRUNC;
    if (!strcasecmp(buf, "ROUND")) return RSX_IR_OP_ROUND;
    if (!strcasecmp(buf, "ABS")) return RSX_IR_OP_ABS;
    if (!strcasecmp(buf, "CMP")) return RSX_IR_OP_CMP;
    if (!strcasecmp(buf, "UCMP")) return RSX_IR_OP_UCMP;
    if (!strcasecmp(buf, "LRP")) return RSX_IR_OP_LRP;
    if (!strcasecmp(buf, "FMA") || !strcasecmp(buf, "DFMA")) return RSX_IR_OP_FMA;
    if (!strcasecmp(buf, "SQRT") || !strcasecmp(buf, "DSQRT")) return RSX_IR_OP_SQRT;
    if (!strcasecmp(buf, "LDEXP") || !strcasecmp(buf, "DLDEXP")) return RSX_IR_OP_LDEXP;
    if (!strcasecmp(buf, "RCP") || !strcasecmp(buf, "DRCP")) return RSX_IR_OP_RCP;
    if (!strcasecmp(buf, "RCC")) return RSX_IR_OP_RCC;
    if (!strcasecmp(buf, "RSQ") || !strcasecmp(buf, "DRSQ")) return RSX_IR_OP_RSQ;
    if (!strcasecmp(buf, "EX2")) return RSX_IR_OP_EX2;
    if (!strcasecmp(buf, "LG2")) return RSX_IR_OP_LG2;
    if (!strcasecmp(buf, "EXP")) return RSX_IR_OP_EXP;
    if (!strcasecmp(buf, "LOG")) return RSX_IR_OP_LOG;
    if (!strcasecmp(buf, "LIT")) return RSX_IR_OP_LIT;
    if (!strcasecmp(buf, "LITEX2")) return RSX_IR_OP_LITEX2;
    if (!strcasecmp(buf, "SIN")) return RSX_IR_OP_SIN;
    if (!strcasecmp(buf, "COS")) return RSX_IR_OP_COS;
    if (!strcasecmp(buf, "POW")) return RSX_IR_OP_POW;
    if (!strcasecmp(buf, "TEX")) return RSX_IR_OP_TEX;
    if (!strcasecmp(buf, "TXP")) return RSX_IR_OP_TXP;
    if (!strcasecmp(buf, "TXD")) return RSX_IR_OP_TXD;
    if (!strcasecmp(buf, "TXB")) return RSX_IR_OP_TXB;
    if (!strcasecmp(buf, "TXL")) return RSX_IR_OP_TXL;
    if (!strcasecmp(buf, "TXF")) return RSX_IR_OP_TXF;
    if (!strcasecmp(buf, "TXQ")) return RSX_IR_OP_TXQ;
    if (!strcasecmp(buf, "TXQS")) return RSX_IR_OP_TXQS;
    if (!strcasecmp(buf, "TEX2")) return RSX_IR_OP_TEX2;
    if (!strcasecmp(buf, "TXB2")) return RSX_IR_OP_TXB2;
    if (!strcasecmp(buf, "TXL2")) return RSX_IR_OP_TXL2;
    if (!strcasecmp(buf, "SAMPLE")) return RSX_IR_OP_SAMPLE;
    if (!strcasecmp(buf, "SAMPLE_I")) return RSX_IR_OP_SAMPLE_I;
    if (!strcasecmp(buf, "SAMPLE_I_MS")) return RSX_IR_OP_SAMPLE_I_MS;
    if (!strcasecmp(buf, "SAMPLE_B")) return RSX_IR_OP_SAMPLE_B;
    if (!strcasecmp(buf, "SAMPLE_C")) return RSX_IR_OP_SAMPLE_C;
    if (!strcasecmp(buf, "SAMPLE_C_LZ")) return RSX_IR_OP_SAMPLE_C_LZ;
    if (!strcasecmp(buf, "SAMPLE_D")) return RSX_IR_OP_SAMPLE_D;
    if (!strcasecmp(buf, "SAMPLE_L")) return RSX_IR_OP_SAMPLE_L;
    if (!strcasecmp(buf, "GATHER4") || !strcasecmp(buf, "TG4")) return RSX_IR_OP_GATHER4;
    if (!strcasecmp(buf, "SVIEWINFO") || !strcasecmp(buf, "RESQ")) return RSX_IR_OP_SVIEWINFO;
    if (!strcasecmp(buf, "SAMPLE_POS")) return RSX_IR_OP_SAMPLE_POS;
    if (!strcasecmp(buf, "SAMPLE_INFO")) return RSX_IR_OP_SAMPLE_INFO;
    if (!strcasecmp(buf, "LOD") || !strcasecmp(buf, "LODQ")) return RSX_IR_OP_LOD;
    if (!strcasecmp(buf, "DDX")) return RSX_IR_OP_DDX;
    if (!strcasecmp(buf, "DDY")) return RSX_IR_OP_DDY;
    if (!strcasecmp(buf, "DDX_FINE")) return RSX_IR_OP_DDX_FINE;
    if (!strcasecmp(buf, "DDY_FINE")) return RSX_IR_OP_DDY_FINE;
    if (!strcasecmp(buf, "KIL") || !strcasecmp(buf, "KILL")) return RSX_IR_OP_KIL;
    if (!strcasecmp(buf, "KILL_IF") || !strcasecmp(buf, "KILP")) return RSX_IR_OP_KILL_IF;
    if (!strcasecmp(buf, "DEMOTE")) return RSX_IR_OP_DEMOTE;
    if (!strcasecmp(buf, "ARL")) return RSX_IR_OP_ARL;
    if (!strcasecmp(buf, "ARR")) return RSX_IR_OP_ARR;
    if (!strcasecmp(buf, "ARA")) return RSX_IR_OP_ARA;
    if (!strcasecmp(buf, "UARL")) return RSX_IR_OP_UARL;
    if (!strcasecmp(buf, "PK4B")) return RSX_IR_OP_PK4B;
    if (!strcasecmp(buf, "UP4B")) return RSX_IR_OP_UP4B;
    if (!strcasecmp(buf, "PK2H")) return RSX_IR_OP_PK2H;
    if (!strcasecmp(buf, "UP2H")) return RSX_IR_OP_UP2H;
    if (!strcasecmp(buf, "PK4UB")) return RSX_IR_OP_PK4UB;
    if (!strcasecmp(buf, "UP4UB")) return RSX_IR_OP_UP4UB;
    if (!strcasecmp(buf, "PK2US")) return RSX_IR_OP_PK2US;
    if (!strcasecmp(buf, "UP2US")) return RSX_IR_OP_UP2US;
    if (!strcasecmp(buf, "PUSHA")) return RSX_IR_OP_PUSHA;
    if (!strcasecmp(buf, "POPA")) return RSX_IR_OP_POPA;
    if (!strcasecmp(buf, "BRA")) return RSX_IR_OP_BRA;
    if (!strcasecmp(buf, "CAL")) return RSX_IR_OP_CAL;
    if (!strcasecmp(buf, "RET")) return RSX_IR_OP_RET;
    if (!strcasecmp(buf, "IF")) return RSX_IR_OP_IF;
    if (!strcasecmp(buf, "UIF")) return RSX_IR_OP_UIF;
    if (!strcasecmp(buf, "ELSE")) return RSX_IR_OP_ELSE;
    if (!strcasecmp(buf, "ENDIF")) return RSX_IR_OP_ENDIF;
    if (!strcasecmp(buf, "BGNLOOP")) return RSX_IR_OP_BGNLOOP;
    if (!strcasecmp(buf, "ENDLOOP")) return RSX_IR_OP_ENDLOOP;
    if (!strcasecmp(buf, "BGNSUB")) return RSX_IR_OP_BGNSUB;
    if (!strcasecmp(buf, "ENDSUB")) return RSX_IR_OP_ENDSUB;
    if (!strcasecmp(buf, "BRK")) return RSX_IR_OP_BRK;
    if (!strcasecmp(buf, "CONT")) return RSX_IR_OP_CONT;
    if (!strcasecmp(buf, "NOT")) return RSX_IR_OP_NOT;
    if (!strcasecmp(buf, "AND")) return RSX_IR_OP_AND;
    if (!strcasecmp(buf, "OR")) return RSX_IR_OP_OR;
    if (!strcasecmp(buf, "XOR")) return RSX_IR_OP_XOR;
    if (!strcasecmp(buf, "SHL") || !strcasecmp(buf, "U64SHL")) return RSX_IR_OP_SHL;
    if (!strcasecmp(buf, "ISHR") || !strcasecmp(buf, "I64SHR")) return RSX_IR_OP_ISHR;
    if (!strcasecmp(buf, "USHR") || !strcasecmp(buf, "U64SHR")) return RSX_IR_OP_USHR;
    if (!strcasecmp(buf, "MOD") || !strcasecmp(buf, "I64MOD")) return RSX_IR_OP_MOD;
    if (!strcasecmp(buf, "UMOD") || !strcasecmp(buf, "U64MOD")) return RSX_IR_OP_UMOD;
    if (!strcasecmp(buf, "FSEQ") || !strcasecmp(buf, "DSEQ") || !strcasecmp(buf, "U64SEQ")) return RSX_IR_OP_FSEQ;
    if (!strcasecmp(buf, "FSGE") || !strcasecmp(buf, "DSGE") || !strcasecmp(buf, "U64SGE") || !strcasecmp(buf, "I64SGE")) return RSX_IR_OP_FSGE;
    if (!strcasecmp(buf, "FSLT") || !strcasecmp(buf, "DSLT") || !strcasecmp(buf, "U64SLT") || !strcasecmp(buf, "I64SLT")) return RSX_IR_OP_FSLT;
    if (!strcasecmp(buf, "FSNE") || !strcasecmp(buf, "DSNE") || !strcasecmp(buf, "U64SNE")) return RSX_IR_OP_FSNE;
    if (!strcasecmp(buf, "F2I") || !strcasecmp(buf, "D2I") || !strcasecmp(buf, "F2I64") || !strcasecmp(buf, "D2I64")) return RSX_IR_OP_F2I;
    if (!strcasecmp(buf, "I2F") || !strcasecmp(buf, "I2D") || !strcasecmp(buf, "I642F") || !strcasecmp(buf, "I642D")) return RSX_IR_OP_I2F;
    if (!strcasecmp(buf, "F2U") || !strcasecmp(buf, "D2U") || !strcasecmp(buf, "F2U64") || !strcasecmp(buf, "D2U64")) return RSX_IR_OP_F2U;
    if (!strcasecmp(buf, "U2F") || !strcasecmp(buf, "U2D") || !strcasecmp(buf, "U642F") || !strcasecmp(buf, "U642D")) return RSX_IR_OP_U2F;
    if (!strcasecmp(buf, "INEG") || !strcasecmp(buf, "DNEG") || !strcasecmp(buf, "I64NEG")) return RSX_IR_OP_INEG;
    if (!strcasecmp(buf, "IABS") || !strcasecmp(buf, "DABS") || !strcasecmp(buf, "I64ABS")) return RSX_IR_OP_IABS;
    if (!strcasecmp(buf, "ISSG") || !strcasecmp(buf, "DSSG") || !strcasecmp(buf, "I64SSG")) return RSX_IR_OP_ISSG;
    if (!strcasecmp(buf, "IMIN") || !strcasecmp(buf, "DMIN") || !strcasecmp(buf, "I64MIN")) return RSX_IR_OP_IMIN;
    if (!strcasecmp(buf, "IMAX") || !strcasecmp(buf, "DMAX") || !strcasecmp(buf, "I64MAX")) return RSX_IR_OP_IMAX;
    if (!strcasecmp(buf, "UMIN") || !strcasecmp(buf, "U64MIN")) return RSX_IR_OP_UMIN;
    if (!strcasecmp(buf, "UMAX") || !strcasecmp(buf, "U64MAX")) return RSX_IR_OP_UMAX;
    if (!strcasecmp(buf, "UADD") || !strcasecmp(buf, "DADD") || !strcasecmp(buf, "U64ADD")) return RSX_IR_OP_UADD;
    if (!strcasecmp(buf, "UDIV") || !strcasecmp(buf, "DDIV") || !strcasecmp(buf, "U64DIV")) return RSX_IR_OP_UDIV;
    if (!strcasecmp(buf, "IDIV") || !strcasecmp(buf, "I64DIV")) return RSX_IR_OP_IDIV;
    if (!strcasecmp(buf, "UMUL") || !strcasecmp(buf, "DMUL") || !strcasecmp(buf, "U64MUL")) return RSX_IR_OP_UMUL;
    if (!strcasecmp(buf, "UMAD") || !strcasecmp(buf, "DMAD")) return RSX_IR_OP_UMAD;
    if (!strcasecmp(buf, "USEQ")) return RSX_IR_OP_USEQ;
    if (!strcasecmp(buf, "USGE")) return RSX_IR_OP_USGE;
    if (!strcasecmp(buf, "USLT")) return RSX_IR_OP_USLT;
    if (!strcasecmp(buf, "USNE")) return RSX_IR_OP_USNE;
    if (!strcasecmp(buf, "ISGE")) return RSX_IR_OP_ISGE;
    if (!strcasecmp(buf, "ISLT")) return RSX_IR_OP_ISLT;
    if (!strcasecmp(buf, "SWITCH")) return RSX_IR_OP_SWITCH;
    if (!strcasecmp(buf, "CASE")) return RSX_IR_OP_CASE;
    if (!strcasecmp(buf, "DEFAULT")) return RSX_IR_OP_DEFAULT;
    if (!strcasecmp(buf, "ENDSWITCH")) return RSX_IR_OP_ENDSWITCH;
    if (!strcasecmp(buf, "LOAD")) return RSX_IR_OP_LOAD;
    if (!strcasecmp(buf, "STORE")) return RSX_IR_OP_STORE;
    if (!strcasecmp(buf, "BARRIER")) return RSX_IR_OP_BARRIER;
    if (!strcasecmp(buf, "MEMBAR")) return RSX_IR_OP_MEMBAR;
    if (!strcasecmp(buf, "EMIT")) return RSX_IR_OP_EMIT;
    if (!strcasecmp(buf, "ENDPRIM")) return RSX_IR_OP_ENDPRIM;
    if (!strcasecmp(buf, "FBFETCH")) return RSX_IR_OP_FBFETCH;
    if (!strcasecmp(buf, "RFL")) return RSX_IR_OP_RFL;
    if (!strcasecmp(buf, "NOP")) return RSX_IR_OP_NOP;
    if (!strcasecmp(buf, "IBFE")) return RSX_IR_OP_IBFE;
    if (!strcasecmp(buf, "UBFE")) return RSX_IR_OP_UBFE;
    if (!strcasecmp(buf, "BFI")) return RSX_IR_OP_BFI;
    if (!strcasecmp(buf, "BREV")) return RSX_IR_OP_BREV;
    if (!strcasecmp(buf, "POPC")) return RSX_IR_OP_POPC;
    if (!strcasecmp(buf, "LSB")) return RSX_IR_OP_LSB;
    if (!strcasecmp(buf, "IMSB")) return RSX_IR_OP_IMSB;
    if (!strcasecmp(buf, "UMSB")) return RSX_IR_OP_UMSB;
    if (!strcasecmp(buf, "BALLOT")) return RSX_IR_OP_BALLOT;
    if (!strcasecmp(buf, "READ_INVOC")) return RSX_IR_OP_READ_INVOC;
    if (!strcasecmp(buf, "READ_FIRST")) return RSX_IR_OP_READ_FIRST;
    if (!strcasecmp(buf, "READ_HELPER")) return RSX_IR_OP_READ_HELPER;
    if (!strcasecmp(buf, "CLOCK")) return RSX_IR_OP_CLOCK;
    if (!strcasecmp(buf, "IMG2HND")) return RSX_IR_OP_IMG2HND;
    if (!strcasecmp(buf, "SAMP2HND")) return RSX_IR_OP_SAMP2HND;
    if (!strcasecmp(buf, "ATOMUADD")) return RSX_IR_OP_ATOMUADD;
    if (!strcasecmp(buf, "ATOMXCHG")) return RSX_IR_OP_ATOMXCHG;
    if (!strcasecmp(buf, "ATOMCAS")) return RSX_IR_OP_ATOMCAS;
    if (!strcasecmp(buf, "ATOMAND")) return RSX_IR_OP_ATOMAND;
    if (!strcasecmp(buf, "ATOMOR")) return RSX_IR_OP_ATOMOR;
    if (!strcasecmp(buf, "ATOMXOR")) return RSX_IR_OP_ATOMXOR;
    if (!strcasecmp(buf, "ATOMUMIN")) return RSX_IR_OP_ATOMUMIN;
    if (!strcasecmp(buf, "ATOMUMAX")) return RSX_IR_OP_ATOMUMAX;
    if (!strcasecmp(buf, "ATOMIMIN")) return RSX_IR_OP_ATOMIMIN;
    if (!strcasecmp(buf, "ATOMIMAX")) return RSX_IR_OP_ATOMIMAX;
    if (!strcasecmp(buf, "ATOMFADD")) return RSX_IR_OP_ATOMFADD;
    if (!strcasecmp(buf, "ATOMINC_WRAP")) return RSX_IR_OP_ATOMINC_WRAP;
    if (!strcasecmp(buf, "ATOMDEC_WRAP")) return RSX_IR_OP_ATOMDEC_WRAP;
    if (!strcasecmp(buf, "IMUL_HI")) return RSX_IR_OP_IMUL_HI;
    if (!strcasecmp(buf, "UMUL_HI")) return RSX_IR_OP_UMUL_HI;
    if (!strcasecmp(buf, "INTERP_CENTROID")) return RSX_IR_OP_INTERP_CENTROID;
    if (!strcasecmp(buf, "INTERP_SAMPLE")) return RSX_IR_OP_INTERP_SAMPLE;
    if (!strcasecmp(buf, "INTERP_OFFSET")) return RSX_IR_OP_INTERP_OFFSET;
    if (!strcasecmp(buf, "F2D")) return RSX_IR_OP_F2D;
    if (!strcasecmp(buf, "D2F")) return RSX_IR_OP_D2F;
    if (!strcasecmp(buf, "DFRAC") || !strcasecmp(buf, "DFRC")) return RSX_IR_OP_DFRAC;
    if (!strcasecmp(buf, "DTRUNC")) return RSX_IR_OP_DTRUNC;
    if (!strcasecmp(buf, "DCEIL")) return RSX_IR_OP_DCEIL;
    if (!strcasecmp(buf, "DFLR")) return RSX_IR_OP_DFLR;
    if (!strcasecmp(buf, "DROUND")) return RSX_IR_OP_DROUND;
    if (!strcasecmp(buf, "VOTE_ANY")) return RSX_IR_OP_VOTE_ANY;
    if (!strcasecmp(buf, "VOTE_ALL")) return RSX_IR_OP_VOTE_ALL;
    if (!strcasecmp(buf, "VOTE_EQ")) return RSX_IR_OP_VOTE_EQ;
    if (!strcasecmp(buf, "END")) return 0xFFFFFFFF;
    return 0;
}

static uint8_t parse_swizzle_char(char c) {
    switch (tolower((unsigned char)c)) {
        case 'x': case 'r': return NVFX_SWZ_X;
        case 'y': case 'g': return NVFX_SWZ_Y;
        case 'z': case 'b': return NVFX_SWZ_Z;
        case 'w': case 'a': return NVFX_SWZ_W;
        default: return NVFX_SWZ_X;
    }
}

static void parse_dst_reg(const char *str, rsxIRDst *dst) {
    dst->file = NVFXSR_TEMP;
    dst->index = 0;
    dst->writemask = 0xF;

    char buf[64];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *dot = strchr(buf, '.');
    if (dot) {
        *dot = '\0';
        char *mask = dot + 1;
        uint8_t wm = 0;
        for (int i = 0; mask[i]; i++) {
            switch (tolower((unsigned char)mask[i])) {
                case 'x': case 'r': wm |= 1; break;
                case 'y': case 'g': wm |= 2; break;
                case 'z': case 'b': wm |= 4; break;
                case 'w': case 'a': wm |= 8; break;
            }
        }
        if (wm) dst->writemask = wm;
    }

    if (!strncasecmp(buf, "OUT[", 4)) {
        dst->file = NVFXSR_OUTPUT;
        dst->index = atoi(buf + 4);
    } else if (!strncasecmp(buf, "TEMP[", 5)) {
        dst->file = NVFXSR_TEMP;
        dst->index = atoi(buf + 5);
    } else if (!strncasecmp(buf, "ADDR[", 5)) {
        dst->file = NVFXSR_TEMP;
        dst->index = atoi(buf + 5);
    } else if (tolower((unsigned char)buf[0]) == 'r' && isdigit((unsigned char)buf[1])) {
        dst->file = NVFXSR_TEMP;
        dst->index = atoi(buf + 1);
    } else if (tolower((unsigned char)buf[0]) == 'o' && isdigit((unsigned char)buf[1])) {
        dst->file = NVFXSR_OUTPUT;
        dst->index = atoi(buf + 1);
    } else if (!strcasecmp(buf, "oPos")) {
        dst->file = NVFXSR_OUTPUT;
        dst->index = 0;
    } else if (!strcasecmp(buf, "oCol") || !strcasecmp(buf, "oCol0")) {
        dst->file = NVFXSR_OUTPUT;
        dst->index = 1;
    } else if (!strcasecmp(buf, "oCol1") || !strcasecmp(buf, "oBCol0")) {
        dst->file = NVFXSR_OUTPUT;
        dst->index = 2;
    } else if (!strcasecmp(buf, "oFog")) {
        dst->file = NVFXSR_OUTPUT;
        dst->index = 6;
    } else if (!strcasecmp(buf, "oPSize")) {
        dst->file = NVFXSR_OUTPUT;
        dst->index = 7;
    } else if (!strncasecmp(buf, "oTex", 4)) {
        dst->file = NVFXSR_OUTPUT;
        dst->index = 8 + atoi(buf + 4);
    } else if (!strcasecmp(buf, "NULL")) {
        dst->file = NVFXSR_NONE;
        dst->index = 0;
    }
}

static void parse_src_reg(rsxCompilerContext *ctx, const char *str, rsxIRSrc *src) {
    src->file = NVFXSR_TEMP;
    src->index = 0;
    src->swizzle[0] = NVFX_SWZ_X;
    src->swizzle[1] = NVFX_SWZ_Y;
    src->swizzle[2] = NVFX_SWZ_Z;
    src->swizzle[3] = NVFX_SWZ_W;
    src->negate = false;
    src->absolute = false;

    char buf[64];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *ptr = buf;

    /* Check for negate and absolute modifiers (-|x|, |-x|, |x|, -x) */
    while (*ptr == '-' || *ptr == '|') {
        if (*ptr == '-') {
            src->negate = !src->negate;
            ptr++;
        } else if (*ptr == '|') {
            src->absolute = true;
            ptr++;
            char *bar = strrchr(ptr, '|');
            if (bar) *bar = '\0';
        }
    }

    char *dot = strchr(ptr, '.');
    if (dot) {
        *dot = '\0';
        char *swz = dot + 1;
        int len = strlen(swz);
        for (int i = 0; i < 4; i++) {
            if (i < len) src->swizzle[i] = parse_swizzle_char(swz[i]);
            else src->swizzle[i] = parse_swizzle_char(swz[len - 1]);
        }
    }

    /* Check Inline Vector Literal: (1.0, 0.0, 0.0, 1.0) or {1.0, 0.0, 0.0, 1.0} */
    if (*ptr == '(' || *ptr == '{') {
        float vals[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        sscanf(ptr + 1, "%f , %f , %f , %f", &vals[0], &vals[1], &vals[2], &vals[3]);
        if (ctx && ctx->num_constants < RSX_MAX_CONSTANTS) {
            int imm_idx = (int)ctx->num_constants++;
            rsxCompilerConst *c = &ctx->constants[imm_idx];
            memset(c, 0, sizeof(*c));
            c->index = imm_idx;
            c->type = PARAM_FLOAT4;
            c->is_internal = true;
            memcpy(c->values, vals, sizeof(vals));
            snprintf(c->name, sizeof(c->name), "lit%d", imm_idx);
            src->file = NVFXSR_IMM;
            src->index = imm_idx;
            return;
        }
    }

    if (!strncasecmp(ptr, "IN[", 3)) {
        src->file = NVFXSR_INPUT;
        src->index = atoi(ptr + 3);
    } else if (!strncasecmp(ptr, "TEMP[", 5)) {
        src->file = NVFXSR_TEMP;
        src->index = atoi(ptr + 5);
    } else if (!strncasecmp(ptr, "CONST[", 6)) {
        src->file = NVFXSR_CONST;
        /* Check 2D constant syntax CONST[0][n] */
        char *bracket2 = strstr(ptr + 6, "][");
        if (bracket2) {
            src->index = atoi(bracket2 + 2);
        } else {
            src->index = atoi(ptr + 6);
        }
    } else if (!strncasecmp(ptr, "IMM[", 4)) {
        src->file = NVFXSR_IMM;
        src->index = atoi(ptr + 4);
    } else if (!strncasecmp(ptr, "SAMP[", 5)) {
        src->file = NVFXSR_TEMP;
        src->index = atoi(ptr + 5);
    } else if (!strncasecmp(ptr, "SV[", 3)) {
        src->file = NVFXSR_INPUT;
        src->index = atoi(ptr + 3);
    } else if (!strncasecmp(ptr, "ADDR[", 5)) {
        src->file = NVFXSR_TEMP;
        src->index = atoi(ptr + 5);
    } else if (tolower((unsigned char)ptr[0]) == 'r' && isdigit((unsigned char)ptr[1])) {
        src->file = NVFXSR_TEMP;
        src->index = atoi(ptr + 1);
    } else if (tolower((unsigned char)ptr[0]) == 'v' && isdigit((unsigned char)ptr[1])) {
        src->file = NVFXSR_INPUT;
        src->index = atoi(ptr + 1);
    } else if (tolower((unsigned char)ptr[0]) == 'c' && isdigit((unsigned char)ptr[1])) {
        src->file = NVFXSR_CONST;
        src->index = atoi(ptr + 1);
    } else if (tolower((unsigned char)ptr[0]) == 'i' && isdigit((unsigned char)ptr[1])) {
        src->file = NVFXSR_IMM;
        src->index = atoi(ptr + 1);
    } else if (tolower((unsigned char)ptr[0]) == 's' && isdigit((unsigned char)ptr[1])) {
        src->file = NVFXSR_TEMP;
        src->index = atoi(ptr + 1);
    } else if (!strcasecmp(ptr, "vPos")) {
        src->file = NVFXSR_INPUT;
        src->index = 0;
    } else if (!strcasecmp(ptr, "vCol") || !strcasecmp(ptr, "vCol0")) {
        src->file = NVFXSR_INPUT;
        src->index = 1;
    } else if (!strcasecmp(ptr, "vCol1")) {
        src->file = NVFXSR_INPUT;
        src->index = 2;
    } else if (!strcasecmp(ptr, "vNormal") || !strcasecmp(ptr, "vNorm")) {
        src->file = NVFXSR_INPUT;
        src->index = 2;
    } else if (!strcasecmp(ptr, "vTex")) {
        src->file = NVFXSR_INPUT;
        src->index = 2;
    } else if (!strncasecmp(ptr, "vTex", 4)) {
        src->file = NVFXSR_INPUT;
        src->index = 8 + atoi(ptr + 4);
    }
}

/*
 * Parse TGSI source string into Compiler Context IR
 */
bool rsx_compiler_parse_tgsi(rsxCompilerContext *ctx, const char *tgsi_source) {
    if (!ctx || !tgsi_source) return false;

    char *source_copy = strdup(tgsi_source);
    if (!source_copy) return false;

    char *line_saveptr = NULL;
    char *line = strtok_r(source_copy, "\r\n", &line_saveptr);
    while (line) {
        /* Strip inline comments: //, #, ; */
        char *comment_pos = strstr(line, "//");
        if (!comment_pos) comment_pos = strchr(line, '#');
        if (!comment_pos) comment_pos = strchr(line, ';');
        if (comment_pos) *comment_pos = '\0';

        char *trimmed = trim_whitespace(line);
        if (*trimmed == '\0') {
            line = strtok_r(NULL, "\r\n", &line_saveptr);
            continue;
        }

        /* Check and strip instruction numeric label: e.g. "0: MOV ..." or "10: ADD ..." */
        if (isdigit((unsigned char)*trimmed)) {
            char *colon = strchr(trimmed, ':');
            if (colon) {
                trimmed = trim_whitespace(colon + 1);
            }
        }
        if (*trimmed == '\0') {
            line = strtok_r(NULL, "\r\n", &line_saveptr);
            continue;
        }

        /* Check Shader Type Header */
        if (!strcasecmp(trimmed, "VERT") || !strncasecmp(trimmed, "VERT:", 5)) {
            ctx->type = RSX_PROGRAM_VERTEX;
            line = strtok_r(NULL, "\r\n", &line_saveptr);
            continue;
        }
        if (!strcasecmp(trimmed, "FRAG") || !strncasecmp(trimmed, "FRAG:", 5)) {
            ctx->type = RSX_PROGRAM_FRAGMENT;
            line = strtok_r(NULL, "\r\n", &line_saveptr);
            continue;
        }

        /* Check Declarations */
        if (!strncasecmp(trimmed, "DCL", 3)) {
            char dcl_buf[128];
            strncpy(dcl_buf, trimmed + 3, sizeof(dcl_buf) - 1);
            dcl_buf[sizeof(dcl_buf) - 1] = '\0';
            char *dcl_trim = trim_whitespace(dcl_buf);

            if (!strncasecmp(dcl_trim, "IN[", 3)) {
                int idx = atoi(dcl_trim + 3);
                if (ctx->num_attributes < RSX_MAX_ATTRIBUTES) {
                    rsxCompilerAttrib *attr = &ctx->attributes[ctx->num_attributes++];
                    attr->index = idx;
                    attr->type = PARAM_FLOAT4;
                    char *sem = strchr(dcl_trim, ',');
                    if (sem) {
                        strncpy(attr->name, trim_whitespace(sem + 1), sizeof(attr->name) - 1);
                    } else {
                        snprintf(attr->name, sizeof(attr->name), "attr%d", idx);
                    }
                }
            } else if (!strncasecmp(dcl_trim, "CONST[", 6)) {
                int idx = atoi(dcl_trim + 6);
                if (ctx->num_constants < RSX_MAX_CONSTANTS) {
                    rsxCompilerConst *c = &ctx->constants[ctx->num_constants++];
                    c->index = idx;
                    c->type = PARAM_FLOAT4;
                    c->is_internal = false;
                    char *name = strchr(dcl_trim, ',');
                    if (name) {
                        strncpy(c->name, trim_whitespace(name + 1), sizeof(c->name) - 1);
                    } else {
                        snprintf(c->name, sizeof(c->name), "c%d", idx);
                    }
                }
            }
            line = strtok_r(NULL, "\r\n", &line_saveptr);
            continue;
        }

        /* Check Immediates: IMM[0] FLT32 { 1.0, 0.0, 0.0, 1.0 } */
        if (!strncasecmp(trimmed, "IMM[", 4)) {
            int imm_idx = atoi(trimmed + 4);
            char *brace = strchr(trimmed, '{');
            if (brace) {
                float v[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                sscanf(brace + 1, "%f , %f , %f , %f", &v[0], &v[1], &v[2], &v[3]);
                if (ctx->num_constants < RSX_MAX_CONSTANTS) {
                    rsxCompilerConst *c = &ctx->constants[ctx->num_constants++];
                    c->index = imm_idx;
                    c->type = PARAM_FLOAT4;
                    c->is_internal = true;
                    memcpy(c->values, v, sizeof(v));
                    snprintf(c->name, sizeof(c->name), "imm%d", imm_idx);
                }
            }
            line = strtok_r(NULL, "\r\n", &line_saveptr);
            continue;
        }

        /* Check Instructions */
        char op_str[32] = {0};
        char args_str[256] = {0};
        if (sscanf(trimmed, "%31s %255[^\n]", op_str, args_str) >= 1) {
            bool is_sat = false;
            uint32_t op = parse_opcode_with_modifiers(op_str, &is_sat);
            if (op == 0xFFFFFFFF) {
                /* END reached */
                break;
            }

            if (op != 0 && ctx->num_instructions < RSX_MAX_INSTRUCTIONS) {
                rsxIRInstruction *ir = &ctx->instructions[ctx->num_instructions++];
                memset(ir, 0, sizeof(*ir));
                ir->opcode = op;
                ir->saturate = is_sat;

                char *arg_saveptr = NULL;
                char *arg_tok = strtok_r(args_str, ",", &arg_saveptr);
                int arg_idx = 0;
                while (arg_tok) {
                    char *arg_clean = trim_whitespace(arg_tok);
                    if (arg_idx == 0) {
                        parse_dst_reg(arg_clean, &ir->dst);
                    } else if (arg_idx <= 3) {
                        /* Check if arg is a texture sampler argument: SAMP[n] or s<n> */
                        if (!strncasecmp(arg_clean, "SAMP[", 5) ||
                            (tolower((unsigned char)arg_clean[0]) == 's' && isdigit((unsigned char)arg_clean[1]))) {
                            ir->tex_unit = (!strncasecmp(arg_clean, "SAMP[", 5)) ? atoi(arg_clean + 5) : atoi(arg_clean + 1);
                        }
                        parse_src_reg(ctx, arg_clean, &ir->src[arg_idx - 1]);
                    } else if (arg_idx == 4) {
                        /* Texture sampler target or unit */
                        if (!strcasecmp(arg_clean, "1D")) ir->tex_target = 1;
                        else if (!strcasecmp(arg_clean, "2D") || !strcasecmp(arg_clean, "RECT") || !strcasecmp(arg_clean, "SHADOW2D")) ir->tex_target = 2;
                        else if (!strcasecmp(arg_clean, "3D")) ir->tex_target = 3;
                        else if (!strcasecmp(arg_clean, "CUBE") || !strcasecmp(arg_clean, "SHADOWCUBE")) ir->tex_target = 4;
                        else if (!strcasecmp(arg_clean, "2D_ARRAY")) ir->tex_target = 2;
                        else ir->tex_target = 2;
                    }
                    arg_idx++;
                    arg_tok = strtok_r(NULL, ",", &arg_saveptr);
                }
            }
        }

        line = strtok_r(NULL, "\r\n", &line_saveptr);
    }

    free(source_copy);
    return (ctx->num_instructions > 0);
}
