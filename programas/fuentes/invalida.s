# invalida.s — Programa que contiene una instrucción que NO es RV32I.
# `mul` pertenece a la extensión M; el simulador debe detenerse y reportarla.
.text
.globl _start
_start:
    li    a0, 7
    li    a1, 6
    add   a2, a0, a1            # 13: esto sí es RV32I
    .word 0x02B50533            # mul a0, a0, a1 (extensión M)
    sw    a0, 0x100(zero)       # nunca se ejecuta
end:
    j     end
