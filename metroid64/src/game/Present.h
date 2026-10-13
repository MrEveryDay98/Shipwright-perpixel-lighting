#pragma once

// Bookkeeping for the presentation (not part of the original game): which part of the game drew each
// sprite, so the 3D renderer can replace Samus or an enemy with a model. Sprites are built in sprite
// RAM ($0200) at SpritePagePos, so the entries a routine adds are the ones between SpritePagePos before
// and after it runs.

#include <cstdint>

namespace Game::Present {

enum class Owner : uint8_t { None, Samus, Enemy, Projectile, Hud, Scenery, Other };

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

} // namespace Game::Present
