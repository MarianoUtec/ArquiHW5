// debugger.cpp — Comandos de la terminal del simulador.
#include "debugger.hpp"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace rv {

// --------------------------------------------------------- Ctrl+C (SIGINT) --
// Durante `run`/`continue`, Ctrl+C detiene la simulación en vez de cerrar el
// programa. Fuera de una ejecución, Ctrl+C cierra el simulador.
namespace {
volatile std::sig_atomic_t g_running = 0;
volatile std::sig_atomic_t g_interrupted = 0;

void onSigint(int) {
    if (g_running) {
        g_interrupted = 1;
        std::signal(SIGINT, onSigint);  // algunos sistemas reinstalan el default
    } else {
        std::_Exit(130);
    }
}

std::string hex8(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08X", v);
    return b;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<std::string> split(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> v;
    for (std::string t; iss >> t;) v.push_back(t);
    return v;
}

// Número: decimal, 0x hexadecimal, 0b binario; admite signo negativo.
bool parseNumber(const std::string& s, uint32_t& out) {
    if (s.empty()) return false;
    size_t i = 0;
    bool neg = false;
    if (s[0] == '-' || s[0] == '+') { neg = (s[0] == '-'); i = 1; }
    int base = 10;
    if (s.size() > i + 2 && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) { base = 16; i += 2; }
    else if (s.size() > i + 2 && s[i] == '0' && (s[i + 1] == 'b' || s[i + 1] == 'B')) { base = 2; i += 2; }
    if (i >= s.size()) return false;
    unsigned long long v = 0;
    for (; i < s.size(); ++i) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c == '_') continue;
        else return false;
        if (d >= base) return false;
        v = v * base + d;
        if (v > 0xFFFFFFFFull) return false;
    }
    if (neg && v > 0x80000000ull) return false;
    out = neg ? static_cast<uint32_t>(0u - static_cast<uint32_t>(v)) : static_cast<uint32_t>(v);
    return true;
}

// Texto de `line` que queda después de saltar `n` palabras.
std::string restAfter(const std::string& line, size_t n) {
    size_t i = 0;
    for (size_t k = 0; k < n; ++k) {
        i = line.find_first_not_of(" \t", i);
        if (i == std::string::npos) return "";
        i = line.find_first_of(" \t", i);
        if (i == std::string::npos) return "";
    }
    i = line.find_first_not_of(" \t", i);
    return i == std::string::npos ? "" : line.substr(i);
}

std::string unescape(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[++i];
            r += (n == 'n') ? '\n' : (n == 't') ? '\t' : n;
        } else {
            r += s[i];
        }
    }
    return r;
}
}  // namespace

void installInterruptHandler() { std::signal(SIGINT, onSigint); }

// ------------------------------------------------------------ Construcción --
Debugger::Debugger(const Options& opt, std::istream& programInput, std::ostream& out)
    : opt_(opt), out_(out), cpu_(mem_), console_(out), spim_(console_, programInput) {
    cpu_.syscalls = opt_.syscalls ? &spim_ : nullptr;
    spim_.setEchoInput(opt_.echoInput);
}

bool Debugger::load(const std::string& path) {
    Memory fresh;
    Program prog;
    std::string err;
    if (!loadProgram(path, opt_.format, opt_.base, fresh, prog, err)) {
        out_ << "Error al cargar: " << err << "\n";
        return false;
    }
    mem_ = std::move(fresh);
    prog_ = std::move(prog);
    cpu_.reset(prog_.entry);
    if (opt_.hasSp) cpu_.setReg(2, opt_.sp);
    spim_.setHeapStart(std::max<uint32_t>(0x10040000u, (prog_.endAddr + 0xFFFu) & ~0xFFFu));
    console_.clearCapture();
    takeSnapshot();
    loaded_ = true;
    return true;
}

int Debugger::exitCode() const {
    // Con verificaciones (expect), el código dice si pasaron: 0 = todas OK.
    if (failed_ > 0) return 2;
    if (passed_ > 0) return 0;
    if (cpu_.halted) return cpu_.exitCode;
    return 0;
}

// ------------------------------------------------------------- Utilidades --
DisasmOptions Debugger::disasmOptions() const {
    DisasmOptions d;
    d.abiNames = opt_.abiNames;
    d.symbols = prog_.symbols.empty() ? nullptr : &prog_.symbols;
    return d;
}

std::string Debugger::disasmAt(uint32_t addr) const {
    const uint32_t raw = mem_.read32(addr);
    char head[32];
    std::snprintf(head, sizeof head, "0x%08X:  %08X  ", addr, raw);
    return head + disassemble(decode(raw), addr, disasmOptions());
}

std::string Debugger::regLabel(unsigned i) const {
    char b[24];
    std::snprintf(b, sizeof b, "x%u (%s)", i, abiName(i));
    return b;
}

void Debugger::takeSnapshot() {
    for (unsigned i = 0; i < 32; ++i) snapshot_[i] = cpu_.reg(i);
    snapshotPc_ = cpu_.pc;
}

// Acepta: número, registro (x5, t0, pc), símbolo del ELF, con +/- desplazamiento.
//   ej: 0x1000, 4096, -1, sp, sp+8, main, main+12
bool Debugger::parseValue(const std::string& tok, uint32_t& out) {
    if (parseNumber(tok, out)) return true;
    std::string baseTok = tok;
    uint32_t offset = 0;
    bool negOff = false;
    const size_t pos = tok.find_last_of("+-");
    if (pos != std::string::npos && pos > 0) {
        if (!parseNumber(tok.substr(pos + 1), offset)) return false;
        negOff = (tok[pos] == '-');
        baseTok = tok.substr(0, pos);
    }
    uint32_t base;
    const std::string b = lower(baseTok);
    const int r = parseRegister(b);
    if (r >= 0) base = cpu_.reg(static_cast<unsigned>(r));
    else if (b == "pc") base = cpu_.pc;
    else if (!parseNumber(baseTok, base) && !prog_.symbols.find(baseTok, base)) return false;
    out = negOff ? base - offset : base + offset;
    return true;
}

void Debugger::printLocation() {
    const std::string sym = prog_.symbols.describe(cpu_.pc);
    out_ << "=> " << disasmAt(cpu_.pc);
    if (!sym.empty()) out_ << "   <" << sym << ">";
    out_ << "\n";
}

void Debugger::printStepHeader() {
    out_ << "   " << disasmAt(cpu_.pc) << "\n";
    const Instr in = decode(mem_.read32(cpu_.pc));
    if (in.op == Op::ECALL && cpu_.syscalls)
        out_ << std::string(26, ' ') << "ecall: servicio a7 = " << cpu_.reg(17) << " ("
             << SpimSyscalls::serviceName(cpu_.reg(17)) << ")\n";
}

void Debugger::printStepRecord(const StepRecord& r) {
    const std::string indent(26, ' ');
    if (r.regWritten) {
        out_ << indent << (opt_.abiNames ? abiName(r.rd) : xName(r.rd).c_str()) << ": "
             << hex8(r.regOld) << " -> " << hex8(r.regNew)
             << "  (" << static_cast<int32_t>(r.regNew) << ")";
        if (r.memRead) out_ << "  leído de " << hex8(r.loadAddr);
        out_ << "\n";
    }
    if (r.memWritten) {
        char b[96];
        const int w = static_cast<int>(r.memSize * 2);
        std::snprintf(b, sizeof b, "mem[%s] (%u B): 0x%0*X -> 0x%0*X  (%d)",
                      hex8(r.memAddr).c_str(), r.memSize, w, r.memOld, w, r.memNew,
                      static_cast<int32_t>(r.memNew));
        out_ << indent << b << "\n";
    }
    const bool isBranch = r.in.op == Op::BEQ || r.in.op == Op::BNE || r.in.op == Op::BLT ||
                          r.in.op == Op::BGE || r.in.op == Op::BLTU || r.in.op == Op::BGEU;
    if (isBranch) out_ << indent << (r.controlTransfer ? "branch tomado -> " + hex8(r.nextPc)
                                                      : std::string("branch no tomado")) << "\n";
    else if (r.controlTransfer) out_ << indent << "salto -> " << hex8(r.nextPc) << "\n";

}

void Debugger::reportEvent(Event e) {
    console_.ensureNewline();
    switch (e) {
        case Event::Exit:
            out_ << "El programa terminó con exit (código " << cpu_.exitCode << ").\n";
            break;
        case Event::SelfLoop:
            out_ << "Fin del programa: " << cpu_.haltMessage << ".\n";
            break;
        case Event::Ebreak:
            out_ << "ebreak: ejecución pausada en " << hex8(cpu_.pc) << ".\n";
            break;
        case Event::EcallHalt:
            out_ << "ecall alcanzado con las syscalls desactivadas: CPU detenida.\n";
            break;
        case Event::Error:
            out_ << "ERROR: " << cpu_.haltMessage << "\n";
            break;
        case Event::AlreadyHalted:
            out_ << "La CPU está detenida (" << cpu_.haltMessage
                 << "). Usa 'reset' para volver a empezar.\n";
            break;
        case Event::None:
            break;
    }
}

void Debugger::check(bool ok, const std::string& what, const std::string& detail) {
    if (ok) {
        ++passed_;
        out_ << "[OK]    " << what << "\n";
    } else {
        ++failed_;
        out_ << "[FALLO] " << what << "  -> " << detail << "\n";
    }
}

// ------------------------------------------------------------ Bucle REPL --
void Debugger::commandLoop(std::istream& in, bool prompt) {
    std::string line;
    while (true) {
        console_.ensureNewline();
        if (prompt) out_ << "> " << std::flush;
        if (!std::getline(in, line)) {
            if (prompt) out_ << "\n";
            break;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line[first] == '#') continue;  // comentario
        if (opt_.echoCommands && first != std::string::npos) out_ << "> " << line.substr(first) << "\n";
        if (!execute(line)) break;
    }
}

bool Debugger::execute(const std::string& rawLine) {
    std::string line = rawLine;
    if (line.find_first_not_of(" \t") == std::string::npos) {
        // Enter vacío repite el último step (como en gdb).
        if (lastCommand_.empty()) return true;
        line = lastCommand_;
    }
    std::vector<std::string> tok = split(line);
    const std::string cmd = lower(tok[0]);
    std::vector<std::string> a(tok.begin() + 1, tok.end());
    lastCommand_ = (cmd == "step" || cmd == "s" || cmd == "si") ? line : "";

    const bool needsProgram = !(cmd == "help" || cmd == "h" || cmd == "?" || cmd == "load" ||
                                cmd == "exit" || cmd == "quit" || cmd == "q" || cmd == "echo");
    if (needsProgram && !loaded_) {
        out_ << "No hay programa cargado. Usa: load <archivo>\n";
        return true;
    }

    if (cmd == "exit" || cmd == "quit" || cmd == "q") {
        out_ << "¡Hasta la próxima!\n";
        return false;
    }
    if (cmd == "help" || cmd == "h" || cmd == "?") cmdHelp();
    else if (cmd == "step" || cmd == "s" || cmd == "si") cmdStep(a);
    else if (cmd == "run" || cmd == "r" || cmd == "continue" || cmd == "c") cmdRun(a);
    else if (cmd == "break" || cmd == "b") cmdBreak(a);
    else if (cmd == "delete" || cmd == "d") cmdDelete(a);
    else if (cmd == "breaks") cmdBreak({});
    else if (cmd == "pc") cmdPc(a);
    else if (cmd == "regs" || cmd == "reg") cmdRegs(a);
    else if (cmd == "set") cmdSet(a);
    else if (cmd == "mem" || cmd == "m") cmdMem(a);
    else if (cmd == "dump") cmdDump(a);
    else if (cmd == "words" || cmd == "w") cmdWords(a);
    else if (cmd == "str") cmdStr(a);
    else if (cmd == "setmem") cmdSetMem(a);
    else if (cmd == "dis" || cmd == "x") cmdDis(a);
    else if (cmd == "info" || cmd == "status") cmdInfo();
    else if (cmd == "trace") {
        if (!a.empty()) trace_ = (lower(a[0]) == "on");
        out_ << "trace " << (trace_ ? "activado" : "desactivado") << "\n";
    } else if (cmd == "abi") {
        if (!a.empty()) opt_.abiNames = (lower(a[0]) == "on");
        out_ << "Nombres de registros: " << (opt_.abiNames ? "ABI (sp, a0, ...)" : "x0..x31") << "\n";
    } else if (cmd == "reset") {
        if (load(prog_.path)) {
            out_ << "Programa recargado. PC = " << hex8(cpu_.pc) << "\n";
        }
    } else if (cmd == "load") {
        if (a.empty()) { out_ << "Uso: load <archivo>\n"; return true; }
        const std::string path = restAfter(line, 1);
        if (load(path)) {
            breakpoints_.clear();
            out_ << "\"" << path << "\" cargado a memoria (" << prog_.loadedBytes << " bytes, "
                 << (prog_.format == FileFormat::Elf ? "ELF" : "binario crudo")
                 << "). PC = " << hex8(cpu_.pc) << "\n";
        }
    } else if (cmd == "input") {
        const std::string text = restAfter(line, 1);
        spim_.queueInput(text);
        out_ << "Entrada preparada para el programa: \"" << text << "\"\n";
    } else if (cmd == "echo") {
        out_ << unescape(restAfter(line, 1)) << "\n";
    } else if (cmd == "expect") {
        cmdExpect(a, restAfter(line, 2));
    } else {
        out_ << "Comando desconocido: '" << tok[0] << "'. Escribe 'help'.\n";
    }
    return true;
}

// ------------------------------------------------------------- Comandos --
void Debugger::cmdHelp() {
    out_ <<
        "Ejecución\n"
        "  step [n]            (s)   ejecuta n instrucciones (1 por defecto) y muestra qué cambió\n"
        "  run [max]           (r,c) ejecuta hasta: fin, breakpoint, ebreak, error o max instrucciones\n"
        "  break <dir|símbolo> (b)   pone un breakpoint        delete [dir] (d)  quita uno / todos\n"
        "  breaks                    lista los breakpoints\n"
        "  reset                     recarga el programa desde el archivo y reinicia la CPU\n"
        "  load <archivo>            carga otro programa (.bin crudo o .elf)\n"
        "  input <texto>             deja lista una línea de entrada para read_int/read_string\n"
        "Inspección\n"
        "  pc [valor]                muestra (o cambia) el contador de programa\n"
        "  regs [r ...]              todos los registros, o solo los indicados (x5 t0 a0 ...)\n"
        "  mem <ini> [fin]     (m)   bytes de memoria en [ini, fin]\n"
        "  dump <dir> [n]            volcado hexadecimal + ASCII de n bytes (64 por defecto)\n"
        "  words <dir> [n]     (w)   n palabras de 32 bits (hex y decimal)\n"
        "  str <dir>                 cadena terminada en '\\0'\n"
        "  dis [dir] [n]       (x)   desensambla n instrucciones (desde el PC por defecto)\n"
        "  info                      estado general: archivo, entrada, instrucciones ejecutadas\n"
        "Modificación\n"
        "  set <reg|pc> <valor>      escribe un registro       setmem <dir> <valor> [b|h|w]\n"
        "Opciones\n"
        "  trace on|off              muestra cada instrucción durante run\n"
        "  abi on|off                nombres ABI (a0, sp) o x0..x31 en el desensamblador\n"
        "  help (h) | exit (q)\n"
        "Valores: 4096, 0x1000, 0b101, -1, registros (sp, x5), símbolos ELF (main), sp+8, main+4.\n"
        "Enter vacío repite el último 'step'. Ctrl+C detiene un 'run' largo.\n";
}

void Debugger::cmdStep(const std::vector<std::string>& a) {
    uint32_t n = 1;
    if (!a.empty() && !parseValue(a[0], n)) { out_ << "Uso: step [n]\n"; return; }
    takeSnapshot();
    for (uint32_t i = 0; i < n; ++i) {
        if (cpu_.halted) { reportEvent(Event::AlreadyHalted); return; }
        printStepHeader();
        StepRecord rec;
        const Event e = cpu_.step(&rec);
        console_.ensureNewline();
        if (e == Event::Error) { reportEvent(e); return; }
        printStepRecord(rec);
        if (e != Event::None) { reportEvent(e); return; }
    }
}

void Debugger::cmdRun(const std::vector<std::string>& a) {
    uint64_t limit = opt_.maxSteps;
    if (!a.empty()) {
        uint32_t v;
        if (!parseValue(a[0], v)) { out_ << "Uso: run [max_instrucciones]\n"; return; }
        limit = v;
    }
    if (cpu_.halted) { reportEvent(Event::AlreadyHalted); return; }

    takeSnapshot();
    uint64_t count = 0;
    Event e = Event::None;
    std::string reason;
    g_interrupted = 0;
    g_running = 1;
    while (true) {
        if (count >= limit) { reason = "límite de " + std::to_string(limit) + " instrucciones (¿bucle?)"; break; }
        if (g_interrupted) { reason = "interrumpido con Ctrl+C"; break; }
        // Un breakpoint detiene ANTES de ejecutar la instrucción (salvo la primera).
        if (count > 0 && breakpoints_.count(cpu_.pc)) { reason = "breakpoint en " + hex8(cpu_.pc); break; }
        StepRecord rec;
        if (trace_) printStepHeader();
        e = cpu_.step(trace_ ? &rec : nullptr);
        if (trace_ && e != Event::Error) { console_.ensureNewline(); printStepRecord(rec); }
        if (e != Event::Error) ++count;
        if (e != Event::None) break;
    }
    g_running = 0;

    console_.ensureNewline();
    if (e != Event::None) reportEvent(e);
    out_ << "Ejecutadas " << count << (count == 1 ? " instrucción" : " instrucciones")
         << ". PC = " << hex8(cpu_.pc);
    if (!reason.empty()) out_ << "  [detenido: " << reason << "]";
    out_ << "\n";
    if (!cpu_.halted) printLocation();
}

void Debugger::cmdBreak(const std::vector<std::string>& a) {
    if (a.empty()) {
        if (breakpoints_.empty()) { out_ << "No hay breakpoints.\n"; return; }
        out_ << "Breakpoints:\n";
        for (uint32_t bp : breakpoints_) out_ << "  " << disasmAt(bp) << "\n";
        return;
    }
    for (const auto& t : a) {
        uint32_t addr;
        if (!parseValue(t, addr)) { out_ << "Dirección o símbolo inválido: " << t << "\n"; continue; }
        if (addr & 3) { out_ << "La dirección debe estar alineada a 4: " << hex8(addr) << "\n"; continue; }
        breakpoints_.insert(addr);
        out_ << "Breakpoint en " << hex8(addr);
        const std::string s = prog_.symbols.describe(addr);
        if (!s.empty()) out_ << " <" << s << ">";
        out_ << "\n";
    }
}

void Debugger::cmdDelete(const std::vector<std::string>& a) {
    if (a.empty()) {
        breakpoints_.clear();
        out_ << "Se eliminaron todos los breakpoints.\n";
        return;
    }
    for (const auto& t : a) {
        uint32_t addr;
        if (parseValue(t, addr) && breakpoints_.erase(addr)) out_ << "Breakpoint eliminado: " << hex8(addr) << "\n";
        else out_ << "No hay breakpoint en " << t << "\n";
    }
}

void Debugger::cmdPc(const std::vector<std::string>& a) {
    if (!a.empty()) {
        uint32_t v;
        if (!parseValue(a[0], v)) { out_ << "Valor inválido: " << a[0] << "\n"; return; }
        cpu_.pc = v;
    }
    out_ << "pc = " << hex8(cpu_.pc) << "\n";
}

void Debugger::cmdRegs(const std::vector<std::string>& a) {
    if (a.empty()) {
        out_ << "pc = " << hex8(cpu_.pc) << "    instrucciones ejecutadas: " << cpu_.instret
             << "    (* = cambió en la última ejecución)\n";
        for (unsigned row = 0; row < 8; ++row) {
            for (unsigned col = 0; col < 4; ++col) {
                const unsigned i = col * 8 + row;
                char b[40];
                std::snprintf(b, sizeof b, "%-4s%-5s0x%08X%c   ", xName(i).c_str(), abiName(i),
                              cpu_.reg(i), cpu_.reg(i) != snapshot_[i] ? '*' : ' ');
                std::string cell = b;
                if (col == 3) cell.erase(cell.find_last_not_of(' ') + 1);
                out_ << cell;
            }
            out_ << "\n";
        }
        return;
    }
    for (const auto& t : a) {
        const std::string n = lower(t);
        if (n == "pc") { out_ << "pc        = " << hex8(cpu_.pc) << "\n"; continue; }
        const int r = parseRegister(n);
        if (r < 0) { out_ << "Registro inválido: " << t << "\n"; continue; }
        char b[96];
        const uint32_t v = cpu_.reg(static_cast<unsigned>(r));
        std::snprintf(b, sizeof b, "%-9s = 0x%08X  (%d, sin signo %u)", regLabel(r).c_str(), v,
                      static_cast<int32_t>(v), v);
        out_ << b << "\n";
    }
}

void Debugger::cmdSet(const std::vector<std::string>& a) {
    uint32_t v;
    if (a.size() < 2 || !parseValue(a[1], v)) { out_ << "Uso: set <registro|pc> <valor>\n"; return; }
    const std::string n = lower(a[0]);
    if (n == "pc") { cpu_.pc = v; out_ << "pc = " << hex8(v) << "\n"; return; }
    const int r = parseRegister(n);
    if (r < 0) { out_ << "Registro inválido: " << a[0] << "\n"; return; }
    if (r == 0) { out_ << "x0 (zero) siempre vale 0: no se puede escribir.\n"; return; }
    cpu_.setReg(static_cast<unsigned>(r), v);
    out_ << regLabel(r) << " = " << hex8(v) << "\n";
}

void Debugger::cmdMem(const std::vector<std::string>& a) {
    uint32_t lo, hi;
    if (a.empty() || !parseValue(a[0], lo)) { out_ << "Uso: mem <ini> [fin]\n"; return; }
    if (a.size() >= 2) {
        if (!parseValue(a[1], hi)) { out_ << "Dirección final inválida.\n"; return; }
    } else {
        hi = lo + 15;
    }
    if (hi < lo) std::swap(lo, hi);
    if (hi - lo >= 4096) { out_ << "Rango demasiado grande (máx. 4096 bytes); usa 'dump'.\n"; return; }
    char b[48];
    std::snprintf(b, sizeof b, "Memoria (0x%X-0x%X):", lo, hi);
    out_ << b;
    const bool multiline = (hi - lo) >= 16;
    for (uint64_t addr = lo; addr <= hi; ++addr) {
        if (multiline && (addr - lo) % 16 == 0) {
            std::snprintf(b, sizeof b, "\n  %s:", hex8(static_cast<uint32_t>(addr)).c_str());
            out_ << b;
        }
        std::snprintf(b, sizeof b, " %02X", mem_.read8(static_cast<uint32_t>(addr)));
        out_ << b;
    }
    out_ << "\n";
}

void Debugger::cmdDump(const std::vector<std::string>& a) {
    uint32_t start, len = 64;
    if (a.empty() || !parseValue(a[0], start) || (a.size() > 1 && !parseValue(a[1], len))) {
        out_ << "Uso: dump <dir> [n_bytes]\n";
        return;
    }
    len = std::min<uint32_t>(len, 4096);
    for (uint32_t off = 0; off < len; off += 16) {
        char b[16];
        out_ << hex8(start + off) << "  ";
        std::string ascii;
        for (uint32_t k = 0; k < 16; ++k) {
            if (off + k < len) {
                const uint8_t v = mem_.read8(start + off + k);
                std::snprintf(b, sizeof b, "%02X ", v);
                out_ << b;
                ascii += (v >= 32 && v < 127) ? static_cast<char>(v) : '.';
            } else {
                out_ << "   ";
            }
            if (k == 7) out_ << ' ';
        }
        out_ << " |" << ascii << "|\n";
    }
}

void Debugger::cmdWords(const std::vector<std::string>& a) {
    uint32_t start, n = 8;
    if (a.empty() || !parseValue(a[0], start) || (a.size() > 1 && !parseValue(a[1], n))) {
        out_ << "Uso: words <dir> [n]\n";
        return;
    }
    n = std::min<uint32_t>(n, 1024);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t addr = start + 4 * i, v = mem_.read32(addr);
        char b[64];
        std::snprintf(b, sizeof b, "%s:  0x%08X  %11d", hex8(addr).c_str(), v, static_cast<int32_t>(v));
        out_ << b << "\n";
    }
}

void Debugger::cmdStr(const std::vector<std::string>& a) {
    uint32_t addr;
    if (a.empty() || !parseValue(a[0], addr)) { out_ << "Uso: str <dir>\n"; return; }
    std::string s;
    for (uint32_t i = 0; i < 1024; ++i) {
        const char c = static_cast<char>(mem_.read8(addr + i));
        if (!c) break;
        if (c == '\n') s += "\\n";
        else s += c;
    }
    out_ << hex8(addr) << ": \"" << s << "\"\n";
}

void Debugger::cmdSetMem(const std::vector<std::string>& a) {
    uint32_t addr, v;
    if (a.size() < 2 || !parseValue(a[0], addr) || !parseValue(a[1], v)) {
        out_ << "Uso: setmem <dir> <valor> [b|h|w]\n";
        return;
    }
    const std::string size = a.size() > 2 ? lower(a[2]) : "w";
    if (size == "b") mem_.write8(addr, static_cast<uint8_t>(v));
    else if (size == "h") mem_.write16(addr, static_cast<uint16_t>(v));
    else mem_.write32(addr, v);
    out_ << "mem[" << hex8(addr) << "] <- " << hex8(v) << " (" << size << ")\n";
}

void Debugger::cmdDis(const std::vector<std::string>& a) {
    uint32_t addr = cpu_.pc, n = 10;
    if (!a.empty() && !parseValue(a[0], addr)) { out_ << "Uso: dis [dir] [n]\n"; return; }
    if (a.size() > 1 && !parseValue(a[1], n)) { out_ << "Uso: dis [dir] [n]\n"; return; }
    addr &= ~3u;
    n = std::min<uint32_t>(n, 512);
    for (uint32_t i = 0; i < n; ++i, addr += 4) {
        const std::string label = prog_.symbols.at(addr);
        if (!label.empty()) out_ << "<" << label << ">:\n";
        const char mark = (addr == cpu_.pc) ? '=' : ' ';
        out_ << (mark == '=' ? "=>" : "  ") << (breakpoints_.count(addr) ? '*' : ' ') << disasmAt(addr) << "\n";
    }
}

void Debugger::cmdInfo() {
    out_ << "Programa:        " << prog_.path << " ("
         << (prog_.format == FileFormat::Elf ? "ELF32" : "binario crudo") << ", "
         << prog_.loadedBytes << " bytes)\n";
    for (const auto& s : prog_.segments)
        out_ << "  segmento:      " << hex8(s.addr) << " - " << hex8(s.addr + s.size - (s.size ? 1 : 0)) << "\n";
    out_ << "Punto de entrada " << hex8(prog_.entry) << "\n";
    if (!prog_.symbols.empty()) out_ << "Símbolos:        " << prog_.symbols.size() << "\n";
    out_ << "PC:              " << hex8(cpu_.pc) << "\n";
    out_ << "Instrucciones:   " << cpu_.instret << "\n";
    out_ << "Estado:          ";
    if (!cpu_.halted) out_ << "listo para ejecutar\n";
    else out_ << (cpu_.error ? "ERROR - " : "terminado - ") << cpu_.haltMessage
              << " (código " << cpu_.exitCode << ")\n";
    out_ << "Breakpoints:     " << breakpoints_.size() << "\n";
    out_ << "Syscalls (ecall): " << (cpu_.syscalls ? "SPIM activadas" : "desactivadas") << "\n";
    out_ << "Memoria usada:   " << mem_.pagesInUse() << " página(s) de 4 KiB\n";
}

// expect: verificación automática para scripts de prueba.
//   expect <reg> <valor>             expect pc <valor>
//   expect mem8|mem16|mem32 <dir> <valor>
//   expect words <dir> v0 v1 v2 ...  (palabras consecutivas)
//   expect exit <código>             expect error
//   expect output <texto>            (la salida del programa contiene el texto)
void Debugger::cmdExpect(const std::vector<std::string>& a, const std::string& rest) {
    if (a.empty()) { out_ << "Uso: expect <qué> <valor...>\n"; return; }
    const std::string what = lower(a[0]);
    uint32_t addr, v;

    if (what == "exit") {
        if (a.size() < 2 || !parseValue(a[1], v)) { out_ << "Uso: expect exit <código>\n"; return; }
        const bool ok = cpu_.halted && !cpu_.error && static_cast<uint32_t>(cpu_.exitCode) == v;
        check(ok, "programa terminado con código " + std::to_string(static_cast<int32_t>(v)),
              cpu_.halted ? ("estado: " + cpu_.haltMessage + ", código " + std::to_string(cpu_.exitCode))
                          : std::string("el programa no ha terminado"));
        return;
    }
    if (what == "error") {
        check(cpu_.halted && cpu_.error, "la ejecución terminó con error",
              cpu_.halted ? "terminó sin error" : "no ha terminado");
        return;
    }
    if (what == "output") {
        const std::string text = unescape(rest);
        const bool ok = console_.captured().find(text) != std::string::npos;
        check(ok, "la salida contiene \"" + rest + "\"", "salida: \"" + console_.captured() + "\"");
        return;
    }
    if (what == "mem8" || what == "mem16" || what == "mem32") {
        if (a.size() < 3 || !parseValue(a[1], addr) || !parseValue(a[2], v)) { out_ << "Uso: expect memN <dir> <valor>\n"; return; }
        const uint32_t got = what == "mem8" ? mem_.read8(addr) : what == "mem16" ? mem_.read16(addr) : mem_.read32(addr);
        const uint32_t want = what == "mem8" ? (v & 0xFF) : what == "mem16" ? (v & 0xFFFF) : v;
        check(got == want, what + "[" + hex8(addr) + "] = " + hex8(want), "se leyó " + hex8(got));
        return;
    }
    if (what == "words") {
        if (a.size() < 3 || !parseValue(a[1], addr)) { out_ << "Uso: expect words <dir> v0 v1 ...\n"; return; }
        bool ok = true;
        std::string want = "{", got = "{";
        for (size_t i = 2; i < a.size(); ++i) {
            if (!parseValue(a[i], v)) { out_ << "Valor inválido: " << a[i] << "\n"; return; }
            const uint32_t g = mem_.read32(addr + 4 * static_cast<uint32_t>(i - 2));
            ok = ok && (g == v);
            want += (i > 2 ? "," : "") + std::to_string(static_cast<int32_t>(v));
            got += (i > 2 ? "," : "") + std::to_string(static_cast<int32_t>(g));
        }
        check(ok, "palabras en " + hex8(addr) + " = " + want + "}", "se leyó " + got + "}");
        return;
    }
    // Registro o pc
    if (a.size() < 2 || !parseValue(a[1], v)) { out_ << "Uso: expect <registro> <valor>\n"; return; }
    uint32_t got;
    std::string name;
    if (what == "pc") { got = cpu_.pc; name = "pc"; }
    else {
        const int r = parseRegister(what);
        if (r < 0) { check(false, "expect " + a[0], "no es un registro ni una verificación conocida"); return; }
        got = cpu_.reg(static_cast<unsigned>(r));
        name = regLabel(r);
    }
    check(got == v, name + " = " + hex8(v), "vale " + hex8(got));
}

void Debugger::runToEnd() {
    cmdRun({});
    out_ << "\n";
    cmdRegs({});
}

}  // namespace rv
