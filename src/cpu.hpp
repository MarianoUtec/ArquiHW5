// cpu.hpp — Estado arquitectural de una CPU RV32I y ejecución de instrucciones.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "isa.hpp"
#include "memory.hpp"

namespace rv {

class Cpu;

// Interfaz para atender `ecall`. La CPU no sabe nada de E/S: delega aquí.
class SyscallHandler {
public:
    struct Result {
        enum Kind { Continue, Exit, Error } kind = Continue;
        int code = 0;          // código de salida (Exit)
        std::string message;   // detalle (Error)
    };
    virtual ~SyscallHandler() = default;
    virtual Result handle(Cpu& cpu) = 0;
};

// Lo que pasó al ejecutar un paso (lo devuelve Cpu::step).
enum class Event {
    None,          // instrucción normal
    Exit,          // el programa llamó a exit (ecall 10/17/93)
    SelfLoop,      // salto a sí mismo: fin típico de los programas de CPUlator
    Ebreak,        // ebreak: pausa la ejecución (como un breakpoint)
    EcallHalt,     // ecall con las syscalls desactivadas
    Error,         // instrucción ilegal, salto desalineado, syscall inválida
    AlreadyHalted, // se intentó ejecutar con la CPU detenida
};

// Registro detallado de un paso, para mostrar al usuario qué cambió.
struct StepRecord {
    uint32_t pc = 0;
    Instr    in;
    bool     regWritten = false;
    unsigned rd = 0;
    uint32_t regOld = 0, regNew = 0;
    bool     memWritten = false;
    uint32_t memAddr = 0;
    unsigned memSize = 0;
    uint32_t memOld = 0, memNew = 0;
    bool     memRead = false;
    uint32_t loadAddr = 0;
    bool     controlTransfer = false;  // branch tomado o salto
    uint32_t nextPc = 0;
};

class Cpu {
public:
    explicit Cpu(Memory& memory) : mem(memory) {}

    Memory& mem;
    uint32_t pc = 0;
    std::array<uint32_t, 32> x{};   // x[0] siempre se lee como 0
    uint64_t instret = 0;           // instrucciones ejecutadas (retired)
    SyscallHandler* syscalls = nullptr;

    // Estado de detención
    bool halted = false;
    bool error = false;
    int  exitCode = 0;
    std::string haltMessage;

    uint32_t reg(unsigned i) const { return i == 0 ? 0u : x[i]; }
    void setReg(unsigned i, uint32_t v) { if (i != 0) x[i] = v; }

    // Reinicia registros y estado; el PC queda en `entry`.
    void reset(uint32_t entry);

    // Ejecuta una instrucción: fetch -> decode -> execute -> actualizar PC.
    Event step(StepRecord* rec = nullptr);

private:
    Event fail(const std::string& message);
};

}  // namespace rv
