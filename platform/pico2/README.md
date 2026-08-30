# CP/M Neo — Raspberry Pi Pico 2 (RP2350) Platform

CP/M Neo running **natively on the Raspberry Pi Pico 2's RISC-V cores**
(Hazard3).  No emulator, no Pico SDK — the same bare-metal code that runs on
`vemu`, linked into the RP2350's SRAM and booted by the RP2350 bootrom from
its RISC-V flash boot path.

## Hardware map

| CP/M Neo | Pico 2 (RP2350) |
| --- | --- |
| Console | UART0 — GPIO0 (TX), GPIO1 (RX), 115200 8N1 |
| CPU / RAM | Hazard3 RISC-V core, 256 KB CP/M RAM mapped at SRAM `0x20000000` |
| Disk A–D | CP/M disk image in the 4 MB flash at offset `0x4000` (persistent) |
| Time | TIMER0 @ 1 MHz tick (milliseconds) |

The disk image sits in the same flash as the boot code, so files created in
CP/M Neo survive power cycles.  Writes go through the bootrom's flash
erase/program routines (read-modify-write of the containing 4 KB sector).

## Building

Requires the RISC-V bare-metal toolchain (`riscv64-unknown-elf-*` — e.g.
`brew install riscv64-elf-binutils riscv64-elf-gcc` on macOS, which provides
the `riscv64-elf-*` prefix; it is picked up automatically), plus `make`, `sh`
and `python3`:

```sh
make -C sysgen
./sysgen/build/sysgen new --disk-size=2048K --platform=pico2
python3 platform/pico2/mkuf2.py
```

Outputs in `sysgen/build/pico2/`:

| File | Purpose |
| --- | --- |
| `cpmx-pico2.uf2` | Drag-and-drop flash image |
| `cpmx-pico2.bin` | Raw flash image (for `picotool`/`openocd` users) |

Notes:

- RAM is set to 256 KB in `platform/pico2/config.sh`, keeping the image inside
  the parity-free SRAM banks. Change `RAM_SIZE` there for a different layout.
- `--disk-size` follows the CP/M Neo format limit (`2081K` max); the pico2
  flash easily fits it (`mkuf2.py` validates the final image anyway).

## Flashing

1. Hold **BOOTSEL** and plug the Pico 2 into USB — a drive named `RP2350`
   appears.
2. Copy `cpmx-pico2.uf2` onto it.  The board reboots into CP/M Neo.
3. Open a serial terminal on the Pico's UART (115200 8N1):
   - GPIO0/GPIO1 pins, via any 3.3 V USB-UART adapter, **or**
   - a [Pi Debug Probe](https://www.raspberrypi.com/products/debug-probe/)
     (UART header), **or**
   - a Pimoroni/Adafruit board with a built-in USB-UART bridge.

You should see the CP/M Neo banner and the `A>` prompt.

## Rebuilding the OS only

`sysgen new` rebuilds everything and rewrites `disk.img`, so just run
`mkuf2.py` again and re-flash.  Files on the disk are replaced by the fresh
image (the disk image is part of the flash).

## Implementation notes

- `bios.c` — UART console, flash-backed disk (bootrom `flash_range_erase` /
  `flash_range_program` + bootram XIP-restore stub), TIMER0 time service.
- `boot_extra.S` — picobin `EXE1` image definition (RISC-V / RP2350) so the
  bootrom boots the image directly; RP2350 needs no second-stage bootloader.
- `linker_boot.ld` — boot code at `0x10000100`, image def at `0x10000000`.
- `config.sh` — selects RISC-V and maps the CP/M address space onto SRAM at
  `0x20000000`; the build derives the TPA base as `0x20000100`.
- The kernel, CCP and apps are built by the normal sysgen flow; only the
  platform memory map differs from targets with RAM at address 0.

Debug tip: `picotool` (if installed) can inspect the image:
`picotool info -a build/pico2/cpmx-pico2.bin -t bin`.
