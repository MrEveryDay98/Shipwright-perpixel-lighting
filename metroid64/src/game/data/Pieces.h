#pragma once

// Pieces of data that pointers lead to (enemy movement scripts, sprite frames...), and putting them back.
// Several pointers may lead to the same bytes, and pieces may overlap (one piece's bytes the start of
// another's): an unmodified piece goes back where it was, a changed one where it still fits and agrees
// with the pieces sharing its bytes, any other into free space.

#include "game/data/Image.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace tinyxml2 {
class XMLElement;
}

namespace Game::Data {

struct Piece {
    uint16_t address = 0; // where it was in the ROM (0: new)
    uint16_t size = 0;    // its size there
    std::vector<uint8_t> bytes;
};

// Free space for `length` bytes; 0 if there is none.
using Allocator = std::function<uint16_t(size_t length)>;
// Told about every piece put into the image (address, length).
using PutListener = std::function<void(uint16_t address, size_t length)>;

// Puts the pieces into the bank; `at` gets each one's address (0 for an empty new piece). A piece keeps its
// address when its bytes fit in its old size and agree, byte for byte, with what most of the pieces covering
// those bytes hold there; the others are shared with an identical piece or go where `allocate` says (called
// once the pieces that keep their place are in).
bool PlacePieces(Image& image, int bank, const std::vector<const Piece*>& pieces, std::vector<uint16_t>& at,
                 const Allocator& allocate, const std::string& what, std::string& error,
                 const PutListener& onPut = nullptr);

// Helpers for the readers and writers of these resources.
namespace Bytes {

struct Prg {
    const uint8_t* p;
    uint8_t Get(int bank, uint16_t a) const;
    uint16_t Get16(int bank, uint16_t a) const;
};

// Whether the bytes are all data in the original (in the asset map), not code.
bool InData(int bank, uint16_t address, size_t length);
void Cover(std::vector<uint8_t>& covered, int bank, uint16_t address, size_t length);

const char* AreaName(int bank); // banks 1-5
int AreaBank(const char* name); // 0 if unknown

std::string Hex(unsigned v, int digits = 2);
std::string HexList(const std::vector<uint8_t>& bytes);
// Signed decimal of a byte.
std::string Signed(uint8_t v);
unsigned HexAttr(const tinyxml2::XMLElement* e, const char* name, unsigned def = 0);
std::vector<uint8_t> HexBytes(const char* text);

} // namespace Bytes

} // namespace Game::Data
