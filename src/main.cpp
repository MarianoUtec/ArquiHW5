// main.cpp — Punto de entrada: argumentos de línea de comandos.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "debugger.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#define STDIN_IS_TTY() (_isatty(_fileno(stdin)) != 0)
#else
#include <unistd.h>
#define STDIN_IS_TTY() (isatty(fileno(stdin)) != 0)
#endif

namespace rv { void installInterruptHandler(); }

namespace {

const char* kVersion = "rvsim 1.0 — simulador RV32I (CS3051 Arquitectura de Computadores)";

void usage(const char* argv0) {
    std::cout <<
        kVersion << "\n\n"
        "Uso: " << argv0 << " [opciones] <programa.bin | programa.elf>\n\n"
        "Opciones:\n"
        "  -b, --base <dir>      dirección de carga de un binario crudo (por defecto 0x0)\n"
        "  -f, --format <fmt>    raw | elf | auto (por defecto auto: detecta ELF por su cabecera)\n"
        "  -r, --run             ejecuta todo de corrido y muestra el estado final (sin terminal)\n"
        "  -x, --script <arch>   ejecuta los comandos de un archivo (uno por línea)\n"
        "  -c, --cmd \"a; b\"      ejecuta comandos separados por ';'\n"
        "  -m, --max <n>         límite de instrucciones para 'run' (por defecto 10000000)\n"
        "      --sp <dir>        valor inicial del stack pointer (por defecto 0, como CPUlator)\n"
        "      --no-syscalls     ecall detiene la CPU en vez de atender servicios SPIM\n"
        "      --xnames          mostrar registros como x0..x31 en lugar de nombres ABI\n"
        "  -h, --help            esta ayuda          -v, --version   versión\n\n"
        "Sin -r/-x/-c se abre la terminal interactiva (escribe 'help' dentro).\n"
        "Código de salida: el del programa simulado (exit), 1 si hubo error de ejecución.\n"
        "En scripts con 'expect': 0 si todas las verificaciones pasan, 2 si alguna falla.\n";
}

bool parseAddr(const std::string& s, uint32_t& out) {
    try {
        size_t pos = 0;
        const unsigned long long v = std::stoull(s, &pos, 0);
        if (pos != s.size() || v > 0xFFFFFFFFull) return false;
        out = static_cast<uint32_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // para que se vean bien las tildes en Windows
#endif
    rv::Options opt;
    bool runMode = false;
    std::string scriptPath, inlineCmds;
    bool haveCmds = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](std::string& dst) {
            if (i + 1 >= argc) {
                std::cerr << "Falta el valor de " << arg << "\n";
                std::exit(64);
            }
            dst = argv[++i];
        };
        std::string val;
        if (arg == "-h" || arg == "--help") { usage(argv[0]); return 0; }
        else if (arg == "-v" || arg == "--version") { std::cout << kVersion << "\n"; return 0; }
        else if (arg == "-r" || arg == "--run") runMode = true;
        else if (arg == "-x" || arg == "--script") next(scriptPath);
        else if (arg == "-c" || arg == "--cmd") { next(val); inlineCmds += val + ";"; haveCmds = true; }
        else if (arg == "--no-syscalls") opt.syscalls = false;
        else if (arg == "--xnames") opt.abiNames = false;
        else if (arg == "-b" || arg == "--base") {
            next(val);
            if (!parseAddr(val, opt.base)) { std::cerr << "Dirección base inválida: " << val << "\n"; return 64; }
        } else if (arg == "--sp") {
            next(val);
            if (!parseAddr(val, opt.sp)) { std::cerr << "Valor de sp inválido: " << val << "\n"; return 64; }
            opt.hasSp = true;
        } else if (arg == "-m" || arg == "--max") {
            next(val);
            uint32_t m;
            if (!parseAddr(val, m) || m == 0) { std::cerr << "Límite inválido: " << val << "\n"; return 64; }
            opt.maxSteps = m;
        } else if (arg == "-f" || arg == "--format") {
            next(val);
            if (val == "raw") opt.format = rv::FileFormat::Raw;
            else if (val == "elf") opt.format = rv::FileFormat::Elf;
            else if (val == "auto") opt.format = rv::FileFormat::Auto;
            else { std::cerr << "Formato inválido: " << val << " (raw|elf|auto)\n"; return 64; }
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Opción desconocida: " << arg << " (usa --help)\n";
            return 64;
        } else if (opt.path.empty()) {
            opt.path = arg;
        } else {
            std::cerr << "Sobra el argumento: " << arg << "\n";
            return 64;
        }
    }

    rv::installInterruptHandler();

    const bool batch = runMode || !scriptPath.empty() || haveCmds;
    opt.echoCommands = !scriptPath.empty() || haveCmds;
    opt.echoInput = !STDIN_IS_TTY();
    rv::Debugger dbg(opt, std::cin, std::cout);

    if (!opt.path.empty()) {
        if (!dbg.load(opt.path)) return 1;
        std::cout << "\"" << opt.path << "\" cargado a memoria. Escribe 'help' para ver los comandos.\n";
    } else if (batch) {
        usage(argv[0]);
        return 64;
    } else {
        std::cout << kVersion << "\nNo se indicó programa: usa 'load <archivo>' o 'help'.\n";
    }

    if (runMode) {
        dbg.runToEnd();
    } else if (!scriptPath.empty()) {
        std::ifstream f(scriptPath);
        if (!f) { std::cerr << "No se pudo abrir el script \"" << scriptPath << "\"\n"; return 1; }
        dbg.commandLoop(f, false);
    } else if (haveCmds) {
        std::string cmds = inlineCmds;
        for (char& ch : cmds) if (ch == ';') ch = '\n';
        std::istringstream in(cmds);
        dbg.commandLoop(in, false);
    } else {
        dbg.commandLoop(std::cin, true);
    }

    if (dbg.failedExpectations() + dbg.passedExpectations() > 0)
        std::cout << "Verificaciones: " << dbg.passedExpectations() << " OK, "
                  << dbg.failedExpectations() << " fallidas.\n";
    const int code = dbg.exitCode();
    std::cout << "Programa finalizado con código " << code << ".\n";
    return code;
}
