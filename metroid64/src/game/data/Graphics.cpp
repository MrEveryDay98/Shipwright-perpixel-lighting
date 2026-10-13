#include "game/data/Graphics.h"

#include "game/data/Png.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <tinyxml2.h>

namespace Game::Data {

namespace {

// GFXInfo in bank 7: 7-byte entries [bank][source address][PPU address][length], all words little endian.
constexpr uint16_t kGFXInfo = 0xC6E0;
constexpr int kEntrySize = 7;

// Names of the blocks in the area banks, which the disassembly only describes in comments. Bank 6's
// blocks have labels (the asset map's names).
struct BlockName {
    int bank;
    uint16_t address;
    const char* name;
};
constexpr BlockName kNames[] = {
    { 1, 0x8D60, "GFXTheEndFont" },     { 1, 0x9160, "GFXBrinstarEnemies" }, { 2, 0x8D60, "GFXNorfairEnemies" },
    { 2, 0x9160, "GFXTourianEnemies" }, { 3, 0x8D60, "GFXKraidEnemies" },    { 3, 0x9160, "GFXRidleyEnemies" },
    { 4, 0x8D60, "GFXSamusEnd" },       { 4, 0x9360, "GFXMiscBank4" },       { 5, 0x8D60, "GFXMiscBank5" },
    { 5, 0x91B0, "GFXJapaneseFont" },
};

std::string BlockNameOf(int bank, uint16_t address) {
    for (const BlockName& n : kNames) {
        if (n.bank == bank && n.address == address) {
            return n.name;
        }
    }
    if (const char* label = AssetBlockName(bank, address)) {
        return label;
    }
    char name[32];
    snprintf(name, sizeof(name), "GFX_%d_%04X", bank, address);
    return name;
}

struct Entry {
    int index;
    int bank;
    uint16_t source;
    uint16_t dest;
    uint16_t length;
};

template <typename Get> std::vector<Entry> ReadEntries(Get get) {
    std::vector<Entry> entries;
    int count = (int)(AssetBlockLength(7, kGFXInfo) / kEntrySize);
    for (int i = 0; i < count; i++) {
        uint16_t a = (uint16_t)(kGFXInfo + i * kEntrySize);
        auto word = [&](int o) { return (uint16_t)(get((uint16_t)(a + o)) | (get((uint16_t)(a + o + 1)) << 8)); };
        entries.push_back({ i, get(a), word(1), word(3), word(5) });
    }
    return entries;
}

// The blocks the entries load: (bank, source) -> the longest length any entry loads from there.
std::map<std::pair<int, int>, size_t> Extents(const std::vector<Entry>& entries) {
    std::map<std::pair<int, int>, size_t> extents;
    for (const Entry& e : entries) {
        size_t& ext = extents[{ e.bank, e.source }];
        ext = std::max<size_t>(ext, e.length);
    }
    return extents;
}

} // namespace

std::string GraphicsToPng(const std::vector<uint8_t>& chr) {
    int tiles = (int)(chr.size() / 16);
    int width = kTilesPerRow * 8;
    int height = std::max(1, (tiles + kTilesPerRow - 1) / kTilesPerRow) * 8;
    std::vector<uint8_t> pixels((size_t)width * height, 0);
    for (int t = 0; t < tiles; t++) {
        int tx = (t % kTilesPerRow) * 8, ty = (t / kTilesPerRow) * 8;
        for (int y = 0; y < 8; y++) {
            uint8_t lo = chr[t * 16 + y], hi = chr[t * 16 + 8 + y];
            for (int x = 0; x < 8; x++) {
                pixels[(size_t)(ty + y) * width + tx + x] =
                    (uint8_t)(((lo >> (7 - x)) & 1) | (((hi >> (7 - x)) & 1) << 1));
            }
        }
    }
    return WritePng(width, height, pixels);
}

bool PngToGraphics(const std::string& png, int tiles, std::vector<uint8_t>& chr, std::string& error) {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels;
    if (!ReadPng(png, width, height, pixels, error)) {
        return false;
    }
    int rows = (tiles + kTilesPerRow - 1) / kTilesPerRow;
    if (width != kTilesPerRow * 8 || height < rows * 8) {
        error = "the image must be " + std::to_string(kTilesPerRow * 8) + " pixels wide and at least " +
                std::to_string(rows * 8) + " high for " + std::to_string(tiles) + " tiles";
        return false;
    }
    chr.assign((size_t)tiles * 16, 0);
    for (int t = 0; t < tiles; t++) {
        int tx = (t % kTilesPerRow) * 8, ty = (t / kTilesPerRow) * 8;
        for (int y = 0; y < 8; y++) {
            uint8_t lo = 0, hi = 0;
            for (int x = 0; x < 8; x++) {
                uint8_t p = pixels[(size_t)(ty + y) * width + tx + x];
                lo |= (uint8_t)((p & 1) << (7 - x));
                hi |= (uint8_t)(((p >> 1) & 1) << (7 - x));
            }
            chr[t * 16 + y] = lo;
            chr[t * 16 + 8 + y] = hi;
        }
    }
    return true;
}

void ExtractGraphics(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered) {
    auto offset = [](int bank, uint16_t a) { return (size_t)bank * 0x4000 + (a - BankBase(bank)); };
    std::vector<Entry> entries = ReadEntries([&](uint16_t a) { return prg[offset(7, a)]; });
    for (const auto& [where, extent] : Extents(entries)) {
        int bank = where.first;
        uint16_t address = (uint16_t)where.second;
        if (bank > 6 || extent == 0 || extent % 16 != 0 || address < 0x8000 ||
            address + extent > (size_t)BankBase(bank) + 0x4000) {
            continue; // not something this resource can hold; stays in the data blocks
        }
        Graphics g;
        g.bank = bank;
        g.address = address;
        g.name = BlockNameOf(bank, address);
        size_t off = offset(bank, address);
        g.chr.assign(prg + off, prg + off + extent);
        std::fill(covered.begin() + off, covered.begin() + off + extent, 1);
        std::string path = "metroid/graphics/" + g.name;
        files[path] = WriteGraphics(g, path + ".png");
        files[path + ".png"] = GraphicsToPng(g.chr);
    }
}

std::string WriteGraphics(const Graphics& g, const std::string& imagePath) {
    char s[320];
    snprintf(s, sizeof(s),
             "<Graphics Version=\"0\" Bank=\"%d\" Address=\"%04X\" Name=\"%s\" Tiles=\"%d\" Image=\"%s\"/>\n", g.bank,
             g.address, g.name.c_str(), (int)(g.chr.size() / 16), imagePath.c_str());
    return s;
}

bool ReadGraphics(const tinyxml2::XMLElement* root, const FileLoader& load, Graphics& out, std::string& error) {
    out.bank = root->IntAttribute("Bank", 6);
    const char* address = root->Attribute("Address");
    out.address = address ? (uint16_t)strtoul(address, nullptr, 16) : 0;
    const char* name = root->Attribute("Name");
    out.name = name ? name : "";
    int tiles = root->IntAttribute("Tiles");
    const char* image = root->Attribute("Image");
    if (out.bank < 0 || out.bank > 6 || out.address < 0x8000 || tiles < 1 || tiles > 0x200 || image == nullptr) {
        error = "Graphics " + out.name + " needs Bank (0-6), Address, Tiles (1-512) and Image";
        return false;
    }
    std::string png;
    if (!load || !load(image, png)) {
        error = std::string("Graphics ") + out.name + ": no image " + image;
        return false;
    }
    if (!PngToGraphics(png, tiles, out.chr, error)) {
        error = std::string("Graphics ") + out.name + " (" + image + "): " + error;
        return false;
    }
    return true;
}

bool AssembleGraphics(const std::map<std::pair<int, int>, Graphics>& graphics, Image& image, std::string& error) {
    // The entries as the data blocks put them (before any is moved).
    std::vector<Entry> entries = ReadEntries([&](uint16_t a) { return image.Get(7, a); });
    std::map<std::pair<int, int>, size_t> extents = Extents(entries);
    for (const auto& [where, g] : graphics) {
        auto ext = extents.find(where);
        if (ext == extents.end()) {
            error = "Graphics " + g.name + ": no GFXInfo entry loads graphics from there";
            return false;
        }
        size_t extent = ext->second, size = g.chr.size();
        // Entries that loaded the whole block load all of it; the others (a part) at most all of it.
        auto lengthOf = [&](const Entry& e) { return (e.length == extent) ? size : std::min<size_t>(e.length, size); };
        for (const Entry& e : entries) {
            if (e.bank == where.first && e.source == where.second && e.dest + lengthOf(e) > 0x2000) {
                error = "Graphics " + g.name + ": " + std::to_string(size / 16) +
                        " tiles run past the end of the pattern tables";
                return false;
            }
        }
        int bank = g.bank;
        uint16_t address = g.address;
        if (size > extent) {
            // Bigger than the original: free space, in its bank if it has room, else in any bank the
            // loaders can switch in (GFXInfo entries carry the bank).
            address = image.Allocate(bank, size);
            for (int b = 0; address == 0 && b <= 6; b++) {
                bank = b;
                address = image.Allocate(bank, size);
            }
            if (address == 0) {
                error = "Graphics " + g.name + ": no free space for " + std::to_string(size / 16) + " tiles";
                return false;
            }
        }
        image.Put(bank, address, g.chr);
        for (const Entry& e : entries) {
            if (e.bank != where.first || e.source != where.second) {
                continue;
            }
            size_t length = lengthOf(e);
            uint16_t a = (uint16_t)(kGFXInfo + e.index * kEntrySize);
            if (bank != e.bank || address != e.source || length != e.length) {
                image.Put(7, a, { (uint8_t)bank, (uint8_t)address, (uint8_t)(address >> 8) });
                image.Put16(7, (uint16_t)(a + 5), (uint16_t)length);
            }
        }
    }
    return true;
}

} // namespace Game::Data
