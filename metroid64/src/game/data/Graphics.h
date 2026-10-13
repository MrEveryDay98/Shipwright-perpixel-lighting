#pragma once

// Tile graphics as resources: one per block of graphics bank 7's loaders copy into the pattern tables
// (the GFXInfo table at $C6E0, read by LoadGFX in Engine.cpp), as a PNG of its tiles that mods can edit.
//
//   metroid/graphics/<name>      <Graphics>  where the block goes, how many tiles, the image's path
//   metroid/graphics/<name>.png  the tiles, 16 per row: pixel values 0-3 are the tiles' 2-bit pixels
//
// <name> is the disassembly's label (GFXSuitedSamus...) or, for the blocks in the area banks, one after
// the disassembly's comment (GFXBrinstarEnemies...).

#include "game/data/Image.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace tinyxml2 {
class XMLElement;
}

namespace Game::Data {

// Loads a file of the game's data by its path (the PNG a resource refers to). False if there is none.
using FileLoader = std::function<bool(const std::string& path, std::string& contents)>;

struct Graphics {
    int bank = 6;
    uint16_t address = 0; // where the block was in the ROM: the GFXInfo entries that load it point there
    std::string name;
    std::vector<uint8_t> chr; // the tiles as the PPU takes them: 16 bytes each (two bit planes)
};

constexpr int kTilesPerRow = 16;

// The tiles as an image (kTilesPerRow tiles across, pixel values 0-3), and back. `tiles` tiles are read
// from the image, which must be kTilesPerRow tiles wide and hold them.
std::string GraphicsToPng(const std::vector<uint8_t>& chr);
bool PngToGraphics(const std::string& png, int tiles, std::vector<uint8_t>& chr, std::string& error);

// Reads the graphics out of the PRG ROM into resource files (the XML and the PNG); marks the bytes it
// covers in `covered`.
void ExtractGraphics(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered);

// The Graphics resource file for a block, referring to its image at `imagePath`.
std::string WriteGraphics(const Graphics& g, const std::string& imagePath);
// Reads a Graphics resource; its image comes from `load`.
bool ReadGraphics(const tinyxml2::XMLElement* root, const FileLoader& load, Graphics& out, std::string& error);

// Puts the graphics into the image: back where they were when they still fit, else into free space, with
// the GFXInfo entries that load them updated (bank, address, length).
bool AssembleGraphics(const std::map<std::pair<int, int>, Graphics>& graphics, Image& image, std::string& error);

} // namespace Game::Data
