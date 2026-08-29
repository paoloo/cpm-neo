# platform/vemu/config.sh
# CP/M Neo platform metadata — sourced by build_disk.sh / app_build.sh.
#
# Supplies the platform facts everything else is derived from:
#   ARCH      — ISA directory under arch/ (selects the toolchain)
#   IO_BASE   — base address of the peripheral MMIO window
#   RAM_BASE  — base address of the RAM region holding CP/M Neo
#               (TPA + kernel), independent of how the CPU addresses it
#   ID        — 8-char max OS platform id stamped into S0_PLATFORM
#               (the platform identity used by --platform)

ARCH=riscv32
IO_BASE=0xFF00
RAM_BASE=0x0000
ID="vemu"