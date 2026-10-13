// Test tool only: runs a routine of the original game on a 6502 against the port's memory and
// hardware models, so the port can be built and verified one routine at a time.

#include "game/Game.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Original.h"

#include <map>
#include "nes/Cpu6502.h"

#include <cstdio>
#include <cstdlib>

namespace {

class PortBus : public Nes::Bus {
  public:
    uint8_t Read(uint16_t addr) override {
        using namespace Game;
        if (addr < 0x2000) {
            return gRam[addr & 0x7FF];
        }
        if (addr < 0x4000) {
            switch (addr & 7) {
                case 2:
                    return Hw::PpuStatus();
                case 4:
                    return Hw::gVideo.oam[Hw::gVideo.oamAddr];
                case 7:
                    return Hw::PpuDataRead();
                default:
                    return 0;
            }
        }
        if (addr == 0x4015) {
            return Hw::ApuStatus();
        }
        if (addr == 0x4016 || addr == 0x4017) {
            return Hw::JoypadRead(addr & 1);
        }
        if (addr < 0x6000) {
            return 0;
        }
        return Game::Read(addr);
    }

    void Write(uint16_t addr, uint8_t v) override {
        using namespace Game;
        if (addr < 0x2000) {
            gRam[addr & 0x7FF] = v;
            return;
        }
        if (addr < 0x4000) {
            switch (addr & 7) {
                case 0:
                    Hw::PpuCtrl(v);
                    break;
                case 1:
                    Hw::PpuMask(v);
                    break;
                case 3:
                    Hw::OamAddr(v);
                    break;
                case 4:
                    Hw::gVideo.oam[Hw::gVideo.oamAddr++] = v;
                    break;
                case 5:
                    Hw::PpuScroll(v);
                    break;
                case 6:
                    Hw::PpuAddr(v);
                    break;
                case 7:
                    Hw::PpuData(v);
                    break;
            }
            return;
        }
        if (addr == 0x4014) {
            Hw::OamDma(v);
            return;
        }
        if (addr == 0x4016) {
            Hw::JoypadStrobe(v);
            return;
        }
        if (addr <= 0x4017) {
            Hw::ApuWrite(addr, v);
            return;
        }
        if (addr < 0x6000) {
            return;
        }
        if (addr < 0x8000) {
            gWram[addr - 0x6000] = v;
            return;
        }
        // MMC1 serial port.
        if (v & 0x80) {
            mShift = 0x10;
            Hw::SetMmcControl(Hw::gMmcControl | 0x0C);
            return;
        }
        bool full = mShift & 1;
        mShift = (uint8_t)((mShift >> 1) | ((v & 1) << 4));
        if (!full) {
            return;
        }
        uint8_t data = mShift;
        mShift = 0x10;
        switch ((addr >> 13) & 3) {
            case 0:
                Hw::SetMmcControl(data);
                break;
            case 3:
                Hw::SetPrgBank(data);
                break;
            default:
                break; // CHR banks: Metroid uses CHR RAM
        }
    }

    uint8_t mShift = 0x10;
};

PortBus sBus;
Nes::Cpu6502 sCpu(sBus);
constexpr uint16_t kSentinel = 0xFFF0; // RTS lands here when the routine returns
int sDepth = 0;

} // namespace

namespace Game {

// How often each untranslated routine was run, for the end-of-run report.
std::map<uint16_t, int> gOriginalCalls;

Regs Original(uint16_t addr, Regs in) {
    gOriginalCalls[addr]++;
    // A routine running here may wait for the NMI, whose port code may call back in here: keep the
    // outer routine's CPU state and give the inner one stack below it.
    struct Saved {
        uint16_t pc;
        uint8_t a, x, y, p, s;
    } saved{ sCpu.PC(), sCpu.A(), sCpu.X(), sCpu.Y(), sCpu.P(), sCpu.S() };
    bool nested = sDepth++ > 0;
    uint8_t p = 0x24 | (in.c ? 1 : 0) | (in.z ? 2 : 0) | (in.v ? 0x40 : 0) | (in.n ? 0x80 : 0);
    uint8_t s = nested ? (uint8_t)(saved.s - 4) : 0xF0;
    uint16_t ret = kSentinel - 1;
    gRam[0x100 + s] = ret >> 8;
    gRam[0x100 + s - 1] = ret & 0xFF;
    sCpu.SetState(addr, in.a, in.x, in.y, p, (uint8_t)(s - 2));
    long steps = 0;
    while (sCpu.PC() != kSentinel) {
        uint16_t pc = sCpu.PC();
        // WaitNMIPass loops ($C42F, $C442): the port runs the NMI for them.
        if ((pc == 0xC42F || pc == 0xC442) && gRam[0x1A] == 0) {
            WaitForNmi();
        }
        sCpu.Step();
        if (++steps > 20000000) {
            fprintf(stderr, "Original($%04X) did not return (PC=$%04X)\n", addr, sCpu.PC());
            abort();
        }
    }
    sDepth--;
    Regs out;
    out.a = sCpu.A();
    out.x = sCpu.X();
    out.y = sCpu.Y();
    out.c = sCpu.P() & 1;
    out.z = sCpu.P() & 2;
    out.v = sCpu.P() & 0x40;
    out.n = sCpu.P() & 0x80;
    if (nested) {
        sCpu.SetState(saved.pc, saved.a, saved.x, saved.y, saved.p, saved.s);
    }
    return out;
}

} // namespace Game
