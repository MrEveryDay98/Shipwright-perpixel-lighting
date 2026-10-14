#pragma once

// Bookkeeping for the presentation (not part of the original game): which part of the game drew each
// sprite, so the 3D renderer can replace Samus or an enemy with a model. Sprites are built in sprite
// RAM ($0200) at SpritePagePos, so the entries a routine adds are the ones between SpritePagePos before
// and after it runs.

#include <cstdint>

namespace Game::Present {

enum class Owner : uint8_t { None, Samus, Enemy, Projectile, Hud, Scenery, Other, Item };

struct Tag {
    Owner owner = Owner::None;
    uint8_t slot = 0; // for enemies, the enemy's RAM offset (PageIndex)
};

// Tags of the sprite RAM being built, and of the sprites on screen (copied at the sprite DMA).
extern Tag gBufferTags[64];
extern Tag gOamTags[64];

// Tags the sprite RAM entries from `from` up to (not including) SpritePagePos.
void TagSprites(uint8_t from, Owner owner, uint8_t slot = 0);
// Clears the tags of the unused entries from SpritePagePos to the end of the page.
void UntagRest();
// The sprite DMA copied sprite RAM to the screen.
void OnSpriteDma();

// Which room of the world map is drawn in each half of the name tables: room RAM A ($6000, name table
// page 0) and B ($6400, page 1). Custom geometry for a room replaces its tiles while it is on screen.
struct RoomTag {
    bool valid = false;
    uint8_t bank = 0; // area bank: 1 Brinstar, 2 Norfair, 3 Tourian, 4 Kraid, 5 Ridley
    uint8_t mapX = 0, mapY = 0;
    uint8_t room = 0;      // room number in the area's room table
    bool complete = false; // all of it is in room RAM (EndOfRoom ran)
};
extern RoomTag gRooms[2];
// SetupRoom started drawing RoomNumber (at MapPosX/MapPosY) into the room RAM CartRAMPtr points to.
void OnRoomDrawn();
// EndOfRoom: the room is all in room RAM (page 0 or 1). Tests can watch this (the lockstep tool checks
// the room layouts against what the game drew).
void OnRoomFinished();
extern void (*gRoomFinishedHook)(const RoomTag& tag, int page);
// Scrolling reached the edge of a screen going `dir` (ScrollDir: 0 up, 1 down, 2 left, 3 right): `entered`
// if it went on into the next map screen (MapPosX/Y is now that screen), else it stopped there.
void OnScroll(uint8_t dir, bool entered);
extern void (*gScrollHook)(uint8_t dir, bool entered);

} // namespace Game::Present
