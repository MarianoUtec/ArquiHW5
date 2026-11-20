cmake_minimum_required(VERSION 3.10)
project(rvsim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(rvsim
  src/main.cpp
  src/debugger.cpp
  src/cpu.cpp
  src/isa.cpp
  src/disasm.cpp
  src/memory.cpp
  src/loader.cpp
  src/syscalls.cpp
)
if(MSVC)
  target_compile_options(rvsim PRIVATE /W4 /utf-8)
else()
  target_compile_options(rvsim PRIVATE -Wall -Wextra)
endif()
if(MINGW)
  # Ejecutable autónomo en Windows (no pide las DLL de MinGW)
  target_link_options(rvsim PRIVATE -static)
endif()

# Pruebas: cada script de tests/ verifica un programa con 'expect'.
enable_testing()
set(P ${CMAKE_SOURCE_DIR}/programas)
set(T ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME riscvtest   COMMAND rvsim -x ${T}/riscvtest.cmd   ${P}/riscvtest.bin)
add_test(NAME quicksort   COMMAND rvsim -x ${T}/quicksort.cmd   ${P}/quicksort.bin)
add_test(NAME arbol       COMMAND rvsim -x ${T}/arbol.cmd       ${P}/arbol.bin)
add_test(NAME rv32i_suite COMMAND rvsim -x ${T}/rv32i_suite.cmd ${P}/rv32i_suite.bin)
add_test(NAME fibonacci   COMMAND rvsim -x ${T}/fibonacci.cmd   ${P}/fibonacci.bin)
add_test(NAME primos_elf  COMMAND rvsim -x ${T}/primos.cmd      ${P}/primos.elf)
add_test(NAME invalida    COMMAND rvsim -x ${T}/invalida.cmd    ${P}/invalida.bin)
