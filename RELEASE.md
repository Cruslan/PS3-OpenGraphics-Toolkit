# PS3 OpenGraphics Toolkit — Initial Release (v1.0.0)

Welcome to the **Initial Release (v1.0.0)** of the **PS3 OpenGraphics Toolkit**, a modern, free and open-source (FOSS) standalone shader compilation and disassembly toolchain for the Sony PlayStation 3 (Cell Broadband Engine & RSX Reality Synthesizer / NV47 GPU).

This release provides pre-compiled 64-bit Linux binaries, ready-to-install PlayStation 3 homebrew packages, and comprehensive conformance test suites designed to replace legacy proprietary NVIDIA Cg tools (`cgc`, `cgc-disasm`, `libCg.so`).

---

## Release Files

- **`rsxcomp-linux-amd64`**: Standalone offline shader compiler (64-bit Linux). Compiles SPIR-V bytecode and Gallium3D TGSI shaders into native PlayStation 3 RSX (NV40/NV47) microcode (`.vpo`, `.fpo`, `.h`).
- **`rsxdeasm-linux-amd64`**: Microcode decompiler and disassembler (64-bit Linux). Disassembles compiled RSX binary shaders (`.vpo`, `.fpo`, `.bin`) back into readable ARB assembly text.
- **`ps3_opengraphics_testsuite.pkg`**: Retail NPDRM-signed PlayStation 3 homebrew package. Installable on PS3 (CFW/HEN) and RPCS3, featuring 57 interactive GPU test scenes and a real-time Ray Tracing engine.
- **`ps3_opengraphics_testsuite.gnpdrm.pkg`**: GNPDRM-signed alternative homebrew package variant for standard PS3 loaders.
- **`ps3-opengraphics-toolkit-linux-amd64.tar.gz`**: Standalone compressed tarball archive containing all Linux binaries and PS3 packages.
- **`ps3-opengraphics-toolkit-linux-amd64.zip`**: Universal ZIP distribution archive containing all Linux binaries and PS3 packages.

---

## Key Highlights

- **100% Shader Model 3.0 Math Coverage**: Full 54/54 mathematical intrinsic and operator support.
- **Branchless Range-Reduced Trigonometry**: Singularity-free minimax approximations for `atan`, `atan2`, `asin`, `acos`, and `radians`.
- **Dual-Issue VPE Vertex Scheduling**: Automatically pairs scalar and vector vertex operations for dual-issue execution on RSX VPE hardware.
- **Bi-Endian Native Support**: Built-in PowerPC Big-Endian byte-swapping (`htobe16`/`htobe32`) and float bitcasting.
- **Zero Closed-Source Dependencies**: 100% open-source toolchain built on standard C11/C++17.

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

The **PS3 OpenGraphics Toolkit** is distributed under the **GNU General Public License v2.0 (GPL-2.0)**. Refer to the [LICENSE](LICENSE) file for complete terms and conditions.
