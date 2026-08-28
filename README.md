<div align="center">

# CP/M Neo

**A CP/M-inspired operating system**

[![Try it Online](https://img.shields.io/badge/Try_it_Online-mazin--o3.github.io%2Fvemu-blue?style=for-the-badge&logo=riscv)](https://mazin-o3.github.io/vemu/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)

<img src="docs/images/cpm-neo-main.png" alt="cpm-neo main" width="100%">

</div>

## Quick Start

CP/M Neo requires a RISC-V bare-metal toolchain (`riscv64-unknown-elf-gcc` + binutils), `make`, and `sh`.

```sh
make -C sysgen
./sysgen/build/sysgen new --disk-size=2048K --mem=64K --platform=vemu --arch=riscv32
```

Everything outputs directly to `sysgen/build/`:

| File | Purpose |
| --- | --- |
| `disk.img` | Final disk image |
| `bootloader.bin` | Boot image |
| `core/int/kernel.bin` | kernel binary |
| `core/int/ccp.bin` | Console Command Processor (CCP) binary |
| `apps/` | Bundled Apps |

To clean up the build environment, run:

```sh
make -C sysgen clean
```

See the [User Guide](docs/user-guide.md) for the full build and execution walkthrough.

---

## The Sysgen Tool

`sysgen` is the host utility for building and inspecting CP/M Neo disk images. Commands other than `new` support appending `--disk=path` to target a specific image; `new` always writes to `sysgen/build/disk.img`.

| Command | Description |
| --- | --- |
| `new --disk-size=KBK --mem=KBK --platform=NAME --arch=ISA` | Build the OS and create a disk image (the disk is divided into 1 KB blocks; `--disk-size` is capped at the useful maximum: 4 volumes × 2 MB) |
| `add <file> [--dst=Vn] [--attr=R/W\|R/O\|SYS]` | Add an external file to an image |
| `install <folder> [--dst=Vn] [--attr=...]` | Compile a source folder and install the binaries |
| `dir [Vn]` | List files on a volume |
| `type <name> [Vn]` | Print a file |
| `era <name> [Vn]` | Delete a file |
| `ren <old> <new> [Vn]` | Rename a file |
| `stat` | Show volume usage and metadata |

Platforms are defined in `platform/<name>/bios.c`. Included platforms:

| Platform | Target | Notes |
| --- | --- | --- |
| `vemu` | [vemu](https://mazin-o3.github.io/vemu/) web emulator | default, RAM at address 0 |
| `pico2` | Raspberry Pi Pico 2 (RP2350, RISC-V cores) | UART console, disk in flash — see [platform/pico2](platform/pico2/README.md) |

See the [Developer Guide](docs/developer-guide.md) to add your own.

---

### CCP Navigation

 CP/M Neo has 4 volumes:

    A:  B:  C:  D:

  Switch volume:

    > B:

  Each volume contains 16 user areas:

    0  1  2  ... 15

  Switch user area:

    > USER 5

  Or use the short form:

    > 12:

  Switch volume and user area:

    > D7:
    > A0:

Type `help` at the command prompt for the full list of commands.

## Architecture

CP/M Neo is structured to isolate user applications from the host hardware, with the Console Command Processor (CCP) and user applications sharing the `Transient Program Area (TPA)`.

<img src="docs/images/os-arch.png" alt="os arch" width="100%">

## Boot Process
<img src="docs/images/boot-process.png" alt="boot process" width="100%">

## Documentation Library

| Document | Description |
| --- | --- |
| [User Guide](docs/user-guide.md) | Build, run, and get started |
| [CCP Reference](docs/ccp-reference.md) | Console Command Processor guide |
| [Disk Format](docs/disk-format.md) | CP/M Neo disk image layout |
| [Syscall Reference](docs/syscall-reference.md) | Complete syscall API and ABI specifications |
| [Bundled Apps](docs/bundled-apps.md) | Auto-installed programs |
| [Developer Guide](docs/developer-guide.md) | SDK usage, compiling applications, and adding platforms |

---

## Contributing

Contributions to CP/M Neo are welcome!

If you'd like to contribute, see [CONTRIBUTING.md](CONTRIBUTING.md) for more info.

---

## License

CP/M Neo is distributed under the terms of the open-source [MIT](LICENSE).