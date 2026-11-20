// isa.hpp — Definiciones del ISA RV32I: campos, inmediatos e identificación
// de instrucciones. Es la única fuente de verdad que comparten la CPU y el
// desensamblador, para que ambos interpreten los bits exactamente igual.
#pragma once
#include <cstdint>
#include <string>

namespace rv {

// Opcodes (bits [6:0]) del ISA base.
enum Opcode : uint32_t {
    OPC_LOAD     = 0x03,
    OPC_MISC_MEM = 0x0F,  // fence (se trata como nop)
    OPC_OP_IMM   = 0x13,
    OPC_AUIPC    = 0x17,
    OPC_STORE    = 0x23,
    OPC_OP       = 0x33,
    OPC_LUI      = 0x37,
    OPC_BRANCH   = 0x63,
    OPC_JALR     = 0x67,
    OPC_JAL      = 0x6F,
    OPC_SYSTEM   = 0x73,
};

// Cada instrucción concreta que el simulador sabe ejecutar.
enum class Op {
    // Loads
    LB, LH, LW, LBU, LHU,
    // OP-IMM
    ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI, ANDI,
    // Upper immediate
    AUIPC, LUI,
    // Stores
    SB, SH, SW,
    // OP (registro-registro)
    ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
    // Branches
    BEQ, BNE, BLT, BGE, BLTU, BGEU,
    // Saltos
    JALR, JAL,
    // Sistema / misceláneas (no son parte del Anexo A, pero aparecen en
    // programas reales)
    ECALL, EBREAK, FENCE,
    // Cualquier patrón de bits que no sea RV32I válido
    ILLEGAL,
};

enum class Format { R, I, S, B, U, J, NONE };

// Instrucción decodificada: todos los campos posibles ya extraídos.
struct Instr {
    uint32_t raw    = 0;
    uint32_t opcode = 0;
    uint32_t rd     = 0;
    uint32_t funct3 = 0;
    uint32_t rs1    = 0;
    uint32_t rs2    = 0;
    uint32_t funct7 = 0;
    int32_t  imm    = 0;   // inmediato ya con extensión de signo
    Format   fmt    = Format::NONE;
    Op       op     = Op::ILLEGAL;
    const char* why = "";  // motivo, si op == ILLEGAL
};

// Extensión de signo de un valor de `bits` bits a 32 bits.
inline int32_t sext(uint32_t value, unsigned bits) {
    const uint32_t mask = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    const uint32_t sign = 1u << (bits - 1);
    value &= mask;
    return static_cast<int32_t>((value ^ sign) - sign);
}

// Decodifica una palabra de 32 bits: extrae campos, arma el inmediato según
// el formato y determina qué instrucción es (o por qué es ilegal).
Instr decode(uint32_t raw);

// Nombre en minúsculas de la instrucción ("addi", "beq", ...).
const char* mnemonic(Op op);

// Nombres de registros.
const char* abiName(unsigned reg);        // "zero", "ra", "sp", ...
std::string xName(unsigned reg);          // "x0", "x1", ...
// Convierte "x5", "t0", "fp", "zero" -> índice. Devuelve -1 si no es registro.
int parseRegister(const std::string& name);

}  // namespace rv
