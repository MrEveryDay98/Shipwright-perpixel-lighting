#include "game/SaveState.h"

#include "game/AllActive.h"
#include "game/Explored.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Present.h"
#include "game/Ram.h"

#include <cstring>

namespace Game::SaveState {

namespace {

constexpr char kMagic[4] = { 'M', '6', '4', 'S' };
// 1: the game; 2: and the map's visited screens and their scrolling (Explored.h).
constexpr uint8_t kVersion = 2;

void Put(std::string& out, const void* p, size_t n) {
    out.append((const char*)p, n);
}
void Put8(std::string& out, uint8_t v) {
    out.push_back((char)v);
}

struct Reader {
    const std::string& d;
    size_t p = 0;
    bool ok = true;
    void Get(void* dst, size_t n) {
        if (p + n > d.size()) {
            ok = false;
            return;
        }
        memcpy(dst, d.data() + p, n);
        p += n;
    }
    uint8_t U8() {
        uint8_t v = 0;
        Get(&v, 1);
        return v;
    }
};

void PutTags(std::string& out, const Present::Tag* tags) {
    for (int i = 0; i < 64; i++) {
        Put8(out, (uint8_t)tags[i].owner);
        Put8(out, tags[i].slot);
    }
}
void GetTags(Reader& r, Present::Tag* tags) {
    for (int i = 0; i < 64; i++) {
        tags[i].owner = (Present::Owner)r.U8();
        tags[i].slot = r.U8();
    }
}

} // namespace

bool CanSave() {
    using namespace Vars;
    return GameMode == 0 && MainRoutine == 0x05 && (uint8_t)(EndTimerHi + 1) == 0;
}

std::string Capture() {
    std::string out;
    Put(out, kMagic, sizeof(kMagic));
    Put8(out, kVersion);
    Put(out, gRam, sizeof(gRam));
    Put(out, gWram, sizeof(gWram));
    const Hw::Video& v = Hw::gVideo;
    Put(out, v.nametables, sizeof(v.nametables));
    Put(out, v.palette, sizeof(v.palette));
    Put(out, v.chr, sizeof(v.chr));
    Put(out, v.oam, sizeof(v.oam));
    for (uint8_t b : { v.ctrl, v.mask, v.scrollX, v.scrollY, v.mirroring, (uint8_t)v.addr, (uint8_t)(v.addr >> 8),
                       (uint8_t)v.latch, v.readBuffer, v.oamAddr, Hw::gPrgBank, Hw::gMmcControl }) {
        Put8(out, b);
    }
    for (const Present::RoomTag& t : Present::gRooms) {
        for (uint8_t b : { (uint8_t)t.valid, t.bank, t.mapX, t.mapY, t.room, (uint8_t)t.complete }) {
            Put8(out, b);
        }
    }
    PutTags(out, Present::gBufferTags);
    PutTags(out, Present::gOamTags);
    uint8_t visited[Explored::kBytes];
    Explored::Get(visited);
    Put(out, visited, sizeof(visited));
    return out;
}

bool Restore(const std::string& data) {
    Reader r{ data };
    char magic[4] = {};
    r.Get(magic, sizeof(magic));
    uint8_t version = r.U8();
    if (!r.ok || memcmp(magic, kMagic, sizeof(kMagic)) != 0 || version < 1 || version > kVersion) {
        return false;
    }
    // Read into copies first, so a short file changes nothing.
    static uint8_t ram[sizeof(gRam)], wram[sizeof(gWram)];
    Hw::Video v;
    r.Get(ram, sizeof(ram));
    r.Get(wram, sizeof(wram));
    r.Get(v.nametables, sizeof(v.nametables));
    r.Get(v.palette, sizeof(v.palette));
    r.Get(v.chr, sizeof(v.chr));
    r.Get(v.oam, sizeof(v.oam));
    v.ctrl = r.U8();
    v.mask = r.U8();
    v.scrollX = r.U8();
    v.scrollY = r.U8();
    v.mirroring = r.U8();
    v.addr = r.U8();
    v.addr |= (uint16_t)(r.U8() << 8);
    v.latch = r.U8() != 0;
    v.readBuffer = r.U8();
    v.oamAddr = r.U8();
    uint8_t prgBank = r.U8(), mmcControl = r.U8();
    Present::RoomTag rooms[2];
    for (Present::RoomTag& t : rooms) {
        t.valid = r.U8() != 0;
        t.bank = r.U8();
        t.mapX = r.U8();
        t.mapY = r.U8();
        t.room = r.U8();
        t.complete = r.U8() != 0;
    }
    Present::Tag bufferTags[64], oamTags[64];
    GetTags(r, bufferTags);
    GetTags(r, oamTags);
    uint8_t visited[Explored::kBytes] = {};
    if (version >= 2) {
        r.Get(visited, sizeof(visited));
    }
    if (!r.ok || r.p != data.size()) {
        return false;
    }
    memcpy(gRam, ram, sizeof(ram));
    memcpy(gWram, wram, sizeof(wram));
    Hw::gVideo = v;
    Hw::gPrgBank = prgBank;
    Hw::gMmcControl = mmcControl;
    memcpy(Present::gRooms, rooms, sizeof(rooms));
    memcpy(Present::gBufferTags, bufferTags, sizeof(bufferTags));
    memcpy(Present::gOamTags, oamTags, sizeof(oamTags));
    Explored::Set(visited);
    AllActive::Reset();
    return true;
}

} // namespace Game::SaveState
