// Lockstep test: runs the original game (emulated) and the C++ port side by side with the same inputs and
// reports the first frame where their memory or picture memory differs.
//
//   lockstep <rom> [options]
//     --script <frame:hex,...>   buttons held from each frame on (like METROID64_INPUT)
//     --sync <frame>             emulated frame at which the port takes over the original's state
//     --frames <n>               frames to compare after the sync point
//     --random <seed>            random inputs after the scripted ones
//     --ram-h <path>             Ram.h, for naming differing addresses
//     --poke <addr=val,...>      written into both games' memory before every frame (e.g. items,
//                                health), so random play can reach more of the game
//     --dump <addr[-addr],...>   prints these RAM/WRAM bytes of the original at the end (or failure)
//     --password <area,gear,missiles[,suitless]>   (hex) starts the game from a password made with the
//                                port's own password code: area $10-$14, all items collected none.
//                                Replaces --script; --sync defaults to 30
//     --picture                  also compares the picture the app draws from the port's video memory with
//                                the emulated PPU's, every compared frame
//     --check-lag                also compares at the frames where the original lagged and prints the
//                                differences without failing (a few are expected: the original ran extra
//                                NMIs, so joypad, sound and sprite bytes differ)

#include "Picture.h"
#include "game/Game.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Ram.h"
#include "game/Title.h"
#include "nes/Nes.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
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

// APU register writes in order, from both sides.
std::vector<std::pair<uint16_t, uint8_t>> gEmuApu, gPortApu;

class RecordingMachine : public Nes::Machine {
  public:
    void Write(uint16_t addr, uint8_t value) override {
        if (addr >= 0x4000 && addr <= 0x4017 && addr != 0x4014 && addr != 0x4016) {
            gEmuApu.emplace_back(addr, value);
        }
        Nes::Machine::Write(addr, value);
    }
};

bool CompareApu(int frame) {
    if (gEmuApu == gPortApu) {
        return true;
    }
    printf("frame %d: APU writes differ\n", frame);
    size_t n = std::max(gEmuApu.size(), gPortApu.size());
    for (size_t i = 0, shown = 0; i < n && shown < 24; i++) {
        bool same = i < gEmuApu.size() && i < gPortApu.size() && gEmuApu[i] == gPortApu[i];
        if (same && shown == 0) {
            continue;
        }
        auto fmt = [](const std::vector<std::pair<uint16_t, uint8_t>>& v, size_t k) {
            char b[16];
            if (k < v.size()) {
                snprintf(b, sizeof(b), "$%04X=%02X", v[k].first, v[k].second);
            } else {
                snprintf(b, sizeof(b), "-");
            }
            return std::string(b);
        };
        printf("  #%zu original %-10s port %-10s%s\n", i, fmt(gEmuApu, i).c_str(), fmt(gPortApu, i).c_str(),
               same ? "" : "  <--");
        shown++;
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

// Prints the original's bytes in the ranges of a --dump list ("1F,6870-6877").
void Dump(const char* tag, Nes::Machine& emu, const std::string& spec) {
    if (spec.empty()) {
        return;
    }
    printf("%s:", tag);
    for (size_t p = 0; p < spec.size();) {
        size_t q = spec.find(',', p);
        if (q == std::string::npos) {
            q = spec.size();
        }
        std::string item = spec.substr(p, q - p);
        p = q + 1;
        size_t dash = item.find('-');
        unsigned first = strtoul(item.c_str(), nullptr, 16);
        unsigned last = dash == std::string::npos ? first : strtoul(item.c_str() + dash + 1, nullptr, 16);
        printf(" %04X:", first);
        for (unsigned a = first; a <= last; a++) {
            printf("%02X", a >= 0x6000 ? emu.Wram()[a - 0x6000] : emu.Ram()[a & 0x7FF]);
        }
    }
    printf("\n");
}

// Buttons that go from the title to the password screen and type a password made for the given
// area, gear and missiles, then press START. Leaves the port's RAM dirty (it is replaced at the sync).
Script PasswordScript(const char* spec) {
    unsigned v[4] = { 0x10, 0, 0, 0 };
    sscanf(spec, "%x,%x,%x,%x", &v[0], &v[1], &v[2], &v[3]);
    using namespace Game::Vars;
    for (int i = 0; i < 0x12; i++) {
        PasswordByte00[i] = 0;
    }
    PasswordByte08 = (uint8_t)((v[0] & 0x3F) | (v[3] ? 0x80 : 0));
    PasswordByte09 = (uint8_t)v[1];
    PasswordByte0A = (uint8_t)v[2];
    PasswordByte10 = 0x01;
    Game::PswrdChksmNScrmbl();
    Game::LoadPasswordChar();

    // Title: START, SELECT (to CONTINUE), START; then the 13x5 character grid, cursor at 0.
    Script sc;
    sc.events = { { 120, 0x08 }, { 125, 0 }, { 160, 0x04 }, { 165, 0 }, { 200, 0x08 }, { 205, 0 } };
    int f = 300, row = 0, col = 0;
    auto press = [&](uint8_t b, int on, int off) {
        sc.events.emplace_back(f, b);
        sc.events.emplace_back(f + on, 0);
        f += on + off;
    };
    for (int i = 0; i < 24; i++) {
        int r = (PasswordChar00[i] & 0x3F) / 13, c = (PasswordChar00[i] & 0x3F) % 13;
        while (row != r) {
            press(0x20, 2, 3); // down
            row = (row + 1) % 5;
        }
        while (col != c) {
            press(0x80, 2, 3); // right, wrapping to the next row
            if (++col == 13) {
                col = 0;
                row = (row + 1) % 5;
                while (row != r) {
                    press(0x20, 2, 3);
                    row = (row + 1) % 5;
                }
            }
        }
        press(0x01, 2, 3); // A
    }
    f += 30;
    press(0x08, 3, 60);
    return sc;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: lockstep <rom> [--script s] [--sync n] [--frames n] [--random seed] [--ram-h path] "
                        "[--poke a=v,...] [--dump ranges] [--check-lag]\n");
        return 2;
    }
    Script script;
    int sync = 300, frames = 600;
    long seed = -1;
    std::vector<std::pair<uint16_t, uint8_t>> pokes;
    const char* ramH = "metroid64/src/game/Ram.h";
    std::string dump;
    const char* password = nullptr;
    bool syncGiven = false;
    bool checkLag = false;
    bool picture = false;
    for (int i = 2; i < argc; i += 2) {
        std::string opt = argv[i];
        if (opt == "--check-lag") {
            checkLag = true;
            i--;
            continue;
        }
        if (opt == "--picture") {
            picture = true;
            i--;
            continue;
        }
        if (i + 1 >= argc) {
            break;
        }
        if (opt == "--password") {
            password = argv[i + 1];
        } else if (opt == "--dump") {
            dump = argv[i + 1];
        } else if (opt == "--script") {
            script = ParseScript(argv[i + 1]);
        } else if (opt == "--sync") {
            sync = atoi(argv[i + 1]);
            syncGiven = true;
        } else if (opt == "--frames") {
            frames = atoi(argv[i + 1]);
        } else if (opt == "--random") {
            seed = atol(argv[i + 1]);
        } else if (opt == "--ram-h") {
            ramH = argv[i + 1];
        } else if (opt == "--poke") {
            for (const char* p = argv[i + 1]; *p;) {
                uint16_t a = (uint16_t)strtol(p, nullptr, 16);
                const char* eq = strchr(p, '=');
                if (!eq) {
                    break;
                }
                pokes.emplace_back(a, (uint8_t)strtol(eq + 1, nullptr, 16));
                const char* comma = strchr(eq, ',');
                p = comma ? comma + 1 : eq + strlen(eq);
            }
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
    if (password) {
        script = PasswordScript(password);
        if (!syncGiven) {
            sync = 30;
        }
    }
    static RecordingMachine emu;
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
    Game::Hw::SetApuSink([](uint16_t reg, uint8_t v) { gPortApu.emplace_back(reg, v); }, nullptr);
    gEmuApu.clear();
    printf("synced at frame %d (GameMode %02X MainRoutine %02X)\n", frame, emu.Ram()[0x1D], emu.Ram()[0x1E]);

    // Each port frame is matched against as many emulated frames as it ran NMIs. Lag frames of the
    // original (its main loop not done by the next vblank) can't be matched; the port resyncs after them.
    int portFrameButtons = 0;
    Game::SetNmiHook([&](int nmiIndex) { Game::Hw::gPads[0] = buttonsAt(portFrameButtons + nmiIndex); });
    int compared = 0, resyncs = 0;
    auto applyPokes = [&]() {
        for (const auto& [a, v] : pokes) {
            emu.Write(a, v);
            Game::Mem(a) = v;
        }
    };
    for (int n = 0; n < frames; n++) {
        applyPokes();
        portFrameButtons = frame;
        gEmuApu.clear();
        gPortApu.clear();
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
                applyPokes();
                emu.SetButtons(buttonsAt(frame));
                emu.RunFrame();
                frame++;
            }
            if (checkLag) {
                printf("lag frames up to %d (the port ran %d NMIs):\n", frame, nmis);
                Compare(emu, names, frame);
            }
            CopyState(emu);
            resyncs++;
            continue;
        }
        bool pictureOk = true;
        if (picture) {
            static uint32_t a[256 * 240], b[256 * 240];
            emu.RenderFrame(a);
            Metroid::RenderPicture(Game::Hw::gVideo, b);
            for (int i = 0; i < 256 * 240 && pictureOk; i++) {
                if (a[i] != b[i]) {
                    printf("frame %d: picture differs at x %d y %d (emulator %08X, port %08X)\n", frame, i % 256,
                           i / 256, a[i], b[i]);
                    pictureOk = false;
                }
            }
        }
        if (Compare(emu, names, frame) > 0 || !CompareApu(frame) || !pictureOk) {
            printf("FAILED after %d compared frames (%d resyncs). GameMode %02X MainRoutine %02X\n", compared, resyncs,
                   emu.Ram()[0x1D], emu.Ram()[0x1E]);
            Dump("original", emu, dump);
            return 1;
        }
        compared++;
    }
    printf("OK: %d frames matched (%d resyncs after lag frames), ended at frame %d (GameMode %02X MainRoutine "
           "%02X)\n",
           compared, resyncs, frame, emu.Ram()[0x1D], emu.Ram()[0x1E]);
    Dump("original", emu, dump);
    return 0;
}
