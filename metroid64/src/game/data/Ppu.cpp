#include "game/data/Ppu.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <tinyxml2.h>

namespace Game::Data {

namespace {

constexpr uint16_t kPalPntrTbl = 0x9560;
constexpr int kPaletteCount = 28;      // up to AreaPointers ($9598)
constexpr int kTitlePaletteCount = 19; // bank 0: up to the palettes themselves ($9586)

// The title, password and ending screens' strings (bank 0), named after what the code does with them.
struct ScreenDef {
    const char* name;
    uint16_t address; // the string, or the pointer table
    int count;        // 0: one string at a fixed address; negative: a row of -count tiles
};
constexpr ScreenDef kScreens[] = {
    { "intro", 0x82F4, 0 },           // the title screen's background
    { "error_message", 0x8759, -15 }, // "ERROR TRY AGAIN" (password screen)
    { "error_blank", 0x8768, -15 },   // blanks over it
    { "password_title", 0x937F, 0 },  // "PASS WORD"
    { "game_over", 0x93B9, 0 },       // "GAME OVER"
    { "start_continue", 0x9984, 0 },  // "START" / "CONTINUE"
    { "password_please", 0x99E3, 0 }, // "PASS WORD PLEASE"
    { "end_screen", 0xA052, 0 },      // Samus on the planet's surface
    { "ending_palettes", 0x9F81, 13 },
    { "end_messages_0", 0xA1BA, 4 },
    { "end_messages_1", 0xA265, 4 },
    { "credits", 0xA291, 44 },
};

const char* AreaName(int bank) {
    static const char* const kNames[] = { "title", "brinstar", "norfair", "tourian", "kraid", "ridley" };
    return (bank >= 0 && bank <= 5) ? kNames[bank] : "";
}

int AreaBank(const char* name) {
    for (int b = 0; b <= 5; b++) {
        if (name && strcmp(name, AreaName(b)) == 0) {
            return b;
        }
    }
    return -1;
}

uint8_t Get(const uint8_t* prg, int bank, uint16_t a) {
    return prg[(size_t)bank * 0x4000 + (a - BankBase(bank))];
}

std::string Hex(unsigned v, int digits) {
    char b[8];
    snprintf(b, sizeof(b), "%0*X", digits, v);
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

void Cover(std::vector<uint8_t>& covered, int bank, uint16_t address, size_t length) {
    size_t off = (size_t)bank * 0x4000 + (address - BankBase(bank));
    for (size_t i = 0; i < length && off + i < covered.size(); i++) {
        covered[off + i] = 1;
    }
}

std::string WriteString(const PpuString& s, const std::string& indent) {
    std::string out;
    for (const PpuWrite& w : s.writes) {
        if (w.fill) {
            out += indent + "<Fill Address=\"" + Hex(w.address, 4) + "\" Count=\"" + Hex(w.count, 2) + "\" Value=\"" +
                   Hex(w.bytes.empty() ? 0 : w.bytes[0], 2) + "\"" + (w.vertical ? " Vertical=\"1\"" : "") + "/>\n";
        } else {
            out +=
                indent + "<Write Address=\"" + Hex(w.address, 4) + "\"" + (w.vertical ? " Vertical=\"1\"" : "") + ">";
            for (size_t i = 0; i < w.bytes.size(); i++) {
                out += (i ? " " : "") + Hex(w.bytes[i], 2);
            }
            out += "</Write>\n";
        }
    }
    return out;
}

bool ReadString(const tinyxml2::XMLElement* parent, PpuString& s, std::string& error) {
    for (auto e = parent->FirstChildElement(); e; e = e->NextSiblingElement()) {
        PpuWrite w;
        std::string n = e->Name();
        w.address = (uint16_t)HexAttr(e, "Address");
        w.vertical = e->IntAttribute("Vertical") != 0;
        if (n == "Fill") {
            w.fill = true;
            w.count = (uint8_t)HexAttr(e, "Count");
            w.bytes = { (uint8_t)HexAttr(e, "Value") };
            if (w.count == 0 || w.count > 0x3F) {
                error = "a Fill writes 1 to 3F bytes";
                return false;
            }
        } else if (n == "Write") {
            w.bytes = HexBytes(e->GetText());
            if (w.bytes.empty() || w.bytes.size() > 0x3F) {
                error = "a Write has 1 to 3F bytes (split longer ones)";
                return false;
            }
        } else {
            error = "unknown PPU write " + n;
            return false;
        }
        if ((w.address >> 8) == 0) {
            error = "PPU address high byte can't be 0";
            return false;
        }
        s.writes.push_back(w);
    }
    return true;
}

} // namespace

std::vector<uint8_t> PpuString::Encode() const {
    std::vector<uint8_t> out;
    for (const PpuWrite& w : writes) {
        out.push_back((uint8_t)(w.address >> 8));
        out.push_back((uint8_t)w.address);
        uint8_t count = w.fill ? w.count : (uint8_t)w.bytes.size();
        out.push_back((uint8_t)((w.vertical ? 0x80 : 0) | (w.fill ? 0x40 : 0) | (count & 0x3F)));
        if (w.fill) {
            out.push_back(w.bytes.empty() ? 0 : w.bytes[0]);
        } else {
            out.insert(out.end(), w.bytes.begin(), w.bytes.end());
        }
    }
    out.push_back(0x00);
    return out;
}

PpuString DecodePpuString(const uint8_t* prg, int bank, uint16_t address) {
    PpuString s;
    s.address = address;
    uint16_t a = address;
    for (;;) {
        uint8_t hi = Get(prg, bank, a);
        if (hi == 0) {
            a++;
            break;
        }
        PpuWrite w;
        w.address = (uint16_t)((hi << 8) | Get(prg, bank, (uint16_t)(a + 1)));
        uint8_t control = Get(prg, bank, (uint16_t)(a + 2));
        w.vertical = control & 0x80;
        w.fill = control & 0x40;
        uint8_t count = control & 0x3F;
        a += 3;
        if (w.fill) {
            w.count = count;
            w.bytes = { Get(prg, bank, a) };
            a += 1;
        } else {
            for (int i = 0; i < count; i++) {
                w.bytes.push_back(Get(prg, bank, a++));
            }
        }
        s.writes.push_back(w);
    }
    s.size = (uint16_t)(a - address);
    return s;
}

uint16_t Get16(const uint8_t* prg, int bank, uint16_t a) {
    return (uint16_t)(Get(prg, bank, a) | (Get(prg, bank, (uint16_t)(a + 1)) << 8));
}

void ExtractPpu(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered) {
    for (int bank = 0; bank <= 5; bank++) {
        PaletteTable t;
        t.bank = bank;
        t.table = kPalPntrTbl;
        t.count = bank == 0 ? kTitlePaletteCount : kPaletteCount;
        Cover(covered, bank, kPalPntrTbl, (size_t)t.count * 2);
        for (int i = 0; i < t.count; i++) {
            PpuString s = DecodePpuString(prg, bank, Get16(prg, bank, (uint16_t)(kPalPntrTbl + 2 * i)));
            Cover(covered, bank, s.address, s.size);
            t.palettes[(uint8_t)i] = s;
        }
        files[std::string("metroid/palettes/") + AreaName(bank)] = WritePalettes(t);
    }
    for (const ScreenDef& d : kScreens) {
        ScreenSet set;
        set.name = d.name;
        set.bank = 0;
        if (d.count < 0) {
            set.rowAddress = d.address;
            for (int i = 0; i < -d.count; i++) {
                set.row.push_back(Get(prg, 0, (uint16_t)(d.address + i)));
            }
            Cover(covered, 0, d.address, (size_t)-d.count);
        } else if (d.count == 0) {
            PpuString s = DecodePpuString(prg, 0, d.address);
            Cover(covered, 0, s.address, s.size);
            set.strings[0] = s;
        } else {
            set.table = d.address;
            set.count = d.count;
            Cover(covered, 0, d.address, (size_t)d.count * 2);
            for (int i = 0; i < d.count; i++) {
                PpuString s = DecodePpuString(prg, 0, Get16(prg, 0, (uint16_t)(d.address + 2 * i)));
                Cover(covered, 0, s.address, s.size);
                set.strings[(uint8_t)i] = s;
            }
        }
        files[std::string("metroid/screens/") + d.name] = WriteScreen(set);
    }
}

std::string WritePalettes(const PaletteTable& t) {
    std::string s = "<Palettes Version=\"0\" Area=\"" + std::string(AreaName(t.bank)) + "\" Table=\"" +
                    Hex(t.table, 4) + "\" Count=\"" + std::to_string(t.count) + "\">\n";
    s += "    <!-- Palette N is what PalDataPending = N+1 writes. Video memory 3F00-3F0F: background palettes,\n";
    s += "         3F10-3F1F: sprite palettes (4 colours each; colour values are NES colour numbers). -->\n";
    for (const auto& [n, str] : t.palettes) {
        s += "    <Palette Number=\"" + Hex(n, 2) + "\"";
        if (str.address) {
            s += " Address=\"" + Hex(str.address, 4) + "\" Size=\"" + std::to_string(str.size) + "\"";
        }
        s += ">\n" + WriteString(str, "        ") + "    </Palette>\n";
    }
    return s + "</Palettes>\n";
}

std::string WriteScreen(const ScreenSet& set) {
    std::string s = "<Screen Version=\"0\" Name=\"" + set.name + "\" Bank=\"" + std::to_string(set.bank) + "\"";
    if (!set.row.empty()) {
        s += " Address=\"" + Hex(set.rowAddress, 4) + "\" Tiles=\"" + std::to_string(set.row.size()) + "\">\n";
        s += "    <!-- A row of tile numbers the game copies to the screen; it must keep its length. -->\n    <Row>";
        for (size_t i = 0; i < set.row.size(); i++) {
            s += (i ? " " : "") + Hex(set.row[i], 2);
        }
        return s + "</Row>\n</Screen>\n";
    }
    if (set.table) {
        s += " Table=\"" + Hex(set.table, 4) + "\" Count=\"" + std::to_string(set.count) + "\">\n";
        s += "    <!-- Writes to video memory; values are tile numbers (see the graphics). -->\n";
        for (const auto& [n, str] : set.strings) {
            s += "    <String Number=\"" + Hex(n, 2) + "\"";
            if (str.address) {
                s += " Address=\"" + Hex(str.address, 4) + "\" Size=\"" + std::to_string(str.size) + "\"";
            }
            s += ">\n" + WriteString(str, "        ") + "    </String>\n";
        }
    } else {
        const PpuString& str = set.strings.at(0);
        s += " Address=\"" + Hex(str.address, 4) + "\" Size=\"" + std::to_string(str.size) + "\">\n";
        s += "    <!-- Writes to video memory; values are tile numbers (see the graphics). The game uses this\n";
        s += "         string at its address, so it can't grow past Size. -->\n";
        s += WriteString(str, "    ");
    }
    return s + "</Screen>\n";
}

bool ReadPpuResource(const tinyxml2::XMLElement* root, PpuAssets& out, std::string& error) {
    if (root != nullptr && strcmp(root->Name(), "Screen") == 0) {
        ScreenSet set;
        set.name = root->Attribute("Name") ? root->Attribute("Name") : "";
        set.bank = root->IntAttribute("Bank");
        set.table = (uint16_t)HexAttr(root, "Table");
        set.count = root->IntAttribute("Count");
        if (root->IntAttribute("Tiles") > 0) {
            set.rowAddress = (uint16_t)HexAttr(root, "Address");
            auto row = root->FirstChildElement("Row");
            set.row = HexBytes(row ? row->GetText() : nullptr);
            if ((int)set.row.size() != root->IntAttribute("Tiles")) {
                error = "Screen " + set.name + ": the row must have " + std::to_string(root->IntAttribute("Tiles")) +
                        " tiles";
                return false;
            }
        } else if (set.table) {
            for (auto e = root->FirstChildElement("String"); e; e = e->NextSiblingElement("String")) {
                PpuString str;
                str.address = (uint16_t)HexAttr(e, "Address");
                str.size = (uint16_t)e->IntAttribute("Size");
                if (!ReadString(e, str, error)) {
                    return false;
                }
                set.strings[(uint8_t)HexAttr(e, "Number")] = str;
            }
            if (!set.strings.empty() && set.strings.rbegin()->first >= set.count) {
                error = "Screen " + set.name + ": more strings than the table holds";
                return false;
            }
        } else {
            PpuString str;
            str.address = (uint16_t)HexAttr(root, "Address");
            str.size = (uint16_t)root->IntAttribute("Size");
            if (!ReadString(root, str, error)) {
                return false;
            }
            set.strings[0] = str;
        }
        out.screens[set.name] = set;
        return true;
    }
    if (root == nullptr || strcmp(root->Name(), "Palettes") != 0) {
        error = "not a PPU resource";
        return false;
    }
    PaletteTable t;
    t.bank = AreaBank(root->Attribute("Area"));
    if (t.bank < 0) {
        error = "Palettes: unknown Area";
        return false;
    }
    t.table = (uint16_t)HexAttr(root, "Table", kPalPntrTbl);
    t.count = root->IntAttribute("Count", t.bank == 0 ? kTitlePaletteCount : kPaletteCount);
    for (auto e = root->FirstChildElement("Palette"); e; e = e->NextSiblingElement("Palette")) {
        PpuString str;
        str.address = (uint16_t)HexAttr(e, "Address");
        str.size = (uint16_t)e->IntAttribute("Size");
        if (!ReadString(e, str, error)) {
            return false;
        }
        t.palettes[(uint8_t)HexAttr(e, "Number")] = str;
    }
    if ((int)t.palettes.size() > t.count || (!t.palettes.empty() && t.palettes.rbegin()->first >= t.count)) {
        error = "Palettes: more palettes than the table holds";
        return false;
    }
    out.palettes[t.bank] = t;
    return true;
}

namespace {

// Places strings (keyed by table index) and fills their pointer table (table 0: fixed address, no
// table). Strings go back where they were when the strings whose places overlap there still agree
// byte for byte (unmodified ones do; several entries often share a string or its tail). Otherwise the
// table's strings go to free space; a fixed string can't move.
bool PlaceStrings(int bank, uint16_t table, const std::map<uint8_t, PpuString>& strings, Image& image,
                  const std::string& what, std::string& error) {
    std::map<uint8_t, std::vector<uint8_t>> enc;
    for (const auto& [n, s] : strings) {
        enc[n] = s.Encode();
    }
    // Overlap groups of the original places.
    std::vector<uint8_t> keys;
    for (const auto& [n, s] : strings) {
        keys.push_back(n);
    }
    std::map<uint8_t, int> group;
    int groups = 0;
    for (uint8_t n : keys) {
        group[n] = groups++;
    }
    auto overlaps = [&](uint8_t a, uint8_t b) {
        const PpuString& x = strings.at(a);
        const PpuString& y = strings.at(b);
        return x.address && y.address && x.address < y.address + y.size && y.address < x.address + x.size;
    };
    for (bool changed = true; changed;) {
        changed = false;
        for (uint8_t a : keys) {
            for (uint8_t b : keys) {
                if (group[a] != group[b] && overlaps(a, b)) {
                    int from = group[b], to = group[a];
                    for (auto& [k, g] : group) {
                        if (g == from) {
                            g = to;
                        }
                    }
                    changed = true;
                }
            }
        }
    }
    std::map<int, bool> keep;
    for (const auto& [n, g] : group) {
        keep.emplace(g, true);
    }
    for (const auto& [g, k] : std::map<int, bool>(keep)) {
        std::map<uint16_t, uint8_t> bytes;
        bool ok = true;
        for (uint8_t n : keys) {
            if (group[n] != g) {
                continue;
            }
            const PpuString& s = strings.at(n);
            if (!s.address || enc[n].size() > s.size) {
                ok = false;
                break;
            }
            for (size_t i = 0; i < enc[n].size(); i++) {
                auto [it, fresh] = bytes.emplace((uint16_t)(s.address + i), enc[n][i]);
                if (!fresh && it->second != enc[n][i]) {
                    ok = false;
                }
            }
        }
        keep[g] = ok;
    }
    std::map<std::vector<uint8_t>, uint16_t> placed;
    for (uint8_t n : keys) {
        const PpuString& s = strings.at(n);
        uint16_t at = 0;
        if (keep[group[n]]) {
            at = s.address;
            image.Put(bank, at, enc[n]);
        } else if (table == 0) {
            error = what + " must fit in its " + std::to_string(s.size) + " bytes (the game uses it at its address)";
            return false;
        } else if (placed.count(enc[n])) {
            at = placed[enc[n]];
        } else {
            at = image.Allocate(bank, enc[n].size());
            if (at == 0) {
                error = "no room left in the bank for " + what;
                return false;
            }
            image.Put(bank, at, enc[n]);
            placed[enc[n]] = at;
        }
        if (table) {
            image.Put16(bank, (uint16_t)(table + 2 * n), at);
        }
    }
    return true;
}

} // namespace

bool AssemblePpu(const PpuAssets& a, Image& image, std::string& error) {
    for (const auto& [bank, t] : a.palettes) {
        if (!PlaceStrings(bank, t.table, t.palettes, image, std::string(AreaName(bank)) + " palettes", error)) {
            return false;
        }
    }
    for (const auto& [name, set] : a.screens) {
        if (!set.row.empty()) {
            image.Put(set.bank, set.rowAddress, set.row);
            continue;
        }
        if (!PlaceStrings(set.bank, set.table, set.strings, image, "screen " + name, error)) {
            return false;
        }
    }
    return true;
}

} // namespace Game::Data
