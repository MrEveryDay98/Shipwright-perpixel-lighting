#include "game/data/Data.h"

#include "game/Memory.h"
#include "game/data/Image.h"

#include <cstdio>
#include <algorithm>
#include <cstring>
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

constexpr uint32_t kUsaPrgCrc32 = 0x70080810;
constexpr size_t kPrgSize = 8 * 0x4000;

uint32_t Crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

int HexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

} // namespace

const char* BankFolder(int bank) {
    static const char* const kFolders[8] = { "title", "brinstar", "norfair",  "tourian",
                                             "kraid", "ridley",   "graphics", "engine" };
    return kFolders[bank & 7];
}

bool Extract(const std::vector<uint8_t>& ines, std::map<std::string, std::string>& files, std::string& error) {
    if (ines.size() < 16 || memcmp(ines.data(), "NES\x1A", 4) != 0) {
        error = "Not an iNES ROM (.nes)";
        return false;
    }
    size_t start = 16 + ((ines[6] & 4) ? 512 : 0);
    if (ines.size() < start + kPrgSize || Crc32(&ines[start], kPrgSize) != kUsaPrgCrc32) {
        error = "This is not Metroid (USA) (PRG CRC32 mismatch)";
        return false;
    }
    const uint8_t* prg = &ines[start];
    // Typed resources first; the generic blocks keep whatever they don't cover.
    std::vector<uint8_t> covered(kPrgSize, 0);
    ExtractLevel(prg, files, covered);
    ExtractPpu(prg, files, covered);
    for (const AssetEntry& a : kAssets) {
        size_t base = (size_t)a.bank * 0x4000 + (a.address - BankBase(a.bank));
        size_t i = 0;
        while (i < a.length) {
            if (covered[base + i]) {
                i++;
                continue;
            }
            size_t j = i;
            while (j < a.length && !covered[base + j]) {
                j++;
            }
            Block b;
            b.bank = a.bank;
            b.address = (uint16_t)(a.address + i);
            b.name = a.name;
            if (i != 0) {
                char suffix[16];
                snprintf(suffix, sizeof(suffix), "_%X", (unsigned)i);
                b.name += suffix;
            }
            b.bytes.assign(prg + base + i, prg + base + j);
            files[std::string("metroid/data/") + BankFolder(a.bank) + "/" + b.name] = WriteBlock(b);
            i = j;
        }
    }
    char version[64];
    snprintf(version, sizeof(version), "<Version Version=\"0\" Data=\"%d\"/>\n", kDataVersion);
    files["metroid/version"] = version;
    return true;
}

std::string WriteBlock(const Block& block) {
    std::string s;
    char head[160];
    snprintf(head, sizeof(head), "<DataBlock Version=\"0\" Bank=\"%d\" Address=\"%04X\" Name=\"%s\">", block.bank,
             block.address, block.name.c_str());
    s += head;
    static const char* const kHex = "0123456789ABCDEF";
    for (size_t i = 0; i < block.bytes.size(); i++) {
        s += (i % 16 == 0) ? "\n    " : " ";
        s += kHex[block.bytes[i] >> 4];
        s += kHex[block.bytes[i] & 15];
    }
    s += "\n</DataBlock>\n";
    return s;
}

bool ReadBlock(const tinyxml2::XMLElement* root, Block& out, std::string& error) {
    if (root == nullptr || strcmp(root->Name(), "DataBlock") != 0) {
        error = "not a DataBlock";
        return false;
    }
    out.bank = (uint8_t)root->IntAttribute("Bank");
    const char* address = root->Attribute("Address");
    out.address = address ? (uint16_t)strtoul(address, nullptr, 16) : 0;
    const char* name = root->Attribute("Name");
    out.name = name ? name : "";
    out.bytes.clear();
    const char* text = root->GetText();
    for (const char* p = text ? text : ""; *p;) {
        int hi = HexDigit(p[0]);
        if (hi < 0) {
            p++;
            continue;
        }
        int lo = HexDigit(p[1]);
        if (lo < 0) {
            error = "bad hex in DataBlock " + out.name;
            return false;
        }
        out.bytes.push_back((uint8_t)((hi << 4) | lo));
        p += 2;
    }
    if (out.bank > 7 || out.address < BankBase(out.bank) ||
        out.address + out.bytes.size() > (size_t)BankBase(out.bank) + 0x4000) {
        error = "DataBlock " + out.name + " is outside its bank";
        return false;
    }
    return true;
}

const char* const kResourceTypes[] = {
    "DataBlock", "WorldMap", "Room", "Structure", "Macros", "SpecialItems", nullptr
};

bool Assets::Read(const tinyxml2::XMLElement* root, std::string& error) {
    if (root != nullptr && strcmp(root->Name(), "DataBlock") == 0) {
        Block b;
        if (!ReadBlock(root, b, error)) {
            return false;
        }
        blocks.push_back(std::move(b));
        return true;
    }
    if (root != nullptr && (strcmp(root->Name(), "Palettes") == 0 || strcmp(root->Name(), "Screen") == 0)) {
        return ReadPpuResource(root, ppu, error);
    }
    return ReadLevelResource(root, level, error);
}

void Assets::Merge(const Assets& other) {
    blocks.insert(blocks.end(), other.blocks.begin(), other.blocks.end());
    if (other.level.haveWorld) {
        level.world = other.level.world;
        level.haveWorld = true;
    }
    for (const auto& [bank, rooms] : other.level.rooms) {
        for (const auto& [n, r] : rooms) {
            level.rooms[bank][n] = r;
        }
    }
    for (const auto& [bank, structs] : other.level.structures) {
        for (const auto& [n, st] : structs) {
            level.structures[bank][n] = st;
        }
    }
    for (const auto& [bank, m] : other.level.macros) {
        level.macros[bank] = m;
    }
    for (const auto& [bank, t] : other.level.specialItems) {
        level.specialItems[bank] = t;
    }
    for (const auto& [bank, a] : other.level.areaInfo) {
        level.areaInfo[bank] = a;
    }
    for (const auto& [bank, t] : other.ppu.palettes) {
        ppu.palettes[bank] = t;
    }
    for (const auto& [name, set] : other.ppu.screens) {
        ppu.screens[name] = set;
    }
}

bool Install(const Assets& assets, std::string& error) {
    Image image;
    // Same result whatever order the resources came in: blocks by bank and address.
    std::vector<const Block*> blocks;
    for (const Block& b : assets.blocks) {
        blocks.push_back(&b);
    }
    std::sort(blocks.begin(), blocks.end(), [](const Block* a, const Block* b) {
        return a->bank != b->bank ? a->bank < b->bank : a->address < b->address;
    });
    for (const Block* b : blocks) {
        image.Put(b->bank, b->address, b->bytes);
    }
    if (!AssembleLevel(assets.level, image, error) || !AssemblePpu(assets.ppu, image, error)) {
        return false;
    }
    SetRomImage(std::move(image.Prg()));
    return true;
}

bool InstallFromRom(const std::vector<uint8_t>& ines, std::string& error) {
    std::map<std::string, std::string> files;
    if (!Extract(ines, files, error)) {
        return false;
    }
    Assets assets;
    for (const auto& [path, text] : files) {
        if (path == "metroid/version") {
            continue;
        }
        tinyxml2::XMLDocument doc;
        if (doc.Parse(text.c_str(), text.size()) != tinyxml2::XML_SUCCESS) {
            error = "XML error in " + path;
            return false;
        }
        if (!assets.Read(doc.FirstChildElement(), error)) {
            error = path + ": " + error;
            return false;
        }
    }
    return Install(assets, error);
}

} // namespace Game::Data
