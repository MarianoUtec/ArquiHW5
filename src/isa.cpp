// isa.cpp — Decodificación de instrucciones RV32I.
#include "isa.hpp"

#include <cctype>

namespace rv {

namespace {

Format formatOf(uint32_t opcode) {
    switch (opcode) {
        case OPC_OP:       return Format::R;
        case OPC_LOAD:
        case OPC_OP_IMM:
        case OPC_JALR:
        case OPC_MISC_MEM:
        case OPC_SYSTEM:   return Format::I;
        case OPC_STORE:    return Format::S;
        case OPC_BRANCH:   return Format::B;
        case OPC_LUI:
        case OPC_AUIPC:    return Format::U;
        case OPC_JAL:      return Format::J;
        default:           return Format::NONE;
    }
}

int32_t immediateOf(uint32_t raw, Format fmt) {
    switch (fmt) {
        case Format::I:
            // imm[11:0] = inst[31:20]
            return sext(raw >> 20, 12);
        case Format::S:
            // imm[11:5] = inst[31:25], imm[4:0] = inst[11:7]
            return sext(((raw >> 25) << 5) | ((raw >> 7) & 0x1F), 12);
        case Format::B:
            // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
            return sext(((raw >> 31) & 0x1) << 12 |
                         ((raw >> 7)  & 0x1) << 11 |
                         ((raw >> 25) & 0x3F) << 5 |
                         ((raw >> 8)  & 0xF) << 1, 13);
        case Format::U:
            // imm[31:12] = inst[31:12]
            return static_cast<int32_t>(raw & 0xFFFFF000u);
        case Format::J:
            // imm[20|10:1|11|19:12] = inst[31|30:21|20|19:12]
            return sext(((raw >> 31) & 0x1) << 20 |
                         ((raw >> 12) & 0xFF) << 12 |
                         ((raw >> 20) & 0x1) << 11 |
                         ((raw >> 21) & 0x3FF) << 1, 21);
        default:
            return 0;
    }
}

// Marca la instrucción como ilegal con un motivo legible.
void illegal(Instr& in, const char* why) {
    in.op = Op::ILLEGAL;
    in.why = why;
}

void identify(Instr& in) {
    const uint32_t f3 = in.funct3, f7 = in.funct7;

    switch (in.opcode) {
        case OPC_LUI:   in.op = Op::LUI;   return;
        case OPC_AUIPC: in.op = Op::AUIPC; return;
        case OPC_JAL:   in.op = Op::JAL;   return;

        case OPC_JALR:
            if (f3 == 0) in.op = Op::JALR;
            else illegal(in, "jalr con funct3 distinto de 0");
            return;

        case OPC_BRANCH: {
            static const Op t[8] = {Op::BEQ, Op::BNE, Op::ILLEGAL, Op::ILLEGAL,
                                    Op::BLT, Op::BGE, Op::BLTU, Op::BGEU};
            in.op = t[f3];
            if (in.op == Op::ILLEGAL) illegal(in, "branch con funct3 inválido");
            return;
        }

        case OPC_LOAD: {
            static const Op t[8] = {Op::LB, Op::LH, Op::LW, Op::ILLEGAL,
                                    Op::LBU, Op::LHU, Op::ILLEGAL, Op::ILLEGAL};
            in.op = t[f3];
            if (in.op == Op::ILLEGAL) illegal(in, "load con funct3 inválido (¿lwu/ld de RV64?)");
            return;
        }

        case OPC_STORE: {
            static const Op t[8] = {Op::SB, Op::SH, Op::SW, Op::ILLEGAL,
                                    Op::ILLEGAL, Op::ILLEGAL, Op::ILLEGAL, Op::ILLEGAL};
            in.op = t[f3];
            if (in.op == Op::ILLEGAL) illegal(in, "store con funct3 inválido");
            return;
        }

        case OPC_OP_IMM:
            switch (f3) {
                case 0: in.op = Op::ADDI;  return;
                case 2: in.op = Op::SLTI;  return;
                case 3: in.op = Op::SLTIU; return;
                case 4: in.op = Op::XORI;  return;
                case 6: in.op = Op::ORI;   return;
                case 7: in.op = Op::ANDI;  return;
                case 1:  // slli: imm[11:5] debe ser 0000000
                    if (f7 == 0x00) in.op = Op::SLLI;
                    else illegal(in, "slli con imm[11:5] distinto de 0");
                    return;
                case 5:  // srli (0000000) / srai (0100000)
                    if (f7 == 0x00) in.op = Op::SRLI;
                    else if (f7 == 0x20) in.op = Op::SRAI;
                    else illegal(in, "srli/srai con imm[11:5] inválido");
                    return;
            }
            return;

        case OPC_OP:
            if (f7 == 0x00) {
                static const Op t[8] = {Op::ADD, Op::SLL, Op::SLT, Op::SLTU,
                                        Op::XOR, Op::SRL, Op::OR, Op::AND};
                in.op = t[f3];
            } else if (f7 == 0x20 && f3 == 0) {
                in.op = Op::SUB;
            } else if (f7 == 0x20 && f3 == 5) {
                in.op = Op::SRA;
            } else if (f7 == 0x01) {
                illegal(in, "instrucción de la extensión M (mul/div) no soportada en RV32I");
            } else {
                illegal(in, "funct7 inválido para instrucción tipo R");
            }
            return;

        case OPC_MISC_MEM:
            if (f3 == 0) in.op = Op::FENCE;
            else illegal(in, "fence.i / misc-mem no soportado");
            return;

        case OPC_SYSTEM:
            if (in.raw == 0x00000073u) in.op = Op::ECALL;
            else if (in.raw == 0x00100073u) in.op = Op::EBREAK;
            else illegal(in, "instrucción de sistema/CSR no soportada");
            return;

        default:
            if (in.raw == 0x00000000u)
                illegal(in, "palabra 0x00000000 (memoria vacía: ¿el programa se salió de su código?)");
            else if ((in.raw & 0x3) != 0x3)
                illegal(in, "instrucción comprimida (extensión C) no soportada");
            else
                illegal(in, "opcode desconocido");
            return;
    }
}

}  // namespace

Instr decode(uint32_t raw) {
    Instr in;
    in.raw    = raw;
    in.opcode = raw & 0x7F;
    in.rd     = (raw >> 7) & 0x1F;
    in.funct3 = (raw >> 12) & 0x07;
    in.rs1    = (raw >> 15) & 0x1F;
    in.rs2    = (raw >> 20) & 0x1F;
    in.funct7 = (raw >> 25) & 0x7F;
    in.fmt    = formatOf(in.opcode);
    in.imm    = immediateOf(raw, in.fmt);
    identify(in);
    return in;
}

const char* mnemonic(Op op) {
    switch (op) {
        case Op::LB: return "lb";       case Op::LH: return "lh";
        case Op::LW: return "lw";       case Op::LBU: return "lbu";
        case Op::LHU: return "lhu";     case Op::ADDI: return "addi";
        case Op::SLLI: return "slli";   case Op::SLTI: return "slti";
        case Op::SLTIU: return "sltiu"; case Op::XORI: return "xori";
        case Op::SRLI: return "srli";   case Op::SRAI: return "srai";
        case Op::ORI: return "ori";     case Op::ANDI: return "andi";
        case Op::AUIPC: return "auipc"; case Op::LUI: return "lui";
        case Op::SB: return "sb";       case Op::SH: return "sh";
        case Op::SW: return "sw";       case Op::ADD: return "add";
        case Op::SUB: return "sub";     case Op::SLL: return "sll";
        case Op::SLT: return "slt";     case Op::SLTU: return "sltu";
        case Op::XOR: return "xor";     case Op::SRL: return "srl";
        case Op::SRA: return "sra";     case Op::OR: return "or";
        case Op::AND: return "and";     case Op::BEQ: return "beq";
        case Op::BNE: return "bne";     case Op::BLT: return "blt";
        case Op::BGE: return "bge";     case Op::BLTU: return "bltu";
        case Op::BGEU: return "bgeu";   case Op::JALR: return "jalr";
        case Op::JAL: return "jal";     case Op::ECALL: return "ecall";
        case Op::EBREAK: return "ebreak"; case Op::FENCE: return "fence";
        case Op::ILLEGAL: return "???";
    }
    return "???";
}

static const char* const kAbi[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

const char* abiName(unsigned reg) { return reg < 32 ? kAbi[reg] : "?"; }

std::string xName(unsigned reg) { return "x" + std::to_string(reg); }

int parseRegister(const std::string& name) {
    std::string s;
    for (char c : name) s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s.empty()) return -1;
    if (s == "fp") return 8;  // alias de s0
    for (int i = 0; i < 32; ++i)
        if (s == kAbi[i]) return i;
    if (s[0] == 'x' && s.size() >= 2 && s.size() <= 3) {
        int v = 0;
        for (size_t i = 1; i < s.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) return -1;
            v = v * 10 + (s[i] - '0');
        }
        if (s.size() == 3 && s[1] == '0') return -1;  // "x05" no
        return v < 32 ? v : -1;
    }
    return -1;
}

}  // namespace rv
