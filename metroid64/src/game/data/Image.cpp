#include "game/data/Image.h"

namespace Game::Data {

namespace {

struct AssetEntry {
    uint8_t bank;
    uint16_t address;
    uint16_t length;
    const char* name;
};

constexpr AssetEntry kAssets[] = {
#include "game/data/AssetMap.inc"
};

} // namespace

uint16_t BankBase(int bank) {
    return bank == 7 ? 0xC000 : 0x8000;
}

size_t AssetBlockLength(int bank, uint16_t address) {
    for (const AssetEntry& a : kAssets) {
        if (a.bank == bank && a.address == address) {
            return a.length;
        }
    }
    return 0;
}

Image::Image() : mPrg(8 * 0x4000, 0), mUsed(8 * 0x4000, 0) {
    // Everything the original used as data stays reserved, typed or not: free space is what was code.
    for (const AssetEntry& a : kAssets) {
        Claim(a.bank, a.address, a.length);
    }
    // The last bytes of the fixed bank hold the 6502 vectors; keep them out of the way too.
    Claim(7, 0xFFFA, 6);
}

uint8_t Image::Get(int bank, uint16_t address) const {
    return mPrg[(size_t)bank * 0x4000 + (address - BankBase(bank))];
}

void Image::Put(int bank, uint16_t address, const std::vector<uint8_t>& bytes) {
    size_t off = (size_t)bank * 0x4000 + (address - BankBase(bank));
    for (size_t i = 0; i < bytes.size() && off + i < mPrg.size(); i++) {
        mPrg[off + i] = bytes[i];
    }
    Claim(bank, address, bytes.size());
}

void Image::Put16(int bank, uint16_t address, uint16_t value) {
    Put(bank, address, { (uint8_t)value, (uint8_t)(value >> 8) });
}

void Image::Claim(int bank, uint16_t address, size_t length) {
    size_t off = (size_t)bank * 0x4000 + (address - BankBase(bank));
    for (size_t i = 0; i < length && off + i < mUsed.size(); i++) {
        mUsed[off + i] = 1;
    }
}

uint16_t Image::Allocate(int bank, size_t length) {
    size_t base = (size_t)bank * 0x4000;
    size_t run = 0;
    for (size_t i = 0; i < 0x4000; i++) {
        run = mUsed[base + i] ? 0 : run + 1;
        if (run == length) {
            uint16_t address = (uint16_t)(BankBase(bank) + i + 1 - length);
            Claim(bank, address, length);
            return address;
        }
    }
    return 0;
}

} // namespace Game::Data
