// syscalls.hpp — Environment calls estilo SPIM (como CPUlator, sistema RV32 SPIM).
//
// El número de servicio va en a7; los argumentos en a0/a1; el resultado vuelve
// en a0 (y a1 para el tiempo).
#pragma once
#include <cstdint>
#include <iosfwd>
#include <deque>
#include <string>

#include "cpu.hpp"

namespace rv {

// Salida de consola del programa simulado. Recuerda si la última línea quedó
// abierta para que el prompt del depurador no se pegue al texto del programa.
class Console {
public:
    explicit Console(std::ostream& out) : out_(out) {}
    void write(const std::string& s);
    void ensureNewline();           // termina la línea si quedó abierta
    void markLineStart() { atLineStart_ = true; }
    const std::string& captured() const { return captured_; }
    void clearCapture() { captured_.clear(); }

private:
    std::ostream& out_;
    bool atLineStart_ = true;
    std::string captured_;          // todo lo que imprimió el programa (para tests)
};

class SpimSyscalls : public SyscallHandler {
public:
    SpimSyscalls(Console& console, std::istream& in) : console_(console), in_(in) {}
    Result handle(Cpu& cpu) override;
    static const char* serviceName(uint32_t service);

    // Puntero de heap para sbrk (servicio 9).
    void setHeapStart(uint32_t addr) { heap_ = addr; }
    // Si la entrada no es una terminal (tubería o archivo), se muestra lo leído
    // para que la salida tenga sentido, como el eco de CPUlator.
    void setEchoInput(bool on) { echoInput_ = on; }
    // Entrada preparada de antemano (comando 'input'): se consume antes que stdin.
    void queueInput(const std::string& line) { queued_.push_back(line); }
    void clearInput() { queued_.clear(); }

private:
    Console& console_;
    std::istream& in_;
    uint32_t heap_ = 0x10040000;
    bool echoInput_ = false;
    std::deque<std::string> queued_;

    bool readLine(std::string& line);
};

}  // namespace rv
