#pragma once

// PPU strings: the lists of writes to video memory the game queues for palettes and screens
// (address, then a run of bytes or one byte repeated, horizontally or down a column), and the palette
// tables of the areas made of them.
//
//   metroid/palettes/<area>   <Palettes>   PalPntrTbl ($9560): the palettes PalDataPending selects
//                                          (area "title" is bank 0: title, password and ending screens)
//   metroid/screens/<name>    <Screen>     the title, password and ending screens' text and layouts:
//                                          one string at a fixed address, or a table of them

#include "game/data/Image.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tinyxml2 {
class XMLElement;
}

namespace Game::Data {

struct PpuWrite {
    uint16_t address = 0;
    bool vertical = false; // add 32 after each byte (down a column)
    bool fill = false;     // one byte, repeated `count` times
    uint8_t count = 0;     // fill only
    std::vector<uint8_t> bytes;
};

struct PpuString {
    uint16_t address = 0; // where it was in the ROM (0: new)
    uint16_t size = 0;
    std::vector<PpuWrite> writes;

    std::vector<uint8_t> Encode() const;
};

// One PPU string at a ROM address: decodes it and returns its length in bytes.
PpuString DecodePpuString(const uint8_t* prg, int bank, uint16_t address);

struct PaletteTable {
    int bank = 1;
    uint16_t table = 0x9560; // the pointer table
    int count = 0;
    std::map<uint8_t, PpuString> palettes; // [table index]
};

// Screen strings: either one string the code uses at a fixed address (table == 0, string 0), or a
// pointer table of strings.
struct ScreenSet {
    std::string name;
    int bank = 0;
    uint16_t table = 0;
    int count = 0;
    std::map<uint8_t, PpuString> strings;
    // A row of tiles the code copies itself (fixed length), instead of strings.
    uint16_t rowAddress = 0;
    std::vector<uint8_t> row;
};

struct PpuAssets {
    std::map<int, PaletteTable> palettes;     // [bank]
    std::map<std::string, ScreenSet> screens; // [name]
};

void ExtractPpu(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered);
std::string WritePalettes(const PaletteTable& t);
std::string WriteScreen(const ScreenSet& s);
bool ReadPpuResource(const tinyxml2::XMLElement* root, PpuAssets& out, std::string& error);
bool AssemblePpu(const PpuAssets& a, Image& image, std::string& error);

} // namespace Game::Data
