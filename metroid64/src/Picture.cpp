#include "Picture.h"

namespace Metroid {

namespace {

// NTSC 2C02 colours.
constexpr uint32_t kPalette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00, 0x333500, 0x0B4800, 0x005200,
    0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000, 0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B,
    0xB53120, 0x994E00, 0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000, 0xFFFEFF,
    0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22, 0xBCBE00, 0x88D800, 0x5CE430, 0x45E082,
    0x48CDDE, 0x4F4F4F, 0x000000, 0x000000, 0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5,
    0xF7D8A5, 0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
};

} // namespace

uint32_t NesColor(uint8_t index) {
    return (kPalette[index & 0x3F] << 8) | 0xFF;
}

// The picture, the way the NES draws it from picture memory: the background scrolled as the game set it
// (Metroid never changes the scroll mid-picture), then up to 8 sprites a line in front of or behind it.
void RenderPicture(const Game::Hw::Video& v, uint32_t* out) {
    const uint8_t* chr = v.chr;
    int startX = (v.ctrl & 0x01) * 256 + v.scrollX;
    int startY = ((v.ctrl >> 1) & 0x01) * 240 + v.scrollY;
    for (int line = 0; line < 240; line++) {
        uint8_t bg[256] = {};
        if (v.mask & 0x08) {
            uint16_t patBase = (v.ctrl & 0x10) ? 0x1000 : 0;
            int py = (startY + line) % 480;
            int ntY = py / 240, y = py % 240;
            for (int x = 0; x < 256; x++) {
                if (x < 8 && !(v.mask & 0x02)) {
                    continue;
                }
                int px = (startX + x) & 511;
                int ntX = px >> 8, cx = (px >> 3) & 31, cy = y >> 3;
                uint16_t nt = (uint16_t)(0x2000 | (((ntY << 1) | ntX) << 10));
                uint8_t tile = v.Nametable((uint16_t)(nt | (cy << 5) | cx));
                uint8_t attr = v.Nametable((uint16_t)(nt | 0x3C0 | ((cy >> 2) << 3) | (cx >> 2)));
                uint8_t pal = (attr >> (((cy & 2) << 1) | (cx & 2))) & 3;
                uint16_t p = (uint16_t)(patBase + tile * 16 + (y & 7));
                int bit = 7 - (px & 7);
                uint8_t ci = ((chr[p] >> bit) & 1) | (((chr[p + 8] >> bit) & 1) << 1);
                if (ci) {
                    bg[x] = (uint8_t)(pal * 4 + ci);
                }
            }
        }
        uint8_t sp[256] = {};
        bool behind[256] = {};
        if (v.mask & 0x10) {
            int h = (v.ctrl & 0x20) ? 16 : 8;
            int found = 0;
            for (int s = 0; s < 64 && found < 8; s++) {
                const uint8_t* o = &v.oam[s * 4];
                int row = line - (o[0] + 1);
                if (row < 0 || row >= h) {
                    continue;
                }
                found++;
                if (o[2] & 0x80) {
                    row = h - 1 - row;
                }
                uint16_t p;
                if (h == 16) {
                    p = (uint16_t)(((o[1] & 1) ? 0x1000 : 0) + (o[1] & 0xFE) * 16 + (row >= 8 ? 16 : 0) + (row & 7));
                } else {
                    p = (uint16_t)(((v.ctrl & 0x08) ? 0x1000 : 0) + o[1] * 16 + row);
                }
                for (int i = 0; i < 8; i++) {
                    int x = o[3] + i;
                    if (x > 255 || sp[x] || (x < 8 && !(v.mask & 0x04))) {
                        continue;
                    }
                    int bit = (o[2] & 0x40) ? i : 7 - i;
                    uint8_t ci = ((chr[p] >> bit) & 1) | (((chr[p + 8] >> bit) & 1) << 1);
                    if (ci) {
                        sp[x] = (uint8_t)(16 + (o[2] & 3) * 4 + ci);
                        behind[x] = o[2] & 0x20;
                    }
                }
            }
        }
        for (int x = 0; x < 256; x++) {
            uint8_t idx = 0;
            if (sp[x] && (!behind[x] || !bg[x])) {
                idx = sp[x];
            } else if (bg[x]) {
                idx = bg[x];
            }
            out[line * 256 + x] = NesColor(v.palette[idx] & 0x3F);
        }
    }
}

} // namespace Metroid
