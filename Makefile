# ==============================================================================
# PS3 OpenGraphics Toolkit (PS3-OpenGraphics-Toolkit)
# Modular Offline Shader Compiler (rsxcomp) & Decompiler/Disassembler (rsxdeasm)
# ==============================================================================

CC       ?= gcc
CFLAGS   ?= -O2 -Wall -Wextra -Irsxcomp/include -std=c11

PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
DESTDIR  ?=

# Source module directories
COMPILER_DIR     = rsxcomp
DISASM_DIR       = rsxdeasm
TESTSUITE_DIR    = testsuite
PS3DEV_DIR       = ps3dev
BUILD_DIR        = build

# Centralized build artifact outputs
COMPILER_BIN     = $(BUILD_DIR)/rsxcomp
DISASM_BIN       = $(BUILD_DIR)/rsxdeasm
TEST_HARNESS     = $(BUILD_DIR)/test_all_intrinsics
STD_PKG          = $(BUILD_DIR)/ps3_opengraphics_testsuite.pkg
STD_GNPDRM_PKG   = $(BUILD_DIR)/ps3_opengraphics_testsuite.gnpdrm.pkg

# ------------------------------------------------------------------------------
# Build Rules
# ------------------------------------------------------------------------------

.PHONY: default all rsxcomp rsxdeasm test check prepare pkg install uninstall clean help

# Default target: builds compiler (rsxcomp) and decompiler (rsxdeasm) into build/ directory
default: $(COMPILER_BIN) $(DISASM_BIN)
	@echo "=================================================="
	@echo " PS3-OpenGraphics-Toolkit binaries built successfully in $(BUILD_DIR)/!"
	@echo " Compiler ELF:   $(COMPILER_BIN)"
	@echo " Decompiler ELF: $(DISASM_BIN)"
	@echo " Run 'make test' to compile shaders and package test PKG."
	@echo "=================================================="

# 'all' target executes the entire pipeline: builds toolchain, compiles test shaders, and produces PKGs in build/
all: default test
	@echo "=================================================="
	@echo " PS3-OpenGraphics-Toolkit full pipeline completed!"
	@echo " Toolchain ELFs: $(COMPILER_BIN), $(DISASM_BIN)"
	@echo " Retail Package: $(STD_PKG)"
	@echo " GNPDRM Package: $(STD_GNPDRM_PKG)"
	@echo "=================================================="

# Build compiler binary in rsxcomp and copy to build/
$(COMPILER_BIN):
	@mkdir -p $(BUILD_DIR)
	@$(MAKE) -C $(COMPILER_DIR)
	@cp $(COMPILER_DIR)/rsxcomp $(COMPILER_BIN)

# Build decompiler binary in rsxdeasm and copy to build/
$(DISASM_BIN):
	@mkdir -p $(BUILD_DIR)
	@$(MAKE) -C $(DISASM_DIR)
	@cp $(DISASM_DIR)/rsxdeasm $(DISASM_BIN)

# Host validation intrinsics test harness
$(TEST_HARNESS): testsuite/tests/test_all_intrinsics.c
	@mkdir -p $(BUILD_DIR)
	@echo "  [CC]  $< -> $@"
	@$(CC) $(CFLAGS) $< -o $@

# ------------------------------------------------------------------------------
# Test Target
# Verifies prerequisites (compiler binary & ps3dev SDK), compiles test shaders,
# builds the PPU homebrew executable, and copies the finalized PKG into build/.
# ------------------------------------------------------------------------------
test:
	@# 1. Verify compiler binary exists in build/
	@if [ ! -f $(COMPILER_BIN) ]; then \
		echo "================================================================="; \
		echo "[ERROR] 'rsxcomp' compiler is not built yet!"; \
		echo "[INFO]  Please run 'make' first to build the compiler."; \
		echo "================================================================="; \
		exit 1; \
	fi
	@# 2. Verify ps3dev / PSL1GHT SDK exists
	@if [ ! -d $(PS3DEV_DIR) ] || [ ! -f $(PS3DEV_DIR)/bin/make_self_npdrm ]; then \
		echo "================================================================="; \
		echo "[ERROR] PS3DEV / PSL1GHT SDK tools not found!"; \
		echo "[INFO]  Please run 'make prepare' or configure the SDK components."; \
		echo "================================================================="; \
		exit 1; \
	fi
	@echo "=================================================="
	@echo " Prerequisites verified. Building Test Suite PKG..."
	@echo "=================================================="
	@$(MAKE) -C $(TESTSUITE_DIR) pkg
	@# Copy generated packages from testsuite directory to build/
	@mkdir -p $(BUILD_DIR)
	@cp $(TESTSUITE_DIR)/ps3_opengraphics_testsuite.pkg $(STD_PKG)
	@cp $(TESTSUITE_DIR)/ps3_opengraphics_testsuite.gnpdrm.pkg $(STD_GNPDRM_PKG)
	@echo "=================================================="
	@echo "[SUCCESS] Test PKGs created in $(BUILD_DIR)/ directory:"
	@echo "  -> $(STD_PKG)"
	@echo "  -> $(STD_GNPDRM_PKG)"
	@echo "=================================================="

# Alias for packaging
pkg: test

# ------------------------------------------------------------------------------
# Prepare Target (Stub)
# Future implementation: automated download and extraction of the isolated ps3dev SDK toolchain
# ------------------------------------------------------------------------------
prepare:
	@# STUB: Automated PSL1GHT / ps3dev toolchain download routine will be added here
	@echo "[STUB] 'make prepare' is not implemented yet. Automated SDK provisioning will be added in a future release."

# ------------------------------------------------------------------------------
# Check Target (Stub)
# Future implementation: automated headless CI validation checks and IR linting
# ------------------------------------------------------------------------------
check:
	@# STUB: Automated CI conformance validation checks will be added here
	@echo "[STUB] 'make check' is not implemented yet. Advanced test and verification checks will be added."

# ------------------------------------------------------------------------------
# Installation Rules
# ------------------------------------------------------------------------------
install: default
	@$(MAKE) -C $(COMPILER_DIR) install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR) PREFIX=$(PREFIX)
	@$(MAKE) -C $(DISASM_DIR) install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR) PREFIX=$(PREFIX)
	@echo "Installation completed."

uninstall:
	@$(MAKE) -C $(COMPILER_DIR) uninstall DESTDIR=$(DESTDIR) BINDIR=$(BINDIR) PREFIX=$(PREFIX)
	@$(MAKE) -C $(DISASM_DIR) uninstall DESTDIR=$(DESTDIR) BINDIR=$(BINDIR) PREFIX=$(PREFIX)
	@echo "Uninstallation completed."

# ------------------------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------------------------
clean:
	@echo "Cleaning build artifacts..."
	@gio trash $(BUILD_DIR) 2>/dev/null || true
	@if [ -d $(COMPILER_DIR) ]; then $(MAKE) -C $(COMPILER_DIR) clean; fi
	@if [ -d $(DISASM_DIR) ]; then $(MAKE) -C $(DISASM_DIR) clean; fi
	@if [ -d $(TESTSUITE_DIR) ]; then $(MAKE) -C $(TESTSUITE_DIR) clean; fi
	@echo "Clean completed."

# ------------------------------------------------------------------------------
# Help
# ------------------------------------------------------------------------------
help:
	@echo "PS3-OpenGraphics-Toolkit Makefile Targets:"
	@echo "  make (default) - Build $(COMPILER_BIN) and $(DISASM_BIN) in $(BUILD_DIR)/"
	@echo "  make all       - Full pipeline: build toolchain, compile shaders, and generate PKG in $(BUILD_DIR)/"
	@echo "  make test      - Verify prerequisites, compile test shaders, and build PKG in $(BUILD_DIR)/"
	@echo "  make pkg       - Alias for 'make test'"
	@echo "  make prepare   - [Stub] Download and configure ps3dev SDK toolchain"
	@echo "  make check     - [Stub] Run automated CI/IR verification checks"
	@echo "  make install   - Install binaries to $(DESTDIR)$(BINDIR)"
	@echo "  make uninstall - Remove installed binaries"
	@echo "  make clean     - Remove $(BUILD_DIR)/ and submodule artifacts"
	@echo "  make help      - Show this help message"
