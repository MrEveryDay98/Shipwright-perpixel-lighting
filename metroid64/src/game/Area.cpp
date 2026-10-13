// The switchable area banks 1-5: dispatches the engine's calls into the bank mapped at $8000 to the
// native routines. Code identical in all five banks is in AreaCommon.cpp; each area's own code is in
// Brinstar.cpp, Norfair.cpp, Tourian.cpp, Kraid.cpp and Ridley.cpp.

#include "game/Area.h"

#include "game/AreaCommon.h"
#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Ram.h"

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

void AreaRoutine() {
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
