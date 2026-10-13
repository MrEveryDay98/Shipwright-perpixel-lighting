#pragma once

// Enemy data as resources: per area, what each of the 16 enemy types is (hit points, delays, animations,
// how it moves), the movement scripts, and the projectiles' tables.
//
//   metroid/enemies/<area>   <Enemies>   the per-type tables at $960B-$97AE of the area bank (EnemyHitPointTbl
//                                        $962B, EnemyInitDelayTbl $96BB and their neighbours), the move sets
//                                        ($97D1) and the movement scripts and fireball paths they lead to
//
// Names follow what the code (Enemies.cpp, AreaCommon.cpp) does with each table; docs/DATA.md says which.

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

struct EnemyType {
    uint8_t hitPoints = 0;       // $962B
    uint8_t delay = 0;           // $96BB
    uint8_t timer = 0;           // $969B
    uint8_t flags = 0;           // $968B
    uint8_t turnAnimRewind = 0;  // $967B
    uint8_t distance = 0;        // $96AB
    uint8_t flags2 = 0;          // $977B
    uint8_t explodeAnim[2] = {}; // $960B, by direction
    uint8_t runAnim[2] = {};     // $963B
    uint8_t turnAnim[2] = {};    // $965B
    uint8_t moveSet = 0;         // $96CB: where its move set was (from $97D1)
    bool haveMoves = false;      // false: moveSet doesn't lead to a move set, it is kept as it is
    std::vector<uint8_t> moves;  // the move set: move numbers, one picked at random
};

struct EnemyMove {
    int script[2] = { -1, -1 };                             // by direction: script numbers (-1: none)
    uint8_t accelY = 0, accelX = 0, speedY = 0, speedX = 0; // $972B, $973F, $9753, $9767
};

struct EnemyProjectile {
    uint8_t anim[2] = {};    // $978B, by direction
    uint8_t offsetX[2] = {}; // $979B, by direction
    uint8_t offsetY = 0;     // $97A3
};

struct EnemyArea {
    int bank = 1;
    EnemyType types[16];
    EnemyMove moves[20];
    EnemyProjectile projectiles[4];
    int fireballPaths[4] = { -1, -1, -1, -1 }; // $97A7: script numbers
    int moveSetArea = 0;                       // bytes from $97D1 the move sets had
    std::map<int, Piece> scripts;              // movement scripts and fireball paths, [number]
};

struct EnemyAssets {
    std::map<int, EnemyArea> areas; // [bank]
};

void ExtractEnemies(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered);
std::string WriteEnemies(const EnemyArea& e);
bool ReadEnemiesResource(const tinyxml2::XMLElement* root, EnemyAssets& out, std::string& error);
bool AssembleEnemies(const EnemyAssets& a, Image& image, std::string& error);

} // namespace Game::Data
