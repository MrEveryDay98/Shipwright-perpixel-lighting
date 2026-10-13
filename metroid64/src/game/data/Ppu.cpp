#include "game/data/Ppu.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <tinyxml2.h>

namespace Game::Data {

namespace {

constexpr uint16_t kPalPntrTbl = 0x9560;
constexpr int kPaletteCount = 28; // up to AreaPointers ($9598)

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

void ExtractPpu(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered) {
    for (int bank = 1; bank <= 5; bank++) {
        PaletteTable t;
        t.bank = bank;
        t.table = kPalPntrTbl;
        t.count = kPaletteCount;
        Cover(covered, bank, kPalPntrTbl, kPaletteCount * 2);
        for (int i = 0; i < kPaletteCount; i++) {
            uint16_t p = (uint16_t)(Get(prg, bank, (uint16_t)(kPalPntrTbl + 2 * i)) |
                                    (Get(prg, bank, (uint16_t)(kPalPntrTbl + 2 * i + 1)) << 8));
            PpuString s = DecodePpuString(prg, bank, p);
            Cover(covered, bank, s.address, s.size);
            t.palettes[(uint8_t)i] = s;
        }
        files[std::string("metroid/palettes/") + AreaName(bank)] = WritePalettes(t);
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

bool ReadPpuResource(const tinyxml2::XMLElement* root, PpuAssets& out, std::string& error) {
    if (root == nullptr || strcmp(root->Name(), "Palettes") != 0) {
        error = "not a PPU resource";
        return false;
    }
    PaletteTable t;
    t.bank = AreaBank(root->Attribute("Area"));
    if (t.bank == 0) {
        error = "Palettes: unknown Area";
        return false;
    }
    t.table = (uint16_t)HexAttr(root, "Table", kPalPntrTbl);
    t.count = root->IntAttribute("Count", kPaletteCount);
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

bool AssemblePpu(const PpuAssets& a, Image& image, std::string& error) {
    for (const auto& [bank, t] : a.palettes) {
        // Strings sharing an address: the most common encoding keeps it (unmodified tables share a lot).
        std::map<uint16_t, std::map<std::vector<uint8_t>, int>> shared;
        for (const auto& [n, s] : t.palettes) {
            if (s.address) {
                shared[s.address][s.Encode()]++;
            }
        }
        std::map<std::vector<uint8_t>, uint16_t> placed;
        for (const auto& [n, s] : t.palettes) {
            std::vector<uint8_t> bytes = s.Encode();
            uint16_t at = 0;
            auto done = placed.find(bytes);
            if (done != placed.end()) {
                at = done->second;
            } else {
                if (s.address && bytes.size() <= s.size) {
                    const auto& enc = shared[s.address];
                    auto best = std::max_element(enc.begin(), enc.end(),
                                                 [](const auto& x, const auto& y) { return x.second < y.second; });
                    if (best->first == bytes) {
                        at = s.address;
                    }
                }
                if (at == 0) {
                    at = image.Allocate(bank, bytes.size());
                    if (at == 0) {
                        error = "no room left in the bank for palette " + Hex(n, 2);
                        return false;
                    }
                }
                image.Put(bank, at, bytes);
                placed[bytes] = at;
            }
            image.Put16(bank, (uint16_t)(t.table + 2 * n), at);
        }
    }
    return true;
}

} // namespace Game::Data
