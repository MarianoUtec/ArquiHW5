# rv32i_suite.s — Prueba autoverificable de las 37 instrucciones del Anexo A.
# Cada prueba incrementa s11 (número de prueba) y compara el resultado con el
# valor esperado. Si algo falla, imprime el número de la prueba y termina con
# exit2(número). Si todo pasa, imprime un mensaje y termina con exit2(0).
# Usa las syscalls SPIM: 4 (print_string), 1 (print_int), 17 (exit2).

.macro CHECK reg, val
    addi s11, s11, 1
    li   t6, \val
    bne  \reg, t6, fail
.endm

.text
.globl _start
_start:
    li   s11, 0

# ---------------- U-type ----------------
    lui   t0, 0x12345
    CHECK t0, 0x12345000
    auipc t0, 1                 # t0 = pc + 0x1000
    auipc t1, 0                 # t1 = (pc + 4)
    sub   t0, t0, t1
    CHECK t0, 0xFFC

# ---------------- OP-IMM ----------------
    li    t1, 5
    addi  t0, t1, -7
    CHECK t0, -2
    li    t1, 0x7FFFFFFF
    addi  t0, t1, 1             # overflow: da la vuelta
    CHECK t0, 0x80000000
    li    t1, -1
    slti  t0, t1, 1             # -1 < 1 (con signo)
    CHECK t0, 1
    li    t1, 1
    slti  t0, t1, -1
    CHECK t0, 0
    sltiu t0, t1, -1            # 1 < 0xFFFFFFFF (sin signo)
    CHECK t0, 1
    li    t1, -1
    sltiu t0, t1, 1
    CHECK t0, 0
    li    t1, 0x12345678
    xori  t0, t1, -1            # not
    CHECK t0, 0xEDCBA987
    ori   t0, t1, 0x0F0
    CHECK t0, 0x123456F8
    andi  t0, t1, -16           # inmediato con signo: 0xFFFFFFF0
    CHECK t0, 0x12345670
    li    t1, 1
    slli  t0, t1, 31
    CHECK t0, 0x80000000
    li    t1, 0x80000000
    srli  t0, t1, 31
    CHECK t0, 1
    srai  t0, t1, 31            # desplazamiento aritmético: copia el signo
    CHECK t0, 0xFFFFFFFF

# ---------------- OP (R-type) ----------------
    li    t1, 0x7FFFFFFF
    li    t2, 1
    add   t0, t1, t2
    CHECK t0, 0x80000000
    sub   t0, zero, t2
    CHECK t0, -1
    li    t2, 33                # solo cuentan los 5 bits bajos -> 1
    li    t1, 3
    sll   t0, t1, t2
    CHECK t0, 6
    li    t1, -5
    li    t2, 3
    slt   t0, t1, t2
    CHECK t0, 1
    sltu  t0, t1, t2            # 0xFFFFFFFB > 3 sin signo
    CHECK t0, 0
    li    t1, 0xFF00FF00
    li    t2, 0x0FF00FF0
    xor   t0, t1, t2
    CHECK t0, 0xF0F0F0F0
    or    t0, t1, t2
    CHECK t0, 0xFFF0FFF0
    and   t0, t1, t2
    CHECK t0, 0x0F000F00
    li    t2, 4
    srl   t0, t1, t2
    CHECK t0, 0x0FF00FF0
    sra   t0, t1, t2
    CHECK t0, 0xFFF00FF0

# ---------------- Stores y loads (little-endian) ----------------
    la    s0, buf
    li    t1, 0x80FF7F01
    sw    t1, 0(s0)
    lw    t0, 0(s0)
    CHECK t0, 0x80FF7F01
    lbu   t0, 0(s0)             # byte menos significativo en la dirección menor
    CHECK t0, 0x01
    lb    t0, 2(s0)             # 0xFF -> -1
    CHECK t0, -1
    lbu   t0, 2(s0)
    CHECK t0, 0xFF
    lb    t0, 3(s0)             # 0x80 -> -128
    CHECK t0, -128
    lh    t0, 0(s0)
    CHECK t0, 0x7F01
    lh    t0, 2(s0)             # 0x80FF -> extensión de signo
    CHECK t0, 0xFFFF80FF
    lhu   t0, 2(s0)
    CHECK t0, 0x80FF
    li    t1, 0xAB
    sb    t1, 1(s0)
    lw    t0, 0(s0)
    CHECK t0, 0x80FFAB01
    li    t1, 0x1234
    sh    t1, 2(s0)
    lw    t0, 0(s0)
    CHECK t0, 0x1234AB01
    sw    t1, -4(s0)            # offset negativo
    lw    t0, -4(s0)
    CHECK t0, 0x1234

# ---------------- Branches (tomados y no tomados) ----------------
    li    t1, -1
    li    t2, 1
    li    t0, 0
    beq   t1, t1, 1f
    li    t0, 99
1:  CHECK t0, 0
    beq   t1, t2, fail
    bne   t1, t2, 1f
    li    t0, 99
1:  CHECK t0, 0
    bne   t1, t1, fail
    blt   t1, t2, 1f            # -1 < 1 con signo
    li    t0, 99
1:  CHECK t0, 0
    blt   t2, t1, fail
    bge   t2, t1, 1f            # 1 >= -1 con signo
    li    t0, 99
1:  CHECK t0, 0
    bge   t1, t2, fail
    bltu  t2, t1, 1f            # 1 < 0xFFFFFFFF sin signo
    li    t0, 99
1:  CHECK t0, 0
    bltu  t1, t2, fail
    bgeu  t1, t2, 1f
    li    t0, 99
1:  CHECK t0, 0
    bgeu  t2, t1, fail

# ---------------- Saltos ----------------
    jal   ra, sub1              # jal guarda pc+4 en ra
ret1:
    la    t1, ret1
    addi  s11, s11, 1
    bne   ra, t1, fail
    CHECK s1, 7                 # sub1 se ejecutó
    la    t1, sub2
    addi  t1, t1, -3
    jalr  ra, 4(t1)             # destino = (sub2 + 1) & ~1 = sub2
ret2:
    la    t2, ret2
    addi  s11, s11, 1
    bne   ra, t2, fail
    CHECK s2, 9

# ---------------- x0 siempre vale 0 ----------------
    addi  zero, zero, 5
    CHECK zero, 0
    j     done

sub1:
    li    s1, 7
    ret
sub2:
    li    s2, 9
    jalr  zero, 0(ra)

# ---------------- Resultado ----------------
done:
    la    a0, msg_ok
    li    a7, 4                 # print_string
    ecall
    mv    a0, s11
    li    a7, 1                 # print_int
    ecall
    la    a0, msg_nl
    li    a7, 4
    ecall
    li    a0, 0
    li    a7, 17                # exit2(0)
    ecall

fail:
    la    a0, msg_fail
    li    a7, 4
    ecall
    mv    a0, s11
    li    a7, 1
    ecall
    la    a0, msg_nl
    li    a7, 4
    ecall
    mv    a0, s11
    li    a7, 17                # exit2(número de prueba)
    ecall

msg_ok:   .asciz "RV32I: todas las pruebas pasaron. Total: "
msg_fail: .asciz "RV32I: FALLO en la prueba #"
msg_nl:   .asciz "\n"
    .align 2
    .word 0                     # buf - 4 (para el offset negativo)
buf:
    .word 0, 0
