// debugger.hpp — Interfaz de terminal: intérprete de comandos sobre la CPU.
#pragma once
#include <array>
#include <cstdint>
#include <iosfwd>
#include <set>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "disasm.hpp"
#include "loader.hpp"
#include "memory.hpp"
#include "syscalls.hpp"

namespace rv {

struct Options {
    std::string path;
    FileFormat format = FileFormat::Auto;
    uint32_t base = 0;                  // dirección de carga de binarios crudos
    bool syscalls = true;               // atender ecall estilo SPIM
    bool abiNames = true;               // nombres ABI en el desensamblador
    uint64_t maxSteps = 10000000;       // límite de seguridad para run
    bool hasSp = false;                 // valor inicial de sp forzado por el usuario
    uint32_t sp = 0;
    bool echoCommands = false;          // mostrar cada comando (modo script)
    bool echoInput = false;             // mostrar la entrada leída por ecall
};

class Debugger {
public:
    Debugger(const Options& opt, std::istream& programInput, std::ostream& out);

    // Carga (o recarga) el programa y deja la CPU lista en el punto de entrada.
    bool load(const std::string& path);

    // Lee comandos de `in` hasta exit/EOF. `prompt` = mostrar "> ".
    void commandLoop(std::istream& in, bool prompt);

    // Ejecuta un comando. Devuelve false si el usuario pidió salir.
    bool execute(const std::string& line);

    // Ejecuta todo de corrido (modo --run) y muestra el estado final.
    void runToEnd();

    // Código de salida del simulador.
    int exitCode() const;
    int failedExpectations() const { return failed_; }
    int passedExpectations() const { return passed_; }
    bool loaded() const { return loaded_; }

private:
    Options opt_;
    std::ostream& out_;
    Memory mem_;
    Cpu cpu_;
    Console console_;
    SpimSyscalls spim_;
    Program prog_;
    bool loaded_ = false;

    std::set<uint32_t> breakpoints_;
    std::array<uint32_t, 32> snapshot_{};   // registros antes del último step/run
    uint32_t snapshotPc_ = 0;
    bool trace_ = false;
    std::string lastCommand_;
    int failed_ = 0, passed_ = 0;

    // Comandos
    void cmdHelp();
    void cmdStep(const std::vector<std::string>& a);
    void cmdRun(const std::vector<std::string>& a);
    void cmdBreak(const std::vector<std::string>& a);
    void cmdDelete(const std::vector<std::string>& a);
    void cmdPc(const std::vector<std::string>& a);
    void cmdRegs(const std::vector<std::string>& a);
    void cmdSet(const std::vector<std::string>& a);
    void cmdMem(const std::vector<std::string>& a);
    void cmdDump(const std::vector<std::string>& a);
    void cmdWords(const std::vector<std::string>& a);
    void cmdStr(const std::vector<std::string>& a);
    void cmdSetMem(const std::vector<std::string>& a);
    void cmdDis(const std::vector<std::string>& a);
    void cmdInfo();
    void cmdExpect(const std::vector<std::string>& a, const std::string& rest);

    // Utilidades
    bool parseValue(const std::string& tok, uint32_t& out);
    DisasmOptions disasmOptions() const;
    std::string disasmAt(uint32_t addr) const;
    std::string regLabel(unsigned i) const;
    void printLocation();
    void printStepHeader();                    // instrucción a punto de ejecutarse
    void printStepRecord(const StepRecord& r); // efectos (registros, memoria, saltos)
    void reportEvent(Event e);
    void takeSnapshot();
    void check(bool ok, const std::string& what, const std::string& detail);
};

}  // namespace rv
