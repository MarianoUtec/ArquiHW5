// disasm.cpp — Implementación del desensamblador.
#include "disasm.hpp"

#include <cstdio>

#include "loader.hpp"

namespace rv {

namespace {

struct Fmt {
    const DisasmOptions& opt;
    std::string r(unsigned i) const { return opt.abiNames ? abiName(i) : xName(i); }
    std::string hex(uint32_t v) const {
        char b[16];
        std::snprintf(b, sizeof b, "0x%X", v);
        return b;
    }
    std::string target(uint32_t addr) const {
        char b[16];
        std::snprintf(b, sizeof b, "0x%08X", addr);
        std::string s = b;
        if (opt.symbols) {
            const std::string name = opt.symbols->describe(addr);
            if (!name.empty()) s += " <" + name + ">";
        }
        return s;
    }
};

std::string pad(const char* m) {
    std::string s = m;
    while (s.size() < 7) s += ' ';
    return s;
}

}  // namespace

std::string disassemble(const Instr& in, uint32_t pc, const DisasmOptions& opt) {
    const Fmt f{opt};
    const std::string m = pad(mnemonic(in.op));
    const std::string rd = f.r(in.rd), rs1 = f.r(in.rs1), rs2 = f.r(in.rs2);
    const int32_t imm = in.imm;
    const uint32_t uimm20 = static_cast<uint32_t>(imm) >> 12;
    std::string text, pseudo;

    switch (in.op) {
        case Op::LB: case Op::LH: case Op::LW: case Op::LBU: case Op::LHU:
            text = m + rd + ", " + std::to_string(imm) + "(" + rs1 + ")";
            break;
        case Op::SB: case Op::SH: case Op::SW:
            text = m + rs2 + ", " + std::to_string(imm) + "(" + rs1 + ")";
            break;

        case Op::SLLI: case Op::SRLI: case Op::SRAI:
            text = m + rd + ", " + rs1 + ", " + std::to_string(imm & 0x1F);
            break;
        case Op::ADDI: case Op::SLTI: case Op::SLTIU:
        case Op::XORI: case Op::ORI: case Op::ANDI:
            text = m + rd + ", " + rs1 + ", " + std::to_string(imm);
            if (in.op == Op::ADDI) {
                if (in.rd == 0 && in.rs1 == 0 && imm == 0) pseudo = "nop";
                else if (in.rs1 == 0) pseudo = "li " + rd + ", " + std::to_string(imm);
                else if (imm == 0) pseudo = "mv " + rd + ", " + rs1;
            } else if (in.op == Op::XORI && imm == -1) {
                pseudo = "not " + rd + ", " + rs1;
            } else if (in.op == Op::SLTIU && imm == 1) {
                pseudo = "seqz " + rd + ", " + rs1;
            }
            break;

        case Op::LUI: case Op::AUIPC:
            text = m + rd + ", " + f.hex(uimm20);
            break;

        case Op::ADD: case Op::SUB: case Op::SLL: case Op::SLT: case Op::SLTU:
        case Op::XOR: case Op::SRL: case Op::SRA: case Op::OR: case Op::AND:
            text = m + rd + ", " + rs1 + ", " + rs2;
            if (in.op == Op::SUB && in.rs1 == 0) pseudo = "neg " + rd + ", " + rs2;
            else if (in.op == Op::SLTU && in.rs1 == 0) pseudo = "snez " + rd + ", " + rs2;
            break;

        case Op::BEQ: case Op::BNE: case Op::BLT: case Op::BGE: case Op::BLTU: case Op::BGEU: {
            const std::string t = f.target(pc + static_cast<uint32_t>(imm));
            text = m + rs1 + ", " + rs2 + ", " + t;
            if (in.rs2 == 0 && in.op == Op::BEQ) pseudo = "beqz " + rs1 + ", " + t;
            else if (in.rs2 == 0 && in.op == Op::BNE) pseudo = "bnez " + rs1 + ", " + t;
            break;
        }

        case Op::JAL: {
            const std::string t = f.target(pc + static_cast<uint32_t>(imm));
            text = m + rd + ", " + t;
            if (in.rd == 0) pseudo = "j " + t;
            else if (in.rd == 1) pseudo = "call " + t;
            break;
        }
        case Op::JALR:
            text = m + rd + ", " + std::to_string(imm) + "(" + rs1 + ")";
            if (in.rd == 0 && in.rs1 == 1 && imm == 0) pseudo = "ret";
            else if (in.rd == 0 && imm == 0) pseudo = "jr " + rs1;
            break;

        case Op::ECALL: case Op::EBREAK: case Op::FENCE:
            text = mnemonic(in.op);
            break;

        case Op::ILLEGAL: {
            char b[32];
            std::snprintf(b, sizeof b, ".word  0x%08X", in.raw);
            text = b;
            pseudo = "inválida";
            break;
        }
    }

    if (opt.pseudo && !pseudo.empty()) {
        while (text.size() < 28) text += ' ';
        text += "# " + pseudo;
    }
    return text;
}

}  // namespace rv
