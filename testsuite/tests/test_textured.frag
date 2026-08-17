FRAG
# Textured Fragment Shader with Color Tint
DCL IN[0], COLOR
DCL IN[1], GENERIC[0]
DCL CONST[0], tintColor
IMM[0] FLT32 { 0.5, 0.5, 0.5, 1.0 }
DCL OUT[0], COLOR

TEX TEMP[0], IN[1], SAMP[0], 2D
MUL TEMP[1], TEMP[0], IN[0]
MAD OUT[0], TEMP[1], CONST[0], IMM[0]
END
