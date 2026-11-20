// disasm.hpp — Desensamblador RV32I (código máquina -> texto ensamblador).
#pragma once
#include <cstdint>
#include <string>

#include "isa.hpp"

namespace rv {

class SymbolTable;

struct DisasmOptions {
    bool abiNames = true;      // "sp" en vez de "x2"
    bool pseudo = true;        // agrega "# li a0, 5" cuando hay pseudoinstrucción
    const SymbolTable* symbols = nullptr;
};

// Texto de la instrucción ubicada en `pc` (el pc se usa para calcular los
// destinos absolutos de branches y jal).
std::string disassemble(const Instr& in, uint32_t pc, const DisasmOptions& opt);

}  // namespace rv
