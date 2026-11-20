// memory.hpp — Memoria plana byte-addressable de 32 bits, little-endian.
//
// Un arreglo de 4 GiB no es viable, así que la memoria se divide en páginas
// de 4 KiB que se crean solo cuando se escriben. Leer una dirección que nunca
// se escribió devuelve 0 (sin crear la página).
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace rv {

class Memory {
public:
    static constexpr uint32_t kPageBits = 12;
    static constexpr uint32_t kPageSize = 1u << kPageBits;  // 4096 bytes

    uint8_t  read8(uint32_t addr) const;
    uint16_t read16(uint32_t addr) const;   // little-endian
    uint32_t read32(uint32_t addr) const;   // little-endian

    void write8(uint32_t addr, uint8_t value);
    void write16(uint32_t addr, uint16_t value);
    void write32(uint32_t addr, uint32_t value);

    // Copia un bloque de bytes a partir de `base` (usado por el cargador).
    void load(uint32_t base, const uint8_t* data, size_t size);

    void clear();
    size_t pagesInUse() const { return pages_.size(); }

private:
    using Page = std::array<uint8_t, kPageSize>;
    std::unordered_map<uint32_t, std::unique_ptr<Page>> pages_;

    const Page* findPage(uint32_t addr) const;
    Page& pageFor(uint32_t addr);
};

}  // namespace rv
