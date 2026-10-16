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
//     --set <addr=val,...>       written into the original's memory once, just before the port takes over
//                                (to start in a given state, e.g. the ending)
//     --dump <addr[-addr],...>   prints these RAM/WRAM bytes of the original at the end (or failure)
//     --password <area,gear,missiles[,suitless]>   (hex) starts the game from a password made with the
//                                port's own password code: area $10-$14, all items collected none.
//                                Replaces --script; --sync defaults to 30
//     --picture                  also compares the picture the app draws from the port's video memory with
//                                the emulated PPU's, every compared frame
//     --rom-coverage <file>      records which ROM bytes the port read, added to the file (128 KiB map)
//     --raw-rom                  the port reads the ROM file directly instead of the data extracted from
//                                it (by default the data goes through the extractor and its XML first)
//     --all-active               plays the port alone with "everything active" on (game/AllActive.h),
//                                which changes the game: nothing is compared; it reports how it went
//     --solo                     plays the port alone (nothing is compared) and reports how often Samus
//                                wasn't drawn while playing and not hurt (with --no-flicker, only in
//                                doors), and the frames that took more than one NMI while playing
//     --save-test <n>            (with --solo) saves the game (game/SaveState.h) after n frames, plays on,
//                                loads it, plays the same frames again and checks the game ends up the same
//     --shots-past-edge          (with --solo) shots fly past the screen edge (game/Options.h); counts
//                                the frames with shots drawn there
//     --write-save <file>        (with --solo) writes a saved game (game/SaveState.h) at the end, for the app
//     --editor-tiles             (with --solo) at the end, compares the pattern tables with the tiles the
//                                extractor writes for the editor for that area (metroid/editor/tiles_<area>.png)
//     --progress-test <n>        (with --solo) after n frames saves Samus's progress (game/Progress.h), plays on,
//                                starts the game from it and checks she is back in that room, where she was,
//                                with the same energy, missiles and items (Brinstar's InArea is $00 after a
//                                start, as it is in the original, so only the area number counts)
//     --progress-entry           (with --progress-test) save at the place she came into the room by
//     --no-flicker               (with --solo) no sprite flicker (game/Options.h)
//     --check-lag                also compares at the frames where the original lagged and prints the
//                                differences without failing (a few are expected: the original ran extra
//                                NMIs, so joypad, sound and sprite bytes differ)

#include "Picture.h"
#include "game/Constants.h"
#include "game/Game.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Options.h"
#include "game/data/Data.h"
#include "game/data/Graphics.h"
#include "game/data/Layout.h"
#include "game/AllActive.h"
#include "game/Present.h"
#include "game/Progress.h"
#include "game/Explored.h"
#include "game/Ram.h"
#include "game/SaveState.h"
#include "game/Title.h"
#include "nes/Nes.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <set>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <regex>
#include <string>
#include <vector>

namespace Game {
extern std::map<uint16_t, int> gOriginalCalls;
}

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

// Lists the untranslated routines the port still ran through the original.
void ReportOriginalCalls() {
    if (Game::gOriginalCalls.empty()) {
        return;
    }
    printf("original routines run:");
    for (const auto& [addr, n] : Game::gOriginalCalls) {
        printf(" $%04X x%d", addr, n);
    }
    printf("\n");
}

// Every room the port draws is checked against the room layout made from the level data
// (Game::Data::DrawScreen), which the presentation builds its room meshes from.
int sRoomsChecked = 0, sRoomsDiffer = 0;

void CheckRoom(const Game::Present::RoomTag& tag, int page) {
    const Game::Data::Level* level = Game::Data::InstalledLevel();
    if (level == nullptr || !tag.valid) {
        return;
    }
    Game::Data::ScreenTiles screen;
    if (!Game::Data::DrawScreen(*level, tag.bank, tag.room, [](uint16_t a) { return Game::Read(a); }, screen)) {
        // A room number past the area's room table: the game reads the next table as room pointers.
        printf("room not in the room table drawn: bank %d room %02X (map %02X,%02X), ScrollDir %d, ElevatorStatus "
               "%02X, PPUMASK %02X\n",
               tag.bank, tag.room, tag.mapX, tag.mapY, Game::Vars::ScrollDir, (uint8_t)Game::Mem(0x0320),
               Game::Vars::PPUCNT1ZP);
        return;
    }
    sRoomsChecked++;
    for (int i = 0; i < 0x400; i++) {
        uint8_t ram = Game::Mem((uint16_t)(0x6000 + page * 0x400 + i));
        if (ram != screen.ram[i]) {
            if (sRoomsDiffer++ < 5) {
                printf("room layout differs: bank %d room %02X (map %02X,%02X) at +%03X: game %02X, layout %02X\n",
                       tag.bank, tag.room, tag.mapX, tag.mapY, i, ram, screen.ram[i]);
            }
            return;
        }
    }
}

// Every time the port's scrolling reaches the edge of a screen, the sections (Game::Data::FindSection)
// must agree: a stop is at the end of the section, a step into the next screen stays in it (outside
// doors, which scroll into the next section).
int sScrollsChecked = 0, sScrollsDiffer = 0;

void CheckScroll(uint8_t dir, bool entered) {
    using namespace Game::Vars;
    const Game::Data::Level* level = Game::Data::InstalledLevel();
    if (level == nullptr || DoorStatus != 0) {
        return;
    }
    bool horizontal = dir >= 2;
    int bank = Game::Hw::gPrgBank;
    uint8_t x = MapPosX, y = MapPosY;
    Game::Data::Section s = Game::Data::FindSection(*level, bank, x, y, horizontal);
    bool ok;
    if (entered) {
        uint8_t ox = dir == 2 ? x + 1 : dir == 3 ? x - 1 : x;
        uint8_t oy = dir == 0 ? y + 1 : dir == 1 ? y - 1 : y;
        ok = Game::Data::FindSection(*level, bank, ox, oy, horizontal) == s;
    } else {
        int last = s.length - 1;
        ok = dir == 0 ? y == s.y : dir == 1 ? y == s.y + last : dir == 2 ? x == s.x : x == s.x + last;
    }
    sScrollsChecked++;
    if (!ok && sScrollsDiffer++ < 5) {
        printf("section differs: bank %d map %02X,%02X dir %d %s; section %s %02X,%02X length %d\n", bank, x, y, dir,
               entered ? "entered" : "stopped", s.horizontal ? "horizontal" : "vertical", s.x, s.y, s.length);
    }
}

// Every section the port scrolls through must be one of Game::Data::AllSections (the room meshes are made
// for those).
std::set<Game::Data::Section> sAllSections;
std::set<Game::Data::Section> sMissedSections;
int sSectionFrames = 0;
bool sBlankTileWarned = false;

void CheckSection() {
    using namespace Game::Vars;
    const Game::Data::Level* level = Game::Data::InstalledLevel();
    if (level == nullptr || GameMode != 0 || MainRoutine != 0x03 || DoorStatus != 0) {
        return;
    }
    if (level->world.rooms[MapPosY & 0x1F][MapPosX & 0x1F] == 0xFF) {
        return;
    }
    Game::Data::Section s =
        Game::Data::FindSection(*level, Game::Hw::gPrgBank, MapPosX, MapPosY, (ScrollDir & 0x02) != 0);
    sSectionFrames++;
    if (!sAllSections.count(s) && sMissedSections.insert(s).second) {
        printf("section not found by AllSections: bank %d %s %02X,%02X length %d\n", s.bank,
               s.horizontal ? "horizontal" : "vertical", s.x, s.y, s.length);
    }
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
    std::vector<std::pair<uint16_t, uint8_t>> sets;
    const char* ramH = "metroid64/src/game/Ram.h";
    std::string dump;
    const char* password = nullptr;
    const char* coverageFile = nullptr;
    static uint8_t coverage[8 * 0x4000];
    bool syncGiven = false;
    bool checkLag = false;
    bool picture = false;
    bool rawRom = false;
    bool allActive = false;
    bool solo = false;
    int saveTest = -1, progressTest = -1;
    bool progressAtEntry = false;
    const char* writeSave = nullptr;
    bool editorTiles = false;
    for (int i = 2; i < argc; i += 2) {
        std::string opt = argv[i];
        if (opt == "--all-active") {
            allActive = true;
            i--;
            continue;
        }
        if (opt == "--solo") {
            solo = true;
            i--;
            continue;
        }
        if (opt == "--editor-tiles") {
            editorTiles = true;
            i--;
            continue;
        }
        if (opt == "--shots-past-edge") {
            Game::Options::gShotsPastEdge = true;
            i--;
            continue;
        }
        if (opt == "--write-save" && i + 1 < argc) {
            writeSave = argv[i + 1];
            continue;
        }
        if (opt == "--no-flicker") {
            Game::Options::gNoFlicker = true;
            i--;
            continue;
        }
        if (opt == "--check-lag") {
            checkLag = true;
            i--;
            continue;
        }
        if (opt == "--raw-rom") {
            rawRom = true;
            i--;
            continue;
        }
        if (opt == "--picture") {
            picture = true;
            i--;
            continue;
        }
        if (opt == "--progress-entry") {
            progressAtEntry = true;
            i--;
            continue;
        }
        if (opt == "--progress-test" && i + 1 < argc) {
            progressTest = atoi(argv[i + 1]);
            continue;
        }
        if (opt == "--save-test" && i + 1 < argc) {
            saveTest = atoi(argv[i + 1]);
            continue;
        }
        if (i + 1 >= argc) {
            break;
        }
        if (opt == "--rom-coverage") {
            coverageFile = argv[i + 1];
        } else if (opt == "--password") {
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
        } else if (opt == "--set") {
            for (const char* p = argv[i + 1]; *p;) {
                uint16_t a = (uint16_t)strtol(p, nullptr, 16);
                const char* eq = strchr(p, '=');
                if (!eq) {
                    break;
                }
                sets.emplace_back(a, (uint8_t)strtol(eq + 1, nullptr, 16));
                const char* comma = strchr(eq, ',');
                p = comma ? comma + 1 : eq + strlen(eq);
            }
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
    if (!rawRom) {
        // The game's data as the app gets it: extracted, written as resource XML, read back.
        std::string why;
        if (!Game::Data::InstallFromRom(rom, why)) {
            fprintf(stderr, "data extraction: %s\n", why.c_str());
            return 2;
        }
        Game::Present::gRoomFinishedHook = CheckRoom;
        Game::Present::gScrollHook = CheckScroll;
        for (const auto& sec :
             Game::Data::AllSections(*Game::Data::InstalledLevel(), [](uint16_t a) { return Game::Read(a); })) {
            sAllSections.insert(sec);
        }
    }
    if (coverageFile) {
        std::ifstream old(coverageFile, std::ios::binary);
        old.read((char*)coverage, sizeof(coverage));
        Game::TrackRomReads(coverage);
    }
    auto saveCoverage = [&]() {
        if (coverageFile) {
            std::ofstream out(coverageFile, std::ios::binary);
            out.write((const char*)coverage, sizeof(coverage));
        }
    };
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
    for (const auto& [a, v] : sets) {
        emu.Write(a, v);
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
    if (solo && progressTest >= 0) {
        auto play = [&](int count) {
            for (int n = 0; n < count; n++) {
                for (const auto& [a, v] : pokes) {
                    Game::Mem(a) = v;
                }
                portFrameButtons = frame;
                frame += std::max(Game::RunFrame(), 1);
                Game::Explored::Update();
                Game::Progress::TrackEntry();
            }
        };
        play(progressTest);
        Game::Progress::Data saved;
        if (!Game::Progress::Capture(saved, progressAtEntry)) {
            printf("PROGRESS-TEST: nothing to save at frame %d (MainRoutine %02X)\n", progressTest,
                   (uint8_t)Game::Vars::MainRoutine);
            return 0;
        }
        Game::Progress::Data back;
        if (!Game::Progress::Decode(Game::Progress::Encode(saved), back) || memcmp(&back.stats, &saved.stats, sizeof(saved.stats))) {
            printf("PROGRESS-TEST: FAIL (the save doesn't read back)\n");
            return 1;
        }
        play(600);
        Game::Progress::Start(back);
        // No input while she appears.
        pokes.clear();
        int n = 0;
        for (; n < 1200 && !(Game::Vars::GameMode == 0 && Game::Vars::MainRoutine == 0x03); n++) {
            Game::Hw::gPads[0] = 0;
            Game::SetNmiHook([](int) { Game::Hw::gPads[0] = 0; });
            Game::RunFrame();
        }
        int mx = 0, my = 0;
        bool where = Game::Explored::SamusScreen(mx, my);
        bool ok = Game::Vars::MainRoutine == 0x03 && where && mx == saved.mapX && my == saved.mapY &&
                  Game::Vars::ObjectX == saved.samusX && Game::Vars::ObjectY == saved.samusY &&
                  Game::Vars::HealthLo == saved.healthLo && Game::Vars::HealthHi == saved.healthHi &&
                  Game::Vars::SamusGear == saved.stats[0x6878 - 0x6876] &&
                  Game::Vars::MissileCount == saved.stats[0x6879 - 0x6876] && (Game::Vars::InArea & 0x0F) == (saved.inArea & 0x0F);
        printf("PROGRESS-TEST: %s (saved in area %02X at map %02X,%02X x %02X y %02X, %s, energy %X%X; back in play "
               "after %d frames at %02X,%02X x %02X y %02X, energy %X%X)\n",
               ok ? "OK" : "FAIL", saved.inArea, saved.mapX, saved.mapY, saved.samusX, saved.samusY,
               saved.horizontal ? "horizontal" : "vertical", saved.healthHi & 0x0F, saved.healthLo >> 4, n, mx, my,
               (uint8_t)Game::Vars::ObjectX, (uint8_t)Game::Vars::ObjectY, Game::Vars::HealthHi & 0x0F,
               Game::Vars::HealthLo >> 4);
        if (!ok) {
            printf("  area %02X/%02X gear %02X/%02X missiles %02X/%02X health %02X%02X/%02X%02X routine %02X\n",
                   saved.inArea, (uint8_t)Game::Vars::InArea, saved.stats[0x6878 - 0x6876], (uint8_t)Game::Vars::SamusGear,
                   saved.stats[0x6879 - 0x6876], (uint8_t)Game::Vars::MissileCount, saved.healthHi, saved.healthLo,
                   (uint8_t)Game::Vars::HealthHi, (uint8_t)Game::Vars::HealthLo, (uint8_t)Game::Vars::MainRoutine);
        }
        return ok ? 0 : 1;
    }
    if (solo && saveTest >= 0) {
        // Save at saveTest, play to `frames`, remember the state; play as far again; load and play the same
        // frames to `frames` again: the state must be the same.
        auto play = [&](int n0, int n1) {
            for (int n = n0; n < n1; n++) {
                for (const auto& [a, v] : pokes) {
                    Game::Mem(a) = v;
                }
                portFrameButtons = frame;
                frame += std::max(Game::RunFrame(), 1);
            }
        };
        play(0, saveTest);
        std::string save = Game::SaveState::Capture();
        int savedFrame = frame;
        uint8_t savedMode = Game::Vars::GameMode, savedRoutine = Game::Vars::MainRoutine;
        play(saveTest, frames);
        std::string first = Game::SaveState::Capture();
        play(frames, frames * 2 - saveTest);
        if (!Game::SaveState::Restore(save)) {
            printf("SAVE-TEST: FAIL (the save didn't load)\n");
            return 1;
        }
        frame = savedFrame;
        play(saveTest, frames);
        std::string second = Game::SaveState::Capture();
        size_t diff = 0;
        while (diff < first.size() && first[diff] == second[diff]) {
            diff++;
        }
        bool same = first == second;
        printf("SAVE-TEST: %s (saved at frame %d in GameMode %02X MainRoutine %02X, %zu-byte save, %d frames "
               "replayed)",
               same ? "OK" : "FAIL", saveTest, savedMode, savedRoutine,
               save.size(), frames - saveTest);
        if (!same) {
            printf(" first difference at byte %zu of the save", diff);
        }
        printf("\n");
        return same ? 0 : 1;
    }
    if (solo) {
        int playing = 0, samusHidden = 0, lagged = 0, pastEdge = 0;
        for (int n = 0; n < frames; n++) {
            for (const auto& [a, v] : pokes) {
                Game::Mem(a) = v;
            }
            portFrameButtons = frame;
            int nmis = Game::RunFrame();
            frame += std::max(nmis, 1);
            lagged += nmis > 1 && Game::Vars::MainRoutine == 0x03 ? 1 : 0;
            pastEdge += Game::Present::gExtraBuffer.empty() ? 0 : 1;
            // Playing (MainRoutine 3) or Samus appearing (8, once the fade lets her show: Timer3 below $1F),
            // Samus alive and not blinking after a hit ($77 bit 7).
            bool appearing = Game::Vars::MainRoutine == 0x08 && Game::Vars::Timer3 < 0x1F;
            if (Game::Vars::GameMode != 0 || (Game::Vars::MainRoutine != 0x03 && !appearing) ||
                (Game::Mem(0x77) & 0x80) ||
                Game::Vars::ObjAction == Game::Const::sa_Dead || Game::Vars::ObjAction == Game::Const::sa_Dead2) {
                continue;
            }
            playing++;
            bool drawn = false;
            for (int i = 0; i < 64; i++) {
                // SamusIntro's sprites aren't tagged (it is not in UpdateWorld), and they are all there is.
                drawn |= (appearing || Game::Present::gOamTags[i].owner == Game::Present::Owner::Samus) &&
                         Game::Hw::gVideo.oam[i * 4] < 0xEF;
            }
            samusHidden += drawn ? 0 : 1;
        }
        printf("SOLO: %d frames, %d playing, Samus not drawn in %d, %d frames longer than one while playing; ended GameMode %02X MainRoutine %02X area "
               "bank %d\n",
               frames, playing, samusHidden, lagged, (uint8_t)Game::Vars::GameMode, (uint8_t)Game::Vars::MainRoutine,
               Game::Hw::gPrgBank);
        if (Game::Options::gShotsPastEdge) {
            printf("PAST-EDGE: shots drawn past the screen edge in %d frames\n", pastEdge);
        }
        if (editorTiles) {
            static const char* const kAreas[] = { "", "brinstar", "norfair", "tourian", "kraid", "ridley" };
            int bank = Game::Hw::gPrgBank;
            std::map<std::string, std::string> files;
            std::string why;
            std::vector<uint8_t> chr;
            bool ok = bank >= 1 && bank <= 5 && Game::Data::Extract(rom, files, why) &&
                      Game::Data::PngToGraphics(files[std::string("metroid/editor/tiles_") + kAreas[bank] + ".png"],
                                                512, chr, why);
            int differ = 0;
            for (size_t k = 0; ok && k < chr.size(); k++) {
                differ += chr[k] != Game::Hw::gVideo.chr[k] ? 1 : 0;
            }
            printf("EDITOR-TILES: %s %s, %d of 8192 bytes differ %s\n", ok ? "" : "FAIL", kAreas[bank], differ,
                   why.c_str());
        }
        if (writeSave != nullptr) {
            std::string data = Game::SaveState::Capture();
            FILE* f = fopen(writeSave, "wb");
            if (f == nullptr || fwrite(data.data(), 1, data.size(), f) != data.size()) {
                printf("couldn't write %s\n", writeSave);
                return 1;
            }
            fclose(f);
        }
        return 0;
    }
    if (allActive) {
        Game::AllActive::gEnabled = true;
        Game::SetNmiHook([&](int nmiIndex) { Game::Hw::gPads[0] = buttonsAt(portFrameButtons + nmiIndex); });
        size_t maxRoster = 0;
        int shotFrames = 0, pipeFrames = 0;
        int sectionsWithRoster = 0;
        for (int n = 0; n < frames; n++) {
            for (const auto& [a, v] : pokes) {
                Game::Mem(a) = v;
            }
            portFrameButtons = frame;
            Game::AllActive::BeforeFrame();
            frame += std::max(Game::RunFrame(), 1);
            maxRoster = std::max(maxRoster, Game::AllActive::RosterSize());
            shotFrames += Game::AllActive::RosterShots() > 0 ? 1 : 0;
            bool pipe = false;
            for (const auto& e : Game::Present::gExtraBuffer) {
                pipe |= e.owner == Game::Present::Owner::Enemy;
            }
            pipeFrames += pipe ? 1 : 0;
            sectionsWithRoster += Game::AllActive::RosterSize() > 0 ? 1 : 0;
        }
        printf("ALL-ACTIVE: %d frames, %d rosters built, at most %zu enemies off screen, %d frames with some, %d with "
               "enemy shots off screen, %d with pipe enemies past the edge; ended GameMode %02X MainRoutine %02X area "
               "bank %d\n",
               frames, Game::AllActive::RostersBuilt(), maxRoster, sectionsWithRoster, shotFrames, pipeFrames,
               (uint8_t)Game::Vars::GameMode, (uint8_t)Game::Vars::MainRoutine, Game::Hw::gPrgBank);
        return 0;
    }
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
            ReportOriginalCalls();
            saveCoverage();
            return 1;
        }
        CheckSection();
        // The room meshes leave out tile $FF (what room RAM is filled with): it must be blank in play.
        if (Game::Vars::GameMode == 0 && Game::Vars::MainRoutine == 0x03) {
            for (int a = 0x1FF0; a < 0x2000; a++) {
                if (emu.Chr()[a] != 0 && !sBlankTileWarned) {
                    sBlankTileWarned = true;
                    printf("background tile $FF isn't blank (area bank %d)\n", Game::Hw::gPrgBank);
                }
            }
        }
        compared++;
    }
    printf("OK: %d frames matched (%d resyncs after lag frames), ended at frame %d (GameMode %02X MainRoutine "
           "%02X); %d room layouts checked, %d differ; %d scroll edges checked, %d differ; %zu sections, %zu missed\n",
           compared, resyncs, frame, emu.Ram()[0x1D], emu.Ram()[0x1E], sRoomsChecked, sRoomsDiffer, sScrollsChecked,
           sScrollsDiffer, sAllSections.size(), sMissedSections.size());
    Dump("original", emu, dump);
    ReportOriginalCalls();
    saveCoverage();
    return 0;
}
