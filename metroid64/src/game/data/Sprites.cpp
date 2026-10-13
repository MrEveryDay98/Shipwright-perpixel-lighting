#include "game/data/Sprites.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <tinyxml2.h>

namespace Game::Data {

using namespace Bytes;

namespace {

// The objects' tables, at the same addresses in the five area banks (the drawing code reads them there).
constexpr uint16_t kObjectAnimIdxTbl = 0x8572;
constexpr uint16_t kFramePtrTable = 0x860B;
constexpr uint16_t kPlacePtrTable = 0x86DF;
constexpr int kObjectAnimSize = kFramePtrTable - kObjectAnimIdxTbl;  // 153
constexpr int kObjectFrames = (kPlacePtrTable - kFramePtrTable) / 2; // 106
constexpr int kObjectPlaces = 15;                                    // the placement data follows ($86FD)

// AreaPointers entries that lead to the enemies' tables.
constexpr uint16_t kEnemyFramePtrTbl1Ptr = 0x95A0;
constexpr uint16_t kEnemyFramePtrTbl2Ptr = 0x95A2;
constexpr uint16_t kEnemyPlacePtrTblPtr = 0x95A4;
constexpr uint16_t kEnemyAnimIndexTblPtr = 0x95A6;

// Frame bytes: control, y radius, x radius, then sprites: a tile below $FC; $FC y x moves the rest of the
// frame; $FD a: new sprite attributes; $FE: skips a placement entry; $FF ends the frame.
constexpr uint8_t kOffset = 0xFC;
constexpr uint8_t kAttributes = 0xFD;
constexpr uint8_t kSkip = 0xFE;
constexpr uint8_t kEnd = 0xFF;

std::vector<uint8_t> DecodeFrame(const Prg& prg, int bank, uint16_t address) {
    std::vector<uint8_t> out;
    uint16_t a = address;
    for (int i = 0; i < 3; i++) {
        out.push_back(prg.Get(bank, a++));
    }
    while (out.size() < 0x100) {
        uint8_t b = prg.Get(bank, a++);
        out.push_back(b);
        if (b == kEnd) {
            break;
        }
        if (b == kOffset) {
            out.push_back(prg.Get(bank, a++));
            out.push_back(prg.Get(bank, a++));
        } else if (b == kAttributes) {
            out.push_back(prg.Get(bank, a++));
        }
    }
    return out;
}

// How many placement entries a frame uses (one per sprite and per skip).
int PlacementEntries(const std::vector<uint8_t>& frame) {
    int n = 0;
    for (size_t i = 3; i < frame.size(); i++) {
        uint8_t b = frame[i];
        if (b == kEnd) {
            break;
        }
        if (b == kOffset) {
            i += 2;
        } else if (b == kAttributes) {
            i += 1;
        } else {
            n++;
        }
    }
    return n;
}

// Placement bytes $80-$8F are explosion pieces: they read their start offset 12 bytes further on.
bool IsExplodePiece(uint8_t b) {
    return (b & 0xF0) == 0x80;
}

// Reads one set out of a bank (the tables must be filled in).
void DecodeSet(const Prg& prg, int bank, SpriteSet& s) {
    s.anims.clear();
    for (int i = 0; i < s.animSize; i++) {
        s.anims.push_back(prg.Get(bank, (uint16_t)(s.animTable + i)));
    }
    std::map<uint16_t, int> seen;
    auto frame = [&](int number, uint16_t ptr) {
        auto it = seen.find(ptr);
        if (it != seen.end()) {
            s.frameSameAs[number] = it->second;
            return;
        }
        seen[ptr] = number;
        Piece p;
        p.address = ptr;
        p.bytes = DecodeFrame(prg, bank, ptr);
        p.size = (uint16_t)p.bytes.size();
        s.frames[number] = p;
    };
    for (int i = 0; i < s.frameCount; i++) {
        frame(i, prg.Get16(bank, (uint16_t)(s.frameTable + 2 * i)));
    }
    for (int i = 0; i < s.frameCount2; i++) {
        frame(0x80 + i, prg.Get16(bank, (uint16_t)(s.frameTable2 + 2 * i)));
    }
    // Placement tables: as far as the next one (or the frames), or as far as a frame reads.
    std::vector<uint16_t> ptrs;
    for (int i = 0; i < s.placeCount; i++) {
        ptrs.push_back(prg.Get16(bank, (uint16_t)(s.placeTable + 2 * i)));
    }
    std::map<uint16_t, int> need;
    for (const auto& [n, f] : s.frames) {
        int index = f.bytes[0] & 0x0F;
        if (index >= s.placeCount) {
            continue;
        }
        uint16_t p = ptrs[index];
        int bytes = 2 * PlacementEntries(f.bytes);
        int reach = bytes;
        for (int i = 0; i < bytes; i++) {
            if (IsExplodePiece(prg.Get(bank, (uint16_t)(p + i)))) {
                reach = std::max(reach, i + 12 + 1);
            }
        }
        need[p] = std::max(need[p], reach);
    }
    seen.clear();
    for (int i = 0; i < s.placeCount; i++) {
        uint16_t p = ptrs[i];
        auto it = seen.find(p);
        if (it != seen.end()) {
            s.placeSameAs[i] = it->second;
            continue;
        }
        seen[p] = i;
        uint32_t next = 0x10000;
        for (uint16_t q : ptrs) {
            if (q > p) {
                next = std::min<uint32_t>(next, q);
            }
        }
        for (const auto& [n, f] : s.frames) {
            if (f.address >= p) {
                next = std::min<uint32_t>(next, f.address);
            }
        }
        int size = std::max<int>(need[p], next == 0x10000 ? 0 : (int)(next - p));
        Piece pl;
        pl.address = p;
        pl.size = (uint16_t)size;
        for (int k = 0; k < size; k++) {
            pl.bytes.push_back(prg.Get(bank, (uint16_t)(p + k)));
        }
        s.places[i] = pl;
    }
}

void CoverSet(const SpriteSet& s, int bank, std::vector<uint8_t>& covered) {
    Cover(covered, bank, s.animTable, s.animSize);
    Cover(covered, bank, s.frameTable, (size_t)s.frameCount * 2);
    if (s.frameCount2) {
        Cover(covered, bank, s.frameTable2, (size_t)s.frameCount2 * 2);
    }
    Cover(covered, bank, s.placeTable, (size_t)s.placeCount * 2);
    for (const auto& [n, f] : s.frames) {
        Cover(covered, bank, f.address, f.size);
    }
    for (const auto& [n, p] : s.places) {
        Cover(covered, bank, p.address, p.size);
    }
}

bool SameData(const SpriteSet& a, const SpriteSet& b) {
    auto same = [](const std::map<int, Piece>& x, const std::map<int, Piece>& y) {
        if (x.size() != y.size()) {
            return false;
        }
        for (const auto& [n, p] : x) {
            auto it = y.find(n);
            if (it == y.end() || it->second.address != p.address || it->second.bytes != p.bytes) {
                return false;
            }
        }
        return true;
    };
    return a.anims == b.anims && a.frameSameAs == b.frameSameAs && a.placeSameAs == b.placeSameAs &&
           same(a.frames, b.frames) && same(a.places, b.places);
}

std::string FilePath(const SpriteSet& s) {
    if (s.enemies) {
        return std::string("metroid/sprites/") + AreaName(s.area) + "/enemies";
    }
    if (s.area) {
        return std::string("metroid/sprites/") + AreaName(s.area) + "/objects";
    }
    return "metroid/sprites/objects";
}

// Placement entry bytes: signed offsets, or *n for the explosion piece byte $8n.
std::string PlaceByte(uint8_t b) {
    if (IsExplodePiece(b)) {
        return "*" + Hex(b & 0x0F, 1);
    }
    return Signed(b);
}

bool ParsePlaceByte(const std::string& t, uint8_t& out) {
    if (t.empty()) {
        return false;
    }
    if (t[0] == '*') {
        out = (uint8_t)(0x80 | (strtoul(t.c_str() + 1, nullptr, 16) & 0x0F));
        return t.size() > 1;
    }
    char* end = nullptr;
    long v = strtol(t.c_str(), &end, 10);
    if (*end != 0 || v < -128 || v > 255) {
        return false;
    }
    out = (uint8_t)v;
    return true;
}

std::string WriteFrame(int number, const Piece& f) {
    const std::vector<uint8_t>& b = f.bytes;
    std::string s = "    <Frame Number=\"" + Hex(number) + "\" Address=\"" + Hex(f.address, 4) + "\" Size=\"" +
                    std::to_string(f.size) + "\"";
    if (b.size() < 3) {
        return s + "/>\n";
    }
    s += " Placement=\"" + Hex(b[0] & 0x0F, 1) + "\" Palette=\"" + Hex((b[0] >> 4) & 0x03, 1) + "\"";
    if (b[0] & 0x40) {
        s += " FlipX=\"1\"";
    }
    if (b[0] & 0x80) {
        s += " FlipY=\"1\"";
    }
    s += " RadiusY=\"" + Hex(b[1]) + "\" RadiusX=\"" + Hex(b[2]) + "\">\n";
    for (size_t i = 3; i < b.size(); i++) {
        if (b[i] == kEnd) {
            break;
        }
        if (b[i] == kOffset && i + 2 < b.size()) {
            s += "        <Offset Y=\"" + Signed(b[i + 1]) + "\" X=\"" + Signed(b[i + 2]) + "\"/>\n";
            i += 2;
        } else if (b[i] == kAttributes && i + 1 < b.size()) {
            s += "        <Attributes Value=\"" + Hex(b[i + 1]) + "\"/>\n";
            i += 1;
        } else if (b[i] == kSkip) {
            s += "        <Skip/>\n";
        } else {
            s += "        <Sprite Tile=\"" + Hex(b[i]) + "\"/>\n";
        }
    }
    return s + "    </Frame>\n";
}

bool ReadSignedAttr(const tinyxml2::XMLElement* e, const char* name, uint8_t& out) {
    const char* v = e->Attribute(name);
    if (v == nullptr) {
        out = 0;
        return true;
    }
    char* end = nullptr;
    long n = strtol(v, &end, 10);
    if (*end != 0 || n < -128 || n > 127) {
        return false;
    }
    out = (uint8_t)n;
    return true;
}

bool ReadFrame(const tinyxml2::XMLElement* e, Piece& f, std::string& error) {
    f.address = (uint16_t)HexAttr(e, "Address");
    f.size = (uint16_t)e->IntAttribute("Size");
    unsigned placement = HexAttr(e, "Placement");
    unsigned palette = HexAttr(e, "Palette");
    if (placement > 0x0F || palette > 3) {
        error = "a frame's Placement is 0-F, its Palette 0-3";
        return false;
    }
    f.bytes.push_back((uint8_t)((e->IntAttribute("FlipY") ? 0x80 : 0) | (e->IntAttribute("FlipX") ? 0x40 : 0) |
                                (palette << 4) | placement));
    f.bytes.push_back((uint8_t)HexAttr(e, "RadiusY"));
    f.bytes.push_back((uint8_t)HexAttr(e, "RadiusX"));
    for (auto c = e->FirstChildElement(); c; c = c->NextSiblingElement()) {
        std::string n = c->Name();
        if (n == "Sprite") {
            unsigned tile = HexAttr(c, "Tile");
            if (tile >= 0xFC) {
                error = "sprite tiles FC-FF can't be used (they are frame commands)";
                return false;
            }
            f.bytes.push_back((uint8_t)tile);
        } else if (n == "Skip") {
            f.bytes.push_back(kSkip);
        } else if (n == "Offset") {
            uint8_t y, x;
            if (!ReadSignedAttr(c, "Y", y) || !ReadSignedAttr(c, "X", x)) {
                error = "an Offset's Y and X are -128 to 127";
                return false;
            }
            f.bytes.insert(f.bytes.end(), { kOffset, y, x });
        } else if (n == "Attributes") {
            f.bytes.insert(f.bytes.end(), { kAttributes, (uint8_t)HexAttr(c, "Value") });
        } else {
            error = "unknown frame element " + n;
            return false;
        }
    }
    f.bytes.push_back(kEnd);
    return true;
}

// Resolves SameAs chains: the number whose data a frame or placement uses.
bool Resolve(const std::map<int, int>& sameAs, const std::map<int, Piece>& own, int n, int& out) {
    for (int steps = 0; steps < 512; steps++) {
        if (own.count(n)) {
            out = n;
            return true;
        }
        auto it = sameAs.find(n);
        if (it == sameAs.end()) {
            return false;
        }
        n = it->second;
    }
    return false;
}

bool AssembleSet(const SpriteSet& s, int bank, Image& image, std::string& error) {
    const std::string where = s.enemies ? std::string(AreaName(s.area)) + " enemy" : std::string("object");
    // Animations.
    if (s.anims.size() > 0x100) {
        error = "the " + where + " animation table is over 256 bytes";
        return false;
    }
    if (s.anims.size() > s.animSize) {
        if (!s.enemies) {
            error = "the object animation table can't grow (it holds " + std::to_string(s.animSize) + " bytes)";
            return false;
        }
        uint16_t at = image.Allocate(bank, s.anims.size());
        if (at == 0) {
            error = "no room left in bank " + std::to_string(bank) + " for the " + where + " animations";
            return false;
        }
        image.Put(bank, at, s.anims);
        image.Put16(bank, kEnemyAnimIndexTblPtr, at);
    } else {
        image.Put(bank, s.animTable, s.anims);
    }
    auto allocate = [&](size_t length) { return image.Allocate(bank, length); };
    // Pointer tables: in place, or moved (with its AreaPointers entry) when it has more entries now.
    auto table = [&](uint16_t original, int originalCount, int count, uint16_t areaPtr,
                     const std::vector<uint16_t>& entries, const char* name) {
        uint16_t at = original;
        if (count > originalCount) {
            if (areaPtr == 0) {
                error = std::string("the object ") + name + " table can't grow (it has " +
                        std::to_string(originalCount) + " entries)";
                return false;
            }
            at = image.Allocate(bank, (size_t)count * 2);
            if (at == 0) {
                error = "no room left in bank " + std::to_string(bank) + " for the " + where + " " + name + " table";
                return false;
            }
            image.Put16(bank, areaPtr, at);
        }
        for (int i = 0; i < count; i++) {
            image.Put16(bank, (uint16_t)(at + 2 * i), entries[i]);
        }
        return true;
    };
    // Placements.
    std::vector<const Piece*> pieces;
    std::vector<int> numbers;
    for (const auto& [n, p] : s.places) {
        pieces.push_back(&p);
        numbers.push_back(n);
    }
    std::vector<uint16_t> at;
    if (!PlacePieces(image, bank, pieces, at, allocate, "the " + where + " placement data", error)) {
        return false;
    }
    std::map<int, uint16_t> placeAt;
    for (size_t i = 0; i < numbers.size(); i++) {
        placeAt[numbers[i]] = at[i];
    }
    int count = s.placeCount;
    for (const auto& [n, p] : s.places) {
        count = std::max(count, n + 1);
    }
    for (const auto& [n, o] : s.placeSameAs) {
        count = std::max(count, n + 1);
    }
    std::vector<uint16_t> entries(count, 0);
    for (int i = 0; i < count; i++) {
        int own;
        if (Resolve(s.placeSameAs, s.places, i, own)) {
            entries[i] = placeAt[own];
        } else if (s.placeSameAs.count(i)) {
            error = "placement " + Hex(i) + " is SameAs a placement that isn't there";
            return false;
        }
    }
    if (!table(s.placeTable, s.placeCount, count, s.enemies ? kEnemyPlacePtrTblPtr : 0, entries, "placement")) {
        return false;
    }
    // Frames.
    pieces.clear();
    numbers.clear();
    for (const auto& [n, f] : s.frames) {
        pieces.push_back(&f);
        numbers.push_back(n);
    }
    if (!PlacePieces(image, bank, pieces, at, allocate, "the " + where + " frames", error)) {
        return false;
    }
    std::map<int, uint16_t> frameAt;
    for (size_t i = 0; i < numbers.size(); i++) {
        frameAt[numbers[i]] = at[i];
    }
    int count1 = s.frameCount, count2 = s.frameCount2;
    auto grow = [&](int n) {
        if (s.enemies && n >= 0x80) {
            count2 = std::max(count2, n - 0x80 + 1);
        } else {
            count1 = std::max(count1, n + 1);
        }
    };
    for (const auto& [n, f] : s.frames) {
        grow(n);
    }
    for (const auto& [n, o] : s.frameSameAs) {
        grow(n);
    }
    if ((s.enemies && (count1 > 0x80 || count2 > 0x80)) || count1 > 0x100) {
        error = "too many " + where + " frames";
        return false;
    }
    auto frameEntries = [&](int first, int n, std::vector<uint16_t>& out) {
        out.assign(n, 0);
        for (int i = 0; i < n; i++) {
            int own;
            if (Resolve(s.frameSameAs, s.frames, first + i, own)) {
                out[i] = frameAt[own];
            } else if (s.frameSameAs.count(first + i)) {
                error = "frame " + Hex(first + i) + " is SameAs a frame that isn't there";
                return false;
            }
        }
        return true;
    };
    if (!frameEntries(0, count1, entries) ||
        !table(s.frameTable, s.frameCount, count1, s.enemies ? kEnemyFramePtrTbl1Ptr : 0, entries, "frame")) {
        return false;
    }
    if (s.enemies && (!frameEntries(0x80, count2, entries) ||
                      !table(s.frameTable2, s.frameCount2, count2, kEnemyFramePtrTbl2Ptr, entries, "frame"))) {
        return false;
    }
    return true;
}

} // namespace

std::string SpriteSet::Key() const {
    if (enemies) {
        return std::string("enemies/") + AreaName(area);
    }
    return area ? std::string("objects/") + AreaName(area) : std::string("objects");
}

void ExtractSprites(const uint8_t* bytes, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered) {
    Prg prg{ bytes };
    // Objects: the same in every area bank, so one resource for all of them (else one per bank).
    std::vector<SpriteSet> objects;
    for (int bank = 1; bank <= 5; bank++) {
        SpriteSet s;
        s.area = bank;
        s.banks = { bank };
        s.animTable = kObjectAnimIdxTbl;
        s.animSize = kObjectAnimSize;
        s.frameTable = kFramePtrTable;
        s.frameCount = kObjectFrames;
        s.placeTable = kPlacePtrTable;
        s.placeCount = kObjectPlaces;
        DecodeSet(prg, bank, s);
        CoverSet(s, bank, covered);
        objects.push_back(s);
    }
    bool same =
        std::all_of(objects.begin(), objects.end(), [&](const SpriteSet& s) { return SameData(s, objects[0]); });
    if (same) {
        SpriteSet s = objects[0];
        s.area = 0;
        s.banks = { 1, 2, 3, 4, 5 };
        files[FilePath(s)] = WriteSprites(s);
    } else {
        for (const SpriteSet& s : objects) {
            files[FilePath(s)] = WriteSprites(s);
        }
    }
    // Enemies, per area.
    for (int bank = 1; bank <= 5; bank++) {
        SpriteSet s;
        s.enemies = true;
        s.area = bank;
        s.banks = { bank };
        s.animTable = prg.Get16(bank, kEnemyAnimIndexTblPtr);
        s.frameTable = prg.Get16(bank, kEnemyFramePtrTbl1Ptr);
        s.frameTable2 = prg.Get16(bank, kEnemyFramePtrTbl2Ptr);
        s.placeTable = prg.Get16(bank, kEnemyPlacePtrTblPtr);
        // The tables follow each other: animations, frames 00-7F, frames 80-, placements, placement data.
        s.animSize = (uint16_t)(s.frameTable - s.animTable);
        s.frameCount = std::min(0x80, (s.frameTable2 - s.frameTable) / 2);
        s.frameCount2 = std::min(0x80, (s.placeTable - s.frameTable2) / 2);
        // The placement table ends where the first placement data starts (16 entries at most).
        uint16_t first = 0xFFFF;
        s.placeCount = 0;
        for (int i = 0; i < 16 && s.placeTable + 2 * i < first; i++) {
            uint16_t p = prg.Get16(bank, (uint16_t)(s.placeTable + 2 * i));
            s.placeCount = i + 1;
            if (p > s.placeTable) {
                first = std::min(first, p);
            }
        }
        DecodeSet(prg, bank, s);
        CoverSet(s, bank, covered);
        files[FilePath(s)] = WriteSprites(s);
    }
}

std::string WriteSprites(const SpriteSet& s) {
    std::string out = "<Sprites Version=\"0\" Kind=\"" + std::string(s.enemies ? "enemies" : "objects") + "\"";
    if (s.area) {
        out += " Area=\"" + std::string(AreaName(s.area)) + "\"";
    } else {
        out += " Banks=\"";
        for (size_t i = 0; i < s.banks.size(); i++) {
            out += (i ? " " : "") + std::to_string(s.banks[i]);
        }
        out += "\"";
    }
    out += " AnimTable=\"" + Hex(s.animTable, 4) + "\" AnimSize=\"" + std::to_string(s.animSize) + "\" FrameTable=\"" +
           Hex(s.frameTable, 4) + "\" Frames=\"" + std::to_string(s.frameCount) + "\"";
    if (s.enemies) {
        out += " FrameTable2=\"" + Hex(s.frameTable2, 4) + "\" Frames2=\"" + std::to_string(s.frameCount2) + "\"";
    }
    out += " PlaceTable=\"" + Hex(s.placeTable, 4) + "\" Placements=\"" + std::to_string(s.placeCount) + "\">\n";
    out += "    <!-- Animations: frame numbers; the game starts an animation at its Index (or at an entry inside\n"
           "         it) and goes back to where it started when it reaches the end. Frame F7 shows nothing. -->\n";
    for (size_t i = 0; i < s.anims.size();) {
        size_t j = i;
        while (j < s.anims.size() && s.anims[j] != 0xFF) {
            j++;
        }
        std::vector<uint8_t> run(s.anims.begin() + i, s.anims.begin() + j);
        out += "    <Animation Index=\"" + Hex((unsigned)i) + "\"" + (j == s.anims.size() ? " Open=\"1\"" : "") + ">" +
               HexList(run) + "</Animation>\n";
        i = j + 1;
    }
    out += "    <!-- Frames: each Sprite (an 8x8 tile) takes the next entry of the frame's Placement table.\n"
           "         Offset moves the rest of the frame, Attributes sets the sprite attribute byte of the next\n"
           "         sprites, Skip passes over a placement entry. RadiusY/X: the object's size. -->\n";
    std::set<int> numbers;
    for (const auto& [n, f] : s.frames) {
        numbers.insert(n);
    }
    for (const auto& [n, o] : s.frameSameAs) {
        numbers.insert(n);
    }
    for (int n : numbers) {
        auto it = s.frames.find(n);
        if (it != s.frames.end()) {
            out += WriteFrame(n, it->second);
        } else {
            out += "    <Frame Number=\"" + Hex(n) + "\" SameAs=\"" + Hex(s.frameSameAs.at(n)) + "\"/>\n";
        }
    }
    out += "    <!-- Placement tables: y,x offsets of the sprites from the object's position (mirrored frames\n"
           "         mirror them). *n is an explosion piece (byte 8n): it flies out over time from the offset\n"
           "         given 6 entries further on. -->\n";
    numbers.clear();
    for (const auto& [n, p] : s.places) {
        numbers.insert(n);
    }
    for (const auto& [n, o] : s.placeSameAs) {
        numbers.insert(n);
    }
    for (int n : numbers) {
        auto it = s.places.find(n);
        if (it == s.places.end()) {
            out += "    <Placement Number=\"" + Hex(n) + "\" SameAs=\"" + Hex(s.placeSameAs.at(n)) + "\"/>\n";
            continue;
        }
        const Piece& p = it->second;
        out += "    <Placement Number=\"" + Hex(n) + "\" Address=\"" + Hex(p.address, 4) + "\" Size=\"" +
               std::to_string(p.size) + "\">";
        for (size_t i = 0; i < p.bytes.size(); i += 2) {
            out += (i ? " " : "") + PlaceByte(p.bytes[i]);
            if (i + 1 < p.bytes.size()) {
                out += "," + PlaceByte(p.bytes[i + 1]);
            }
        }
        out += "</Placement>\n";
    }
    return out + "</Sprites>\n";
}

bool ReadSpritesResource(const tinyxml2::XMLElement* root, SpriteAssets& out, std::string& error) {
    if (root == nullptr || strcmp(root->Name(), "Sprites") != 0) {
        error = "not a Sprites resource";
        return false;
    }
    SpriteSet s;
    const char* kind = root->Attribute("Kind");
    s.enemies = kind && strcmp(kind, "enemies") == 0;
    if (!s.enemies && !(kind && strcmp(kind, "objects") == 0)) {
        error = "Sprites: Kind is objects or enemies";
        return false;
    }
    const char* area = root->Attribute("Area");
    if (area) {
        s.area = AreaBank(area);
        if (s.area == 0) {
            error = "Sprites: unknown Area";
            return false;
        }
        s.banks = { s.area };
    } else if (s.enemies) {
        error = "Sprites: enemies need an Area";
        return false;
    } else {
        for (uint8_t b : HexBytes(root->Attribute("Banks"))) {
            if (b < 1 || b > 5) {
                error = "Sprites: Banks are area banks 1-5";
                return false;
            }
            s.banks.push_back(b);
        }
    }
    s.animTable = (uint16_t)HexAttr(root, "AnimTable");
    s.animSize = (uint16_t)root->IntAttribute("AnimSize");
    s.frameTable = (uint16_t)HexAttr(root, "FrameTable");
    s.frameCount = root->IntAttribute("Frames");
    s.frameTable2 = (uint16_t)HexAttr(root, "FrameTable2");
    s.frameCount2 = root->IntAttribute("Frames2");
    s.placeTable = (uint16_t)HexAttr(root, "PlaceTable");
    s.placeCount = root->IntAttribute("Placements");
    if (!s.enemies &&
        (s.animTable != kObjectAnimIdxTbl || s.frameTable != kFramePtrTable || s.placeTable != kPlacePtrTable ||
         s.animSize > kObjectAnimSize || s.frameCount > kObjectFrames || s.placeCount > kObjectPlaces)) {
        error = "Sprites: the objects' tables are where the game reads them (leave the root's attributes)";
        return false;
    }
    std::vector<int> owner; // which animation wrote each byte
    for (auto e = root->FirstChildElement(); e; e = e->NextSiblingElement()) {
        std::string n = e->Name();
        if (n == "Animation") {
            unsigned index = HexAttr(e, "Index");
            std::vector<uint8_t> run = HexBytes(e->GetText());
            if (!e->IntAttribute("Open")) {
                run.push_back(0xFF);
            }
            if (index + run.size() > 0x100) {
                error = "Sprites: animations end at index FF";
                return false;
            }
            if (s.anims.size() < index + run.size()) {
                s.anims.resize(index + run.size(), 0xFF);
                owner.resize(s.anims.size(), -1);
            }
            for (size_t i = 0; i < run.size(); i++) {
                if (owner[index + i] >= 0) {
                    error = "Sprites: animation " + Hex(index) + " overlaps animation " + Hex(owner[index + i]);
                    return false;
                }
                owner[index + i] = (int)index;
                s.anims[index + i] = run[i];
            }
        } else if (n == "Frame" || n == "Placement") {
            int number = (int)HexAttr(e, "Number");
            bool frame = n == "Frame";
            if (number > (frame ? 0xFF : 0x0F)) {
                error = "Sprites: " + n + " number too big";
                return false;
            }
            if (e->Attribute("SameAs")) {
                (frame ? s.frameSameAs : s.placeSameAs)[number] = (int)HexAttr(e, "SameAs");
                continue;
            }
            Piece p;
            if (frame) {
                if (!ReadFrame(e, p, error)) {
                    error = "Sprites: frame " + Hex(number) + ": " + error;
                    return false;
                }
                s.frames[number] = p;
                continue;
            }
            p.address = (uint16_t)HexAttr(e, "Address");
            p.size = (uint16_t)e->IntAttribute("Size");
            const char* text = e->GetText();
            std::string t = text ? text : "";
            for (size_t i = 0; i < t.size();) {
                while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t' || t[i] == ',')) {
                    i++;
                }
                size_t j = i;
                while (j < t.size() && !(t[j] == ' ' || t[j] == '\n' || t[j] == '\r' || t[j] == '\t' || t[j] == ',')) {
                    j++;
                }
                if (j > i) {
                    uint8_t b;
                    if (!ParsePlaceByte(t.substr(i, j - i), b)) {
                        error = "Sprites: placement " + Hex(number) + ": bad offset " + t.substr(i, j - i);
                        return false;
                    }
                    p.bytes.push_back(b);
                }
                i = j;
            }
            s.places[number] = p;
        } else {
            error = "Sprites: unknown element " + n;
            return false;
        }
    }
    out.sets[s.Key()] = s;
    return true;
}

bool AssembleSprites(const SpriteAssets& a, Image& image, std::string& error) {
    for (const auto& [key, s] : a.sets) {
        for (int bank : s.banks) {
            if (!AssembleSet(s, bank, image, error)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace Game::Data
