#pragma once

// Sprite animations and frames as resources: how objects (Samus, her shots and bombs, doors...) and the
// enemies of each area are drawn.
//
//   metroid/sprites/objects          <Sprites Kind="objects">  in all five area banks (identical there):
//                                    ObjectAnimIdxTbl ($8572), FramePtrTable ($860B), PlacePtrTable ($86DF)
//   metroid/sprites/<area>/enemies   <Sprites Kind="enemies">  EnemyAnimIndexTbl, EnemyFramePtrTbl1/2 and
//                                    EnemyPlacePtrTbl of the area (AreaPointers $95A0-$95A7 lead to them)
//
// An animation is a run of frame numbers ($FF ends it); the game refers to animations by their index
// in the table. A frame is a control byte, two radii and a list of sprites; each sprite takes the next
// entry of the frame's placement table (y and x offsets from the object's position).

#include "game/data/Image.h"
#include "game/data/Pieces.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tinyxml2 {
class XMLElement;
}

namespace Game::Data {

struct SpriteSet {
    bool enemies = false;
    int area = 0;           // enemies: the area's bank
    std::vector<int> banks; // the banks it goes into
    uint16_t animTable = 0;
    uint16_t animSize = 0;
    std::vector<uint8_t> anims; // the whole animation table
    uint16_t frameTable = 0;    // frames 00-7F (objects: all of them)
    int frameCount = 0;
    uint16_t frameTable2 = 0; // enemies: frames 80 and up
    int frameCount2 = 0;
    uint16_t placeTable = 0;
    int placeCount = 0;
    std::map<int, Piece> frames; // [frame number]
    std::map<int, int> frameSameAs;
    std::map<int, Piece> places; // [placement table number]
    std::map<int, int> placeSameAs;

    std::string Key() const;
};

struct SpriteAssets {
    std::map<std::string, SpriteSet> sets; // [Key()]
};

void ExtractSprites(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered);
std::string WriteSprites(const SpriteSet& s);
bool ReadSpritesResource(const tinyxml2::XMLElement* root, SpriteAssets& out, std::string& error);
bool AssembleSprites(const SpriteAssets& a, Image& image, std::string& error);

} // namespace Game::Data
