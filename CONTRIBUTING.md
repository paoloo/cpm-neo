# Contributing to CP/M Neo

Thanks for your interest! This guide gets you from a fresh checkout to your first build and contribution.

## Getting started

1. **Install a RISC-V bare-metal toolchain** (`riscv64-unknown-elf-gcc`, binutils, and libgcc). On Debian/Ubuntu use the [xPack](https://xpack.github.io/riscv-none-elf-gcc/) or SiFive toolchain packages; on macOS use `brew install riscv-tools` (or a toolchain from source). Verify with:

   ```sh
   $ riscv64-unknown-elf-gcc --version
   ```

2. **Build the host `sysgen` tool** and **create a disk image**:

   ```sh
   $ make -C sysgen
   $ ./sysgen/build/sysgen new --disk-size=2048K --mem=64K --platform=vemu --arch=riscv32
   ```

   This produces `sysgen/build/disk.img`, a bootable CP/M Neo disk.

## Project layout

| Path | Purpose |
| --- | --- |
| `core/kernel/` | Kernel: BDOS filesystem, disk layer, syscall dispatch |
| `core/ccp/` | Console Command Processor (DIR, ERA, TYPE, ...) |
| `sdk/` | User-space SDK: headers, libc, linker script |
| `sysgen/` | Host tool that builds the OS and disk images |
| `arch/<isa>/` | Architecture support: `config.sh`, `boot.S`, boot linker |
| `platform/<name>/` | Platform BIOS implementations (hardware abstraction) |
| `apps/` | Bundled applications |
| `docs/` | Project documentation |

See [README.md](README.md) and [docs/architecture.md](docs/architecture.md) for the big picture.

## Code style

- **Braces** on their own line (Allman style), **4-space** indent.
- **Header comment** at the top of every file: `/* path/name.c — purpose */`.
- **Freestanding embedded C** (C89/C90-style): fixed-width types (`uint8_t`, `uint16_t`, `uint32_t`) over `char`/`int` for machine-dependent sizes.
- **No dynamic allocation**.
- Kernel and SDK code must compile with the `-Wall -Wextra` flags used by the build scripts.

## Before making large changes

For significant features, kernel changes, or new architecture ports, **open an issue first** to discuss the design before implementation.

## Pull requests

- Branch from `main`, keep changes focused and commits concise.
- Thanks for contributing!
