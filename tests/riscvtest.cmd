# riscvtest.s (Harris & Harris): si todo sale bien escribe 25 en la dirección 100.
run
expect mem32 0x64 25
expect exit 0
