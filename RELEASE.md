# PS3 OpenGraphics Toolkit — Initial Release (v1.0.0)

Welcome to the **Initial Release (v1.0.0)** of the **PS3 OpenGraphics Toolkit**, a modern, free and open-source (FOSS) standalone shader compilation and disassembly toolchain for the Sony PlayStation 3 (Cell Broadband Engine & RSX Reality Synthesizer / NV47 GPU).

This release provides pre-compiled 64-bit Linux binaries, ready-to-install PlayStation 3 homebrew packages, and comprehensive conformance test suites designed to replace legacy proprietary NVIDIA Cg tools (`cgc`, `cgc-disasm`, `libCg.so`).

---

## Release Artifacts & File Breakdown

The following release assets are provided in this initial release:

### 1. Toolchain Executables (Linux x86_64 / amd64)

- **`rsxcomp-linux-amd64`**
  - **Type**: 64-bit Linux Executable (ELF x86_64, dynamically linked, stripped).
  - **Purpose**: Standalone offline shader compiler.
  - **Functionality**: Translates intermediate representations (Khronos SPIR-V bytecode and Gallium3D TGSI) into native RSX 128-bit microcode words. Packages output into PSL1GHT-compatible Big-Endian `.vpo` (Vertex Program Object) and `.fpo` (Fragment Program Object) binaries or embeddable C/C++ header arrays (`.h`).
  - **Usage**:
    ```bash
    chmod +x rsxcomp-linux-amd64
    # Compile Vertex Shader to PSL1GHT .vpo
    ./rsxcomp-linux-amd64 -v -i vertex.vert -o vertex.vpo
    # Compile Fragment Shader to PSL1GHT .fpo and C Header
    ./rsxcomp-linux-amd64 -f -i fragment.frag -o fragment.fpo -c fragment.h -s fragment_fp
    ```

- **`rsxdeasm-linux-amd64`**
  - **Type**: 64-bit Linux Executable (ELF x86_64, dynamically linked, stripped).
  - **Purpose**: Microcode decompiler and disassembler.
  - **Functionality**: Decompiles compiled PlayStation 3 RSX binary shaders (`.vpo`, `.fpo`, and raw `.bin` microcode dumps) back into human-readable ARB-style assembly text (`.asm`). Extracts register usage, attribute mappings, instruction counts, and uniform constant patch tables.
  - **Usage**:
    ```bash
    chmod +x rsxdeasm-linux-amd64
    # Disassemble .vpo or .fpo to assembly text
    ./rsxdeasm-linux-amd64 shader.vpo -o shader.asm
    # Print disassembly directly to standard output
    ./rsxdeasm-linux-amd64 fragment.fpo --stdout
    ```

---

### 2. PlayStation 3 Homebrew Packages

- **`ps3_opengraphics_testsuite.pkg`**
  - **Type**: Retail NPDRM-signed PlayStation 3 Package (`.pkg`).
  - **Purpose**: Interactive GPU conformance test suite and real-time Ray Tracing homebrew application.
  - **Functionality**: Directly installable on PlayStation 3 consoles (Custom Firmware / HEN) and the RPCS3 PlayStation 3 emulator. Features 57 interactive GPU test scenes covering 100% of Shader Model 3.0 mathematical intrinsics, live on-screen diagnostic HUD, and an autonomous GPU hardware Ray Tracing engine running at 60 FPS.

- **`ps3_opengraphics_testsuite.gnpdrm.pkg`**
  - **Type**: GNPDRM-signed PlayStation 3 Package (`.pkg`).
  - **Purpose**: Alternative homebrew package format signed with GNPDRM for compatibility across various PlayStation 3 homebrew loaders and developer environments.

---

### 3. Distribution Archives

- **`ps3-opengraphics-toolkit-linux-amd64.tar.gz`**
  - **Type**: Compressed Tarball Archive.
  - **Contents**: Bundles all standalone Linux executables (`rsxcomp-linux-amd64`, `rsxdeasm-linux-amd64`) and both PS3 PKG files (`ps3_opengraphics_testsuite.pkg`, `ps3_opengraphics_testsuite.gnpdrm.pkg`).

- **`ps3-opengraphics-toolkit-linux-amd64.zip`**
  - **Type**: Standard ZIP Archive.
  - **Contents**: Bundles all standalone Linux executables and PS3 PKG files for universal extraction.

---

## Key Highlights & Feature Matrix

- **100% Shader Model 3.0 Math Coverage**: Complete implementation of all 54 standard SM 3.0 intrinsics and operators (arithmetic, trigonometry, exponentials, logarithms, rounding, clamping, and vector algebra).
- **Branchless Range-Reduced Trigonometry**: Singularity-free minimax polynomial approximations for `atan`, `atan2`, `asin`, `acos`, and `radians`.
- **Dual-Issue VPE Vertex Scheduling**: Automatically pairs scalar and vector vertex operations for dual-issue execution on RSX VPE hardware.
- **Bi-Endian Native Support**: Built-in PowerPC Big-Endian byte-swapping (`htobe16`/`htobe32`) and float bitcasting.
- **Zero Closed-Source Dependencies**: 100% open-source toolchain built on standard C11/C++17 with pure `Makefile` orchestration.

---

## DualShock 3 Gamepad Controls

| Button | Action |
| :--- | :--- |
| **D-PAD Left / Right** | Switch previous / next test scene (1 to 57) |
| **L1 / R1** | Fast skip 5 test scenes backward / forward |
| **Left Analog Stick** | Interactive camera movement / strafing |
| **Right Analog Stick** | Interactive camera rotation / free look |
| **R2 / L2** | Camera vertical elevation (Up / Down) |
| **TRIANGLE** | Toggle dynamic procedural animation (ON / OFF) |
| **CIRCLE** | Clean exit to PlayStation 3 CrossMediaBar (XMB) |

---

## License

The **PS3 OpenGraphics Toolkit** is distributed under the **GNU General Public License v2.0 (GPL-2.0)**. Refer to the [LICENSE](LICENSE) file for complete terms and upstream component attributions.
