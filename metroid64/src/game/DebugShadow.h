#pragma once
// TEMPORARY DEBUG HELPERS (to be removed): see Shadow().
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Original.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace Game {
// DEBUG: METROID64_SHADOW=1 runs the original routine, then the native one from the same state, and
// prints the RAM differences (the original's results are kept).
inline uint8_t sShRam[0x800], sShWram[0x2000], sOrRam[0x800], sOrWram[0x2000];
inline int sShadowDepth = 0;
template <class F> inline bool Shadow(uint16_t addr, F native, Regs in = {}) {
    static const char* env = getenv("METROID64_SHADOW");
    char key[8];
    snprintf(key, sizeof key, "%04X", addr);
    if (!env || sShadowDepth || !strstr(env, key)) {
        return false;
    }
    sShadowDepth++;
    memcpy(sShRam, gRam, sizeof sShRam);
    memcpy(sShWram, gWram, sizeof sShWram);
    Hw::Video v = Hw::gVideo;
    Original(addr, in);
    memcpy(sOrRam, gRam, sizeof sOrRam);
    memcpy(sOrWram, gWram, sizeof sOrWram);
    Hw::Video vo = Hw::gVideo;
    memcpy(gRam, sShRam, sizeof sShRam);
    memcpy(gWram, sShWram, sizeof sShWram);
    Hw::gVideo = v;
    native();
    int n = 0;
    for (int i = 0x10; i < 0x800; i++) {
        if (i >= 0x100 && i < 0x200) continue;
        if (i == 0x69 || i == 0x6A) continue;
        if (gRam[i] != sOrRam[i] && n++ < 12) printf("  shadow %04X: $%04X orig %02X native %02X\n", addr, i, sOrRam[i], gRam[i]);
    }
    for (int i = 0; i < 0x2000; i++) {
        if (gWram[i] != sOrWram[i] && n++ < 12) printf("  shadow %04X: $%04X orig %02X native %02X\n", addr, i + 0x6000, sOrWram[i], gWram[i]);
    }
    if (memcmp(&vo, &Hw::gVideo, sizeof vo) && n++ < 12) printf("  shadow %04X: video differs\n", addr);
    if (n) printf("  shadow %04X: %d differences (before: RandomNumber1=%02X FrameCount=%02X)\n", addr, n, sShRam[0x2E], sShRam[0x2D]);
    memcpy(gRam, sOrRam, sizeof sOrRam);
    memcpy(gWram, sOrWram, sizeof sOrWram);
    Hw::gVideo = vo;
    sShadowDepth--;
    return true;
}

// DEBUG: runs the original routine and the native one (returning its registers) from the current state,
// compares RAM and registers, then puts the state back as it was (no side effects). Returns the number
// of differences.
template <class F> inline int Probe(uint16_t addr, F native, Regs in, const char* tag) {
    static uint8_t ram0[0x800], wram0[0x2000], ram1[0x800], wram1[0x2000];
    sShadowDepth++;
    memcpy(ram0, gRam, sizeof ram0);
    memcpy(wram0, gWram, sizeof wram0);
    Hw::Video v = Hw::gVideo;
    Regs ro = Original(addr, in);
    memcpy(ram1, gRam, sizeof ram1);
    memcpy(wram1, gWram, sizeof wram1);
    Hw::Video vo = Hw::gVideo;
    memcpy(gRam, ram0, sizeof ram0);
    memcpy(gWram, wram0, sizeof wram0);
    Hw::gVideo = v;
    Regs rn = native();
    int n = 0;
    for (int i = 0x10; i < 0x800; i++) {
        if (i >= 0x100 && i < 0x200) continue;
        if (i == 0x69 || i == 0x6A) continue;
        if (gRam[i] != ram1[i] && n++ < 8) printf("  probe %04X %s: $%04X orig %02X native %02X\n", addr, tag, i, ram1[i], gRam[i]);
    }
    for (int i = 0; i < 0x2000; i++) {
        if (gWram[i] != wram1[i] && n++ < 8) printf("  probe %04X %s: $%04X orig %02X native %02X\n", addr, tag, i + 0x6000, wram1[i], gWram[i]);
    }
    if (memcmp(&vo, &Hw::gVideo, sizeof vo) && n++ < 8) printf("  probe %04X %s: video differs\n", addr, tag);
    if (ro.a != rn.a || ro.x != rn.x || ro.y != rn.y || ro.c != rn.c) {
        n++;
        printf("  probe %04X %s: regs orig A%02X X%02X Y%02X C%d native A%02X X%02X Y%02X C%d (in A%02X X%02X Y%02X)\n", addr, tag, ro.a, ro.x, ro.y, ro.c, rn.a, rn.x, rn.y, rn.c, in.a, in.x, in.y);
    }
    memcpy(gRam, ram0, sizeof ram0);
    memcpy(gWram, wram0, sizeof wram0);
    Hw::gVideo = v;
    sShadowDepth--;
    return n;
}

} // namespace Game
