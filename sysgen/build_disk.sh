#!/usr/bin/env sh
# CP/M Neo OS build backend — driven by the sysgen tool.
#
#   sh sysgen/build_disk.sh --mem=<KB> --platform=<PLATFORM>
#
# Builds the bootloader, kernel and CCP into sysgen/build/, next to the
# tool binary.  Runs from anywhere: it locates the CP/M Neo root relative
# to its own path.  System and user apps are not built here — they are
# compiled by sysgen/app_build.sh and installed into the disk image by
# 'sysgen new' / 'sysgen install'.
#
# The target's memory layout comes from platform/<PLATFORM>/config.sh
# (ARCH, IO_BASE, RAM_BASE).  Everything else is derived here:
#   RAM_TOP   = RAM_BASE + total memory
#   KERN_CEIL = min(RAM_TOP, IO_BASE)   (top of usable RAM)
#   TPA_BASE  = RAM_BASE + 0x100        (CP/M TPA load address)

set -eu

MEM_SIZE=""
PLATFORM=""

for arg in "$@"; do
    case "$arg" in
        --mem=*)    MEM_SIZE="${arg#--mem=}" ;;
        --platform=*) PLATFORM="${arg#--platform=}" ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

MEM_SIZE=${MEM_SIZE:?"--mem is required (e.g. --mem=64K)"}
PLATFORM=${PLATFORM:?"--platform is required"}

# Convert --mem=64K style suffix to hex bytes for ld --defsym
mem_kb=$(printf '%s' "$MEM_SIZE" | tr '[:lower:]' '[:upper:]')
case "$mem_kb" in
    *K) mem_kb=${mem_kb%K} ;;
    *)  echo "--mem value must have K suffix (e.g. --mem=64K)" >&2; exit 1 ;;
esac
MEM_HEX=$(printf '0x%X' "$((mem_kb * 1024))")

SELF=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SELF/.." && pwd)
cd "$ROOT"

BUILD="$SELF/build"
INT="$BUILD/core/int"
K_OBJ="$BUILD/core/obj/kernel"
CCP_OBJ="$BUILD/core/obj/ccp"
SDK_OBJ="$BUILD/sdk/obj"
SDK_LIB="$BUILD/sdk/lib"

# Platform metadata (ARCH, IO_BASE, RAM_BASE) from platform/$PLATFORM/config.sh
# shellcheck source=/dev/null
. "platform/$PLATFORM/config.sh"

ARCH=${ARCH:?"$PLATFORM: ARCH not set in platform/$PLATFORM/config.sh"}
IO_BASE=${IO_BASE:?"$PLATFORM: IO_BASE not set in platform/$PLATFORM/config.sh"}
RAM_BASE=${RAM_BASE:?"$PLATFORM: RAM_BASE not set in platform/$PLATFORM/config.sh"}

# Architecture metadata (toolchain prefix + CFLAGS) from arch/$ARCH/config.sh
# shellcheck source=/dev/null
. "arch/$ARCH/config.sh"

# Derived layout.  The min() clamp keeps the kernel from ever colliding with
# the MMIO window when IO_BASE lies inside RAM (as on vemu), and the linker
# scripts below enforce the real invariants: boot scratch/stack, the kernel
# image, and the CCP/TPA must all fit under __mem_top — a clashing IO_BASE or
# RAM_BASE therefore fails the link, never producing a broken image.
RAM_BASE_DEC=$((RAM_BASE))
RAM_TOP_DEC=$((RAM_BASE + mem_kb * 1024))
IO_BASE_DEC=$((IO_BASE))
if [ "$RAM_TOP_DEC" -lt "$IO_BASE_DEC" ]; then
    KERN_CEIL_DEC=$RAM_TOP_DEC
else
    KERN_CEIL_DEC=$IO_BASE_DEC
fi
TPA_BASE_DEC=$((RAM_BASE + 0x100))
IO_BASE_HEX=$(printf '0x%X' "$IO_BASE_DEC")
KERN_CEIL_HEX=$(printf '0x%X' "$KERN_CEIL_DEC")
TPA_BASE_HEX=$(printf '0x%X' "$TPA_BASE_DEC")

CC=${CROSS_COMPILE}gcc
LD=${CROSS_COMPILE}ld
OBJCOPY=${CROSS_COMPILE}objcopy
OBJDUMP=${CROSS_COMPILE}objdump
AR=${CROSS_COMPILE}ar

ARCH_FLAGS="$ARCH_CFLAGS"
LIBGCC=$($CC $ARCH_FLAGS -print-libgcc-file-name)

CFLAGS="$ARCH_FLAGS -ffreestanding -nostdlib \
        -Os -ffunction-sections -fdata-sections \
        -fno-builtin -fomit-frame-pointer \
        -Wall -Wextra"
LDFLAGS="--gc-sections --strip-debug --no-warn-rwx-segments -m $LD_EMULATION"

PLATFORM_INC="-I platform/$PLATFORM"
KERNEL_INC="-I core/kernel/ -I sdk/include -I core/ -I ./ $PLATFORM_INC"
CCP_INC="-I core/ccp/ -I core/kernel/ -I sdk/include -I core/ -I ./ $PLATFORM_INC"
SDK_INC="-I sdk/include -I core/kernel/ -I core/ -I ./ $PLATFORM_INC"

compile() {
    mkdir -p "$(dirname "$3")"
    $CC $1 -c "$2" -o "$3"
}

mkdir -p "$BUILD" "$INT" "$SDK_LIB"

# ── Bootloader ─────────────────────────────────────────────
echo "  Building bootloader..."
$CC $CFLAGS $PLATFORM_INC -I core/kernel/ \
    -c "platform/$PLATFORM/bios.c" -o "$INT/boot_plat.o"
$CC $CFLAGS -I arch/$ARCH/ -I core/kernel/ \
    -Wl,--gc-sections -Wl,--strip-debug \
    -Wl,--defsym=__io_base="$IO_BASE_HEX" \
    -Wl,--defsym=__mem_top="$KERN_CEIL_HEX" \
    -T arch/$ARCH/linker_boot.ld \
    arch/$ARCH/boot.S "$INT/boot_plat.o" -o "$INT/bootloader.elf"
$OBJCOPY -O binary --only-section=.boot "$INT/bootloader.elf" "$BUILD/bootloader.bin"
SIZE=$(wc -c < "$BUILD/bootloader.bin")
if [ "$SIZE" -gt 1024 ]; then
    echo "ERROR: bootloader.bin $SIZE bytes > 1024" >&2
    exit 1
fi

# ── Kernel (two-pass) ──────────────────────────────────────
echo "  Building kernel..."
KERNEL_C="core/kernel/main.c core/kernel/kernel.c core/kernel/bdos.c \
          core/kernel/disk.c platform/$PLATFORM/bios.c \
          sdk/src/string.c sdk/src/stdio.c sdk/src/fs.c sdk/src/stdlib.c"
KERNEL_S="arch/$ARCH/crt0.S"

KERNEL_OBJS=
for src in $KERNEL_C; do
    obj="$K_OBJ/${src%.c}.o"
    compile "$CFLAGS $KERNEL_INC" "$src" "$obj"
    KERNEL_OBJS="$KERNEL_OBJS $obj"
done
for src in $KERNEL_S; do
    obj="$K_OBJ/${src%.S}.o"
    compile "$CFLAGS $KERNEL_INC" "$src" "$obj"
    KERNEL_OBJS="$KERNEL_OBJS $obj"
done

$LD $LDFLAGS \
    --defsym=__KERN_START=0x4000 \
    --defsym=__io_base="$IO_BASE_HEX" \
    --defsym=__mem_top="$KERN_CEIL_HEX" \
    --defsym=__tpa_base="$TPA_BASE_HEX" \
    -T core/kernel/linker_kernel.ld \
    $KERNEL_OBJS "$LIBGCC" -o "$INT/kernel_pass1.elf"

KERN_TOTAL_HEX=$($OBJDUMP -t "$INT/kernel_pass1.elf" | awk '/[[:space:]]__kernel_total$/{print "0x"$1}')
KSTACK_GUARD_HEX=$($OBJDUMP -t "$INT/kernel_pass1.elf" | awk '/[[:space:]]__kstack_guard$/{print "0x"$1}')
KERN_TOTAL=$(printf "%d" "$KERN_TOTAL_HEX")
KSTACK_GUARD=$(printf "%d" "$KSTACK_GUARD_HEX")
KERN_START=$(((KERN_CEIL_DEC - KERN_TOTAL - KSTACK_GUARD) & ~3))
KERN_START_HEX=0x$(printf '%x' "$KERN_START")

$LD $LDFLAGS \
    --defsym=__KERN_START="$KERN_START_HEX" \
    --defsym=__io_base="$IO_BASE_HEX" \
    --defsym=__mem_top="$KERN_CEIL_HEX" \
    --defsym=__tpa_base="$TPA_BASE_HEX" \
    -T core/kernel/linker_kernel.ld \
    $KERNEL_OBJS "$LIBGCC" -o "$INT/kernel.elf"

BSS_END=$($OBJDUMP -t "$INT/kernel.elf" | awk '/[[:space:]]_bss_end$/{print "0x"$1}')
KSTACK=$($OBJDUMP -t "$INT/kernel.elf" | awk '/[[:space:]]__kstack_origin$/{print "0x"$1}')
if [ -n "$BSS_END" ] && [ -n "$KSTACK" ]; then
    BSS_DEC=$(printf "%d" "$BSS_END")
    STK_DEC=$(printf "%d" "$KSTACK")
    if [ "$BSS_DEC" -gt "$STK_DEC" ]; then
        echo "ERROR: Kernel .bss overlaps the stack!" >&2
        exit 1
    fi
fi
$OBJCOPY -O binary "$INT/kernel.elf" "$INT/kernel.bin"

# ── SDK libc ───────────────────────────────────────────────
echo "  Building SDK libc..."
SDK_LIBC_SRCS="sdk/src/stdio.c sdk/src/string.c sdk/src/stdlib.c sdk/src/fs.c sdk/src/ccplib.c sdk/src/start.c"
SDK_LIBC_OBJS=
for src in $SDK_LIBC_SRCS; do
    obj="$SDK_OBJ/$(basename "$src" .c).o"
    compile "$CFLAGS $SDK_INC" "$src" "$obj"
    SDK_LIBC_OBJS="$SDK_LIBC_OBJS $obj"
done
compile "$CFLAGS $SDK_INC" arch/$ARCH/crt0.S "$SDK_OBJ/crt0.o"
$AR rcs "$SDK_LIB/libc.a" $SDK_LIBC_OBJS

# ── CCP ───────────────────────────────────────────────────
echo "  Building CCP..."
CCP_C="sdk/src/start.c sdk/src/ccplib.c \
       core/ccp/ccp.c core/ccp/cmd_files.c core/ccp/cmd_system.c \
       sdk/src/string.c sdk/src/stdio.c sdk/src/fs.c sdk/src/stdlib.c"
CCP_OBJS=
for src in $CCP_C; do
    obj="$CCP_OBJ/${src%.c}.o"
    compile "$CFLAGS $CCP_INC" "$src" "$obj"
    CCP_OBJS="$CCP_OBJS $obj"
done
$LD $LDFLAGS -T sdk/linker/linker_sdk.ld \
    $CCP_OBJS "$SDK_OBJ/crt0.o" "$LIBGCC" \
    --just-symbols="$INT/kernel.elf" -o "$INT/ccp.elf"
$OBJCOPY -O binary "$INT/ccp.elf" "$INT/ccp.bin"

printf '%s' "$PLATFORM" > "$BUILD/.platform"
printf '  Platform        : %s (ISA: %s)\n' "$PLATFORM" "$ARCH"