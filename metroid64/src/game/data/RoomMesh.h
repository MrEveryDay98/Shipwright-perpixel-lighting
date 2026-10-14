#pragma once

// Room meshes: the 3D geometry of a scrolling section (Layout.h), generated from the tiles the game draws
// there. One file per section in the game's data, metroid/meshes/<area>/<h|v>_<XX>_<YY>, made when the
// data is extracted; the presentation loads the section's mesh when Samus enters it.
//
// The mesh keeps tile and palette numbers rather than colours: the picture of each face comes from the
// game's pattern table and palettes when it is drawn, so palette effects still show.

#include "game/data/Layout.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Game::Data {

// Geometry rules (in NES pixels): tiles below $A0 are solid (bank 7 $E7F9) and become blocks from z -8
// to +8 around the play plane, with faces only where they aren't against another solid tile; other
// tiles are flat decoration just behind the play plane.
constexpr int kBlockFront = 8;
constexpr int kBlockBack = -8;
constexpr int kDecorZ = -6;
constexpr uint8_t kFirstPassableTile = 0xA0;
// What InitTables fills room RAM with; blank in every area's pattern table (the lockstep tool checks), so it
// gets no faces.
constexpr uint8_t kBlankTile = 0xFF;
// Face brightness gives the blocks depth without lights.
constexpr uint8_t kShadeFront = 255, kShadeTop = 210, kShadeSide = 160, kShadeBottom = 110, kShadeDecor = 190;

struct MeshQuad {
    int16_t pos[4][3]; // x right, y up (the section's top-left is 0,0, so y is negative), z out of the screen
    uint8_t uv[4][2];  // texel corners in the 8x8 tile
    uint8_t shade;
};

// The quads drawn with one tile picture: tile number in the background pattern table, palette 0-3.
struct MeshGroup {
    uint8_t tile = 0, palette = 0;
    std::vector<MeshQuad> quads;
};

// Quads of one 8x8-tile block of a screen (the last row of blocks is 6 tiles high), so the renderer can
// swap a block for what the game has drawn there when that changes (blasted blocks).
struct MeshChunk {
    uint8_t screen = 0, cx = 0, cy = 0; // screen in the section; block column 0-3, row 0-3
    std::vector<MeshGroup> groups;
};

struct RoomMesh {
    Section section;
    uint64_t layoutHash = 0; // of the screens it was made from: a mesh made from other tiles is remade
    std::vector<MeshChunk> chunks;
};

// The screens of a section as the game draws them (a room past the area's room table is left blank). False
// if the section is off the map.
bool SectionScreens(const Level& level, const Section& s, const EngineRead& engine, std::vector<ScreenTiles>& out);
uint64_t LayoutHash(const std::vector<ScreenTiles>& screens);

// One 8x8-tile block of the section from these screens' tiles (neighbouring blocks hide faces).
MeshChunk BuildMeshChunk(const Section& s, const std::vector<ScreenTiles>& screens, int screen, int cx, int cy);
RoomMesh BuildRoomMesh(const Section& s, const std::vector<ScreenTiles>& screens);

// The file: "RMSH", version, the section, the layout hash, then the chunks (little endian).
std::string EncodeRoomMesh(const RoomMesh& mesh);
bool DecodeRoomMesh(const std::string& data, RoomMesh& mesh);
std::string RoomMeshPath(const Section& s);
bool IsRoomMeshFile(const std::string& path);

// Makes the meshes of every section (AllSections) into `files`.
void ExtractRoomMeshes(const Level& level, const EngineRead& engine, std::map<std::string, std::string>& files);

} // namespace Game::Data
