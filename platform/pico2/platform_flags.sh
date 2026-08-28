# platform/pico2/platform_flags.sh
# Platform build hooks — sourced by sysgen/build_disk.sh and sysgen/app_build.sh.
#
# PLATFORM_CFLAGS  extra compiler flags for kernel/CCP/app builds
# PLATFORM_LDSYMS  extra linker --defsym flags for kernel and SDK links
#
# The CP/M address space is mapped onto RP2350 SRAM at 0x20000000:
#   - TPA base (TPA_LOAD_ADDR in kernel_abi.h)
#   - SDK link origin (__tpa_base in linker_sdk.ld)
#   - RAM base for the io window / kernel placement (__ram_base in
#     linker_kernel.ld); __KERN_START is then computed as a physical address
#     by the two-pass kernel build and flows into S0_KERN_LOAD unchanged.

PLATFORM_CFLAGS="-DTPA_LOAD_ADDR=0x20000100"
PLATFORM_LDSYMS="--defsym=__ram_base=0x20000000 --defsym=__tpa_base=0x20000100"
