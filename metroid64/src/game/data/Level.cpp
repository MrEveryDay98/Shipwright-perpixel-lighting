#include "game/data/Level.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <tinyxml2.h>

namespace Game::Data {

namespace {

// AreaPointers in each area bank: special items, rooms, structures, macros.
constexpr uint16_t kAreaPointers = 0x9598;
constexpr uint16_t kRoomPtrTblPtr = 0x959A;
constexpr uint16_t kStructPtrTblPtr = 0x959C;
constexpr uint16_t kMacroDefsPtr = 0x959E;

const char* AreaName(int bank) {
    static const char* const kNames[] = { "", "brinstar", "norfair", "tourian", "kraid", "ridley" };
    return (bank >= 1 && bank <= 5) ? kNames[bank] : "";
}

int AreaBank(const char* name) {
    for (int b = 1; b <= 5; b++) {
        if (name && strcmp(name, AreaName(b)) == 0) {
            return b;
        }
    }
    return 0;
}

struct Prg {
    const uint8_t* p;
    uint8_t Get(int bank, uint16_t a) const {
        return p[(size_t)bank * 0x4000 + (a - BankBase(bank))];
    }
    uint16_t Get16(int bank, uint16_t a) const {
        return Get(bank, a) | (Get(bank, (uint16_t)(a + 1)) << 8);
    }
};

void Cover(std::vector<uint8_t>& covered, int bank, uint16_t address, size_t length) {
    size_t off = (size_t)bank * 0x4000 + (address - BankBase(bank));
    for (size_t i = 0; i < length && off + i < covered.size(); i++) {
        covered[off + i] = 1;
    }
}

std::string Hex2(unsigned v) {
    char b[8];
    snprintf(b, sizeof(b), "%02X", v & 0xFF);
    return b;
}
std::string Hex1(unsigned v) {
    char b[8];
    snprintf(b, sizeof(b), "%X", v & 0x0F);
    return b;
}
std::string Hex4(unsigned v) {
    char b[8];
    snprintf(b, sizeof(b), "%04X", v & 0xFFFF);
    return b;
}

unsigned HexAttr(const tinyxml2::XMLElement* e, const char* name, unsigned def = 0) {
    const char* v = e->Attribute(name);
    return v ? (unsigned)strtoul(v, nullptr, 16) : def;
}

std::vector<uint8_t> HexBytes(const char* text) {
    std::vector<uint8_t> out;
    for (const char* p = text ? text : ""; *p;) {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
            p++;
        }
        if (!*p) {
            break;
        }
        out.push_back((uint8_t)strtoul(p, (char**)&p, 16));
    }
    return out;
}

// The next of the area's tables after `start` (to size a table by its neighbour).
uint16_t NextTable(const Prg& prg, int bank, uint16_t start) {
    uint16_t next = 0xFFFF;
    for (int i = 0; i < 4; i++) {
        uint16_t t = prg.Get16(bank, (uint16_t)(kAreaPointers + 2 * i));
        if (t > start && t < next) {
            next = t;
        }
    }
    return next;
}

Room DecodeRoom(const Prg& prg, int bank, uint8_t number, uint16_t address) {
    Room r;
    r.bank = bank;
    r.number = number;
    r.address = address;
    uint16_t a = address;
    r.palette = prg.Get(bank, a++);
    for (;;) {
        uint8_t b = prg.Get(bank, a);
        if (b == 0xFF) {
            a++;
            break;
        }
        if (b == 0xFE) {
            RoomObject o;
            o.pause = true;
            r.objects.push_back(o);
            a++;
            continue;
        }
        if (b == 0xFD) {
            r.hasItems = true;
            a++;
            for (;;) {
                uint8_t k = prg.Get(bank, a);
                if (k == 0xFF) {
                    a++;
                    break;
                }
                RoomItem it;
                it.kind = (RoomItem::Kind)(k & 0x0F);
                it.slot = k >> 4;
                switch (it.kind) {
                    case RoomItem::Kind::Enemy:
                    case RoomItem::Kind::ZebHole: {
                        it.type = prg.Get(bank, (uint16_t)(a + 1));
                        uint8_t pos = prg.Get(bank, (uint16_t)(a + 2));
                        it.x = pos & 0x0F;
                        it.y = pos >> 4;
                        a += 3;
                        break;
                    }
                    case RoomItem::Kind::Door:
                    case RoomItem::Kind::Elevator:
                        it.info = prg.Get(bank, (uint16_t)(a + 1));
                        a += 2;
                        break;
                    case RoomItem::Kind::Statues:
                        a += 1;
                        break;
                    default:
                        a += 1; // not in the game's data
                        break;
                }
                r.items.push_back(it);
            }
            break;
        }
        RoomObject o;
        o.x = b & 0x0F;
        o.y = b >> 4;
        o.structure = prg.Get(bank, (uint16_t)(a + 1));
        o.palette = prg.Get(bank, (uint16_t)(a + 2));
        r.objects.push_back(o);
        a += 3;
    }
    r.size = (uint16_t)(a - address);
    return r;
}

Structure DecodeStructure(const Prg& prg, int bank, uint8_t number, uint16_t address) {
    Structure s;
    s.bank = bank;
    s.number = number;
    s.address = address;
    uint16_t a = address;
    for (;;) {
        uint8_t h = prg.Get(bank, a++);
        if (h == 0xFF) {
            break;
        }
        Structure::Row row;
        row.x = h >> 4;
        int count = (h & 0x0F) ? (h & 0x0F) : 16;
        for (int i = 0; i < count; i++) {
            row.macros.push_back(prg.Get(bank, a++));
        }
        s.rows.push_back(row);
    }
    s.size = (uint16_t)(a - address);
    return s;
}

std::string Num2(uint8_t n) {
    return Hex2(n);
}

// Record sizes of the special item kinds (what each ScanForItems handler returns).
int SpecialItemSize(SpecialItem::Kind k) {
    switch (k) {
        case SpecialItem::Kind::Enemy:
        case SpecialItem::Kind::PowerUp:
            return 3;
        case SpecialItem::Kind::Elevator:
        case SpecialItem::Kind::Cannon:
        case SpecialItem::Kind::Door:
            return 2;
        default:
            return 1;
    }
}

const char* const kSpecialItemNames[] = { "",         "Enemy", "PowerUp", "Memus",  "Elevator", "Cannon", "MotherBrain",
                                          "Zebetite", "Rinka", "Door",    "Palette" };

SpecialItems DecodeSpecialItems(const Prg& prg, int bank, uint16_t address) {
    SpecialItems t;
    t.bank = bank;
    t.address = address;
    uint16_t a = address;
    uint16_t end = address;
    for (;;) {
        uint8_t mapY = prg.Get(bank, a);
        uint16_t next = prg.Get16(bank, (uint16_t)(a + 1));
        uint16_t e = (uint16_t)(a + 3);
        for (;;) {
            uint8_t mapX = prg.Get(bank, e);
            uint8_t delta = prg.Get(bank, (uint16_t)(e + 1));
            auto& list = t.screens[{ mapY, mapX }];
            uint16_t p = (uint16_t)(e + 2);
            for (;;) {
                uint8_t k = prg.Get(bank, p);
                if (k == 0) {
                    p++;
                    break;
                }
                SpecialItem it;
                it.kind = (SpecialItem::Kind)(k & 0x0F);
                it.index = k >> 4;
                if (it.kind == SpecialItem::Kind::Enemy || it.kind == SpecialItem::Kind::PowerUp) {
                    it.type = prg.Get(bank, (uint16_t)(p + 1));
                    uint8_t pos = prg.Get(bank, (uint16_t)(p + 2));
                    it.x = pos & 0x0F;
                    it.y = pos >> 4;
                } else if (SpecialItemSize(it.kind) == 2) {
                    it.info = prg.Get(bank, (uint16_t)(p + 1));
                }
                list.push_back(it);
                p = (uint16_t)(p + SpecialItemSize(it.kind));
            }
            end = std::max(end, p);
            if (delta == 0xFF) {
                break;
            }
            e = (uint16_t)(e + delta);
        }
        if (next == 0xFFFF) {
            break;
        }
        a = next;
    }
    t.size = (uint16_t)(end - address);
    return t;
}

} // namespace

// --- Encoding ------------------------------------------------------------------------------------

std::vector<uint8_t> Room::Encode() const {
    std::vector<uint8_t> out{ palette };
    for (const RoomObject& o : objects) {
        if (o.pause) {
            out.push_back(0xFE);
            continue;
        }
        out.push_back((uint8_t)((o.y << 4) | (o.x & 0x0F)));
        out.push_back(o.structure);
        out.push_back(o.palette);
    }
    if (hasItems) {
        out.push_back(0xFD);
        for (const RoomItem& it : items) {
            out.push_back((uint8_t)((it.slot << 4) | ((uint8_t)it.kind & 0x0F)));
            switch (it.kind) {
                case RoomItem::Kind::Enemy:
                case RoomItem::Kind::ZebHole:
                    out.push_back(it.type);
                    out.push_back((uint8_t)((it.y << 4) | (it.x & 0x0F)));
                    break;
                case RoomItem::Kind::Door:
                case RoomItem::Kind::Elevator:
                    out.push_back(it.info);
                    break;
                default:
                    break;
            }
        }
    }
    out.push_back(0xFF);
    return out;
}

std::vector<uint8_t> SpecialItems::Encode(uint16_t base) const {
    std::vector<uint8_t> out;
    // Screens grouped in rows of the same map y; both in increasing order (the scan stops early).
    std::vector<std::pair<uint8_t, std::vector<std::pair<uint8_t, const std::vector<SpecialItem>*>>>> rows;
    for (const auto& [pos, list] : screens) {
        if (rows.empty() || rows.back().first != pos.first) {
            rows.push_back({ pos.first, {} });
        }
        rows.back().second.push_back({ pos.second, &list });
    }
    for (size_t r = 0; r < rows.size(); r++) {
        out.push_back(rows[r].first);
        size_t nextAt = out.size();
        out.push_back(0xFF);
        out.push_back(0xFF);
        const auto& entries = rows[r].second;
        for (size_t i = 0; i < entries.size(); i++) {
            size_t start = out.size();
            out.push_back(entries[i].first);
            out.push_back(0xFF); // offset to the next screen of the row, filled below
            for (const SpecialItem& it : *entries[i].second) {
                out.push_back((uint8_t)((it.index << 4) | ((uint8_t)it.kind & 0x0F)));
                if (it.kind == SpecialItem::Kind::Enemy || it.kind == SpecialItem::Kind::PowerUp) {
                    out.push_back(it.type);
                    out.push_back((uint8_t)((it.y << 4) | (it.x & 0x0F)));
                } else if (SpecialItemSize(it.kind) == 2) {
                    out.push_back(it.info);
                }
            }
            out.push_back(0x00);
            if (i + 1 < entries.size()) {
                out[start + 1] = (uint8_t)(out.size() - start);
            }
        }
        if (r + 1 < rows.size()) {
            uint16_t next = (uint16_t)(base + out.size());
            out[nextAt] = (uint8_t)next;
            out[nextAt + 1] = (uint8_t)(next >> 8);
        }
    }
    return out;
}

std::vector<uint8_t> Structure::Encode() const {
    std::vector<uint8_t> out;
    for (const Row& row : rows) {
        out.push_back((uint8_t)((row.x << 4) | (row.macros.size() & 0x0F)));
        out.insert(out.end(), row.macros.begin(), row.macros.end());
    }
    out.push_back(0xFF);
    return out;
}

// --- Extraction ------------------------------------------------------------------------------------

void ExtractLevel(const uint8_t* prgBytes, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered) {
    Prg prg{ prgBytes };
    WorldMap world;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            world.rooms[y][x] = prg.Get(0, (uint16_t)(world.address + y * 32 + x));
        }
    }
    files["metroid/world/map"] = WriteWorldMap(world);
    Cover(covered, 0, world.address, 32 * 32);

    for (int bank = 1; bank <= 5; bank++) {
        std::string area = AreaName(bank);
        uint16_t roomTbl = prg.Get16(bank, kRoomPtrTblPtr);
        int rooms = (NextTable(prg, bank, roomTbl) - roomTbl) / 2;
        Cover(covered, bank, roomTbl, (size_t)rooms * 2);
        for (int i = 0; i < rooms; i++) {
            Room r = DecodeRoom(prg, bank, (uint8_t)i, prg.Get16(bank, (uint16_t)(roomTbl + 2 * i)));
            Cover(covered, bank, r.address, r.size);
            files["metroid/rooms/" + area + "/room_" + Num2((uint8_t)i)] = WriteRoom(r);
        }
        uint16_t structTbl = prg.Get16(bank, kStructPtrTblPtr);
        int structs = (NextTable(prg, bank, structTbl) - structTbl) / 2;
        Cover(covered, bank, structTbl, (size_t)structs * 2);
        for (int i = 0; i < structs; i++) {
            Structure s = DecodeStructure(prg, bank, (uint8_t)i, prg.Get16(bank, (uint16_t)(structTbl + 2 * i)));
            Cover(covered, bank, s.address, s.size);
            files["metroid/structures/" + area + "/struct_" + Num2((uint8_t)i)] = WriteStructure(s);
        }
        MacroSet m;
        m.bank = bank;
        m.address = prg.Get16(bank, kMacroDefsPtr);
        size_t count = AssetBlockLength(bank, m.address) / 4;
        for (size_t i = 0; i < count; i++) {
            std::array<uint8_t, 4> t;
            for (int k = 0; k < 4; k++) {
                t[k] = prg.Get(bank, (uint16_t)(m.address + i * 4 + k));
            }
            m.macros.push_back(t);
        }
        m.size = (uint16_t)(count * 4);
        Cover(covered, bank, m.address, m.size);
        files["metroid/macros/" + area] = WriteMacros(m);

        SpecialItems items = DecodeSpecialItems(prg, bank, prg.Get16(bank, kAreaPointers));
        Cover(covered, bank, items.address, items.size);
        files["metroid/items/" + area] = WriteSpecialItems(items);
    }
}

// --- XML ---------------------------------------------------------------------------------------------

std::string WriteWorldMap(const WorldMap& map) {
    std::string s = "<WorldMap Version=\"0\" Address=\"" + Hex4(map.address) + "\">\n";
    s += "    <!-- Room number at each map position, row by row (y 00-1F, x 00-1F); FF: no room. -->\n";
    for (int y = 0; y < 32; y++) {
        s += "    <Row Y=\"" + Hex2((unsigned)y) + "\">";
        for (int x = 0; x < 32; x++) {
            s += (x ? " " : "") + Hex2(map.rooms[y][x]);
        }
        s += "</Row>\n";
    }
    return s + "</WorldMap>\n";
}

std::string WriteRoom(const Room& r) {
    std::string s = "<Room Version=\"0\" Area=\"" + std::string(AreaName(r.bank)) + "\" Number=\"" + Hex2(r.number) +
                    "\" Palette=\"" + Hex2(r.palette) + "\"";
    if (r.address) {
        s += " Address=\"" + Hex4(r.address) + "\" Size=\"" + std::to_string(r.size) + "\"";
    }
    s += ">\n";
    for (const RoomObject& o : r.objects) {
        if (o.pause) {
            s += "    <Pause/>\n";
        } else {
            s += "    <Object X=\"" + Hex1(o.x) + "\" Y=\"" + Hex1(o.y) + "\" Structure=\"" + Hex2(o.structure) +
                 "\" Palette=\"" + Hex2(o.palette) + "\"/>\n";
        }
    }
    if (r.hasItems) {
        s += "    <Items>\n";
        for (const RoomItem& it : r.items) {
            switch (it.kind) {
                case RoomItem::Kind::Enemy:
                case RoomItem::Kind::ZebHole: {
                    s += std::string("        <") + (it.kind == RoomItem::Kind::Enemy ? "Enemy" : "ZebHole") +
                         " Slot=\"" + Hex1(it.slot) + "\" Type=\"" + Hex2(it.type & 0x3F) + "\" X=\"" + Hex1(it.x) +
                         "\" Y=\"" + Hex1(it.y) + "\"";
                    if (it.type & 0x80) {
                        s += " Tough=\"1\"";
                    }
                    if (it.type & 0x40) {
                        s += " MiniBoss=\"1\"";
                    }
                    s += "/>\n";
                    break;
                }
                case RoomItem::Kind::Door:
                    s += std::string("        <Door Side=\"") + ((it.info & 0x10) ? "Left" : "Right") +
                         "\" Palette=\"" + Hex1(it.info & 0x03) + "\"";
                    if ((it.info & 0xEC) != 0xA0) {
                        s += " Info=\"" + Hex2(it.info) + "\"";
                    }
                    if (it.slot) {
                        s += " Slot=\"" + Hex1(it.slot) + "\"";
                    }
                    s += "/>\n";
                    break;
                case RoomItem::Kind::Elevator:
                    s += "        <Elevator Info=\"" + Hex2(it.info) + "\"" +
                         (it.slot ? " Slot=\"" + Hex1(it.slot) + "\"" : std::string()) + "/>\n";
                    break;
                case RoomItem::Kind::Statues:
                    s += "        <Statues" + (it.slot ? " Slot=\"" + Hex1(it.slot) + "\"" : std::string()) + "/>\n";
                    break;
                default:
                    s += "        <Unknown Kind=\"" + Hex1((unsigned)it.kind) + "\" Slot=\"" + Hex1(it.slot) + "\"/>\n";
                    break;
            }
        }
        s += "    </Items>\n";
    }
    return s + "</Room>\n";
}

std::string WriteStructure(const Structure& st) {
    std::string s =
        "<Structure Version=\"0\" Area=\"" + std::string(AreaName(st.bank)) + "\" Number=\"" + Hex2(st.number) + "\"";
    if (st.address) {
        s += " Address=\"" + Hex4(st.address) + "\" Size=\"" + std::to_string(st.size) + "\"";
    }
    s += ">\n";
    for (const Structure::Row& row : st.rows) {
        s += "    <Row X=\"" + Hex1(row.x) + "\">";
        for (size_t i = 0; i < row.macros.size(); i++) {
            s += (i ? " " : "") + Hex2(row.macros[i]);
        }
        s += "</Row>\n";
    }
    return s + "</Structure>\n";
}

std::string WriteMacros(const MacroSet& m) {
    std::string s = "<Macros Version=\"0\" Area=\"" + std::string(AreaName(m.bank)) + "\" Address=\"" +
                    Hex4(m.address) + "\" Size=\"" + std::to_string(m.size) + "\">\n";
    s += "    <!-- 2x2 blocks of background tiles: top left, top right, bottom left, bottom right. -->\n";
    for (size_t i = 0; i < m.macros.size(); i++) {
        s += "    <Macro Number=\"" + Hex2((unsigned)i) + "\">" + Hex2(m.macros[i][0]) + " " + Hex2(m.macros[i][1]) +
             " " + Hex2(m.macros[i][2]) + " " + Hex2(m.macros[i][3]) + "</Macro>\n";
    }
    return s + "</Macros>\n";
}

std::string WriteSpecialItems(const SpecialItems& t) {
    std::string s = "<SpecialItems Version=\"0\" Area=\"" + std::string(AreaName(t.bank)) + "\" Address=\"" +
                    Hex4(t.address) + "\" Size=\"" + std::to_string(t.size) + "\">\n";
    for (const auto& [pos, list] : t.screens) {
        s += "    <Screen X=\"" + Hex2(pos.second) + "\" Y=\"" + Hex2(pos.first) + "\">\n";
        for (const SpecialItem& it : list) {
            unsigned k = (unsigned)it.kind;
            s += std::string("        <") + (k >= 1 && k <= 10 ? kSpecialItemNames[k] : "Unknown");
            if (k < 1 || k > 10) {
                s += " Kind=\"" + Hex1(k) + "\"";
            }
            if (it.index) {
                s += " Index=\"" + Hex1(it.index) + "\"";
            }
            if (it.kind == SpecialItem::Kind::Enemy || it.kind == SpecialItem::Kind::PowerUp) {
                s += " Type=\"" + Hex2(it.type) + "\" X=\"" + Hex1(it.x) + "\" Y=\"" + Hex1(it.y) + "\"";
            } else if (SpecialItemSize(it.kind) == 2) {
                s += " Info=\"" + Hex2(it.info) + "\"";
            }
            s += "/>\n";
        }
        s += "    </Screen>\n";
    }
    return s + "</SpecialItems>\n";
}

bool ReadLevelResource(const tinyxml2::XMLElement* root, Level& level, std::string& error) {
    if (root == nullptr) {
        error = "empty";
        return false;
    }
    std::string kind = root->Name();
    if (kind == "WorldMap") {
        level.world.address = (uint16_t)HexAttr(root, "Address", 0xA53E);
        int y = 0;
        for (auto row = root->FirstChildElement("Row"); row && y < 32; row = row->NextSiblingElement("Row"), y++) {
            std::vector<uint8_t> b = HexBytes(row->GetText());
            if (b.size() != 32) {
                error = "WorldMap row " + std::to_string(y) + " needs 32 rooms";
                return false;
            }
            std::copy(b.begin(), b.end(), level.world.rooms[y]);
        }
        if (y != 32) {
            error = "WorldMap needs 32 rows";
            return false;
        }
        level.haveWorld = true;
        return true;
    }
    int bank = AreaBank(root->Attribute("Area"));
    if (bank == 0) {
        error = kind + ": unknown Area";
        return false;
    }
    if (kind == "Room") {
        Room r;
        r.bank = bank;
        r.number = (uint8_t)HexAttr(root, "Number");
        r.palette = (uint8_t)HexAttr(root, "Palette");
        r.address = (uint16_t)HexAttr(root, "Address");
        r.size = (uint16_t)root->IntAttribute("Size");
        for (auto e = root->FirstChildElement(); e; e = e->NextSiblingElement()) {
            std::string n = e->Name();
            if (n == "Pause") {
                RoomObject o;
                o.pause = true;
                r.objects.push_back(o);
            } else if (n == "Object") {
                RoomObject o;
                o.x = (uint8_t)HexAttr(e, "X");
                o.y = (uint8_t)HexAttr(e, "Y");
                o.structure = (uint8_t)HexAttr(e, "Structure");
                o.palette = (uint8_t)HexAttr(e, "Palette");
                r.objects.push_back(o);
            } else if (n == "Items") {
                r.hasItems = true;
                for (auto i = e->FirstChildElement(); i; i = i->NextSiblingElement()) {
                    std::string in = i->Name();
                    RoomItem it;
                    it.slot = (uint8_t)HexAttr(i, "Slot");
                    if (in == "Enemy" || in == "ZebHole") {
                        it.kind = in == "Enemy" ? RoomItem::Kind::Enemy : RoomItem::Kind::ZebHole;
                        it.type = (uint8_t)((HexAttr(i, "Type") & 0x3F) | (i->IntAttribute("Tough") ? 0x80 : 0) |
                                            (i->IntAttribute("MiniBoss") ? 0x40 : 0));
                        it.x = (uint8_t)HexAttr(i, "X");
                        it.y = (uint8_t)HexAttr(i, "Y");
                    } else if (in == "Door") {
                        it.kind = RoomItem::Kind::Door;
                        const char* side = i->Attribute("Side");
                        it.info = (uint8_t)(0xA0 | ((side && strcmp(side, "Left") == 0) ? 0x10 : 0) |
                                            (HexAttr(i, "Palette") & 0x03));
                        it.info = (uint8_t)HexAttr(i, "Info", it.info);
                    } else if (in == "Elevator") {
                        it.kind = RoomItem::Kind::Elevator;
                        it.info = (uint8_t)HexAttr(i, "Info");
                    } else if (in == "Statues") {
                        it.kind = RoomItem::Kind::Statues;
                    } else if (in == "Unknown") {
                        it.kind = (RoomItem::Kind)HexAttr(i, "Kind");
                    } else {
                        error = "Room: unknown item " + in;
                        return false;
                    }
                    r.items.push_back(it);
                }
            }
        }
        level.rooms[bank][r.number] = r;
        return true;
    }
    if (kind == "Structure") {
        Structure s;
        s.bank = bank;
        s.number = (uint8_t)HexAttr(root, "Number");
        s.address = (uint16_t)HexAttr(root, "Address");
        s.size = (uint16_t)root->IntAttribute("Size");
        for (auto e = root->FirstChildElement("Row"); e; e = e->NextSiblingElement("Row")) {
            Structure::Row row;
            row.x = (uint8_t)HexAttr(e, "X");
            row.macros = HexBytes(e->GetText());
            if (row.macros.empty() || row.macros.size() > 16) {
                error = "Structure rows need 1 to 16 macros";
                return false;
            }
            s.rows.push_back(row);
        }
        level.structures[bank][s.number] = s;
        return true;
    }
    if (kind == "SpecialItems") {
        SpecialItems t;
        t.bank = bank;
        t.address = (uint16_t)HexAttr(root, "Address");
        t.size = (uint16_t)root->IntAttribute("Size");
        for (auto sc = root->FirstChildElement("Screen"); sc; sc = sc->NextSiblingElement("Screen")) {
            auto& list = t.screens[{ (uint8_t)HexAttr(sc, "Y"), (uint8_t)HexAttr(sc, "X") }];
            for (auto e = sc->FirstChildElement(); e; e = e->NextSiblingElement()) {
                SpecialItem it;
                std::string n = e->Name();
                unsigned k = 0;
                for (unsigned i = 1; i <= 10; i++) {
                    if (n == kSpecialItemNames[i]) {
                        k = i;
                    }
                }
                if (n == "Unknown") {
                    k = HexAttr(e, "Kind");
                }
                if (k == 0) {
                    error = "SpecialItems: unknown item " + n;
                    return false;
                }
                it.kind = (SpecialItem::Kind)k;
                it.index = (uint8_t)HexAttr(e, "Index");
                it.type = (uint8_t)HexAttr(e, "Type");
                it.x = (uint8_t)HexAttr(e, "X");
                it.y = (uint8_t)HexAttr(e, "Y");
                it.info = (uint8_t)HexAttr(e, "Info");
                list.push_back(it);
            }
        }
        level.specialItems[bank] = t;
        return true;
    }
    if (kind == "Macros") {
        MacroSet m;
        m.bank = bank;
        m.address = (uint16_t)HexAttr(root, "Address");
        m.size = (uint16_t)root->IntAttribute("Size");
        for (auto e = root->FirstChildElement("Macro"); e; e = e->NextSiblingElement("Macro")) {
            std::vector<uint8_t> b = HexBytes(e->GetText());
            if (b.size() != 4) {
                error = "a Macro has 4 tiles";
                return false;
            }
            m.macros.push_back({ b[0], b[1], b[2], b[3] });
        }
        level.macros[bank] = m;
        return true;
    }
    error = "not a level resource";
    return false;
}

// --- Assembly ------------------------------------------------------------------------------------------

namespace {

// Places numbered pieces (rooms or structures) and their pointer table. Each goes back to its address
// when it still fits there and nothing else that shares the address changed; otherwise into free space.
template <typename T>
bool PlaceAll(const std::map<uint8_t, T>& items, int bank, uint16_t tablePtr, Image& image, const char* what,
              std::string& error) {
    if (items.empty()) {
        return true;
    }
    int count = items.rbegin()->first + 1;
    int original = 0;
    for (const auto& [n, item] : items) {
        if (item.address) {
            original = std::max(original, n + 1);
        }
    }
    uint16_t table = (uint16_t)(image.Get(bank, tablePtr) | (image.Get(bank, (uint16_t)(tablePtr + 1)) << 8));
    if (count > original) {
        table = image.Allocate(bank, (size_t)count * 2);
        if (table == 0) {
            error = std::string("no room left in the bank for the ") + what + " table";
            return false;
        }
        image.Put16(bank, tablePtr, table);
    }
    // Pieces sharing an address: the most common encoding keeps it.
    std::map<uint16_t, std::map<std::vector<uint8_t>, int>> shared;
    for (const auto& [n, item] : items) {
        if (item.address) {
            shared[item.address][item.Encode()]++;
        }
    }
    for (const auto& [n, item] : items) {
        std::vector<uint8_t> bytes = item.Encode();
        uint16_t at = 0;
        if (item.address && bytes.size() <= item.size) {
            const auto& encodings = shared[item.address];
            auto best = std::max_element(encodings.begin(), encodings.end(),
                                         [](const auto& a, const auto& b) { return a.second < b.second; });
            if (best->first == bytes) {
                at = item.address;
            }
        }
        if (at == 0) {
            at = image.Allocate(bank, bytes.size());
            if (at == 0) {
                error = std::string("no room left in the bank for ") + what + " " + Hex2(n);
                return false;
            }
        }
        image.Put(bank, at, bytes);
        image.Put16(bank, (uint16_t)(table + 2 * n), at);
    }
    return true;
}

} // namespace

bool AssembleLevel(const Level& level, Image& image, std::string& error) {
    if (level.haveWorld) {
        std::vector<uint8_t> bytes;
        for (int y = 0; y < 32; y++) {
            bytes.insert(bytes.end(), level.world.rooms[y], level.world.rooms[y] + 32);
        }
        image.Put(0, level.world.address, bytes);
    }
    for (const auto& [bank, m] : level.macros) {
        std::vector<uint8_t> bytes;
        for (const auto& t : m.macros) {
            bytes.insert(bytes.end(), t.begin(), t.end());
        }
        uint16_t at = m.address;
        if (at == 0 || bytes.size() > m.size) {
            at = image.Allocate(bank, bytes.size());
            if (at == 0) {
                error = "no room left in the bank for the macros";
                return false;
            }
            image.Put16(bank, kMacroDefsPtr, at);
        }
        image.Put(bank, at, bytes);
    }
    for (const auto& [bank, t] : level.specialItems) {
        uint16_t at = t.address;
        size_t size = t.Encode(at ? at : 0x8000).size();
        if (at == 0 || size > t.size) {
            at = image.Allocate(bank, size);
            if (at == 0) {
                error = "no room left in the bank for the special items";
                return false;
            }
            image.Put16(bank, kAreaPointers, at);
        }
        image.Put(bank, at, t.Encode(at));
    }
    for (const auto& [bank, structs] : level.structures) {
        if (!PlaceAll(structs, bank, kStructPtrTblPtr, image, "structure", error)) {
            return false;
        }
    }
    for (const auto& [bank, rooms] : level.rooms) {
        if (!PlaceAll(rooms, bank, kRoomPtrTblPtr, image, "room", error)) {
            return false;
        }
    }
    return true;
}

} // namespace Game::Data
