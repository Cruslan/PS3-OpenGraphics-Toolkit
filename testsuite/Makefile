export PS3DEV	?= $(realpath $(CURDIR)/../ps3dev)
export PSL1GHT	?= $(PS3DEV)

TITLE		:= PS3 OpenGraphics TestSuite
APPID		:= OGTS00001
CONTENTID	:= UP0001-$(APPID)_00-0000000000000000
ICON0		:= ICON0.PNG
SFOXML		:= sfo.xml

# Absolute tool paths for isolated local SDK builds
CC			:= $(PS3DEV)/ppu/bin/ppu-gcc
PPU_AS		:= $(PS3DEV)/ppu/bin/ppu-as
STRIP		:= $(PS3DEV)/ppu/bin/ppu-strip
SPRX		:= $(PS3DEV)/bin/sprxlinker
BIN2S		:= $(PS3DEV)/bin/bin2s
HOST_CC		?= gcc

# Offline RSX Shader Compiler (PS3-OpenGraphics-Toolkit FOSS alternative)
PS3_SHADER_COMPILER_DIR ?= $(realpath $(CURDIR)/..)
PS3_SHADER_COMPILER     ?= $(shell if [ -f $(PS3_SHADER_COMPILER_DIR)/build/rsxcomp ]; then echo $(PS3_SHADER_COMPILER_DIR)/build/rsxcomp; elif [ -f $(PS3_SHADER_COMPILER_DIR)/rsxcomp/rsxcomp ]; then echo $(PS3_SHADER_COMPILER_DIR)/rsxcomp/rsxcomp; else echo $(PS3_SHADER_COMPILER_DIR)/rsxcomp; fi)

MAKE_SELF_NPDRM  := $(PS3DEV)/bin/make_self_npdrm
FSELF		     := $(PS3DEV)/bin/fself
SFO			     := $(PS3DEV)/bin/sfo
PKG			     := $(PS3DEV)/bin/pkg
PACKAGE_FINALIZE := $(PS3DEV)/bin/package_finalize

TARGET		:= ps3_opengraphics_testsuite
BUILD		:= build
TIMESTAMP	:= $(shell date +%Y%m%d_%H%M%S)
OUT_PKG			:= $(BUILD)/$(TARGET)-$(TIMESTAMP).pkg
OUT_GNPDRM_PKG	:= $(BUILD)/$(TARGET)-$(TIMESTAMP).gnpdrm.pkg
STD_PKG			:= $(TARGET).pkg
STD_GNPDRM_PKG	:= $(TARGET).gnpdrm.pkg

# Host C generation utilities
BUNDLE_BUILDER	:= $(BUILD)/build_test_suite_bundle
SHADER_GEN		:= $(BUILD)/generate_test_shaders

# Embedded TrueType Font binary
FONT_SRC	:= data/font.ttf
FONT_ASM	:= $(BUILD)/font.s
FONT_OBJ	:= $(BUILD)/font_ttf.o

# RSX GPU Hardware Ray Tracing Shaders
RSX_VS_SRC	:= shaders/test_01_rsxrt.vcg
RSX_VS_BIN	:= $(BUILD)/rsxrt.vpo
RSX_VS_ASM	:= $(BUILD)/rsxrt_vpo.s
RSX_VS_OBJ	:= $(BUILD)/rsxrt_vpo.o

RSX_FS_SRC	:= shaders/test_01_rsxrt.fcg
RSX_FS_BIN	:= $(BUILD)/rsxrt.fpo
RSX_FS_ASM	:= $(BUILD)/rsxrt_fpo.s
RSX_FS_OBJ	:= $(BUILD)/rsxrt_fpo.o

# PPU objects
OFILES		:= $(BUILD)/source/main.o $(BUILD)/source/rsxrt.o $(BUILD)/source/test_suite.o $(BUILD)/source/test_suite_data.o $(FONT_OBJ) $(RSX_VS_OBJ) $(RSX_FS_OBJ)

# Compiler flags for Cell Broadband Engine PPU
CFLAGS		+= -mcpu=cell -maltivec -O2 -Wall -Wextra -Werror=implicit-function-declaration -MMD -MP \
			   -I$(PS3DEV)/ppu/include -I$(PS3DEV)/portlibs/ppu/include -I$(PS3DEV)/portlibs/ppu/include/freetype2 \
			   -Iinclude -fno-lto

LDFLAGS		+= -fno-lto -Wl,--no-undefined -Wl,--as-needed

# Comprehensive PSL1GHT runtime library list with circular dependency resolution
LIBS		:= -L$(PS3DEV)/ppu/lib -L$(PS3DEV)/portlibs/ppu/lib \
			   -Wl,--start-group \
			   -lfont3d -ltiny3d -lfreetype -lvdec -lsysmodule -lsysutil -lio -lrsx -lgcm_sys \
			   -lnet -lnetctl -laudio -lcurl -lpolarssl -lrt -llv2 -lm -lz \
			   -Wl,--end-group

.PHONY: all clean pkg shaders prepare generate-shaders

all: pkg

# Forward prepare target to root Makefile to provision the ps3dev SDK
prepare:
	@$(MAKE) -C .. prepare

shaders: $(RSX_VS_BIN) $(RSX_FS_BIN) include/test_suite_data.h source/test_suite_data.c

# --- Embedded TTF Build Targets ---
$(FONT_ASM): $(FONT_SRC)
	@mkdir -p $(BUILD)
	@echo "[BIN2S] $< -> $@"
	@$(BIN2S) -a 128 $< > $@

$(FONT_OBJ): $(FONT_ASM)
	@echo "[PPU-AS] $< -> $@"
	@$(PPU_AS) $< -o $@

$(PS3_SHADER_COMPILER):
	@echo "[BUILD] Building PS3 Shader Compiler (rsxcomp)..."
	@if [ -d $(PS3_SHADER_COMPILER_DIR)/rsxcomp ]; then $(MAKE) -C $(PS3_SHADER_COMPILER_DIR)/rsxcomp; else $(MAKE) -C $(PS3_SHADER_COMPILER_DIR) rsxcomp; fi

$(RSX_VS_BIN): $(RSX_VS_SRC) $(PS3_SHADER_COMPILER)
	@mkdir -p $(BUILD)
	@echo "[PS3-SHADER-COMPILER-VS] $< -> $@"
	@$(PS3_SHADER_COMPILER) -v -i $< -o $@

$(RSX_VS_ASM): $(RSX_VS_BIN)
	@echo "[BIN2S] $< -> $@"
	@$(BIN2S) -a 128 $< > $@

$(RSX_VS_OBJ): $(RSX_VS_ASM)
	@echo "[PPU-AS] $< -> $@"
	@$(PPU_AS) $< -o $@

$(RSX_FS_BIN): $(RSX_FS_SRC) $(PS3_SHADER_COMPILER)
	@mkdir -p $(BUILD)
	@echo "[PS3-SHADER-COMPILER-FS] $< -> $@"
	@$(PS3_SHADER_COMPILER) -f -i $< -o $@

$(RSX_FS_ASM): $(RSX_FS_BIN)
	@echo "[BIN2S] $< -> $@"
	@$(BIN2S) -a 128 $< > $@

$(RSX_FS_OBJ): $(RSX_FS_ASM)
	@echo "[PPU-AS] $< -> $@"
	@$(PPU_AS) $< -o $@

# --- Host C Tool Build Targets ---
$(BUNDLE_BUILDER): tools/build_test_suite_bundle.c
	@mkdir -p $(BUILD)
	@echo "[HOST-CC] $< -> $@"
	@$(HOST_CC) -O2 -Wall -Wextra $< -o $@

$(SHADER_GEN): tools/generate_test_shaders.c
	@mkdir -p $(BUILD)
	@echo "[HOST-CC] $< -> $@"
	@$(HOST_CC) -O2 -Wall -Wextra $< -o $@

generate-shaders: $(SHADER_GEN)
	@echo "[GENERATE-SHADERS] Generating shader sources..."
	@$(SHADER_GEN) --output-dir shaders

# --- Test Suite Bundle Target ---
include/test_suite_data.h source/test_suite_data.c: $(BUNDLE_BUILDER) $(wildcard shaders/*.fcg) $(PS3_SHADER_COMPILER)
	@echo "[BUNDLE-BUILDER] Generating test suite data bundle via native C host tool..."
	@$(BUNDLE_BUILDER) --rsxcomp "$(PS3_SHADER_COMPILER)" --shaders-dir shaders --include-dir include --source-dir source --build-dir $(BUILD)

# --- PPU Build Targets ---
$(BUILD)/source/%.o: source/%.c include/common.h include/test_suite.h include/test_suite_data.h
	@mkdir -p $(BUILD)/source
	@echo "[PPU-CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
	@echo "[PPU-LD] Linking $@..."
	@$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

# Include dependency files generated by GCC
-include $(OFILES:.o=.d)

# Sign EBOOT.BIN with Retail NPDRM and build finalized packages
pkg: $(TARGET).elf
	@mkdir -p $(BUILD)/pkg/USRDIR
	@echo "[STRIP] Stripping $<..."
	@$(STRIP) $(TARGET).elf -o $(BUILD)/$(TARGET).elf
	@echo "[SPRX-LINKER] Resolving system library stubs..."
	@$(SPRX) $(BUILD)/$(TARGET).elf
	@echo "[NPDRM] Signing EBOOT.BIN with Retail NPDRM..."
	@$(MAKE_SELF_NPDRM) $(BUILD)/$(TARGET).elf $(BUILD)/pkg/USRDIR/EBOOT.BIN $(CONTENTID)
	@cp $(BUILD)/pkg/USRDIR/EBOOT.BIN EBOOT.BIN
	@echo "[FSELF] Creating debug fake SELF..."
	@$(FSELF) $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).fake.self
	@echo "[SFO] Generating PARAM.SFO from $(SFOXML)..."
	@$(SFO) --fromxml $(SFOXML) $(BUILD)/pkg/PARAM.SFO
	@cp $(ICON0) $(BUILD)/pkg/ICON0.PNG 2>/dev/null || true
	@echo "[PKG] Building standard package $(STD_PKG)..."
	@$(PKG) --contentid $(CONTENTID) $(BUILD)/pkg/ $(STD_PKG)
	@cp $(STD_PKG) $(STD_GNPDRM_PKG)
	@echo "[PACKAGE_FINALIZE] Finalizing GNPDRM package..."
	@$(PACKAGE_FINALIZE) $(STD_GNPDRM_PKG) 2>/dev/null || true
	@cp $(STD_PKG) $(OUT_PKG) 2>/dev/null || true
	@cp $(STD_GNPDRM_PKG) $(OUT_GNPDRM_PKG) 2>/dev/null || true
	@echo "============================================================"
	@echo "[SUCCESS] PS3 OpenGraphics TestSuite Build completed successfully!"
	@echo "Standard Package:  $(STD_PKG)"
	@echo "GNPDRM Package:    $(STD_GNPDRM_PKG)"
	@echo "Retail EBOOT.BIN:  EBOOT.BIN"
	@echo "Debug Fake SELF:   $(BUILD)/$(TARGET).fake.self"
	@echo "============================================================"

clean:
	@echo "Cleaning testsuite build artifacts..."
	@gio trash $(TARGET).elf $(TARGET).self $(STD_PKG) $(STD_GNPDRM_PKG) EBOOT.BIN $(BUILD) include/test_suite_data.h source/test_suite_data.c tests/*.vpo tests/*.fpo tests/*_vpo.h tests/*_fpo.h tests/tmp_* $(BUILD)/*.tmp.fpo 2>/dev/null || true
