#!/bin/sh
# Recompila los programas de ejemplo. Requiere clang + ld.lld + llvm-objcopy
# (LLVM trae el backend RISC-V). En Windows se puede usar desde WSL.
# Los .bin/.elf ya vienen compilados en programas/, así que esto es opcional.
set -e
cd "$(dirname "$0")"
OUT=..
AS="clang --target=riscv32 -march=rv32i -mabi=ilp32 -c"
LD="ld.lld -m elf32lriscv -T link.ld"

for p in rv32i_suite fibonacci invalida; do
    $AS $p.s -o /tmp/$p.o
    $LD /tmp/$p.o -o /tmp/$p.elf
    llvm-objcopy -O binary /tmp/$p.elf $OUT/$p.bin
    echo "  $p.bin"
done

clang --target=riscv32 -march=rv32i -mabi=ilp32 -O2 -ffreestanding -fno-builtin \
      -nostdlib -c primos.c -o /tmp/primos.o
$AS crt0.s -o /tmp/crt0.o
$LD /tmp/crt0.o /tmp/primos.o -o $OUT/primos.elf
echo "  primos.elf"
