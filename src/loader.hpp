// loader.hpp — Carga de programas: binario crudo (CPUlator "Raw") o ELF32.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "memory.hpp"

namespace rv {

enum class FileFormat { Auto, Raw, Elf };

// Tabla de símbolos (solo existe para ELF): dirección <-> nombre.
class SymbolTable {
public:
    void add(uint32_t addr, const std::string& name);
    bool empty() const { return byAddr_.empty(); }
    size_t size() const { return byName_.size(); }
    // Busca un símbolo por nombre exacto.
    bool find(const std::string& name, uint32_t& addr) const;
    // Nombre exacto en esa dirección (o "" si no hay).
    std::string at(uint32_t addr) const;
    // "main+12" para direcciones dentro de una función (o "" si no aplica).
    std::string describe(uint32_t addr) const;

private:
    std::map<uint32_t, std::string> byAddr_;
    std::map<std::string, uint32_t> byName_;
};

struct Segment {
    uint32_t addr = 0;
    uint32_t size = 0;
};

struct Program {
    std::string path;
    FileFormat format = FileFormat::Raw;
    uint32_t entry = 0;
    uint32_t loadedBytes = 0;
    uint32_t endAddr = 0;            // fin del último segmento (para el heap)
    std::vector<Segment> segments;
    SymbolTable symbols;
};

// Carga `path` en `mem`. Si hay error devuelve false y deja el motivo en `err`.
//  - Raw: el archivo se copia byte a byte desde `base` (por defecto 0x0).
//  - ELF: se cargan los segmentos PT_LOAD y el PC inicial es e_entry.
bool loadProgram(const std::string& path, FileFormat format, uint32_t base,
                 Memory& mem, Program& prog, std::string& err);

}  // namespace rv
