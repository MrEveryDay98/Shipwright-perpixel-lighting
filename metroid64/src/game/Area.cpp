// The switchable area banks 1-5: dispatches the engine's calls into the bank mapped at $8000 to the
// native routines. Code identical in all five banks is in AreaCommon.cpp; each area's own code is in
// Brinstar.cpp, Norfair.cpp, Tourian.cpp, Kraid.cpp and Ridley.cpp.

#include "game/Area.h"

#include "game/AreaCommon.h"
#include "game/Collision.h"
#include "game/EnemyRam.h"
#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Ram.h"
#include "game/DebugShadow.h"

namespace Game {

using namespace Vars;

namespace {

bool CommonHook(uint16_t addr, const Regs& in, Regs& out) {
    out = in;
    switch (addr) {
        case 0x8058:
            EnemyMove8058();
            return true;
        case 0x80B0: {
            bool c;
            out.a = EnemyType80B0(in.x, c);
            out.c = c;
            out.y = Read((uint16_t)(0x6B02 + in.x)); // EnDataIndex
            out.n = out.a & 0x80;
            out.z = out.a == 0;
            return true;
        }
        case 0x81D1:
            Enemy81D1(in.x);
            return true;
        case 0x81DA:
            Enemy81DA(in.x);
            return true;
        case 0x8206:
            Enemy8206(in.x);
            return true;
        case 0x820F:
            Enemy820F(in.x);
            return true;
        case 0x8296:
            out.a = SpeedFromNibble8296(in.a);
            return true;
        case 0x832F:
            out.a = SpeedFromByte832F(in.a);
            out.x = in.a;
            return true;
        case 0x8B13:
            SamusEnterDoor8B13();
            return true;
        case 0x8B79:
            DrawDoors8B79();
            return true;
        default:
            return false;
    }
}

} // namespace

Regs AreaHook(uint16_t addr, Regs in) {
    Regs out;
    if (Shadow(addr, [&] { AreaHook(addr, in); }, in)) {
        return out; // DEBUG (registers not compared)
    }
    if (getenv("METROID64_AREAORIG")) { // DEBUG
        return Original(addr, in);
    }
    if (CommonHook(addr, in, out)) {
        return out;
    }
    switch (Hw::gPrgBank) {
        case 1:
            if (BrinstarHook(addr, in, out)) {
                return out;
            }
            break;
        case 2:
            if (NorfairHook(addr, in, out)) {
                return out;
            }
            break;
        case 3:
            if (TourianHook(addr, in, out)) {
                return out;
            }
            break;
        case 4:
            if (KraidHook(addr, in, out)) {
                return out;
            }
            break;
        case 5:
            if (RidleyHook(addr, in, out)) {
                return out;
            }
            break;
        default:
            break;
    }
    return Original(addr, in);
}

// DEBUG: METROID64_FUZZ=1 probes the area hooks and enemy routines with random inputs every frame
// (differential test against the original; no side effects).
static uint32_t sFuzz = 12345;
static uint8_t Rnd() {
    sFuzz = sFuzz * 1103515245u + 12345u;
    return (uint8_t)(sFuzz >> 16);
}
static Regs NativeHook(uint16_t addr, Regs in) {
    Regs out;
    if (CommonHook(addr, in, out)) {
        return out;
    }
    bool ok = false;
    switch (Hw::gPrgBank) {
        case 1: ok = BrinstarHook(addr, in, out); break;
        case 2: ok = NorfairHook(addr, in, out); break;
        case 3: ok = TourianHook(addr, in, out); break;
        case 4: ok = KraidHook(addr, in, out); break;
        case 5: ok = RidleyHook(addr, in, out); break;
    }
    return ok ? out : Original(addr, in);
}
static void FuzzAreaRoutine() {
    static uint8_t ram[0x800], wram[0x2000];
    static int total = 0;
    memcpy(ram, gRam, sizeof ram);
    memcpy(wram, gWram, sizeof wram);
    if (Hw::gPrgBank == 3) {
        uint8_t r = Rnd();
        MthrBrainStatus = r % 11;
        uint8_t r2 = Rnd() % 3;
        MotherBrainHits = r2 == 0 ? Rnd() % 9 : r2 == 1 ? Rnd() % 0x21 : (Rnd() | 0x80);
        if (MthrBrainStatus == 0x04) {
            MotherBrainHits = Rnd() % 5;
        }
        if ((MthrBrainStatus == 0x05 || MthrBrainStatus == 0x08) && r2 == 1) {
            MotherBrainHits = Rnd() % 9;
        }
        for (int a = 0x9A; a <= 0x9F; a++) {
            if (Rnd() & 1) {
                Mem((uint16_t)a) = Rnd();
            }
        }
        Mem(0x9D) &= 0x01;
        Mem(0x9C) &= 0x03;
        if (Rnd() & 1) {
            EndTimerHi = Rnd() & 0x7F;
            EndTimerLo = Rnd() & 0x99;
        }
        for (int c = 0; c < 0x80; c += 8) {
            if ((Rnd() & 7) == 0) {
                Mem((uint16_t)(0x6BF4 + c)) = 1;
                Mem((uint16_t)(0x6BF5 + c)) = Rnd();
                Mem((uint16_t)(0x6BF6 + c)) = Rnd();
                Mem((uint16_t)(0x6BF7 + c)) = Rnd() & 1;
                Mem((uint16_t)(0x6BF8 + c)) = Rnd() % 5;
                Mem((uint16_t)(0x6BFA + c)) = Rnd() & 7;
                Mem((uint16_t)(0x6BFB + c)) = Rnd() & 3;
            }
        }
        for (int z = 0; z < 0x28; z += 8) {
            if ((Rnd() & 3) == 0) {
                Mem((uint16_t)(0x0758 + z)) = (Rnd() & 0x80) | 1;
                Mem((uint16_t)(0x075B + z)) = Rnd() & 7;
                Mem((uint16_t)(0x075C + z)) = Rnd() & 3;
                Mem((uint16_t)(0x075D + z)) = Rnd() & 1;
            }
        }
        for (int s = 0; s < 6; s += 3) {
            if (Rnd() & 1) {
                Mem((uint16_t)(0x8B + s)) = Rnd() & 0x81;
                Mem((uint16_t)(0x8C + s)) = Rnd() & 1;
                Mem((uint16_t)(0x8D + s)) = Rnd() % 6;
            }
        }
    }
    char tag[16];
    snprintf(tag, sizeof tag, "AR%d", total++);
    if (getenv("PROBEDUMP") && !strcmp(getenv("PROBEDUMP"), tag)) {
        printf("dump %s: 98=%02X 99=%02X 9A-9F=%02X %02X %02X %02X %02X %02X T=%02X%02X 10C=%02X 10D=%02X cntrl=%02X spp=%02X\n", tag, Mem(0x98), Mem(0x99), Mem(0x9A), Mem(0x9B), Mem(0x9C), Mem(0x9D), Mem(0x9E), Mem(0x9F), Mem(0x10B), Mem(0x10A), Mem(0x10C), Mem(0x10D), Mem(0x6B), Mem(0x5B));
        for (int c = 0; c < 0x80; c += 8) if (Mem((uint16_t)(0x6BF4 + c))) printf("  cannon %02X: %02X %02X %02X %02X %02X %02X %02X\n", c, Mem((uint16_t)(0x6BF5 + c)), Mem((uint16_t)(0x6BF6 + c)), Mem((uint16_t)(0x6BF7 + c)), Mem((uint16_t)(0x6BF8 + c)), Mem((uint16_t)(0x6BF9 + c)), Mem((uint16_t)(0x6BFA + c)), Mem((uint16_t)(0x6BFB + c)));
    }
    Probe(0x95C3, [&] { return NativeHook(0x95C3, {}); }, {}, tag);
    memcpy(gRam, ram, sizeof ram);
    memcpy(gWram, wram, sizeof wram);
}

static void FuzzHooks() {
    FuzzAreaRoutine();
    static uint8_t ram[0x800], wram[0x2000];
    static const uint16_t kHooks[] = { 0x95A8, 0x95AB, 0x95AE, 0x95B1, 0x95B4, 0x95B7, 0x95BA, 0xE9C2, 0x95C0, 0x95C6, 0x8296, 0x832F, 0x80B0 };
    static int total = 0;
    for (uint16_t addr : kHooks) {
        memcpy(ram, gRam, sizeof ram);
        memcpy(wram, gWram, sizeof wram);
        Regs in{ Rnd(), Rnd(), Rnd() };
        Mem(0x00) = Rnd();
        Mem(0x01) = (uint8_t)(0xA0 + (Rnd() & 0x0F));
        Mem(0x04) = Rnd();
        Mem(0x05) = (uint8_t)(0x60 + (Rnd() & 0x07));
        UpdtngPrjctl = Rnd() & 0x01;
        PageIndex = (uint8_t)(0xD0 + (Rnd() % 3) * 0x10);
        ObjAction[PageIndex] = (Rnd() & 1) ? 0x0B : Rnd() & 0x0F;
        if (addr == 0xE9C2 && (Rnd() & 1)) {
            in.a = 0x98;
        }
        if (addr == 0x80B0) {
            in.x = (uint8_t)((Rnd() % 6) * 0x10);
        }
        char tag[16];
        snprintf(tag, sizeof tag, "#%d", total++);
        if (addr == 0xE9C2) {
            Probe(addr, [&] { Regs r = in; r.c = BlastTile(in.a, in.x); return r; }, in, tag); // (registers: only C compared meaningfully)
        } else {
            Probe(addr, [&] { return NativeHook(addr, in); }, in, tag);
        }
        memcpy(gRam, ram, sizeof ram);
        memcpy(gWram, wram, sizeof wram);
    }
    // Enemy routines with random enemy states.
    for (int k = 0; k < 4; k++) {
        memcpy(ram, gRam, sizeof ram);
        memcpy(wram, gWram, sizeof wram);
        uint8_t x = (uint8_t)((Rnd() % 6) * 0x10);
        PageIndex = x;
        Enstatus[x] = (uint8_t)(1 + Rnd() % 6);
        if (Enstatus[x] == 0x05) {
            Enstatus[x] = 0x02; // (statuses 4/5 with random data made the original run away)
        }
        if (Enstatus[x] == 0x04) {
            Enstatus[x] = 0x01;
        }
        Mem(0x81) = (Rnd() & 3) ? Enstatus[x] : (uint8_t)(1 + Rnd() % 6);
        EnDataIndex[x] = Rnd() & 0x0F;
        if (Hw::gPrgBank == 1 && EnDataIndex[x] > 0x0A) {
            EnDataIndex[x] &= 0x07;
        }
        for (int i = 0; i < 0x10; i++) {
            if (i != 0x06 && i != 0x08 && (Rnd() & 1)) {
                Mem((uint16_t)(0x0400 + x + i)) = Rnd();
            }
        }
        EnAnimFrame[x] = 0xF7; // keep the drawing simple most of the time
        if (En040E[x] < 0x02) {
            En0404[x] &= 0xDF; // (the item-drop path through Original($DCFC) ran away with random data)
        }
        if (Rnd() & 1) {
            En0405[x] |= 0x02;
        }
        char tag[16];
        snprintf(tag, sizeof tag, "AI%d", total++);
        if (getenv("PROBEDUMP") && !strcmp(getenv("PROBEDUMP"), tag)) {
            fprintf(stderr, "dump %s: x=%02X 81=%02X st=%02X", tag, x, Mem(0x81), Enstatus[x]);
            for (int i = 0; i < 0x10; i++) fprintf(stderr, " %02X", Mem((uint16_t)(0x0400 + x + i)));
            fprintf(stderr, " | %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", Mem((uint16_t)(0x6AF4 + x)), Mem((uint16_t)(0x6AF5 + x)), Mem((uint16_t)(0x6AF6 + x)), Mem((uint16_t)(0x6AF7 + x)), Mem((uint16_t)(0x6AF8 + x)), Mem((uint16_t)(0x6AF9 + x)), Mem((uint16_t)(0x6AFA + x)), Mem((uint16_t)(0x6AFB + x)), Mem((uint16_t)(0x6AFC + x)), Mem((uint16_t)(0x6AFD + x)), Mem((uint16_t)(0x6AFE + x)), Mem((uint16_t)(0x6AFF + x)), Mem((uint16_t)(0x6B00 + x)), Mem((uint16_t)(0x6B01 + x)), Mem((uint16_t)(0x6B02 + x)), Mem((uint16_t)(0x6B03 + x)));
        }
        Probe(0xF351, [&] { DoOneEnemy(x); return Regs{}; }, { 0, x, 0 }, tag);
        memcpy(gRam, ram, sizeof ram);
        memcpy(gWram, wram, sizeof wram);
    }
}

void AreaRoutine() {
    if (getenv("METROID64_FUZZ")) {
        FuzzHooks();
    }
    AreaHook(0x95C3);
}
void SamusEnterDoor() {
    AreaHook(0x8B13);
}
void DrawDoors() {
    AreaHook(0x8B79);
}

// Banks 1, 2, 4 and 5: the hooks at $95A8-$95C2 are RTS (A, X and Y are left as they were).
bool NoAreaHook(uint16_t addr, const Regs& in, Regs& out) {
    if (addr >= 0x95A8 && addr <= 0x95C0 && (addr - 0x95A8) % 3 == 0) {
        out = in;
        return true;
    }
    return false;
}

} // namespace Game
