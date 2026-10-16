#include "MapView.h"

#include "Menu.h"
#include "SavePoints.h"

#include "game/Explored.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Ram.h"
#include "game/data/Data.h"
#include "game/data/Layout.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <libultraship/bridge/consolevariablebridge.h>

namespace Metroid {

namespace {

// Super Metroid's map colours.
const ImU32 kRoomFill = IM_COL32(200, 56, 152, 255);
const ImU32 kUnvisitedFill = IM_COL32(40, 64, 150, 255);
const ImU32 kWall = IM_COL32(255, 255, 255, 255);
const ImU32 kBlueDoor = IM_COL32(80, 150, 255, 255);
const ImU32 kRedDoor = IM_COL32(255, 70, 60, 255);
const ImU32 kItemDot = IM_COL32(255, 255, 255, 255);
const ImU32 kSaveFill = IM_COL32(40, 150, 230, 255);
const ImU32 kGrid = IM_COL32(40, 60, 120, 120);
const ImU32 kBackground = IM_COL32(8, 10, 40, 230);

const char* const kAreaNames[] = { "", "BRINSTAR", "NORFAIR", "TOURIAN", "KRAID'S LAIR", "RIDLEY'S LAIR" };

int CurrentBank() {
    return Game::Hw::gPrgBank;
}

} // namespace

bool MapView::Ready() {
    const Game::Data::Level* level = Game::Data::InstalledLevel();
    if (level == nullptr) {
        return false;
    }
    if (level != mBuiltFor) {
        Build(*level);
    }
    int bank = CurrentBank();
    return bank >= 1 && bank <= 5;
}

void MapView::Build(const Game::Data::Level& level) {
    using namespace Game::Data;
    mBuiltFor = &level;
    for (auto& row : mCells) {
        for (Cell& c : row) {
            c = Cell{};
        }
    }
    // The area of each screen: the first section through it (AllSections follows the doors from each
    // area's start, so a screen is in its own area's sections; elevator shafts can be in two).
    auto engine = [](uint16_t a) { return Game::Read(a); };
    for (const Section& s : AllSections(level, engine)) {
        for (int i = 0; i < s.length; i++) {
            int x = s.ScreenX(i), y = s.ScreenY(i);
            if (x <= 31 && y <= 31 && mCells[y][x].bank == 0) {
                mCells[y][x].bank = (uint8_t)s.bank;
                mCells[y][x].room = level.world.rooms[y][x];
            }
        }
    }
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            Cell& c = mCells[y][x];
            if (c.bank == 0) {
                continue;
            }
            Section h = FindSection(level, c.bank, (uint8_t)x, (uint8_t)y, true);
            Section v = FindSection(level, c.bank, (uint8_t)x, (uint8_t)y, false);
            c.hOpen[0] = x > h.x;
            c.hOpen[1] = x < h.x + h.length - 1;
            c.vOpen[0] = y > v.y;
            c.vOpen[1] = y < v.y + v.length - 1;
            auto rooms = level.rooms.find(c.bank);
            if (rooms != level.rooms.end()) {
                auto room = rooms->second.find(c.room);
                if (room != rooms->second.end()) {
                    for (const RoomItem& it : room->second.items) {
                        if (it.kind != RoomItem::Kind::Door) {
                            continue;
                        }
                        int side = (it.info & 0x10) ? 0 : 1;
                        c.door[side] = true;
                        uint8_t pal = it.info & 0x03;
                        c.redDoor[side] = pal != 0x01 && pal != 0x03; // LoadDoorData: these need missiles
                    }
                }
            }
            auto items = level.specialItems.find(c.bank);
            if (items != level.specialItems.end()) {
                auto screen = items->second.screens.find({ (uint8_t)y, (uint8_t)x });
                if (screen != items->second.screens.end()) {
                    for (const SpecialItem& it : screen->second) {
                        if (it.kind == SpecialItem::Kind::Elevator) {
                            c.elevator = true;
                        } else if (it.kind == SpecialItem::Kind::PowerUp) {
                            c.item = (int8_t)it.type;
                        }
                    }
                }
            }
        }
    }
}

void MapView::DrawCell(ImDrawList* dl, int x, int y, ImVec2 p, float size, bool showUnvisited) {
    const Cell& c = mCells[y][x];
    bool visited = Game::Explored::Visited(x, y);
    if (c.bank != CurrentBank() || (!visited && !showUnvisited)) {
        return;
    }
    ImVec2 q(p.x + size, p.y + size);
    if (!visited) {
        // Not been there: how it scrolls isn't known, so just the screen.
        dl->AddRectFilled(ImVec2(p.x + 1, p.y + 1), ImVec2(q.x - 1, q.y - 1), kUnvisitedFill);
        return;
    }
    dl->AddRectFilled(p, q, kRoomFill);
    bool h = Game::Explored::Horizontal(x, y);
    bool open[4] = { h && c.hOpen[0], h && c.hOpen[1], !h && c.vOpen[0], !h && c.vOpen[1] };
    float w = std::max(1.0f, size * 0.09f);
    // Walls where the screen doesn't lead on, with the door in the middle of its side.
    if (!open[2]) {
        dl->AddRectFilled(p, ImVec2(q.x, p.y + w), kWall);
    }
    if (!open[3]) {
        dl->AddRectFilled(ImVec2(p.x, q.y - w), q, kWall);
    }
    for (int side = 0; side < 2; side++) {
        if (open[side]) {
            continue;
        }
        float x0 = side == 0 ? p.x : q.x - w;
        dl->AddRectFilled(ImVec2(x0, p.y), ImVec2(x0 + w, q.y), kWall);
        if (c.door[side]) {
            bool red = c.redDoor[side] && !Game::Explored::ItemTaken(0x0A, (uint8_t)(side == 0 ? x - 1 : x), (uint8_t)y);
            float d = size * 0.3f;
            dl->AddRectFilled(ImVec2(x0 - w * 0.5f, p.y + d), ImVec2(x0 + w * 1.5f, q.y - d), red ? kRedDoor : kBlueDoor);
        }
    }
    float cx = p.x + size * 0.5f, cy = p.y + size * 0.5f;
    for (const SavePoint& sp : SavePoints()) {
        if (sp.bank == c.bank && sp.mapX == x && sp.mapY == y) {
            // A save point: Super Metroid's "S" square.
            float s = size * 0.3f;
            dl->AddRectFilled(ImVec2(cx - s, cy - s), ImVec2(cx + s, cy + s), kSaveFill, size * 0.06f);
            float fs = size * 0.5f;
            ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0.0f, "S");
            dl->AddText(ImGui::GetFont(), fs, ImVec2(cx - ext.x * 0.5f, cy - ext.y * 0.5f), kWall, "S");
            return;
        }
    }
    if (c.elevator) {
        float s = size * 0.22f;
        dl->AddTriangleFilled(ImVec2(cx, cy - s * 1.6f), ImVec2(cx - s, cy - s * 0.3f), ImVec2(cx + s, cy - s * 0.3f),
                              kWall);
        dl->AddTriangleFilled(ImVec2(cx, cy + s * 1.6f), ImVec2(cx - s, cy + s * 0.3f), ImVec2(cx + s, cy + s * 0.3f),
                              kWall);
    } else if (c.item >= 0 && !Game::Explored::ItemTaken((uint8_t)c.item, (uint8_t)x, (uint8_t)y)) {
        dl->AddCircleFilled(ImVec2(cx, cy), std::max(1.5f, size * 0.12f), kItemDot);
    }
}

void MapView::DrawSamus(ImDrawList* dl, ImVec2 p, float size, double time) {
    // Pulses between bright and dim, so it stands out but is always there.
    float t = 0.5f + 0.5f * (float)std::sin(time * 8.0);
    int a = 110 + (int)(145 * t);
    float m = size * 0.22f;
    dl->AddRectFilled(ImVec2(p.x + m, p.y + m), ImVec2(p.x + size - m, p.y + size - m), IM_COL32(255, 230, 80, a),
                      size * 0.1f);
}

void MapView::DrawArea(ImDrawList* dl, ImVec2 min, ImVec2 max, double time) {
    if (!Ready()) {
        return;
    }
    int bank = CurrentBank();
    bool showAll = CVarGetInteger(kCVarMapShowAll, 0) != 0;
    // The area's extent on the world map.
    int x0 = 32, y0 = 32, x1 = -1, y1 = -1;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            if (mCells[y][x].bank == bank) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x);
                y1 = std::max(y1, y);
            }
        }
    }
    if (x1 < 0) {
        return;
    }
    dl->AddRectFilled(min, max, kBackground, 8.0f);
    float titleH = (max.y - min.y) * 0.08f;
    const char* name = kAreaNames[bank];
    float fontSize = titleH * 0.7f;
    ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name);
    dl->AddText(ImGui::GetFont(), fontSize, ImVec2((min.x + max.x - ext.x) * 0.5f, min.y + titleH * 0.2f), kWall, name);
    ImVec2 area(min.x + 8, min.y + titleH + 4);
    ImVec2 areaMax(max.x - 8, max.y - 8);
    int cols = x1 - x0 + 1, rows = y1 - y0 + 1;
    float cell = std::floor(std::min((areaMax.x - area.x) / cols, (areaMax.y - area.y) / rows));
    cell = std::min(cell, 40.0f);
    ImVec2 origin((area.x + areaMax.x - cell * cols) * 0.5f, (area.y + areaMax.y - cell * rows) * 0.5f);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            ImVec2 p(origin.x + (x - x0) * cell, origin.y + (y - y0) * cell);
            dl->AddRect(p, ImVec2(p.x + cell, p.y + cell), kGrid);
        }
    }
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            DrawCell(dl, x, y, ImVec2(origin.x + (x - x0) * cell, origin.y + (y - y0) * cell), cell, showAll);
        }
    }
    int sx = 0, sy = 0;
    if (Game::Explored::SamusScreen(sx, sy)) {
        DrawSamus(dl, ImVec2(origin.x + (sx - x0) * cell, origin.y + (sy - y0) * cell), cell, time);
    }
}

void MapView::DrawMinimap(ImDrawList* dl, ImVec2 min, float cell, int cols, int rows, double time) {
    if (!Ready()) {
        return;
    }
    int sx = 0, sy = 0;
    if (!Game::Explored::SamusScreen(sx, sy)) {
        return;
    }
    bool showAll = CVarGetInteger(kCVarMapShowAll, 0) != 0;
    ImVec2 max(min.x + cell * cols, min.y + cell * rows);
    dl->AddRectFilled(ImVec2(min.x - 3, min.y - 3), ImVec2(max.x + 3, max.y + 3), kBackground, 4.0f);
    for (int r = 0; r < rows; r++) {
        for (int k = 0; k < cols; k++) {
            int x = sx - cols / 2 + k, y = sy - rows / 2 + r;
            ImVec2 p(min.x + k * cell, min.y + r * cell);
            dl->AddRect(p, ImVec2(p.x + cell, p.y + cell), kGrid);
            if (x >= 0 && y >= 0 && x < 32 && y < 32) {
                DrawCell(dl, x, y, p, cell, showAll);
            }
        }
    }
    DrawSamus(dl, ImVec2(min.x + (cols / 2) * cell, min.y + (rows / 2) * cell), cell, time);
    dl->AddRect(ImVec2(min.x - 3, min.y - 3), ImVec2(max.x + 3, max.y + 3), kWall, 4.0f, 0, 1.5f);
}

} // namespace Metroid
