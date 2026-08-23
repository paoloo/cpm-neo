.section .text.entry, "ax"
.global _entry
.align 2

_entry:
    la   sp, _stack_top
    .option push
    .option norelax
    la   gp, _data_start + 0x800
    .option pop

    la   t0, _bss_start
    la   t1, _bss_end
1:  sw   zero, 0(t0)
    addi t0, t0, 4
    bltu t0, t1, 1b

    tail _start
