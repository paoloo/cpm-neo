# Developer Guide

← [README](../README.md)

This page covers the SDK, building your own programs, and adding a new hardware
platform.

### `sysgen install` — Compile and install a source file or folder

Put your program's source files in a folder, or provide a single source file, and let `sysgen` compile and install it:

```sh
$ ./sysgen/build/sysgen install myapp --dst=A0 --attr=RW
```

- `install` accepts `.c`, `.s`, and `.S` sources only.

### `sysgen add` — Copy any file

`add` copies any file into the image named by its 8.3 basename.

```sh
$ ./sysgen/build/sysgen add hello.txt --dst=A0 --attr=RW
```

### SDK surface

- `cpm.h` — umbrella header (syscalls + kernel ABI types).
- `syscall.h` — `sys_open`/`sys_read`/… wrappers over the kernel's
  `SyscallTable` (see [Syscall Reference](syscall-reference.md)).
- `kernel_abi.h` — shared ABI: the `SyscallTable` type, `SysInfo`, disk
  constants (VMAP/block layout), env slot layout, `FD_*` handles.
- `bios.h` — BIOS interface (see below).

## The BIOS layer

| Function | Role |
|----------|------|
| `int bios_init(void)` | Initialize hardware; 0 on success |
| `void bios_conout(int c)` | Write a character to the console |
| `int bios_conin(void)` | Blocking console read |
| `int bios_const(void)` | Console status (0xFF = key ready) |
| `void bios_consize(uint8_t *cw, uint8_t *ch)` | Console dimensions |
| `int bios_read(uint16_t lba, uint8_t *buf)` | Read one 512-byte sector |
| `int bios_write(uint16_t lba, const uint8_t *buf)` | Write one sector |
| `uint32_t bios_time(void)` | platform-defined time service |

A program that needs to touch hardware directly can use the SDK's `sys_dev()`
helper, which reads/writes a 32-bit memory-mapped I/O register in
the window at `__io_base`. The register and command offsets are encoded with the
`IOCTL_*` macros in `kernel_abi.h`; the in/out `data` pointer carries the value
being written or read (it is required, and may not be `NULL`).

## Adding a platform

1. Create `platform/<name>/bios.c` implementing the functions in `bios.h`.
2. Build with `sysgen new ... --platform=<name> --march=<isa>`.

See the [User Guide](user-guide.md) for the `sysgen` command reference.
