#include "game/Present.h"

#include "game/Ram.h"

namespace Game::Present {

Tag gBufferTags[64];
Tag gOamTags[64];

void TagSprites(uint8_t from, Owner owner, uint8_t slot) {
    uint8_t to = Vars::SpritePagePos;
    // The page index can wrap past $FF (the status bar does at a full page): tag up to the end then.
    for (int x = from; x != (to < from ? 0x100 : to); x += 4) {
        if (x >= 0x100) {
            break;
        }
        gBufferTags[x >> 2] = { owner, slot };
    }
}

void UntagRest() {
    for (int x = Vars::SpritePagePos; x != 0 && x < 0x100; x += 4) {
        gBufferTags[x >> 2] = {};
    }
}

void OnSpriteDma() {
    for (int i = 0; i < 64; i++) {
        gOamTags[i] = gBufferTags[i];
    }
}

} // namespace Game::Present
