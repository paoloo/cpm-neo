# Bundled Apps

← [README](../README.md)

`sysgen new` installs bundled apps in two groups:

- `apps/sys` — the transient system commands (`COPY`, `DUMP`, `HELP`, `SET`, `STAT`,
  `SUBMIT`, `SYS`). Each top-level `.c`/`.s`/`.S` file is compiled to a
  `.COM` and always installed.
- `apps/extra` — optional tools, installed unless `--no-extra` is given.

### Optional tools (apps/extra)

| App | Purpose | Invoke |
| --- | --- | --- |
| **ED** | Line-oriented text editor | `ED <file>` |
| **ASM** | Two-pass RISC-V assembler producing runnable `.COM` files | `ASM <file>.S` |
| **BASIC** | Tiny BASIC interpreter | `BASIC [prog.bas]` |

**Typical workflow:** Edit source with **ED**, assemble using **ASM**, then execute the resulting `.COM`.

---

## Built-in Assembler

A two-pass assembler supporting the **RV32I** instruction set plus the **M** (multiply/divide) extension. Output is a flat `.COM` executable.

### Usage

```text
ASM <FILE.S>
ASM <FILE.ASM>
```

## Syscall Interface 

Programs communicate with the operating system through a syscall table located at `%SYSCALL`. Please refer to [Syscall reference](../docs/syscall-reference.md) for more detail.

## Program Skeleton

All `.COM` programs **must** begin at `.org 0x100`.

```asm
; HELLO.S

.org 0x100

main:
    li   a0, 1
    la   a1, hello
    li   a2, 14

    la   t1, %SYSCALL
    lw   t2, 8(t1)          ; syscall slot 2: write
    jalr ra, 0(t2)

    li   a0, 0
    la   t1, %SYSCALL
    lw   t2, 16(t1)         ; syscall slot 4: exit
    jalr ra, 0(t2)

hello:
    .asciiz "Hello, World!\n"
```

## Instructions & Directives

| Category | Items |
| --- | --- |
| **RV32I** | `lb lh lw lbu lhu sb sh sw`<br><br>`addi slti sltiu xori ori andi slli srli srai`<br><br>`add sub sll slt sltu xor srl sra or and`<br><br>`beq bne blt bge bltu bgeu`<br><br>`jal jalr`<br><br>`lui auipc` |
| **M Extension** | `mul mulh mulhsu mulhu div divu rem remu` |
| **Pseudo** | `li`, `la`, `mv`, `nop`, `j`, `call`, `jr`, `ret` |
| **Directives** | `.org`, `.byte`, `.word`, `.ascii`, `.asciiz`, `.asciz`, `.align`, `.equ`, `.space`, `.fill`, `.text`, `.data`, `.section` |

**Directives**

| Directive | Description |
| --- | --- |
| `.org ADDR` | Set the current output address |
| `.byte V[, V...]` | Emit raw bytes |
| `.word V[, V...]` | Emit 32-bit words |
| `.ascii "STR"` | Emit string bytes (no terminator) |
| `.asciiz "STR"` | Emit string bytes plus a null terminator |
| `.asciz "STR"` | Alias of `.asciiz` |
| `.align N` | Pad with zeros to a `2^N` boundary |
| `.equ NAME, V` | Define a constant (no forward references) |
| `.space COUNT [, FILL]` | Reserve `COUNT` bytes, each set to `FILL` (default `0`) — handy for allocating stacks and buffers |
| `.fill COUNT [, SIZE] [, VALUE]` | Emit `COUNT` copies of `VALUE` written as `SIZE` little-endian bytes (defaults `SIZE=1`, `VALUE=0`) |
| `.text` / `.data` / `.section` | Accepted for compatibility; ignored (single flat output segment) |

**Limits**

- Maximum 128 labels
- Maximum 128 characters per source line
- `.equ` does not allow forward references

### Example : `FIB.S`

Prints the first ten Fibonacci numbers while demonstrating stack setup, procedures, and console output.

```
.org 0x100

main:
    la   sp, stack_top
    li   s0, 0
    li   s1, 1
    li   s2, 10

loop:
    beq  s2, zero, done

    mv   a0, s0
    call print_hex

    li   a0, 32
    call putc

    add  s3, s0, s1
    mv   s0, s1
    mv   s1, s3

    addi s2, s2, -1
    j    loop               

done:
    li   a0, 10
    call putc

    li   a0, 0
    la   t1, %SYSCALL
    lw   t2, 16(t1)
    jalr ra, 0(t2)

putc:
    addi sp, sp, -16        
    sw   ra, 12(sp)

    la   t0, chbuf
    sb   a0, 0(t0)

    li   a0, 1
    mv   a1, t0
    li   a2, 1

    la   t1, %SYSCALL
    lw   t2, 8(t1)
    jalr ra, 0(t2)

    lw   ra, 12(sp)
    addi sp, sp, 16         
    ret

print_hex:
    addi sp, sp, -16        
    sw   ra, 12(sp)
    sw   s0, 8(sp)          # Save caller's s0
    sw   s1, 4(sp)          # Save caller's s1

    mv   s0, a0             
    li   s1, 8              

ph_loop:
    srli t0, s0, 28
    andi t0, t0, 15
    addi t0, t0, 48

    slti t1, t0, 58
    bne  t1, zero, ph_digit
    addi t0, t0, 7

ph_digit:
    mv   a0, t0
    call putc               

    slli s0, s0, 4
    addi s1, s1, -1
    bne  s1, zero, ph_loop

    lw   ra, 12(sp)
    lw   s0, 8(sp)          # Restore caller's s0
    lw   s1, 4(sp)          # Restore caller's s1
    addi sp, sp, 16         
    ret

    .align 4

chbuf:
    .byte 0

    .align 4
stack_lo:
    .space 128              
stack_top:
```

## BASIC

A small interactive MS-BASIC-inspired interpreter for writing and running BASIC programs.

### Commands & Statements

| Keyword | Purpose | Example |
| --- | --- | --- |
| `LIST`, `RUN`, `NEW` | Program management | `RUN` |
| `LOAD`, `SAVE` | File operations | `SAVE "GAME"` |
| `FRE`, `EXIT` | Free memory / Quit | `EXIT` |
| `LET`, `DIM` | Variables and arrays | `LET A=5` |
| `PRINT`, `INPUT` | Console I/O | `PRINT "X=";X` |
| `GOTO`, `GOSUB`, `RETURN` | Control flow | `GOTO 100` |
| `IF ... THEN` | Conditional execution | `IF X>5 THEN 200` |
| `FOR ... TO [STEP]` / `NEXT` | Loops | `FOR I=1 TO 10 STEP 2` |
| `END`, `REM` | Terminate program / comment | `END` |
| `AND`, `OR` | Logical operators | `IF A>1 AND B<2 THEN 50` |
| `ABS`, `SGN`, `RND` | Math functions | `X = RND(100)` |
| `CLR` | Clear all variables | `CLR` |
| `DEF FN` | User-defined functions | `DEF FNA(X)=X*2` |
| `PEEK`, `POKE` | Memory access | `POKE addr,val` |

**Limits**

- 64-character strings
- Maximum 8 nested `FOR` loops
- Maximum 32 nested `GOSUB`s

### Example : `GUESS.BAS`

```basic
10 N = RND(100)
20 INPUT "Guess 1-100"; G
30 IF G = N THEN 60
40 IF G < N THEN PRINT "Too low" : GOTO 20
50 PRINT "Too high" : GOTO 20
60 PRINT "You got it!"
70 END
```

## ED Text Editor

A line-oriented text editor inspired by CP/M ED.

### Commands

| Command | Description |
| --- | --- |
| `Enter` | Advance to next line |
| `N` | Jump to line `N` |
| `B` | Beginning of file |
| `H` | Beginning of file and clear modified flag |
| `L` | List 10 lines |
| `NL` | List `N` lines |
| `-NL` | List backwards |
| `F,TL` | List line range |
| `ND` | Delete line `N` |
| `F,TD` | Delete line range |
| `NI` | Insert before line `N` (terminate with **Ctrl+Z**) |
| `S/old/new/` | Replace text on current line |
| `#` | Toggle verify flag |
| `R <file>` | Read a file into the buffer |
| `E` | Save and exit |
| `Q` | Quit (prompts when the buffer is modified) |

### Example Session

```text
> ED HELLO.BAS
  NEW FILE
     : *I
     1: 10 PRINT "HELLO"
     2: 20 END
     3: (Ctrl+Z)
     : *
     : *1S/HELLO/HI/
     : *L
     1: 10 PRINT "HI"
     2: 20 END
      : *E
```