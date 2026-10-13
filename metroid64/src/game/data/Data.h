#pragma once

// The game's data (rooms, graphics, music, tables) as resources. On first run the extractor copies
// it out of the player's ROM into resource files (XML); afterwards the game is built from those
// resources, and mods can replace them. The game reads the data where the original kept it in the ROM,
// so each resource carries its original bank and address.
//
// Independent of libultraship: the app wraps these in resources, the lockstep tool uses them directly.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tinyxml2 {
class XMLElement;
}

namespace Game::Data {

// Version of the extracted data. Bump it when the extraction changes, so old archives are redone.
constexpr int kDataVersion = 1;

// A run of bytes at its original place in the ROM. Untyped data is kept this way until it gets a
// resource type of its own.
struct Block {
    uint8_t bank = 0;
    uint16_t address = 0;
    std::string name;
    std::vector<uint8_t> bytes;
};

// Folder name of each 16 KiB bank's data: "title", "brinstar", "norfair", "tourian", "kraid",
// "ridley", "graphics", "engine".
const char* BankFolder(int bank);

// Checks the ROM (Metroid (USA) by PRG CRC32) and extracts every piece of data as a resource file:
// path inside the archive -> file contents. False, with a reason, if it isn't the right ROM.
bool Extract(const std::vector<uint8_t>& ines, std::map<std::string, std::string>& files, std::string& error);

// Reads a DataBlock resource (the XML root element).
bool ReadBlock(const tinyxml2::XMLElement* root, Block& out, std::string& error);
// The DataBlock resource file for a block.
std::string WriteBlock(const Block& block);

// Builds the data the game reads from the blocks (later blocks win where they overlap).
void Install(const std::vector<Block>& blocks);

// Lockstep and tools: extract, write the XML, read it back and install it, all in memory.
bool InstallFromRom(const std::vector<uint8_t>& ines, std::string& error);

} // namespace Game::Data
