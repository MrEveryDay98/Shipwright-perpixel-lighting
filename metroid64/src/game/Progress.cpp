#include "game/Progress.h"

#include "game/AllActive.h"
#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Explored.h"
#include "game/Memory.h"
#include "game/Ram.h"
#include "game/Title.h"

#include <cstring>

namespace Game::Progress {

using namespace Vars;

StartOverride gStart;

namespace {
constexpr uint16_t kStats = 0x6876;
constexpr uint16_t kBankTable = 0xCA30; // the area's bank, per InArea
constexpr char kMagic[4] = { 'M', '6', '4', 'P' };
constexpr uint8_t kVersion = 1;

struct Entry {
    bool pending = true; // she's on her way in (in a door, riding, starting): the place comes once she's out
    bool valid = false;
    uint8_t mapX = 0, mapY = 0, x = 0, y = 0;
    bool horizontal = true;
};
Entry sEntry;
} // namespace

void TrackEntry() {
    if (GameMode != 0 || MainRoutine == 0x01 || MainRoutine == 0x02) {
        sEntry = {}; // the title, or a new start
        return;
    }
    if (MainRoutine != 0x03) {
        return; // paused, the intro, dying ...
    }
    if (DoorStatus != 0 || ObjAction == Const::sa_Door || ObjAction == Const::sa_Elevator) {
        sEntry.pending = true;
        return;
    }
    int mx = 0, my = 0;
    if (sEntry.pending && Explored::SamusScreen(mx, my)) {
        sEntry = { false, true, (uint8_t)mx, (uint8_t)my, ObjectX, ObjectY, (ScrollDir & 0x02) != 0 };
    }
}

bool Capture(Data& out, bool atEntry) {
    int mx = 0, my = 0;
    if (GameMode != 0 || !Explored::SamusScreen(mx, my)) {
        return false;
    }
    for (int i = 0; i < (int)sizeof(out.stats); i++) {
        out.stats[i] = Mem((uint16_t)(kStats + i));
    }
    out.inArea = InArea;
    out.bailey = JustInBailey;
    out.healthLo = HealthLo;
    out.healthHi = HealthHi;
    out.mapX = (uint8_t)mx;
    out.mapY = (uint8_t)my;
    out.samusX = ObjectX;
    out.samusY = ObjectY;
    out.horizontal = (ScrollDir & 0x02) != 0;
    if (atEntry && sEntry.valid && !sEntry.pending) {
        out.mapX = sEntry.mapX;
        out.mapY = sEntry.mapY;
        out.samusX = sEntry.x;
        out.samusY = sEntry.y;
        out.horizontal = sEntry.horizontal;
    }
    Explored::Get(out.explored);
    return true;
}

void Start(const Data& d) {
    // $92D4 InitializeGame, with the progress put in directly instead of from the password (LoadPasswordData).
    ClearRAM_33_DF();
    ClearSamusStats();
    for (int i = 0; i < (int)sizeof(d.stats); i++) {
        Mem((uint16_t)(kStats + i)) = d.stats[i];
    }
    JustInBailey = d.bailey;
    InArea = d.inArea;
    SpritePagePos = 0;
    PageIndex = 0;
    ObjectCntrl = 0;
    ObjectHi = 0;
    SilenceMusic();
    AnimFrame = 0x5A;
    if (++SamusStat02 == 0) {
        SamusStat03++; // the times Samus started over, as the original counts them
    }
    MainRoutine = 0x01;
    ScreenNmiOff();
    LoadSamusGFX();
    NmiOn();
    SwitchPending = Read((uint16_t)(kBankTable + (InArea & 0x0F)));
    sEntry = {};
    gStart.active = true;
    gStart.data = d;
    Explored::Set(d.explored);
    AllActive::Reset();
}

std::string Encode(const Data& d) {
    std::string s(kMagic, sizeof(kMagic));
    s.push_back((char)kVersion);
    s.append((const char*)d.stats, sizeof(d.stats));
    for (uint8_t b : { d.inArea, d.bailey, d.healthLo, d.healthHi, d.mapX, d.mapY, d.samusX, d.samusY,
                       (uint8_t)(d.horizontal ? 1 : 0) }) {
        s.push_back((char)b);
    }
    s.append((const char*)d.explored, sizeof(d.explored));
    return s;
}

namespace {

// A snapshot save of an earlier Metroid64 ("M64S", SaveState.cpp's layout): the RAM, the cartridge RAM, picture
// memory, mapper, the room tags (then sprite tags and, from version 2, the map).
bool FromSnapshot(const std::string& bytes, Data& out) {
    if (bytes.size() < 5 || memcmp(bytes.data(), "M64S", 4) != 0) {
        return false;
    }
    const uint8_t version = (uint8_t)bytes[4];
    const size_t ram = 5, wram = ram + 0x800, rooms = wram + 0x2000 + 0x800 + 0x20 + 0x2000 + 0x100 + 12;
    const size_t tags = rooms + 12, explored = tags + 256;
    if (bytes.size() < tags) {
        return false;
    }
    const uint8_t* b = (const uint8_t*)bytes.data();
    auto mem = [&](uint16_t a) { return a < 0x800 ? b[ram + a] : b[wram + (a - 0x6000)]; };
    for (int i = 0; i < (int)sizeof(out.stats); i++) {
        out.stats[i] = mem((uint16_t)(kStats + i));
    }
    out.inArea = mem(InArea.addr);
    out.bailey = mem(JustInBailey.addr);
    out.healthLo = mem(HealthLo.addr);
    out.healthHi = mem(HealthHi.addr);
    out.samusX = mem(ObjectX.addr);
    out.samusY = mem(ObjectY.addr);
    out.horizontal = (mem(ScrollDir.addr) & 0x02) != 0;
    const uint8_t* tag = b + rooms + 6 * (mem(ObjectHi.addr) & 0x01); // the room on Samus's name table
    out.mapX = tag[0] ? (tag[2] & 0x1F) : (mem(MapPosX.addr) & 0x1F);
    out.mapY = tag[0] ? (tag[3] & 0x1F) : (mem(MapPosY.addr) & 0x1F);
    if (version >= 2 && bytes.size() >= explored + sizeof(out.explored)) {
        memcpy(out.explored, b + explored, sizeof(out.explored));
    }
    return true;
}

} // namespace

bool Decode(const std::string& bytes, Data& out) {
    if (FromSnapshot(bytes, out)) {
        return true;
    }
    const size_t size = sizeof(kMagic) + 1 + sizeof(out.stats) + 9 + sizeof(out.explored);
    if (bytes.size() < size || memcmp(bytes.data(), kMagic, sizeof(kMagic)) != 0 || (uint8_t)bytes[4] != kVersion) {
        return false;
    }
    const uint8_t* p = (const uint8_t*)bytes.data() + 5;
    memcpy(out.stats, p, sizeof(out.stats));
    p += sizeof(out.stats);
    out.inArea = p[0];
    out.bailey = p[1];
    out.healthLo = p[2];
    out.healthHi = p[3];
    out.mapX = p[4] & 0x1F;
    out.mapY = p[5] & 0x1F;
    out.samusX = p[6];
    out.samusY = p[7];
    out.horizontal = p[8] != 0;
    memcpy(out.explored, p + 9, sizeof(out.explored));
    return true;
}

} // namespace Game::Progress
