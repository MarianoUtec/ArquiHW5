# fibonacci.s — Demo de entrada/salida con ecall (servicios SPIM).
# Pide n, calcula los primeros n+1 términos de Fibonacci, los guarda como
# palabras de 32 bits a partir de 0x1000 y muestra fib(n) en decimal y hex.

.text
.globl _start
_start:
    la    a0, msg_titulo
    li    a7, 4                 # print_string
    ecall
    la    a0, msg_pedir
    li    a7, 4
    ecall
    li    a7, 5                 # read_int -> a0
    ecall
    mv    s0, a0                # s0 = n
    bltz  s0, invalido
    li    t0, 46
    bgt   s0, t0, invalido      # fib(47) ya no entra en 32 bits con signo

    lui   s1, 0x1               # s1 = 0x1000: arreglo con la serie
    li    t0, 0                 # fib(i)
    li    t1, 1                 # fib(i+1)
    li    t2, 0                 # i
loop:
    slli  t3, t2, 2
    add   t3, s1, t3
    sw    t0, 0(t3)             # serie[i] = fib(i)
    beq   t2, s0, fin
    add   t4, t0, t1
    mv    t0, t1
    mv    t1, t4
    addi  t2, t2, 1
    j     loop

fin:
    mv    s2, t0                # s2 = fib(n)
    la    a0, msg_fib1
    li    a7, 4
    ecall
    mv    a0, s0
    li    a7, 1                 # print_int
    ecall
    la    a0, msg_fib2
    li    a7, 4
    ecall
    mv    a0, s2
    li    a7, 1
    ecall
    la    a0, msg_hex
    li    a7, 4
    ecall
    mv    a0, s2
    li    a7, 34                # print_int en hexadecimal
    ecall
    li    a0, ')'
    li    a7, 11                # print_char
    ecall
    li    a0, '\n'
    li    a7, 11
    ecall
    mv    a0, s2                # deja el resultado en a0
    li    a7, 10                # exit
    ecall

invalido:
    la    a0, msg_error
    li    a7, 4
    ecall
    li    a0, 1
    li    a7, 17                # exit2(1)
    ecall

msg_titulo: .asciz "== Fibonacci en RV32I ==\n"
msg_pedir:  .asciz "Ingresa n (0-46): "
msg_fib1:   .asciz "fib("
msg_fib2:   .asciz ") = "
msg_hex:    .asciz "  (hex: "
msg_error:  .asciz "n fuera de rango\n"
