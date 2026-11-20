// cpu.cpp — Ciclo de ejecución de una instrucción RV32I.
#include "cpu.hpp"

#include <cstdio>

namespace rv {

namespace {
std::string hex32(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08X", v);
    return buf;
}
}  // namespace

void Cpu::reset(uint32_t entry) {
    x.fill(0);
    pc = entry;
    instret = 0;
    halted = false;
    error = false;
    exitCode = 0;
    haltMessage.clear();
}

Event Cpu::fail(const std::string& message) {
    halted = true;
    error = true;
    exitCode = 1;
    haltMessage = message;
    return Event::Error;
}

Event Cpu::step(StepRecord* rec) {
    if (halted) return Event::AlreadyHalted;

    StepRecord local;
    StepRecord& r = rec ? *rec : local;
    r = StepRecord{};

    // ---- 1. FETCH -------------------------------------------------------
    const uint32_t cur = pc;
    r.pc = cur;
    if (cur & 0x3)
        return fail("PC desalineado " + hex32(cur) + " (instruction-address-misaligned)");
    const uint32_t raw = mem.read32(cur);

    // ---- 2. DECODE ------------------------------------------------------
    const Instr in = decode(raw);
    r.in = in;
    if (in.op == Op::ILLEGAL) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "Instrucción ilegal 0x%08X en %s: ", raw, hex32(cur).c_str());
        return fail(buf + std::string(in.why));
    }

    // ---- 3. EXECUTE -----------------------------------------------------
    const uint32_t a = reg(in.rs1);
    const uint32_t b = reg(in.rs2);
    const uint32_t imm = static_cast<uint32_t>(in.imm);
    const uint32_t shamtI = imm & 0x1F;     // shamt de slli/srli/srai
    const uint32_t shamtR = b & 0x1F;       // shamt de sll/srl/sra
    const int32_t sa = static_cast<int32_t>(a);
    const int32_t sb = static_cast<int32_t>(b);

    uint32_t next = cur + 4;
    bool writesRd = false;
    uint32_t result = 0;
    Event event = Event::None;

    auto branch = [&](bool taken) {
        if (taken) {
            next = cur + imm;
            r.controlTransfer = true;
        }
    };
    auto load = [&](uint32_t value) {
        r.memRead = true;
        r.loadAddr = a + imm;
        result = value;
        writesRd = true;
    };
    auto store = [&](unsigned size) {
        const uint32_t addr = a + imm;
        r.memWritten = true;
        r.memAddr = addr;
        r.memSize = size;
        if (size == 1) {
            r.memOld = mem.read8(addr);
            mem.write8(addr, static_cast<uint8_t>(b));
            r.memNew = b & 0xFF;
        } else if (size == 2) {
            r.memOld = mem.read16(addr);
            mem.write16(addr, static_cast<uint16_t>(b));
            r.memNew = b & 0xFFFF;
        } else {
            r.memOld = mem.read32(addr);
            mem.write32(addr, b);
            r.memNew = b;
        }
    };

    switch (in.op) {
        // --- Loads (con/sin extensión de signo) ---
        case Op::LB:  load(static_cast<uint32_t>(sext(mem.read8(a + imm), 8)));   break;
        case Op::LH:  load(static_cast<uint32_t>(sext(mem.read16(a + imm), 16))); break;
        case Op::LW:  load(mem.read32(a + imm));                                    break;
        case Op::LBU: load(mem.read8(a + imm));                                     break;
        case Op::LHU: load(mem.read16(a + imm));                                    break;

        // --- Stores ---
        case Op::SB: store(1); break;
        case Op::SH: store(2); break;
        case Op::SW: store(4); break;

        // --- Aritmético-lógicas con inmediato ---
        case Op::ADDI:  result = a + imm;                          writesRd = true; break;
        case Op::SLTI:  result = (sa < in.imm) ? 1u : 0u;          writesRd = true; break;
        case Op::SLTIU: result = (a < imm) ? 1u : 0u;              writesRd = true; break;
        case Op::XORI:  result = a ^ imm;                          writesRd = true; break;
        case Op::ORI:   result = a | imm;                          writesRd = true; break;
        case Op::ANDI:  result = a & imm;                          writesRd = true; break;
        case Op::SLLI:  result = a << shamtI;                      writesRd = true; break;
        case Op::SRLI:  result = a >> shamtI;                      writesRd = true; break;
        case Op::SRAI:  result = static_cast<uint32_t>(sa >> shamtI); writesRd = true; break;

        // --- Aritmético-lógicas registro-registro ---
        case Op::ADD:  result = a + b;                             writesRd = true; break;
        case Op::SUB:  result = a - b;                             writesRd = true; break;
        case Op::SLL:  result = a << shamtR;                       writesRd = true; break;
        case Op::SLT:  result = (sa < sb) ? 1u : 0u;               writesRd = true; break;
        case Op::SLTU: result = (a < b) ? 1u : 0u;                 writesRd = true; break;
        case Op::XOR:  result = a ^ b;                             writesRd = true; break;
        case Op::SRL:  result = a >> shamtR;                       writesRd = true; break;
        case Op::SRA:  result = static_cast<uint32_t>(sa >> shamtR); writesRd = true; break;
        case Op::OR:   result = a | b;                             writesRd = true; break;
        case Op::AND:  result = a & b;                             writesRd = true; break;

        // --- Upper immediate ---
        case Op::LUI:   result = imm;       writesRd = true; break;
        case Op::AUIPC: result = cur + imm; writesRd = true; break;

        // --- Branches ---
        case Op::BEQ:  branch(a == b);   break;
        case Op::BNE:  branch(a != b);   break;
        case Op::BLT:  branch(sa < sb);  break;
        case Op::BGE:  branch(sa >= sb); break;
        case Op::BLTU: branch(a < b);    break;
        case Op::BGEU: branch(a >= b);   break;

        // --- Saltos ---
        case Op::JAL:
            next = cur + imm;
            result = cur + 4;
            writesRd = true;
            r.controlTransfer = true;
            break;
        case Op::JALR:
            next = (a + imm) & ~1u;   // el bit 0 del destino se limpia
            result = cur + 4;
            writesRd = true;
            r.controlTransfer = true;
            break;

        // --- Misceláneas / sistema ---
        case Op::FENCE:
            break;  // sin caches ni concurrencia: no hace nada
        case Op::EBREAK:
            event = Event::Ebreak;
            break;
        case Op::ECALL: {
            if (!syscalls) {
                event = Event::EcallHalt;
                halted = true;
                exitCode = 0;
                haltMessage = "ecall (syscalls desactivadas)";
                break;
            }
            SyscallHandler::Result res = syscalls->handle(*this);
            if (res.kind == SyscallHandler::Result::Exit) {
                event = Event::Exit;
                halted = true;
                exitCode = res.code;
                haltMessage = "el programa terminó con exit (ecall)";
            } else if (res.kind == SyscallHandler::Result::Error) {
                return fail("ecall en " + hex32(cur) + ": " + res.message);
            }
            break;
        }
        case Op::ILLEGAL:
            break;  // ya se manejó arriba
    }

    // Sin la extensión C, todo destino de salto debe estar alineado a 4.
    if (r.controlTransfer && (next & 0x3))
        return fail("salto a dirección no alineada " + hex32(next) +
                    " desde " + hex32(cur) + " (instruction-address-misaligned)");

    // ---- 4. WRITE-BACK + actualizar PC -----------------------------------
    if (writesRd && in.rd != 0) {
        r.regWritten = true;
        r.rd = in.rd;
        r.regOld = x[in.rd];
        r.regNew = result;
        x[in.rd] = result;
    }
    pc = next;
    r.nextPc = next;
    ++instret;

    // Un salto a sí mismo (p. ej. "end: j end") nunca termina: se toma como
    // el fin del programa, igual que en los ejemplos exportados de CPUlator.
    if (event == Event::None && next == cur) {
        halted = true;
        exitCode = 0;
        haltMessage = "salto a sí mismo en " + hex32(cur) + " (bucle final del programa)";
        event = Event::SelfLoop;
    }
    return event;
}

}  // namespace rv
