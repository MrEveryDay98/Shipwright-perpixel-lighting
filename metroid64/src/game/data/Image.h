#pragma once

// Building the data the game reads: the 8 banks of 16 KiB, at the addresses the original used. Typed
// resources go back where they were in the ROM when they still fit; resources a mod made bigger (or new
// ones) go into free space (bytes no data uses: the original's code, which the port doesn't need).

#include <cstdint>
#include <vector>

namespace Game::Data {

class Image {
  public:
    Image();

    uint8_t Get(int bank, uint16_t address) const;
    void Put(int bank, uint16_t address, const std::vector<uint8_t>& bytes);
    void Put16(int bank, uint16_t address, uint16_t value);
    // Marks bytes as data (not free).
    void Claim(int bank, uint16_t address, size_t length);
    // Free space for `length` bytes in a bank; 0 if there is none.
    uint16_t Allocate(int bank, size_t length);

    std::vector<uint8_t>& Prg() {
        return mPrg;
    }

  private:
    std::vector<uint8_t> mPrg;
    std::vector<uint8_t> mUsed;
};

uint16_t BankBase(int bank);
// Length of the asset map block starting at this address (0 if none starts there).
size_t AssetBlockLength(int bank, uint16_t address);

} // namespace Game::Data
