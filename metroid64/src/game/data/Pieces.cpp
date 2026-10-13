#include "game/data/Pieces.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <tinyxml2.h>

namespace Game::Data {

namespace {

struct AssetEntry {
    uint8_t bank;
    uint16_t address;
    uint16_t length;
    const char* name;
};

constexpr AssetEntry kAssets[] = {
#include "game/data/AssetMap.inc"
};

} // namespace

bool PlacePieces(Image& image, int bank, const std::vector<const Piece*>& pieces, std::vector<uint16_t>& at,
                 const Allocator& allocate, const std::string& what, std::string& error, const PutListener& onPut) {
    at.assign(pieces.size(), 0);
    auto fits = [](const Piece* p) { return p->address != 0 && p->bytes.size() <= p->size; };
    // What the pieces that fit hold at each address: values in the order they first appear, with counts.
    std::map<uint16_t, std::vector<std::pair<uint8_t, int>>> votes;
    for (const Piece* p : pieces) {
        if (!fits(p)) {
            continue;
        }
        for (size_t i = 0; i < p->bytes.size(); i++) {
            auto& v = votes[(uint16_t)(p->address + i)];
            bool found = false;
            for (auto& [value, count] : v) {
                if (value == p->bytes[i]) {
                    count++;
                    found = true;
                }
            }
            if (!found) {
                v.emplace_back(p->bytes[i], 1);
            }
        }
    }
    auto winner = [&](uint16_t address) {
        const auto& v = votes[address];
        size_t best = 0;
        for (size_t k = 1; k < v.size(); k++) {
            if (v[k].second > v[best].second) {
                best = k;
            }
        }
        return v[best].first;
    };
    std::map<std::vector<uint8_t>, uint16_t> placed;
    std::vector<bool> keep(pieces.size(), false);
    for (size_t n = 0; n < pieces.size(); n++) {
        const Piece* p = pieces[n];
        if (!fits(p)) {
            continue;
        }
        bool agree = true;
        for (size_t i = 0; i < p->bytes.size() && agree; i++) {
            agree = winner((uint16_t)(p->address + i)) == p->bytes[i];
        }
        if (agree) {
            keep[n] = true;
            image.Put(bank, p->address, p->bytes);
            if (onPut) {
                onPut(p->address, p->bytes.size());
            }
            at[n] = p->address;
            placed.emplace(p->bytes, p->address);
        }
    }
    for (size_t n = 0; n < pieces.size(); n++) {
        const Piece* p = pieces[n];
        if (keep[n] || p->bytes.empty()) {
            continue;
        }
        auto same = placed.find(p->bytes);
        if (same != placed.end()) {
            at[n] = same->second;
            continue;
        }
        uint16_t a = allocate(p->bytes.size());
        if (a == 0) {
            error = "no room left in bank " + std::to_string(bank) + " for " + what;
            return false;
        }
        image.Put(bank, a, p->bytes);
        if (onPut) {
            onPut(a, p->bytes.size());
        }
        at[n] = a;
        placed.emplace(p->bytes, a);
    }
    return true;
}

namespace Bytes {

uint8_t Prg::Get(int bank, uint16_t a) const {
    return p[(size_t)bank * 0x4000 + (a - BankBase(bank))];
}

uint16_t Prg::Get16(int bank, uint16_t a) const {
    return (uint16_t)(Get(bank, a) | (Get(bank, (uint16_t)(a + 1)) << 8));
}

bool InData(int bank, uint16_t address, size_t length) {
    for (size_t i = 0; i < length; i++) {
        uint32_t a = address + (uint32_t)i;
        bool in = false;
        for (const AssetEntry& e : kAssets) {
            if (e.bank == bank && a >= e.address && a < (uint32_t)e.address + e.length) {
                in = true;
                break;
            }
        }
        if (!in) {
            return false;
        }
    }
    return true;
}

void Cover(std::vector<uint8_t>& covered, int bank, uint16_t address, size_t length) {
    size_t off = (size_t)bank * 0x4000 + (address - BankBase(bank));
    for (size_t i = 0; i < length && off + i < covered.size(); i++) {
        covered[off + i] = 1;
    }
}

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

std::string Hex(unsigned v, int digits) {
    char b[16];
    snprintf(b, sizeof(b), "%0*X", digits, v);
    return b;
}

std::string HexList(const std::vector<uint8_t>& bytes) {
    std::string s;
    for (size_t i = 0; i < bytes.size(); i++) {
        s += (i ? " " : "") + Hex(bytes[i]);
    }
    return s;
}

std::string Signed(uint8_t v) {
    return std::to_string((int)(int8_t)v);
}

unsigned HexAttr(const tinyxml2::XMLElement* e, const char* name, unsigned def) {
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
        char* end = nullptr;
        out.push_back((uint8_t)strtoul(p, &end, 16));
        if (end == p) {
            break; // not hex: stop rather than loop
        }
        p = end;
    }
    return out;
}

} // namespace Bytes

} // namespace Game::Data
