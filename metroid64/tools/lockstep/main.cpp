// Lockstep test: runs the original game (emulated) and the C++ port side by side with the same inputs and
// reports the first frame where their memory or picture memory differs.
//
//   lockstep <rom> [options]
//     --script <frame:hex,...>   buttons held from each frame on (like METROID64_INPUT)
//     --sync <frame>             emulated frame at which the port takes over the original's state
//     --frames <n>               frames to compare after the sync point
//     --random <seed>            random inputs after the scripted ones
//     --ram-h <path>             Ram.h, for naming differing addresses

#include "game/Game.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "nes/Nes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <regex>
#include <string>
#include <vector>

namespace {

struct Script {
    std::vector<std::pair<int, uint8_t>> events;
    uint8_t At(int frame) const {
        uint8_t b = 0;
        for (const auto& [f, v] : events) {
            if (f <= frame) {
                b = v;
            }
        }
        return b;
    }
};

Script ParseScript(const char* s) {
    Script sc;
    for (const char* p = s; *p;) {
        int f = atoi(p);
        const char* colon = strchr(p, ':');
        if (!colon) {
            break;
        }
        sc.events.emplace_back(f, (uint8_t)strtol(colon + 1, nullptr, 16));
        const char* comma = strchr(colon, ',');
        p = comma ? comma + 1 : colon + strlen(colon);
    }
    return sc;
}

std::map<uint16_t, std::string> LoadNames(const char* ramH) {
    std::map<uint16_t, std::string> names;
    std::ifstream f(ramH);
    std::string line;
    std::regex re(R"(Var (\w+)\{0x([0-9A-Fa-f]+)\})");
    while (std::getline(f, line)) {
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            uint16_t a = (uint16_t)std::stoul(m[2], nullptr, 16);
            if (!names.count(a)) {
                names[a] = m[1];
            }
        }
    }
    return names;
}

std::string Name(const std::map<uint16_t, std::string>& names, uint16_t addr) {
    auto it = names.upper_bound(addr);
    if (it == names.begin()) {
        return "";
    }
    --it;
    if (addr - it->first > 0x40) {
        return "";
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "%s+%d", it->second.c_str(), addr - it->first);
    return addr == it->first ? it->second : std::string(buf);
}

// Scratch bytes the original reuses freely inside a frame; not compared.
bool Ignored(uint16_t addr) {
    if (addr <= 0x0F) {
        return true; // general purpose bytes and pointers
    }
    if (addr == 0x69 || addr == 0x6A) {
        return true; // TempX / TempY
    }
    if (addr >= 0x0110 && addr < 0x0200) {
        return true; // stack
    }
    return false;
}

constexpr uint16_t kWaitLoopStart = 0xC0CB, kWaitLoopEnd = 0xC0D2;

bool InWaitLoop(const Nes::Machine& m) {
    uint16_t pc = m.Cpu().PC();
    return pc >= kWaitLoopStart && pc <= kWaitLoopEnd;
}

void CopyState(const Nes::Machine& emu) {
    using namespace Game;
    memcpy(gRam, emu.Ram(), 0x800);
    memcpy(gWram, emu.Wram(), 0x2000);
    auto& v = Hw::gVideo;
    memcpy(v.nametables, emu.Vram(), 0x800);
    memcpy(v.palette, emu.Palette(), 0x20);
    memcpy(v.chr, emu.Chr(), 0x2000);
    memcpy(v.oam, emu.Oam(), 0x100);
    v.ctrl = emu.PpuCtrl();
    v.mask = emu.PpuMask();
    v.scrollX = emu.ScrollRegX();
    v.scrollY = emu.ScrollRegY();
    v.oamAddr = emu.OamAddrReg();
    Hw::SetMmcControl(emu.MmcControl());
    Hw::SetPrgBank(emu.PrgBank());
}

int Compare(const Nes::Machine& emu, const std::map<uint16_t, std::string>& names, int frame) {
    using namespace Game;
    int diffs = 0;
    auto report = [&](const char* what, uint16_t addr, uint8_t orig, uint8_t port, const std::string& name) {
        if (diffs < 24) {
            printf("  %s $%04X %-24s original %02X  port %02X\n", what, addr, name.c_str(), orig, port);
        }
        diffs++;
    };
    for (uint16_t a = 0; a < 0x800; a++) {
        if (!Ignored(a) && emu.Ram()[a] != gRam[a]) {
            report("RAM ", a, emu.Ram()[a], gRam[a], Name(names, a));
        }
    }
    for (uint16_t a = 0; a < 0x2000; a++) {
        if (emu.Wram()[a] != gWram[a]) {
            report("WRAM", (uint16_t)(0x6000 + a), emu.Wram()[a], gWram[a], Name(names, (uint16_t)(0x6000 + a)));
        }
    }
    const auto& v = Hw::gVideo;
    for (int a = 0; a < 0x800; a++) {
        if (emu.Vram()[a] != v.nametables[a]) {
            report("VRAM", (uint16_t)a, emu.Vram()[a], v.nametables[a], "nametable");
        }
    }
    for (int a = 0; a < 0x20; a++) {
        if (emu.Palette()[a] != v.palette[a]) {
            report("PAL ", (uint16_t)a, emu.Palette()[a], v.palette[a], "palette");
        }
    }
    for (int a = 0; a < 0x2000; a++) {
        if (emu.Chr()[a] != v.chr[a]) {
            report("CHR ", (uint16_t)a, emu.Chr()[a], v.chr[a], "pattern");
        }
    }
    for (int a = 0; a < 0x100; a++) {
        if (emu.Oam()[a] != v.oam[a]) {
            report("OAM ", (uint16_t)a, emu.Oam()[a], v.oam[a], "sprite");
        }
    }
    if (emu.PpuCtrl() != v.ctrl) {
        report("CTRL", 0x2000, emu.PpuCtrl(), v.ctrl, "PPU control");
    }
    if (emu.PpuMask() != v.mask) {
        report("MASK", 0x2001, emu.PpuMask(), v.mask, "PPU mask");
    }
    if (emu.PrgBank() != Hw::gPrgBank) {
        report("BANK", 0x8000, emu.PrgBank(), Hw::gPrgBank, "PRG bank");
    }
    if (diffs > 0) {
        printf("frame %d: %d differences\n", frame, diffs);
    }
    return diffs;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: lockstep <rom> [--script s] [--sync n] [--frames n] [--random seed] [--ram-h path]\n");
        return 2;
    }
    Script script;
    int sync = 300, frames = 600;
    long seed = -1;
    const char* ramH = "metroid64/src/game/Ram.h";
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string opt = argv[i];
        if (opt == "--script") {
            script = ParseScript(argv[i + 1]);
        } else if (opt == "--sync") {
            sync = atoi(argv[i + 1]);
        } else if (opt == "--frames") {
            frames = atoi(argv[i + 1]);
        } else if (opt == "--random") {
            seed = atol(argv[i + 1]);
        } else if (opt == "--ram-h") {
            ramH = argv[i + 1];
        }
    }
    auto names = LoadNames(ramH);

    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)), {});
    const char* error = nullptr;
    if (!Game::LoadRom(rom, &error)) {
        fprintf(stderr, "%s\n", error);
        return 2;
    }
    static Nes::Machine emu;
    emu.Load(rom);
    emu.Reset();

    std::mt19937 rng(seed >= 0 ? (uint32_t)seed : 0);
    int lastScripted = script.events.empty() ? 0 : script.events.back().first;
    // Buttons for every frame, decided up front so both sides see the same ones.
    std::vector<uint8_t> inputs;
    uint8_t randomHeld = 0;
    auto buttonsAt = [&](int frame) -> uint8_t {
        while ((int)inputs.size() <= frame) {
            int f = (int)inputs.size();
            if (seed < 0 || f <= lastScripted) {
                inputs.push_back(script.At(f));
                continue;
            }
            // Random play: hold a combination for a while. Start is left out so the game isn't paused
            // most of the time.
            if (f % 12 == 0) {
                randomHeld = (uint8_t)(rng() & 0xF7);
                if ((randomHeld & 0x30) == 0x30) {
                    randomHeld &= ~0x20;
                }
                if ((randomHeld & 0xC0) == 0xC0) {
                    randomHeld &= ~0x40;
                }
            }
            inputs.push_back(randomHeld);
        }
        return inputs[frame];
    };

    int frame = 0;
    for (; frame < sync || !InWaitLoop(emu); frame++) {
        emu.SetButtons(buttonsAt(frame));
        emu.RunFrame();
    }
    CopyState(emu);
    printf("synced at frame %d (GameMode %02X MainRoutine %02X)\n", frame, emu.Ram()[0x1D], emu.Ram()[0x1E]);

    // Each port frame is matched against as many emulated frames as it ran NMIs. Lag frames of the
    // original (its main loop not done by the next vblank) can't be matched; the port resyncs after them.
    int portFrameButtons = 0;
    Game::SetNmiHook([&](int nmiIndex) { Game::Hw::gPads[0] = buttonsAt(portFrameButtons + nmiIndex); });
    int compared = 0, resyncs = 0;
    for (int n = 0; n < frames; n++) {
        portFrameButtons = frame;
        int nmis = Game::RunFrame();
        bool lag = false;
        for (int k = 0; k < nmis; k++) {
            emu.SetButtons(buttonsAt(frame));
            emu.RunFrame();
            frame++;
        }
        lag = !InWaitLoop(emu);
        if (lag) {
            while (!InWaitLoop(emu)) {
                emu.SetButtons(buttonsAt(frame));
                emu.RunFrame();
                frame++;
            }
            CopyState(emu);
            resyncs++;
            continue;
        }
        if (Compare(emu, names, frame) > 0) {
            printf("FAILED after %d compared frames (%d resyncs). GameMode %02X MainRoutine %02X\n", compared,
                   resyncs, emu.Ram()[0x1D], emu.Ram()[0x1E]);
            return 1;
        }
        compared++;
    }
    printf("OK: %d frames matched (%d resyncs after lag frames), ended at frame %d (GameMode %02X MainRoutine "
           "%02X)\n",
           compared, resyncs, frame, emu.Ram()[0x1D], emu.Ram()[0x1E]);
    return 0;
}
