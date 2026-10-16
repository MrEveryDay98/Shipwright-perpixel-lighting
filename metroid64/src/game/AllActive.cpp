#include "game/AllActive.h"

#include "game/Constants.h"
#include "game/EnemyRam.h"
#include "game/Enemies.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Present.h"
#include "game/Ram.h"
#include "game/Area.h"
#include "game/Items.h"
#include "game/Projectiles.h"
#include "game/Room.h"
#include "game/data/Data.h"
#include "game/data/Layout.h"
#include "game/data/RoomMesh.h"

#include <algorithm>
#include <cstring>

namespace Game::AllActive {

using namespace Vars;

bool gEnabled = false;

namespace {

constexpr uint8_t kEnemySlots[] = { 0x00, 0x10, 0x20, 0x30, 0x40, 0x50 };
// Enemy projectiles ($F93B UpdateObjectsF93B): the same two blocks per slot as the enemies.
constexpr uint8_t kShotSlots[] = { 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0 };
// Enemies only use the game's slots while well inside the screen: the game draws only what is on its
// 256x240 screen (and drops sprites past its left edge), and the 3D view shows beyond it, so near an edge
// with more of the section past it the roster runs and draws them whole. An enemy leaves the slots when
// its centre is less than kOutMargin inside such an edge and comes back at kInMargin. Where the screen
// can't scroll further (the section's end), its edge is the room's and the game keeps them as usual.
constexpr int kOutMargin = 20;
constexpr int kInMargin = 32;

// An enemy's state: its two 16-byte blocks ($0400+slot and $6AF4+slot).
struct EnemyState {
    uint8_t a[16];
    uint8_t b[16];
};
constexpr int kNameTable = 0x6AFB - 0x6AF4; // EnNameTable in block b
constexpr int kStatus = 0;                  // Enstatus in block b

struct RosterEnemy {
    EnemyState state;
    int screen = 0;    // in the section
    bool shot = false; // an enemy projectile (runs in slot $60 through DoOneEnemyObject)
    std::vector<Sprite> sprites;
};

bool sActive = false;
bool sBuilding = false; // loading the section's rooms for the roster
Data::Section sSection;
std::vector<Data::ScreenTiles> sScreens; // the section's room RAM: its rooms, then what the game drew
std::vector<RosterEnemy> sRoster;
std::vector<Sprite> sSprites;
std::vector<std::vector<Sprite>> sObjects; // per screen of the section
int sRostersBuilt = 0;

// The game as it is, to put back after running something in a stand-in.
struct Snapshot {
    uint8_t ram[0x800];
    uint8_t wram[0x2000];
    Present::RoomTag rooms[2];
    Present::Tag tags[64];
    void Take() {
        memcpy(ram, gRam, sizeof(ram));
        memcpy(wram, gWram, sizeof(wram));
        memcpy(rooms, Present::gRooms, sizeof(rooms));
        memcpy(tags, Present::gBufferTags, sizeof(tags));
    }
    void Restore() const {
        memcpy(gRam, ram, sizeof(ram));
        memcpy(gWram, wram, sizeof(wram));
        memcpy(Present::gRooms, rooms, sizeof(rooms));
        memcpy(Present::gBufferTags, tags, sizeof(tags));
    }
};
Snapshot sSnapshot;

void ReadSlot(uint8_t slot, EnemyState& e) {
    for (int k = 0; k < 16; k++) {
        e.a[k] = Mem((uint16_t)(0x0400 + slot + k));
        e.b[k] = Mem((uint16_t)(0x6AF4 + slot + k));
    }
}

void WriteSlot(uint8_t slot, const EnemyState& e) {
    for (int k = 0; k < 16; k++) {
        Mem((uint16_t)(0x0400 + slot + k)) = e.a[k];
        Mem((uint16_t)(0x6AF4 + slot + k)) = e.b[k];
    }
}

int ScreenWidth() {
    return sSection.horizontal ? 256 : 0;
}
int ScreenHeight() {
    return sSection.horizontal ? 0 : 240;
}

// The section screen the room on name table page 0/1 is, or -1.
int ScreenOfPage(int page, bool needComplete) {
    const Present::RoomTag& t = Present::gRooms[page & 1];
    if (!t.valid || t.bank != sSection.bank || (needComplete && !t.complete)) {
        return -1;
    }
    for (int i = 0; i < sSection.length; i++) {
        if (sSection.ScreenX(i) == t.mapX && sSection.ScreenY(i) == t.mapY) {
            return i;
        }
    }
    return -1;
}

// Section pixel position of something at (x, y) on name table page `page`; false if that page isn't in it.
bool ToSection(int page, uint8_t x, uint8_t y, int& sx, int& sy) {
    int i = ScreenOfPage(page, false);
    if (i < 0) {
        return false;
    }
    sx = i * ScreenWidth() + x;
    sy = i * ScreenHeight() + y;
    return true;
}

// The top-left of the screen, in the section.
bool ViewOrigin(int& vx, int& vy) {
    int page = PPUCNT0ZP & 0x01;
    int i = ScreenOfPage(page, false);
    if (i < 0) {
        return false;
    }
    vx = i * ScreenWidth() + (sSection.horizontal ? (int)ScrollX : 0);
    vy = i * ScreenHeight() + (sSection.horizontal ? 0 : (int)ScrollY);
    return true;
}

// Tourian (bank 3): each Metroid has a byte outside its slot ($77F8 + slot / 16, Tourian.cpp MetroidState):
// non-zero while it is latched on Samus. A latched one is on Samus, so it stays the game's; off screen the
// bytes are 0. Rinkas (enemy type 4) are sent by their spawners into whatever slot isn't on screen, so they
// stay the game's like the pipes' enemies.
constexpr uint16_t kMetroidState = 0x77F8;
constexpr int kEnDataIndex = 0x6B02 - 0x6AF4; // in block b
constexpr uint8_t kRinkaType = 0x04;

bool TourianKeeps(uint8_t slot, const EnemyState& e) {
    if (Hw::gPrgBank != 3) {
        return false;
    }
    return Mem((uint16_t)(kMetroidState + (slot >> 4))) != 0 || e.b[kEnDataIndex] == kRinkaType;
}

bool IsRosterType(const EnemyState& e) {
    uint8_t status = e.b[kStatus];
    // Resting, active, frozen and hit; not exploding or a dropped item, not Kraid or Ridley.
    bool movable = status == 0x01 || status == 0x02 || status == 0x04 || status == 0x06;
    return movable && !(e.a[0x0F] & 0x40);
}

// A slot a Zeb hole (pipe) spawns into ($0728+y: type, $FF none; $0729+y: slot). The pipe removes its enemy
// itself once it is off screen and sends another (DoOneSpawn), so those stay the game's.
bool IsSpawnerSlot(uint8_t slot) {
    for (int y = 0; y <= 0x18; y += 0x08) {
        if (Mem((uint16_t)(0x0728 + y)) != 0xFF && Mem((uint16_t)(0x0729 + y)) == slot) {
            return true;
        }
    }
    return false;
}

// Keeps the section's room RAM up to date with what the game has drawn.
void UpdateScreens() {
    for (int page = 0; page < 2; page++) {
        int i = ScreenOfPage(page, true);
        if (i >= 0) {
            memcpy(sScreens[i].ram, &gWram[page * 0x400], 0x400);
        }
    }
}

// Draws the doors, power-ups, elevator and statues the game has on name table `table`, with the screen
// looking at it, and keeps their sprites as screen `screen`'s. The caller restores the game afterwards.
void CaptureObjects(int screen, uint8_t table) {
    PPUCNT0ZP = (uint8_t)((PPUCNT0ZP & ~0x03) | table);
    ScrollX = 0;
    ScrollY = 0;
    FrameCount = (uint8_t)(FrameCount | 0x01); // the elevator only draws on odd frames: draw it steadily
    std::vector<Sprite> out;
    auto take = [&](uint8_t from, bool door) {
        for (int k = from; k < SpritePagePos && k < 0x100; k += 4) {
            uint8_t y = Mem((uint16_t)(0x0200 + k));
            if (y >= 0xEF) {
                continue;
            }
            Sprite sp;
            sp.x = screen * ScreenWidth() + Mem((uint16_t)(0x0203 + k));
            sp.y = screen * ScreenHeight() + y + 1;
            sp.tile = Mem((uint16_t)(0x0201 + k));
            sp.attr = Mem((uint16_t)(0x0202 + k));
            sp.door = door;
            out.push_back(sp);
        }
    };
    SpritePagePos = 0;
    DrawDoors();
    take(0, true);
    uint8_t from = SpritePagePos;
    UpdateItems();
    UpdateElevator();
    UpdateStatues();
    take(from, false);
    sObjects[screen] = std::move(out);
}

// Every enemy the section's rooms place: each room is loaded the game's own way (SetupRoom, which places
// its enemies and the special items' ones) in a stand-in, and its enemies taken.
void BuildRoster() {
    sRostersBuilt++;
    sRoster.clear();
    sObjects.assign(sSection.length, {});
    sBuilding = true;
    sSnapshot.Take();
    for (int i = 0; i < sSection.length; i++) {
        uint8_t mapX = sSection.ScreenX(i), mapY = sSection.ScreenY(i);
        sSnapshot.Restore();
        for (uint8_t s : kEnemySlots) {
            Enstatus[s] = 0;
        }
        MapPosX = mapX;
        MapPosY = mapY;
        RoomNumber = Read((uint16_t)(0x7000 + (mapY << 5) + mapX));
        if (RoomNumber == 0xFF) {
            continue;
        }
        uint8_t table = (PPUCNT0ZP ^ ScrollDir) & 0x01; // GetNameTable: the table the room is drawn on
        for (int n = 0; n < 64 && RoomNumber < 0xF0; n++) {
            SetupRoom();
        }
        // Samus away from this screen while its objects are drawn.
        ObjectHi = (uint8_t)(table ^ 0x01);
        Snapshot loaded;
        loaded.Take();
        CaptureObjects(i, table);
        loaded.Restore();
        for (uint8_t s : kEnemySlots) {
            EnemyState e;
            ReadSlot(s, e);
            if (e.b[kStatus] == 0 || (e.b[kNameTable] & 0x01) != table || !IsRosterType(e)) {
                continue;
            }
            RosterEnemy r;
            r.state = e;
            r.screen = i;
            sRoster.push_back(r);
        }
    }
    sSnapshot.Restore();
    sBuilding = false;
    // The roster has them now: the game's slots take them from there.
    for (uint8_t s : kEnemySlots) {
        EnemyState e;
        ReadSlot(s, e);
        if (e.b[kStatus] != 0 && IsRosterType(e) && !TourianKeeps(s, e)) {
            Enstatus[s] = 0;
        }
    }
}

// Runs a roster enemy (or enemy projectile) one frame in a stand-in for the game, centred on it. Projectiles
// an enemy fires there are added to `fired`.
void RunOffscreen(RosterEnemy& r, std::vector<RosterEnemy>& fired) {
    const bool h = sSection.horizontal;
    const int span = h ? 256 : 240;
    const int length = sSection.length * span;
    int ex = r.screen * ScreenWidth() + r.state.a[1];
    int ey = r.screen * ScreenHeight() + r.state.a[0];
    int pos = h ? ex : ey;
    // The screen looks at the enemy; never exactly on a screen edge, so it can cross into the next one
    // (the enemy movers only cross name tables while the screen is scrolled).
    int view = std::clamp(pos - span / 2, 0, std::max(0, length - span));
    if (view % span == 0 && view + span < length) {
        view++;
    }
    int first = view / span;

    sSnapshot.Take();
    // Room RAM: the two screens the view covers.
    for (int page = 0; page < 2; page++) {
        int i = first + page;
        if (i < sSection.length) {
            memcpy(&gWram[page * 0x400], sScreens[i].ram, 0x400);
        } else {
            memset(&gWram[page * 0x400], 0xFF, 0x400);
        }
    }
    PPUCNT0ZP = (uint8_t)(PPUCNT0ZP & ~0x03);
    if (h) {
        ScrollX = (uint8_t)(view % span);
        ScrollY = 0;
        ScrollDir = 0x03;
    } else {
        ScrollY = (uint8_t)(view % span);
        ScrollX = 0;
        ScrollDir = 0x01;
    }
    // Samus where she is, if she is on these two screens; else at their far side.
    int samusX = 0, samusY = 0;
    bool samusKnown = ToSection(ObjectHi & 0x01, ObjectX, ObjectY, samusX, samusY);
    int sp = h ? samusX : samusY;
    if (!samusKnown || sp < first * span || sp >= (first + 2) * span) {
        bool after = samusKnown && sp >= (first + 2) * span;
        ObjectHi = after ? 1 : 0;
        if (h) {
            ObjectX = after ? 0xF8 : 0x08;
        } else {
            ObjectY = after ? 0xE0 : 0x10;
        }
    } else {
        int page = sp / span - first;
        ObjectHi = (uint8_t)page;
        if (h) {
            ObjectX = (uint8_t)(samusX - (first + page) * span);
        } else {
            ObjectY = (uint8_t)(samusY - (first + page) * span);
        }
    }
    // Nothing else around: no other enemies, enemy shots or Samus's shots.
    for (int s = 0x00; s <= 0xB0; s += 0x10) {
        Enstatus[s] = 0;
    }
    for (int s = 0xD0; s <= 0xF0; s += 0x10) {
        ObjAction[s] = 0;
    }
    if (Hw::gPrgBank == 3) {
        for (int m = 0; m < 6; m++) {
            Mem((uint16_t)(kMetroidState + m)) = 0; // no Metroid latched on Samus here
        }
    }
    EnemyState e = r.state;
    int page = r.screen - first;
    e.b[kNameTable] = (uint8_t)(page & 0x01);
    const uint8_t slot = r.shot ? kShotSlots[0] : kEnemySlots[0];
    WriteSlot(slot, e);
    SpritePagePos = 0;

    if (r.shot) {
        DoOneEnemyObject(slot);
    } else {
        DoOneEnemy(slot);
        // What it fired: the original's projectiles are only drawn and kept on its screen, so the roster
        // runs them too.
        for (uint8_t s : kShotSlots) {
            EnemyState shot;
            ReadSlot(s, shot);
            if (shot.b[kStatus] != 0) {
                RosterEnemy f;
                f.state = shot;
                f.screen = first + (shot.b[kNameTable] & 0x01);
                f.shot = true;
                fired.push_back(f);
            }
        }
    }

    ReadSlot(slot, e);
    uint8_t spritesEnd = SpritePagePos;
    r.sprites.clear();
    if (e.b[kStatus] != 0) {
        r.state = e;
        r.screen = first + (e.b[kNameTable] & 0x01);
        for (int k = 0; k < spritesEnd && k < 0x100; k += 4) {
            uint8_t y = Mem((uint16_t)(0x0200 + k)), tile = Mem((uint16_t)(0x0201 + k));
            uint8_t attr = Mem((uint16_t)(0x0202 + k)), x = Mem((uint16_t)(0x0203 + k));
            if (y >= 0xEF) {
                continue;
            }
            Sprite sp2;
            sp2.x = (h ? view : 0) + x;
            sp2.y = (h ? 0 : view) + y + 1;
            sp2.tile = tile;
            sp2.attr = attr;
            r.sprites.push_back(sp2);
        }
    } else {
        r.state.b[kStatus] = 0; // gone
    }
    sSnapshot.Restore();
}

} // namespace

bool KeepPastEdge(uint8_t slot, int& rx, int& ry) {
    if (!gEnabled || !sActive || slot > 0x50 || !IsSpawnerSlot(slot) || Enstatus[slot] == 0) {
        return false;
    }
    return PastEdge(EnNameTable[slot], EnXRoomPos[slot], EnYRoomPos[slot], rx, ry);
}

bool SuppressRoomEnemies() {
    return sActive && !sBuilding;
}

const std::vector<Sprite>& OffscreenSprites() {
    return sSprites;
}

std::vector<Sprite> ObjectSprites() {
    std::vector<Sprite> all;
    if (sActive) {
        for (const auto& v : sObjects) {
            all.insert(all.end(), v.begin(), v.end());
        }
    }
    return all;
}

bool Active() {
    return sActive;
}

size_t RosterSize() {
    return sRoster.size();
}

size_t RosterShots() {
    return (size_t)std::count_if(sRoster.begin(), sRoster.end(), [](const RosterEnemy& r) { return r.shot; });
}

int RostersBuilt() {
    return sRostersBuilt;
}

void Reset() {
    sActive = false;
    sRoster.clear();
    sSprites.clear();
    sObjects.clear();
}

void BeforeFrame() {
    const Data::Level* level = Data::InstalledLevel();
    int bank = Hw::gPrgBank;
    bool playing = GameMode == 0 && MainRoutine == 0x03;
    bool usable = gEnabled && level != nullptr && bank >= 1 && bank <= 5;
    if (!usable) {
        sActive = false;
        sRoster.clear();
        sSprites.clear();
        return;
    }
    if (!playing) {
        return; // paused, the intro, dying: everything waits
    }
    if (DoorStatus != 0 || ObjAction == Const::sa_Elevator) {
        // Going through a door: the game clears the enemies; the next section gets its own roster. Riding an
        // elevator: the area bank changes on the way, before the area's enemy tables ($41-$46) do, so the
        // roster's enemies can't run (their frames would be read from the wrong tables).
        sActive = false;
        sRoster.clear();
        sSprites.clear();
        return;
    }
    uint8_t mx = MapPosX & 0x1F, my = MapPosY & 0x1F;
    if (level->world.rooms[my][mx] == 0xFF) {
        return;
    }
    Data::Section s = Data::FindSection(*level, bank, mx, my, (ScrollDir & 0x02) != 0);
    if (!sActive || !(s == sSection)) {
        sSection = s;
        sScreens.clear();
        auto engine = [](uint16_t a) { return Read(a); };
        if (!Data::SectionScreens(*level, s, engine, sScreens)) {
            sActive = false;
            return;
        }
        sActive = true;
        BuildRoster();
    }
    UpdateScreens();
    // The loaded screens' objects as they are now.
    for (int page = 0; page < 2; page++) {
        int i = ScreenOfPage(page, true);
        if (i >= 0) {
            sSnapshot.Take();
            CaptureObjects(i, (uint8_t)page);
            sSnapshot.Restore();
        }
    }

    int vx = 0, vy = 0;
    if (!ViewOrigin(vx, vy)) {
        return;
    }
    const int length = sSection.length * (sSection.horizontal ? 256 : 240);
    const bool h = sSection.horizontal;
    const bool moreLeft = h && vx > 0, moreRight = h && vx + 256 < length;
    const bool moreUp = !h && vy > 0, moreDown = !h && vy + 240 < length;
    auto inside = [&](int x, int y, int margin) {
        constexpr int kOpen = -4096; // no limit at the section's own ends
        return x >= vx + (moreLeft ? margin : kOpen) && x < vx + 256 - (moreRight ? margin : kOpen) &&
               y >= vy + (moreUp ? margin : kOpen) && y < vy + 240 - (moreDown ? margin : kOpen);
    };
    // Enemies and their projectiles near or past an edge go to the roster.
    auto handOut = [&](uint8_t slot, bool shot) {
        EnemyState e;
        ReadSlot(slot, e);
        if (e.b[kStatus] == 0 || (!shot && (!IsRosterType(e) || IsSpawnerSlot(slot) || TourianKeeps(slot, e)))) {
            return;
        }
        int ex = 0, ey = 0;
        if (!ToSection(e.b[kNameTable] & 0x01, e.a[1], e.a[0], ex, ey) || inside(ex, ey, kOutMargin)) {
            return;
        }
        RosterEnemy r;
        r.state = e;
        r.screen = ScreenOfPage(e.b[kNameTable] & 0x01, false);
        r.shot = shot;
        sRoster.push_back(r);
        Enstatus[slot] = 0;
    };
    for (uint8_t slot : kEnemySlots) {
        handOut(slot, false);
    }
    for (uint8_t slot : kShotSlots) {
        handOut(slot, true);
    }
    // Roster enemies well inside the screen, on a screen the game has loaded, go into free slots.
    for (size_t i = 0; i < sRoster.size();) {
        RosterEnemy& r = sRoster[i];
        int ex = r.screen * ScreenWidth() + r.state.a[1], ey = r.screen * ScreenHeight() + r.state.a[0];
        int page = -1;
        for (int p = 0; p < 2; p++) {
            if (ScreenOfPage(p, true) == r.screen) {
                page = p;
            }
        }
        uint8_t freeSlot = 0xFF;
        if (r.shot) {
            for (uint8_t slot : kShotSlots) {
                if (Enstatus[slot] == 0) {
                    freeSlot = slot;
                    break;
                }
            }
        } else {
            for (uint8_t slot : kEnemySlots) {
                if (Enstatus[slot] == 0 && !IsSpawnerSlot(slot)) {
                    freeSlot = slot;
                    break;
                }
            }
        }
        if (page >= 0 && freeSlot != 0xFF && inside(ex, ey, kInMargin)) {
            EnemyState e = r.state;
            e.b[kNameTable] = (uint8_t)page;
            WriteSlot(freeSlot, e);
            if (Hw::gPrgBank == 3 && !r.shot) {
                Mem((uint16_t)(kMetroidState + (freeSlot >> 4))) = 0; // not latched
            }
            sRoster.erase(sRoster.begin() + (long)i);
            continue;
        }
        i++;
    }
    // The rest run where they are. Projectiles fired meanwhile run from the next frame on (the game's own
    // are drawn the frame they are fired, from their place by the enemy: so are these).
    sSprites.clear();
    std::vector<RosterEnemy> fired;
    for (size_t i = 0; i < sRoster.size();) {
        RunOffscreen(sRoster[i], fired);
        if (sRoster[i].state.b[kStatus] == 0) {
            sRoster.erase(sRoster.begin() + (long)i);
            continue;
        }
        sSprites.insert(sSprites.end(), sRoster[i].sprites.begin(), sRoster[i].sprites.end());
        i++;
    }
    sRoster.insert(sRoster.end(), fired.begin(), fired.end());
}

} // namespace Game::AllActive
