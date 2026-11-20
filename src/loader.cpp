// loader.cpp — Lectura de binarios crudos y de ejecutables ELF32 RISC-V.
//
// El parser de ELF está hecho a mano (sin <elf.h>) para que compile igual en
// Windows/MinGW, Linux y macOS.
#include "loader.hpp"

#include <fstream>
#include <iterator>

namespace rv {

// ------------------------------------------------------------ SymbolTable --
void SymbolTable::add(uint32_t addr, const std::string& name) {
    if (name.empty()) return;
    byName_[name] = addr;
    // Si hay varios nombres en la misma dirección, se queda el primero.
    byAddr_.emplace(addr, name);
}

bool SymbolTable::find(const std::string& name, uint32_t& addr) const {
    auto it = byName_.find(name);
    if (it == byName_.end()) return false;
    addr = it->second;
    return true;
}

std::string SymbolTable::at(uint32_t addr) const {
    auto it = byAddr_.find(addr);
    return it == byAddr_.end() ? "" : it->second;
}

std::string SymbolTable::describe(uint32_t addr) const {
    auto it = byAddr_.upper_bound(addr);
    if (it == byAddr_.begin()) return "";
    --it;
    const uint32_t off = addr - it->first;
    if (off > 0x10000) return "";  // demasiado lejos: no es parte de ese símbolo
    return off == 0 ? it->second : it->second + "+" + std::to_string(off);
}

// ----------------------------------------------------------------- Helpers --
namespace {

bool readFile(const std::string& path, std::vector<uint8_t>& data) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

// Lectura little-endian desde el buffer del archivo, con control de límites.
struct Reader {
    const std::vector<uint8_t>& d;
    bool ok = true;
    uint32_t u8(size_t off) {
        if (off >= d.size()) { ok = false; return 0; }
        return d[off];
    }
    uint32_t u16(size_t off) { return u8(off) | u8(off + 1) << 8; }
    uint32_t u32(size_t off) { return u16(off) | u16(off + 2) << 16; }
};

bool isElf(const std::vector<uint8_t>& d) {
    return d.size() >= 4 && d[0] == 0x7F && d[1] == 'E' && d[2] == 'L' && d[3] == 'F';
}

bool loadElf(const std::vector<uint8_t>& d, Memory& mem, Program& prog, std::string& err) {
    Reader r{d};
    if (d.size() < 52) { err = "ELF truncado"; return false; }
    if (d[4] != 1) { err = "solo se soporta ELF de 32 bits (ELFCLASS32)"; return false; }
    if (d[5] != 1) { err = "solo se soporta ELF little-endian"; return false; }
    if (r.u16(18) != 243) { err = "el ELF no es para RISC-V (e_machine != 243)"; return false; }

    prog.entry = r.u32(24);
    const uint32_t phoff = r.u32(28), shoff = r.u32(32);
    const uint32_t phentsize = r.u16(42), phnum = r.u16(44);
    const uint32_t shentsize = r.u16(46), shnum = r.u16(48);

    // Program headers: cargar cada segmento PT_LOAD (p_type == 1).
    for (uint32_t i = 0; i < phnum; ++i) {
        const size_t ph = phoff + static_cast<size_t>(i) * phentsize;
        if (r.u32(ph) != 1) continue;
        const uint32_t offset = r.u32(ph + 4), vaddr = r.u32(ph + 8);
        const uint32_t filesz = r.u32(ph + 16), memsz = r.u32(ph + 20);
        if (!r.ok || static_cast<size_t>(offset) + filesz > d.size()) {
            err = "segmento PT_LOAD fuera del archivo";
            return false;
        }
        mem.load(vaddr, d.data() + offset, filesz);
        for (uint32_t k = filesz; k < memsz; ++k) mem.write8(vaddr + k, 0);  // .bss
        prog.segments.push_back({vaddr, memsz});
        prog.loadedBytes += filesz;
        if (vaddr + memsz > prog.endAddr) prog.endAddr = vaddr + memsz;
    }
    if (prog.segments.empty()) { err = "el ELF no tiene segmentos cargables"; return false; }

    // Section headers: buscar .symtab (SHT_SYMTAB == 2) para tener nombres.
    for (uint32_t i = 0; i < shnum && shentsize >= 40; ++i) {
        const size_t sh = shoff + static_cast<size_t>(i) * shentsize;
        if (r.u32(sh + 4) != 2) continue;
        const uint32_t symOff = r.u32(sh + 16), symSize = r.u32(sh + 20);
        const uint32_t link = r.u32(sh + 24), entsize = r.u32(sh + 36);
        const size_t strSh = shoff + static_cast<size_t>(link) * shentsize;
        const uint32_t strOff = r.u32(strSh + 16), strSize = r.u32(strSh + 20);
        if (!r.ok || entsize < 16) break;
        for (uint32_t s = 0; s + entsize <= symSize; s += entsize) {
            const size_t sym = symOff + s;
            const uint32_t nameIdx = r.u32(sym), value = r.u32(sym + 4);
            const uint32_t type = r.u8(sym + 12) & 0xF;
            const uint32_t shndx = r.u16(sym + 14);
            if (type != 0 && type != 1 && type != 2) continue;  // NOTYPE/OBJECT/FUNC
            if (shndx == 0 || nameIdx == 0 || nameIdx >= strSize) continue;
            std::string name;
            for (size_t c = strOff + nameIdx; c < d.size() && d[c]; ++c) name += static_cast<char>(d[c]);
            if (name.empty() || name[0] == '$' || name[0] == '.') continue;  // símbolos internos
            prog.symbols.add(value, name);
        }
        break;
    }
    return r.ok || !prog.segments.empty();
}

}  // namespace

bool loadProgram(const std::string& path, FileFormat format, uint32_t base,
                 Memory& mem, Program& prog, std::string& err) {
    std::vector<uint8_t> data;
    if (!readFile(path, data)) {
        err = "no se pudo abrir \"" + path + "\"";
        return false;
    }
    prog = Program{};
    prog.path = path;

    if (format == FileFormat::Auto) format = isElf(data) ? FileFormat::Elf : FileFormat::Raw;
    prog.format = format;

    if (format == FileFormat::Elf) {
        if (!isElf(data)) { err = "el archivo no es un ELF"; return false; }
        return loadElf(data, mem, prog, err);
    }

    // Binario crudo: los bytes van tal cual a partir de `base`.
    if (data.empty()) { err = "el archivo está vacío"; return false; }
    mem.load(base, data.data(), data.size());
    prog.entry = base;
    prog.loadedBytes = static_cast<uint32_t>(data.size());
    prog.endAddr = base + prog.loadedBytes;
    prog.segments.push_back({base, prog.loadedBytes});
    return true;
}

}  // namespace rv
