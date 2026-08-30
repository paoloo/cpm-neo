# platform/pico2/config.sh
# CP/M Neo platform metadata — sourced by build_disk.sh / app_build.sh.

ID="pico2"
ARCH=riscv32

# Use the first 256 KB of RP2350 SRAM. The final 256 bytes remain the CP/M
# sys_dev I/O window, matching the layout used by the original Pico 2 port.
RAM_SIZE=0x40000
RAM_BASE=0x20000000
IO_BASE=0x2003FF00
