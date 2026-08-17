VERT
# Comprehensive test of Mesa TGSI features and RSX Vertex Program translation
DCL IN[0], POSITION
DCL IN[1], NORMAL
DCL IN[2], COLOR
DCL CONST[0..3]
IMM[0] FLT32 { 1.0, 0.0, 0.0, 1.0 }

0: DP4 OUT[0].x, IN[0], CONST[0] // Matrix multiply X
1: DP4 OUT[0].y, IN[0], CONST[1] # Matrix multiply Y
2: DP4 OUT[0].z, IN[0], CONST[2] ; Matrix multiply Z
3: DP4 OUT[0].w, IN[0], CONST[3]

# Test _SAT modifier and immediate literals
4: MOV_SAT TEMP[0], (0.5, 0.5, 0.5, 1.0)
5: ADD_SAT TEMP[1], TEMP[0], IN[2]
6: MUL_SAT TEMP[2], TEMP[1], CONST[0]
7: MAD_SAT TEMP[3], TEMP[2], IN[1], (0.1, 0.2, 0.3, 1.0)

# Test scalar ops with numeric labels and comments
8: RCP TEMP[4].x, IN[1].x // RCP scalar
9: RSQ TEMP[5].x, IN[1].y # RSQ scalar
10: EX2 TEMP[6].x, IN[1].z
11: LG2 TEMP[7].x, IN[1].w
12: SIN TEMP[8].x, IN[2].x
13: COS TEMP[9].x, IN[2].y
14: LIT TEMP[10], IN[1]

# Test vector and math ops
15: DP3 TEMP[11], IN[1], IN[1]
16: DP2 TEMP[12], IN[1], IN[2]
17: MIN TEMP[13], TEMP[11], TEMP[12]
18: MAX TEMP[14], TEMP[13], IMM[0]
19: SLT TEMP[15], TEMP[14], (0.8, 0.8, 0.8, 0.8)
20: SGE TEMP[16], TEMP[15], CONST[1]
21: SEQ TEMP[17], TEMP[16], TEMP[0]
22: FRC TEMP[18], TEMP[3]
23: FLR TEMP[19], TEMP[18]

24: MAD OUT[1], TEMP[19], IN[2], IMM[0]
END
