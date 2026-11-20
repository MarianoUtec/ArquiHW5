# rvsim — Simulador de RISC-V (RV32I) para terminal

Simulador del ISA base RISC-V de 32 bits hecho para la Tarea 5 de CS3051 Arquitectura de Computadores (UTEC, 2026-II).
Carga un programa **ya compilado** (binario crudo exportado de CPUlator, o un ejecutable ELF) en memoria y lo ejecuta
instrucción por instrucción desde una terminal interactiva. En cualquier momento se pueden inspeccionar el PC, los
32 registros y la memoria.

```
$ ./build/rvsim programas/riscvtest.bin
"programas/riscvtest.bin" cargado a memoria. Escribe 'help' para ver los comandos.
> step
   0x00000000:  00500113  addi   sp, zero, 5          # li sp, 5
                          sp: 0x00000000 -> 0x00000005  (5)
> run
Fin del programa: salto a sí mismo en 0x00000050 (bucle final del programa).
Ejecutadas 18 instrucciones. PC = 0x00000050
> mem 0x64 0x67
Memoria (0x64-0x67): 19 00 00 00
> exit
¡Hasta la próxima!
Programa finalizado con código 0.
```

## Características

**Obligatorias (sección 1.1 del enunciado)**

- Las **37 instrucciones** del Anexo A: `lb lh lw lbu lhu`, `addi slli slti sltiu xori srli srai ori andi`, `auipc lui`,
  `sb sh sw`, `add sub sll slt sltu xor srl sra or and`, `beq bne blt bge bltu bgeu`, `jal jalr`.
- Memoria plana de 32 bits, direccionable por byte y **little-endian**.
- Carga de programas desde archivo, en la dirección **0x00000000**.
- Ejecución **paso a paso** (`step`) controlada por el usuario.
- Inspección de **PC, registros y memoria** (`pc`, `regs`, `mem`, `words`, `dump`).

**Adicionales (sección 1.2 y extras)**

- `ecall` con las **syscalls de SPIM** de CPUlator (a7 = 1, 4, 5, 8, 9, 10, 11, 12, 17, 30, 32, 34, 35, 36, y 93 como exit).
- Carga directa de **archivos ELF32** (segmentos `PT_LOAD`, punto de entrada `e_entry` y tabla de símbolos).
- **Desensamblador** integrado con nombres ABI y pseudoinstrucciones (`li`, `mv`, `j`, `ret`, `beqz`, ...).
- **Breakpoints** por dirección o por símbolo (`break main`), `ebreak` como pausa y Ctrl+C para detener un `run` largo.
- Cada `step` muestra **qué cambió**: registro destino (antes → después), escritura en memoria y si el branch se tomó.
- Errores explícitos: instrucción ilegal (con el motivo, p. ej. «extensión M no soportada»), salto o PC desalineado,
  syscall desconocida, entrada agotada.
- Detección del final típico de los programas de CPUlator (`end: j end`, un salto a sí mismo).
- Modo por lotes (`--run`), scripts de comandos (`-x`) y verificaciones automáticas (`expect`) usados por las pruebas.

No se implementa soporte para GDB.

## Compilar

Requiere un compilador de C++17 (g++, clang o MSVC). No hay dependencias externas.

Con CMake (Linux, macOS o Windows con MinGW):

```
cmake -B build
cmake --build build
```

O con `make`:

```
make            # genera build/rvsim
make test       # corre las pruebas
```

O directamente:

```
g++ -std=c++17 -O2 src/*.cpp -o rvsim
```

En Windows el ejecutable queda como `build\rvsim.exe`. La carpeta `bin/` incluye un `rvsim.exe` ya compilado
(estático, no necesita las DLL de MinGW).

## Uso

```
rvsim [opciones] <programa.bin | programa.elf>
```

| Opción | Descripción |
|---|---|
| `-b, --base <dir>` | dirección donde se carga un binario crudo (por defecto `0x0`) |
| `-f, --format raw\|elf\|auto` | formato del archivo (por defecto se detecta solo por la cabecera ELF) |
| `-r, --run` | ejecuta todo de corrido y muestra los registros al final, sin terminal |
| `-x, --script <archivo>` | ejecuta los comandos de un archivo, uno por línea (`#` = comentario) |
| `-c, --cmd "a; b; c"` | ejecuta comandos separados por `;` |
| `-m, --max <n>` | límite de instrucciones para `run` (por defecto 10 000 000) |
| `--sp <dir>` | valor inicial de `sp` (por defecto 0, igual que CPUlator) |
| `--no-syscalls` | `ecall` detiene la CPU en vez de atender servicios |
| `--xnames` | muestra `x0..x31` en lugar de los nombres ABI |

El código de salida del simulador es el del programa simulado (el que pasa a `exit`), o 1 si hubo un error de ejecución.
En scripts con `expect` es 0 si todas las verificaciones pasan y 2 si alguna falla.

## Comandos de la terminal

| Comando | Alias | Qué hace |
|---|---|---|
| `step [n]` | `s` | Ejecuta n instrucciones (1 por defecto) y muestra sus efectos. **Enter vacío repite el último `step`.** |
| `run [max]` | `r`, `c`, `continue` | Ejecuta hasta el fin del programa, un breakpoint, `ebreak`, un error o `max` instrucciones. |
| `break <dir\|símbolo>` | `b` | Pone un breakpoint. `breaks` los lista y `delete [dir]` (`d`) quita uno o todos. |
| `pc [valor]` | | Muestra (o cambia) el PC. |
| `regs [r ...]` | `reg` | Sin argumentos muestra los 32 registros (con `*` marca los que cambiaron en la última ejecución). Con argumentos (`x5 t0 a0 pc`) muestra el valor en hex, con signo y sin signo. |
| `mem <ini> [fin]` | `m` | Bytes de memoria en el rango, con el mismo formato que el ejemplo del enunciado. |
| `words <dir> [n]` | `w` | n palabras de 32 bits, en hex y decimal. Útil para ver arreglos. |
| `dump <dir> [n]` | | Volcado hexadecimal con columna ASCII. |
| `str <dir>` | | Muestra una cadena terminada en `'\0'`. |
| `dis [dir] [n]` | `x` | Desensambla n instrucciones (desde el PC por defecto). `=>` marca el PC y `*` los breakpoints. |
| `set <reg\|pc> <valor>` | | Escribe un registro. |
| `setmem <dir> <valor> [b\|h\|w]` | | Escribe en memoria (byte, halfword o word). |
| `input <texto>` | | Deja lista una línea de entrada para `read_int` o `read_string` (útil en scripts). |
| `info` | `status` | Archivo, segmentos, punto de entrada, instrucciones ejecutadas y estado de la CPU. |
| `trace on\|off` | | Durante `run`, muestra cada instrucción ejecutada. |
| `abi on\|off` | | Alterna entre nombres ABI (`sp`, `a0`) y `x2`, `x10`. |
| `reset` | | Recarga el programa desde el archivo y reinicia la CPU (memoria incluida). |
| `load <archivo>` | | Carga otro programa. |
| `expect ...` | | Verificación automática (ver `tests/`): `expect a0 1`, `expect mem32 0x64 25`, `expect words 0x1000 1 2 3`, `expect exit 0`, `expect output texto`, `expect error`. |
| `help` / `exit` | `h` / `q`, `quit` | Ayuda o salir. |

Donde se pide un valor o una dirección se puede escribir `4096`, `0x1000`, `0b1010`, `-1`, un registro (`sp`), un símbolo
del ELF (`main`) o una suma o resta simple (`sp+8`, `main+12`).

## Cómo funciona por dentro

```
src/
  isa.hpp/.cpp       decodificación: campos, inmediatos (I/S/B/U/J) e identificación de la instrucción
  cpu.hpp/.cpp       estado arquitectural (PC, x0..x31) y ciclo fetch → decode → execute → write-back
  memory.hpp/.cpp    memoria little-endian de 4 GiB, paginada (páginas de 4 KiB creadas al escribir)
  syscalls.hpp/.cpp  servicios ecall estilo SPIM (E/S de consola, exit, sbrk, tiempo)
  loader.hpp/.cpp    carga de binarios crudos y de ELF32 (parser propio, sin <elf.h>)
  disasm.hpp/.cpp    desensamblador
  debugger.hpp/.cpp  terminal: intérprete de comandos, breakpoints, verificaciones
  main.cpp           opciones de línea de comandos
```

**Ciclo de una instrucción** (`Cpu::step`):

1. **Fetch:** lee 4 bytes en `pc` y los arma en little-endian.
2. **Decode:** `decode()` separa `opcode`, `rd`, `funct3`, `rs1`, `rs2` y `funct7`, arma el inmediato según el formato
   (con extensión de signo) e identifica la instrucción exacta. Las combinaciones que no son RV32I, como `funct7`
   inválido o `mul` de la extensión M, se marcan como ilegales con su motivo.
3. **Execute:** un `switch` sobre la instrucción calcula el resultado con aritmética de `uint32_t`, que hace el overflow
   modular igual que el hardware, y usa `int32_t` solo en las comparaciones y desplazamientos con signo.
4. **Write-back y PC:** escribe `rd` (nunca `x0`) y actualiza `pc` a `pc + 4` o al destino del salto. Si el destino no
   está alineado a 4 se reporta un error `instruction-address-misaligned`.

**Memoria:** reservar 4 GiB no es viable, así que la memoria se guarda en un `unordered_map` de páginas de 4 KiB que se
crean solo al escribir. Leer una dirección que nunca se escribió devuelve 0. Los accesos de 16 y 32 bits se arman byte
a byte, así funcionan aunque crucen el borde de una página.

**Separación núcleo e interfaz:** `Cpu` no imprime nada. Devuelve un `Event` y, si se le pide, un `StepRecord` con lo
que cambió, y la terminal (`Debugger`) decide cómo mostrarlo. Las syscalls se conectan a través de la interfaz
`SyscallHandler`.

## Programas de ejemplo (`programas/`)

| Archivo | Qué hace | Resultado esperado |
|---|---|---|
| `riscvtest.bin` | Prueba de Harris & Harris (add, sub, and, or, slt, addi, lw, sw, beq, jal). | `mem[0x64] = 25` |
| `quicksort.bin` | Quicksort recursivo sobre `{6,4,3,2,1,8,9}` en 0x1000. | `{1,2,3,4,6,8,9}` en 0x1000 |
| `arbol.bin` | `is_symmetric` sobre el árbol `{6,4,4,1,2,2,1}` en 0x1000. | `a0 = 1` |
| `rv32i_suite.bin` | Prueba autoverificable de las 37 instrucciones (46 casos, incluidos los casos borde). | imprime «todas las pruebas pasaron» |
| `fibonacci.bin` | Pide n por consola (`read_int`), guarda la serie en 0x1000 e imprime fib(n). | `fib(10) = 55` |
| `primos.elf` | Criba de Eratóstenes escrita en **C**, compilada para RV32I y cargada como ELF. | primos ≤ N |
| `invalida.bin` | Contiene un `mul` (extensión M). | error explícito en 0xC |

Los tres primeros son los programas que recomienda el enunciado, exportados de CPUlator en formato *Raw*. Las fuentes de
los demás están en `programas/fuentes/`, y `compilar.sh` los regenera con clang/LLVM (opcional; los binarios ya vienen
compilados).

### Ejemplos rápidos

```
./build/rvsim -r programas/quicksort.bin                        # ejecutar de corrido
./build/rvsim -c "run; words 0x1000 7" programas/quicksort.bin  # comandos en línea
./build/rvsim programas/fibonacci.bin                           # interactivo: run, luego escribe n
./build/rvsim programas/primos.elf                              # break criba, run, regs a0, step, ...
```

## Pruebas

`tests/*.cmd` son scripts de comandos con verificaciones `expect`:

```
make test                     # o: sh tests/run_tests.sh build/rvsim
cd build && ctest             # si se compiló con CMake
```

Además, durante el desarrollo el decodificador se comparó contra `llvm-objdump` con 3000 instrucciones aleatorias, y la
ejecución contra un modelo de referencia independiente con 150 programas aleatorios. Ambas comparaciones salieron sin
diferencias.
