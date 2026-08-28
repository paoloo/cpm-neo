#!/usr/bin/env python3
"""
mkuf2.py — assemble the CP/M Neo pico2 flash image and emit a UF2.

Combines sysgen's bootloader.bin (picobin header + boot code) with the
CP/M disk image into a single flash image laid out as:

    0x10000000  bootloader (picobin EXE1 header + boot code)
    0x10004000  CP/M disk image          (PICO2_DISK_OFFSET)

and converts it to a UF2 file the RP2350 BOOTSEL loader accepts
(family RP2350-RISC-V).  Drag the .uf2 onto the Pico 2's USB drive to flash.

Usage:
    python3 platform/pico2/mkuf2.py [--build-dir=sysgen/build] [--out=DIR]
                                    [--flash-size-mb=4] [--boot-slot=0x4000]
"""

import argparse
import struct
import sys
from pathlib import Path

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
RP2350_RISCV_FAMILY_ID = 0xE48BFF5A

PICO2_FLASH_BASE = 0x10000000


def read_bytes(path: Path) -> bytes:
    if not path.is_file():
        sys.exit(f"ERROR: {path} not found — run 'sysgen new ... --platform=pico2' first")
    return path.read_bytes()


def make_uf2(data: bytes, family_id: int) -> bytes:
    """Emit standard 512-byte UF2 records (32-byte header, 476-byte payload
    area of which the first 256 bytes carry data, 4-byte trailing magic)."""
    blocks = (len(data) + 255) // 256
    out = bytearray()
    for block_no in range(blocks):
        chunk = data[block_no * 256 : (block_no + 1) * 256]
        chunk = chunk + b"\x00" * (256 - len(chunk))
        out += struct.pack(
            "<IIIIIIII",
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            UF2_FLAG_FAMILY_ID,
            PICO2_FLASH_BASE + block_no * 256,
            256,
            block_no,
            blocks,
            family_id,
        )
        out += chunk          # 256 bytes of data
        out += b"\x00" * 220  # pad payload area to 476 bytes
        out += struct.pack("<I", UF2_MAGIC_END)
    return bytes(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-dir", default="sysgen/build")
    ap.add_argument("--out", default=None, help="output directory (default: <build-dir>/pico2)")
    ap.add_argument("--flash-size-mb", type=int, default=4)
    ap.add_argument("--boot-slot", type=lambda v: int(v, 0), default=0x4000)
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    out_dir = Path(args.out) if args.out else build_dir / "pico2"

    bootloader = read_bytes(build_dir / "bootloader.bin")
    disk = read_bytes(build_dir / "disk.img")

    if len(bootloader) > args.boot_slot:
        sys.exit(
            f"ERROR: bootloader.bin is {len(bootloader)} bytes, "
            f"boot slot is {args.boot_slot:#x}"
        )

    image = bootloader.ljust(args.boot_slot, b"\xff") + disk

    flash_size = args.flash_size_mb * 1024 * 1024
    if len(image) > flash_size:
        sys.exit(
            f"ERROR: flash image is {len(image)} bytes but the target flash is "
            f"{flash_size} bytes — reduce --disk-size in 'sysgen new'"
        )

    out_dir.mkdir(parents=True, exist_ok=True)

    bin_path = out_dir / "cpmx-pico2.bin"
    bin_path.write_bytes(image)
    uf2_path = out_dir / "cpmx-pico2.uf2"
    uf2_path.write_bytes(make_uf2(image, RP2350_RISCV_FAMILY_ID))

    print(f"  Flash image : {len(image)} bytes ({args.boot_slot:#x} boot + {len(disk)} disk)")
    print(f"  Wrote {bin_path}")
    print(f"  Wrote {uf2_path}")
    print("  Flash it: hold BOOTSEL, plug in the Pico 2, copy the .uf2 onto 'RP2350'.")


if __name__ == "__main__":
    main()
