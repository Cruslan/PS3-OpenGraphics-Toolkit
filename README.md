# PS3 OpenGraphics Toolkit (`rsxcomp` & `rsxdeasm`)

An open-source (FOSS) standalone offline shader compilation and disassembly toolchain for the Sony PlayStation 3 (Cell Broadband Engine / RSX NV47 GPU), designed as a modern, open-source replacement for legacy proprietary NVIDIA Cg tools (`cgc`, `cgc-disasm`).

---

## Overview

The PlayStation 3 RSX GPU (based on NVIDIA NV47 / G70) requires offline-compiled binary microcode. This project provides a complete dual-binary suite and conformance verification environment:
1. **`rsxcomp`** (offline compiler ELF): Translates intermediate shader representations (SPIR-V / TGSI) into native NV40/NV47 128-bit microcode words and packages them into PSL1GHT-compatible Big-Endian binary structures (`.vpo`, `.fpo`) or C/C++ header arrays (`.h`).
2. **`rsxdeasm`** (microcode decompiler / disassembler ELF): Disassembles compiled PlayStation 3 RSX binary shaders (`.vpo`, `.fpo`, `.bin`) back into human-readable ARB assembly text (`.asm`), providing complete decompilation of vector/scalar dual-issue vertex instructions, fragment ALU opcodes, texture samplers, register allocation, and uniform patch metadata.
3. **PS3 OpenGraphics TestSuite**: Full-coverage interactive PS3 homebrew test suite featuring 57 real-time GPU test scenes, dynamic D-Pad navigation, on-screen diagnostic overlays, and an autonomous GPU hardware ray tracing engine running at 60 FPS.

---

## Key Features

- **Dual Toolchain Binaries**: Automatically produces both the compiler ELF (`rsxcomp`) and the decompiler ELF (`rsxdeasm`) when invoking `make`.
- **Zero Proprietary Blobs**: Eliminates any dependency on `libCg.so` or NVIDIA's closed-source compiler binaries.
- **100% SM 3.0 Math Coverage**: Complete implementation of all 54 standard Shader Model 3.0 intrinsics and operators.
- **Branchless Range-Reduced Trigonometry**: Singularity-free minimax polynomial approximations for `atan`, `atan2`, `asin`, `acos`, and `radians`.
- **Dual-Issue Vertex Scheduling**: Automatically pairs scalar and vector vertex operations for dual-issue execution on RSX VPE hardware.
- **Linux Native**: Builds and runs natively on 64-bit Linux (x86_64, AArch64, RISC-V).
- **PSL1GHT Compatible**: Outputs and disassembles binary formats 100% compatible with PSL1GHT's `librsx` (`rsxVertexProgram`, `rsxFragmentProgram`, `rsxProgramConst`, `rsxProgramAttrib`).
- **Bi-Endian Support**: Automatically handles byte-swapping (`htobe16`/`htobe32` and float bitcasting) for PS3 PowerPC runtime execution and decompilation.
- **C-Header Output**: Generates self-contained static byte arrays directly embeddable in PS3 C/C++ source code.
- **Zero Complex Build Dependencies**: Uses a pure standard `Makefile` and native C host build utilities (100% C toolchain with no CMake or Python required across the entire repository).

---

## Building

```bash
# Optional: Download and provision isolated PS3 SDK toolchain (ps3dev)
make prepare

# Build both compiler (rsxcomp) and decompiler (rsxdeasm) into build/
make

# Run complete end-to-end pipeline (build toolchain, compile shaders, create PKG in build/)
make all

# Verify prerequisites (rsxcomp & ps3dev), compile all 57 RSX test shaders, and output PKG to build/
make test

# Install to system (/usr/local/bin by default)
sudo make install

# Clean build artifacts (removes build/ directory and submodule artifacts)
make clean
```

Upon compilation, all binaries and packages are centralized under `build/`:
- `build/rsxcomp`
- `build/rsxdeasm`
- `build/ps3_opengraphics_testsuite.pkg`
- `build/ps3_opengraphics_testsuite.gnpdrm.pkg`

---

## CLI Usage

### Compiler (`rsxcomp`)
```bash
# Compile Vertex Shader to PSL1GHT .vpo binary
./build/rsxcomp -v -i testsuite/tests/test_basic.vert -o test_basic.vpo

# Compile Fragment Shader to PSL1GHT .fpo binary and C Header
./build/rsxcomp -f -i testsuite/tests/test_textured.frag -o test_textured.fpo -c test_textured_fpo.h -s test_textured_fp

# Dump generated microcode dwords to stdout
./build/rsxcomp -v -i testsuite/tests/test_basic.vert -d
```

### Decompiler / Disassembler (`rsxdeasm`)
```bash
# Disassemble to default <filename>.asm
./build/rsxdeasm shader.vpo

# Disassemble with specific output file path
./build/rsxdeasm fragment.fpo -o fragment_disasm.asm

# Print disassembly directly to standard output
./build/rsxdeasm custom.fpo --stdout
```

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

## Credits & Acknowledgments

The PlayStation 3 OpenGraphics Toolkit builds upon and owes immense gratitude to foundational open-source graphics drivers, emulator research, and homebrew SDKs:

- **[Mesa 3D Graphics Library](https://www.mesa3d.org/) & [Nouveau Project](https://nouveau.freedesktop.org/)**:
  - The Gallium3D NV30/NV40/NVFX driver architecture, hardware register definitions, vertex and fragment program microcode emitters, and TGSI intermediate representation.
  - Special thanks to Patrice Mandin, Arthur Huillet, Francisco Jerez, Marcin Kościelnicki, Christoph Bumiller, Stéphane Marchesin, and the entire Nouveau / Mesa 3D development community.

- **[RPCS3 PlayStation 3 Emulator Project](https://rpcs3.net/)**:
  - Groundbreaking reverse-engineering of the Sony PlayStation 3 RSX Reality Synthesizer GPU microcode instruction layout, dual-issue VPE execution pairing, fragment ALU operand swizzling, condition code write masks, and Cg binary container formats.
  - Special thanks to Nekotekina, kd-11, eladash, scribblemaniac, Megamouse, and the RPCS3 contributors.

- **[Khronos Group](https://www.khronos.org/) & SPIR-V Working Group**:
  - The open, cross-API standard SPIR-V (Standard Portable Intermediate Representation) architecture, grammar, opcode registries, and GLSL.std.450 extended instructions.

- **[PSL1GHT Project](https://github.com/ps3dev/PSL1GHT)**:
  - Free and open-source PlayStation 3 SDK, runtime structures (`rsxProgramConst`, `rsxProgramAttrib`, `rsxVertexProgram`, `rsxFragmentProgram`), and Cell PPU toolchain infrastructure.
  - Special thanks to Youness Alaoui (KaKaRoTo), AerialX, fail0verflow, and the PS3 homebrew scene.

---

## License

This project is licensed under the **GNU General Public License v2.0 (GPL-2.0)** - see the [LICENSE](LICENSE) file for complete terms and conditions.

Upstream component attributions:
- **Mesa / Nouveau Components**: Licensed under the permissive MIT License (Copyright (C) Mesa / Nouveau Project contributors).
- **Khronos SPIR-V Specifications**: Licensed under the Khronos Free/Open Source License (Copyright (C) Khronos Group Inc.).
- **RPCS3 Hardware & Microcode Research**: Licensed under the GNU General Public License v2.0 (Copyright (C) RPCS3 Project contributors).
- **PSL1GHT SDK Structures**: Licensed under permissive BSD/MIT-style open-source terms.

