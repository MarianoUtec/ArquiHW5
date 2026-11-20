#!/bin/sh
# Corre todas las pruebas: sh tests/run_tests.sh [ruta/al/rvsim]
# Cada script usa 'expect': el simulador sale con 0 si todas pasan.
cd "$(dirname "$0")/.."
SIM=${1:-build/rvsim}
pass=0; fail=0

run() {  # nombre, script, programa
    if "$SIM" -x "tests/$2" "programas/$3" < /dev/null > /tmp/rvsim_test.log 2>&1; then
        pass=$((pass+1)); echo "  OK     $1"
    else
        fail=$((fail+1)); echo "  FALLO  $1"; cat /tmp/rvsim_test.log
    fi
}

run riscvtest   riscvtest.cmd   riscvtest.bin
run quicksort   quicksort.cmd   quicksort.bin
run arbol       arbol.cmd       arbol.bin
run rv32i_suite rv32i_suite.cmd rv32i_suite.bin
run fibonacci   fibonacci.cmd   fibonacci.bin
run primos_elf  primos.cmd      primos.elf
run invalida    invalida.cmd    invalida.bin

echo "$pass pruebas OK, $fail fallidas"
[ "$fail" -eq 0 ]
