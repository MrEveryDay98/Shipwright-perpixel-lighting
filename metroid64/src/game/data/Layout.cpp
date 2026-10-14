#include "game/data/Layout.h"

#include <cstring>

namespace Game::Data {

namespace {

// Room RAM as DrawStructRow and UpdateAttrib address it, for name table 0 ($6000-$63FF).
struct RoomRam {
    ScreenTiles& out;
    uint8_t& At(uint16_t addr) {
        return out.ram[(addr - 0x6000) & 0x3FF];
    }
};

// $EF9E UpdateAttrib: the attribute bits of the macro just drawn at `ptr` become `objectPal`.
void UpdateAttrib(RoomRam& ram, uint16_t ptr, uint8_t objectPal, uint8_t roomPal, const EngineRead& engine) {
    if (objectPal == roomPal) {
        return;
    }
    uint8_t lo = (uint8_t)ptr, hi = (uint8_t)(ptr >> 8);
    // $02 = the low byte shifted right twice with the high byte's two low bits shifted in.
    uint8_t m02 = (uint8_t)((lo >> 2) | ((hi & 0x03) << 6));
    uint8_t m03 = m02 & 0x07;
    m02 = (uint8_t)(((m02 >> 2) & 0x38) | m03 | 0xC0);
    m03 = (uint8_t)((hi & 0x04) | 0x63);
    uint16_t attrib = (uint16_t)((m03 << 8) | m02);
    int x = (lo & 0x40) ? 2 : 0;
    if (lo & 0x02) {
        x++;
    }
    uint8_t& a = ram.At(attrib);
    a &= engine((uint16_t)(0xEFF4 + x)); // AttribMaskTable
    uint8_t pal = objectPal;
    for (int i = 0; i < x; i++) {
        uint8_t shifted = (uint8_t)(pal << 2);
        bool carry = (pal << 2) & 0x100;
        pal = shifted;
        if (carry) {
            break;
        }
    }
    a |= pal;
}

// $EF8C DrawStruct, with the room RAM pointer at `work`. False once the room RAM is full.
void DrawStruct(RoomRam& ram, const Structure& st, const MacroSet& macros, uint16_t work, uint8_t objectPal,
                uint8_t roomPal, const EngineRead& engine) {
    for (const Structure::Row& row : st.rows) {
        uint16_t ptr = (uint16_t)(work + row.x * 2);
        bool full = false;
        for (uint8_t macro : row.macros) {
            uint8_t hi = (uint8_t)(ptr >> 8), lo = (uint8_t)ptr;
            if (hi == 0x63 || hi == 0x67) {
                if (lo >= 0xA0) {
                    full = true; // reached the attribute table
                    break;
                }
            } else if (hi > 0x67) {
                full = true;
                break;
            }
            // The macro's offset is macro * 4 in 8 bits ($11), so only the low 6 bits count.
            uint8_t index = macro & 0x3F;
            auto mit = macros.macros.size() > index ? &macros.macros[index] : nullptr;
            for (int x = 3; x >= 0; x--) {
                uint8_t tile = mit != nullptr ? (*mit)[3 - x] : 0xFF;
                ram.At((uint16_t)(ptr + engine((uint16_t)(0xEF9A + x)))) = tile; // TilePosTable
            }
            UpdateAttrib(ram, ptr, objectPal, roomPal, engine);
            ptr += 2;
            if ((ptr & 0x1F) == 0) {
                break; // end of the room RAM row: the rest of the structure row is skipped
            }
        }
        if (full) {
            return;
        }
        work += 0x40; // two tile rows down
    }
}

} // namespace

bool DrawScreen(const Level& level, int bank, uint8_t number, const EngineRead& engine, ScreenTiles& out) {
    auto rooms = level.rooms.find(bank);
    if (rooms == level.rooms.end()) {
        return false;
    }
    auto rit = rooms->second.find(number);
    if (rit == rooms->second.end()) {
        return false;
    }
    const Room& room = rit->second;
    static const MacroSet kNoMacros;
    auto mit = level.macros.find(bank);
    const MacroSet& macros = mit != level.macros.end() ? mit->second : kNoMacros;
    auto sit = level.structures.find(bank);

    // $EFF8 InitTables: blank tiles, and the room's palette in every attribute.
    memset(out.ram, 0xFF, 0x3C0);
    memset(out.ram + 0x3C0, engine((uint16_t)(0xF018 + room.palette)), 0x40); // ATDataTable
    RoomRam ram{ out };
    for (const RoomObject& o : room.objects) {
        if (o.pause) {
            continue; // the rest is drawn next frame
        }
        // $EA60 DrawObject: two tile rows per macro row, two tiles per macro column.
        uint16_t work = (uint16_t)(0x6000 + o.y * 0x40 + o.x * 2);
        if (sit == level.structures.end()) {
            continue;
        }
        auto st = sit->second.find(o.structure);
        if (st == sit->second.end()) {
            continue;
        }
        DrawStruct(ram, st->second, macros, work, o.palette, room.palette, engine);
    }
    return true;
}

uint8_t RoomDoors(const Level& level, int bank, uint8_t number) {
    auto rooms = level.rooms.find(bank);
    if (rooms == level.rooms.end()) {
        return 0;
    }
    auto rit = rooms->second.find(number);
    if (rit == rooms->second.end()) {
        return 0;
    }
    uint8_t sides = 0;
    for (const RoomItem& it : rit->second.items) {
        if (it.kind == RoomItem::Kind::Door) {
            sides |= (it.info & 0x10) ? 0x02 : 0x01;
        }
    }
    return sides;
}

Section FindSection(const Level& level, int bank, uint8_t x, uint8_t y, bool horizontal) {
    auto cell = [&](int cx, int cy) -> uint8_t {
        if (cx < 0 || cx > 31 || cy < 0 || cy > 31) {
            return 0xFF;
        }
        return level.world.rooms[cy][cx];
    };
    Section s;
    s.bank = bank;
    s.horizontal = horizontal;
    if (horizontal) {
        // GetRoomNum: scrolling stops at a door on the room's side, or at a screen with no room.
        int left = x, right = x;
        while (!(RoomDoors(level, bank, cell(left, y)) & 0x02) && cell(left - 1, y) != 0xFF) {
            left--;
        }
        while (!(RoomDoors(level, bank, cell(right, y)) & 0x01) && cell(right + 1, y) != 0xFF) {
            right++;
        }
        s.x = (uint8_t)left;
        s.y = y;
        s.length = (uint8_t)(right - left + 1);
    } else {
        int top = y, bottom = y;
        while (cell(x, top - 1) != 0xFF) {
            top--;
        }
        while (cell(x, bottom + 1) != 0xFF) {
            bottom++;
        }
        s.x = x;
        s.y = (uint8_t)top;
        s.length = (uint8_t)(bottom - top + 1);
    }
    return s;
}

} // namespace Game::Data
