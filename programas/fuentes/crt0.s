# crt0.s — Arranque mínimo para programas en C: fija el stack, llama a main
# y termina con la syscall exit (93) usando el valor que devuelve main.
.section .text.start
.globl _start
_start:
    li    sp, 0x00100000        # stack: crece hacia abajo desde 1 MiB
    call  main
    li    a7, 93
    ecall
1:  j     1b
