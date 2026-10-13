#include "game/data/Data.h"

#include "game/Memory.h"

#include <cstdio>
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

uint16_t BankBase(int bank) {
    return bank == 7 ? 0xC000 : 0x8000;
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
    for (const AssetEntry& a : kAssets) {
        Block b;
        b.bank = a.bank;
        b.address = a.address;
        b.name = a.name;
        size_t off = (size_t)a.bank * 0x4000 + (a.address - BankBase(a.bank));
        b.bytes.assign(prg + off, prg + off + a.length);
        files[std::string("metroid/data/") + BankFolder(a.bank) + "/" + a.name] = WriteBlock(b);
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

void Install(const std::vector<Block>& blocks) {
    std::vector<uint8_t> prg(kPrgSize, 0);
    for (const Block& b : blocks) {
        size_t off = (size_t)b.bank * 0x4000 + (b.address - BankBase(b.bank));
        std::copy(b.bytes.begin(), b.bytes.end(), prg.begin() + off);
    }
    SetRomImage(std::move(prg));
}

bool InstallFromRom(const std::vector<uint8_t>& ines, std::string& error) {
    std::map<std::string, std::string> files;
    if (!Extract(ines, files, error)) {
        return false;
    }
    std::vector<Block> blocks;
    for (const auto& [path, text] : files) {
        if (path.rfind("metroid/data/", 0) != 0) {
            continue;
        }
        tinyxml2::XMLDocument doc;
        if (doc.Parse(text.c_str(), text.size()) != tinyxml2::XML_SUCCESS) {
            error = "XML error in " + path;
            return false;
        }
        Block b;
        if (!ReadBlock(doc.FirstChildElement(), b, error)) {
            return false;
        }
        blocks.push_back(std::move(b));
    }
    Install(blocks);
    return true;
}

} // namespace Game::Data
