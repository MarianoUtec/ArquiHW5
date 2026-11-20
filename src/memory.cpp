// memory.cpp — Implementación de la memoria paginada.
#include "memory.hpp"

namespace rv {

const Memory::Page* Memory::findPage(uint32_t addr) const {
    auto it = pages_.find(addr >> kPageBits);
    return it == pages_.end() ? nullptr : it->second.get();
}

Memory::Page& Memory::pageFor(uint32_t addr) {
    auto& slot = pages_[addr >> kPageBits];
    if (!slot) {
        slot.reset(new Page);
        slot->fill(0);
    }
    return *slot;
}

uint8_t Memory::read8(uint32_t addr) const {
    const Page* p = findPage(addr);
    return p ? (*p)[addr & (kPageSize - 1)] : 0;
}

// Los accesos de 16/32 bits se arman byte a byte: así funcionan aunque no
// estén alineados o crucen el borde de una página, y el orden de los bytes
// es explícitamente little-endian (byte menos significativo en addr).
uint16_t Memory::read16(uint32_t addr) const {
    return static_cast<uint16_t>(read8(addr) | (read8(addr + 1) << 8));
}

uint32_t Memory::read32(uint32_t addr) const {
    return static_cast<uint32_t>(read8(addr)) |
           static_cast<uint32_t>(read8(addr + 1)) << 8 |
           static_cast<uint32_t>(read8(addr + 2)) << 16 |
           static_cast<uint32_t>(read8(addr + 3)) << 24;
}

void Memory::write8(uint32_t addr, uint8_t value) {
    pageFor(addr)[addr & (kPageSize - 1)] = value;
}

void Memory::write16(uint32_t addr, uint16_t value) {
    write8(addr, static_cast<uint8_t>(value));
    write8(addr + 1, static_cast<uint8_t>(value >> 8));
}

void Memory::write32(uint32_t addr, uint32_t value) {
    write8(addr, static_cast<uint8_t>(value));
    write8(addr + 1, static_cast<uint8_t>(value >> 8));
    write8(addr + 2, static_cast<uint8_t>(value >> 16));
    write8(addr + 3, static_cast<uint8_t>(value >> 24));
}

void Memory::load(uint32_t base, const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i)
        write8(base + static_cast<uint32_t>(i), data[i]);
}

void Memory::clear() { pages_.clear(); }

}  // namespace rv
