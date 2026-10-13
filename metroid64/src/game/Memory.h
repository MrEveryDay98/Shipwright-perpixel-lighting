#pragma once

// Storage for Metroid's state and helpers the translated code uses.
//
// Metroid64 recreates Metroid by translating the original game's routines into C++ (see
// docs/PORTING.md). The game's variables keep their original meaning and addresses so the port can be
// checked byte for byte against the original. Data tables are read from the player's ROM at runtime;
// none are embedded.

#include <cstdint>
#include <vector>

namespace Game {

// 2 KiB of work RAM ($0000-$07FF) and 8 KiB of cartridge RAM ($6000-$7FFF).
extern uint8_t gRam[0x800];
extern uint8_t gWram[0x2000];

// The variable or table at a RAM or cartridge RAM address.
uint8_t& Mem(uint16_t addr);

// A named variable (see Ram.h). Reads and writes go straight to the game's memory, and indexing
// addresses the bytes that follow, as the original's indexed addressing does.
struct Var {
    uint16_t addr;

    operator uint8_t&() const {
        return Mem(addr);
    }
    uint8_t& operator[](int i) const {
        return Mem((uint16_t)(addr + i));
    }
    const Var& operator=(int v) const {
        Mem(addr) = (uint8_t)v;
        return *this;
    }
    const Var& operator=(const Var& v) const {
        Mem(addr) = Mem(v.addr);
        return *this;
    }
    const Var& operator+=(int v) const {
        Mem(addr) = (uint8_t)(Mem(addr) + v);
        return *this;
    }
    const Var& operator-=(int v) const {
        Mem(addr) = (uint8_t)(Mem(addr) - v);
        return *this;
    }
    const Var& operator|=(int v) const {
        Mem(addr) |= (uint8_t)v;
        return *this;
    }
    const Var& operator&=(int v) const {
        Mem(addr) &= (uint8_t)v;
        return *this;
    }
    const Var& operator^=(int v) const {
        Mem(addr) ^= (uint8_t)v;
        return *this;
    }
    // Little-endian 16-bit value at this address.
    uint16_t Word() const {
        return Mem(addr) | (Mem((uint16_t)(addr + 1)) << 8);
    }
    void SetWord(uint16_t v) const {
        Mem(addr) = (uint8_t)v;
        Mem((uint16_t)(addr + 1)) = (uint8_t)(v >> 8);
    }
};

// --- The player's ROM ---

// Loads and checks the PRG ROM of Metroid (USA). Returns false with a reason if it isn't that game.
bool LoadRom(const std::vector<uint8_t>& ines, const char** error);
bool RomLoaded();
// The game's data, as the 8 banks of 16 KiB the original reads (built by game/data from resources).
void SetRomImage(std::vector<uint8_t> prg);
// A byte of a specific 16 KiB bank (addr $8000-$BFFF, or $C000-$FFFF for the fixed bank 7).
uint8_t RomByte(int bank, uint16_t addr);
// Developer tool: marks coverage[bank * $4000 + offset] for every ROM byte read (null stops).
void TrackRomReads(uint8_t* coverage);

// What the original reads at a CPU address: RAM, cartridge RAM, the bank mapped at $8000 or the
// fixed bank. Used where the original reads through pointers that may point anywhere.
uint8_t Read(uint16_t addr);
uint16_t Read16(uint16_t addr);
// Writes through a pointer (RAM or cartridge RAM only).
void Write(uint16_t addr, uint8_t value);

// --- 6502 arithmetic, for code whose results depend on the carry ---

inline uint8_t Adc(uint8_t a, uint8_t b, bool& carry) {
    unsigned r = a + b + (carry ? 1 : 0);
    carry = r > 0xFF;
    return (uint8_t)r;
}
inline uint8_t Sbc(uint8_t a, uint8_t b, bool& carry) {
    int r = a - b - (carry ? 0 : 1);
    carry = r >= 0;
    return (uint8_t)r;
}
inline uint8_t Asl(uint8_t v, bool& carry) {
    carry = v & 0x80;
    return (uint8_t)(v << 1);
}
inline uint8_t Lsr(uint8_t v, bool& carry) {
    carry = v & 1;
    return v >> 1;
}
inline uint8_t Rol(uint8_t v, bool& carry) {
    bool c = v & 0x80;
    v = (uint8_t)((v << 1) | (carry ? 1 : 0));
    carry = c;
    return v;
}
inline uint8_t Ror(uint8_t v, bool& carry) {
    bool c = v & 1;
    v = (uint8_t)((v >> 1) | (carry ? 0x80 : 0));
    carry = c;
    return v;
}
// CMP / CPX / CPY: carry set when reg >= value.
inline bool Cmp(uint8_t reg, uint8_t value) {
    return reg >= value;
}

} // namespace Game
