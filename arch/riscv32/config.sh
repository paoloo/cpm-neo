#!/usr/bin/env sh
# arch/riscv32/config.sh
# Architecture metadata — sourced by build_disk.sh / app_build.sh after
# the platform config declares ARCH=riscv32.  Supplies the cross toolchain
# prefix and the concrete compiler flags for this ISA

# Toolchain resolution order:
#   1. $CROSS_COMPILE from the environment (always wins)
#   2. first riscv gcc found on PATH (unknown-elf triple, then short
#      variants used by some packagers — e.g. Homebrew's riscv64-elf-*)
if [ -z "${CROSS_COMPILE:-}" ]; then
    for _prefix in riscv64-unknown-elf- riscv64-elf- riscv32-unknown-elf- riscv32-elf-; do
        if command -v "${_prefix}gcc" >/dev/null 2>&1; then
            CROSS_COMPILE=$_prefix
            break
        fi
    done
fi
CROSS_COMPILE=${CROSS_COMPILE:-riscv64-unknown-elf-}
ARCH_CFLAGS="-march=rv32im -mabi=ilp32"
LD_EMULATION="elf32lriscv"
