#include "game/Memory.h"

#include "game/Hw.h"

#include <cstring>

namespace Game {

uint8_t gRam[0x800];
uint8_t gWram[0x2000];

namespace {

constexpr uint32_t kUsaPrgCrc32 = 0x70080810;
constexpr size_t kPrgSize = 8 * 0x4000;
std::vector<uint8_t> sPrg;
uint8_t sOpenBus = 0; // target for stray writes

uint32_t Crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

} // namespace

uint8_t& Mem(uint16_t addr) {
    if (addr < 0x2000) {
        return gRam[addr & 0x7FF];
    }
    if (addr >= 0x6000 && addr < 0x8000) {
        return gWram[addr - 0x6000];
    }
    sOpenBus = 0;
    return sOpenBus;
}

bool LoadRom(const std::vector<uint8_t>& file, const char** error) {
    if (file.size() < 16 || memcmp(file.data(), "NES\x1A", 4) != 0) {
        *error = "Not an iNES ROM (.nes)";
        return false;
    }
    size_t start = 16 + ((file[6] & 4) ? 512 : 0);
    if (file.size() < start + kPrgSize || Crc32(&file[start], kPrgSize) != kUsaPrgCrc32) {
        *error = "This is not Metroid (USA) (PRG CRC32 mismatch)";
        return false;
    }
    sPrg.assign(file.begin() + start, file.begin() + start + kPrgSize);
    return true;
}

bool RomLoaded() {
    return !sPrg.empty();
}

uint8_t RomByte(int bank, uint16_t addr) {
    size_t off = (size_t)(bank & 7) * 0x4000 + (addr & 0x3FFF);
    return off < sPrg.size() ? sPrg[off] : 0;
}

uint8_t Read(uint16_t addr) {
    if (addr < 0x2000 || (addr >= 0x6000 && addr < 0x8000)) {
        return Mem(addr);
    }
    if (addr >= 0xC000) {
        return RomByte(7, addr);
    }
    if (addr >= 0x8000) {
        return RomByte(Hw::gPrgBank, addr);
    }
    return 0;
}

uint16_t Read16(uint16_t addr) {
    return Read(addr) | (Read((uint16_t)(addr + 1)) << 8);
}

void Write(uint16_t addr, uint8_t value) {
    Mem(addr) = value;
}

} // namespace Game
