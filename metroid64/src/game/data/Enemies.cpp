#include "game/data/Enemies.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <tinyxml2.h>

namespace Game::Data {

using namespace Bytes;

namespace {

// The per-type tables, at the same addresses in every area bank (bank 7 and the area code read them there).
constexpr uint16_t kExplodeAnimTbl = 0x960B; // [type * 2 + direction]
constexpr uint16_t kHitPointTbl = 0x962B;    // EnemyHitPointTbl
constexpr uint16_t kRunAnimTbl = 0x963B;     // [type * 2 + direction]
constexpr uint16_t kTurnAnimTbl = 0x965B;    // [type * 2 + direction]
constexpr uint16_t kTurnRewindTbl = 0x967B;
constexpr uint16_t kFlagsTbl = 0x968B;
constexpr uint16_t kTimerTbl = 0x969B;
constexpr uint16_t kDistanceTbl = 0x96AB;
constexpr uint16_t kDelayTbl = 0x96BB; // EnemyInitDelayTbl
constexpr uint16_t kMoveSetTbl = 0x96CB;
constexpr uint16_t kMovePtrTbl = 0x96DB; // [move * 2 + direction] words
constexpr uint16_t kAccelYTbl = 0x972B;  // [move]
constexpr uint16_t kAccelXTbl = 0x973F;
constexpr uint16_t kSpeedYTbl = 0x9753;
constexpr uint16_t kSpeedXTbl = 0x9767;
constexpr uint16_t kFlags2Tbl = 0x977B;
constexpr uint16_t kProjAnimTbl = 0x978B; // [projectile * 2 + direction]
constexpr uint16_t kProjXTbl = 0x979B;    // [projectile * 2 + direction]
constexpr uint16_t kProjYTbl = 0x97A3;    // [projectile]
constexpr uint16_t kFireballPathTbl = 0x97A7;
constexpr uint16_t kMoveSets = 0x97D1; // the move set offsets are from here (one byte)
constexpr int kTypes = 16;
constexpr int kMoves = 20;
constexpr int kProjectiles = 4;
constexpr int kPaths = 4;

// Movement script bytes: a count below $F0 and a speed byte, or one of these.
constexpr uint8_t kStop = 0xFA;
constexpr uint8_t kHold = 0xFB;
constexpr uint8_t kRepeatWhileFree = 0xFC;
constexpr uint8_t kResetCount = 0xFD;
constexpr uint8_t kRepeatWhileFreeThenStop = 0xFE;
constexpr uint8_t kRestart = 0xFF;

// A script (as EnemyVertSpeed8244 reads it) or, `path`, a fireball path (EnemyObjectPath: pairs until $FF).
std::vector<uint8_t> DecodeScript(const Prg& prg, int bank, uint16_t address, bool path) {
    std::vector<uint8_t> out;
    uint16_t a = address;
    while (out.size() < 0x100) {
        uint8_t c = prg.Get(bank, a++);
        out.push_back(c);
        if (path ? c != kRestart : c < 0xF0) {
            out.push_back(prg.Get(bank, a++));
            continue;
        }
        if (!path && (c == kRepeatWhileFree || c == kResetCount)) {
            continue;
        }
        break;
    }
    return out;
}

// Speed byte of a step: y in bits 4-6 (bit 7: up), x in bits 0-2 (bit 3: left), in pixels per frame.
std::string SpeedPart(uint8_t magnitude, bool negative) {
    return (negative ? "-" : "") + std::to_string(magnitude);
}

bool ParseSpeedPart(const char* v, uint8_t& magnitude, bool& negative) {
    if (v == nullptr) {
        magnitude = 0;
        negative = false;
        return true;
    }
    negative = *v == '-';
    if (*v == '-' || *v == '+') {
        v++;
    }
    char* end = nullptr;
    long n = strtol(v, &end, 10);
    if (end == v || *end != 0 || n < 0 || n > 7) {
        return false;
    }
    magnitude = (uint8_t)n;
    return true;
}

std::string WriteScript(int number, const Piece& p, bool path) {
    std::string s = "    <Script Number=\"" + Hex(number) + "\" Address=\"" + Hex(p.address, 4) + "\" Size=\"" +
                    std::to_string(p.size) + "\"" + (path ? " Path=\"1\"" : "") + ">\n";
    const auto& b = p.bytes;
    for (size_t i = 0; i < b.size(); i++) {
        uint8_t c = b[i];
        if ((path ? c != kRestart : c < 0xF0) && i + 1 < b.size()) {
            uint8_t v = b[++i];
            s += "        <Step Count=\"" + Hex(c) + "\" Y=\"" + SpeedPart((v >> 4) & 7, v & 0x80) + "\" X=\"" +
                 SpeedPart(v & 7, v & 0x08) + "\"/>\n";
            continue;
        }
        switch (c) {
            case kStop:
                s += "        <Stop/>\n";
                break;
            case kHold:
                s += "        <Hold/>\n";
                break;
            case kRepeatWhileFree:
                s += "        <RepeatWhileFree/>\n";
                break;
            case kResetCount:
                s += "        <ResetCount/>\n";
                break;
            case kRepeatWhileFreeThenStop:
                s += "        <RepeatWhileFreeThenStop/>\n";
                break;
            case kRestart:
                s += "        <Restart/>\n";
                break;
            default:
                s += "        <Restart Code=\"" + Hex(c) + "\"/>\n";
                break;
        }
    }
    return s + "    </Script>\n";
}

bool ReadScript(const tinyxml2::XMLElement* e, Piece& p, std::string& error) {
    p.address = (uint16_t)HexAttr(e, "Address");
    p.size = (uint16_t)e->IntAttribute("Size");
    for (auto c = e->FirstChildElement(); c; c = c->NextSiblingElement()) {
        std::string n = c->Name();
        if (n == "Step") {
            uint8_t y, x;
            bool up, left;
            if (!ParseSpeedPart(c->Attribute("Y"), y, up) || !ParseSpeedPart(c->Attribute("X"), x, left)) {
                error = "a Step's Y and X are -7 to 7";
                return false;
            }
            p.bytes.push_back((uint8_t)HexAttr(c, "Count"));
            p.bytes.push_back((uint8_t)((up ? 0x80 : 0) | (y << 4) | (left ? 0x08 : 0) | x));
        } else if (n == "Stop") {
            p.bytes.push_back(kStop);
        } else if (n == "Hold") {
            p.bytes.push_back(kHold);
        } else if (n == "RepeatWhileFree") {
            p.bytes.push_back(kRepeatWhileFree);
        } else if (n == "ResetCount") {
            p.bytes.push_back(kResetCount);
        } else if (n == "RepeatWhileFreeThenStop") {
            p.bytes.push_back(kRepeatWhileFreeThenStop);
        } else if (n == "Restart") {
            unsigned code = HexAttr(c, "Code", kRestart);
            if (code < 0xF0 || code == kStop || code == kHold || code == kRepeatWhileFree || code == kResetCount ||
                code == kRepeatWhileFreeThenStop) {
                error = "a Restart's Code is F0-F9 or FF";
                return false;
            }
            p.bytes.push_back((uint8_t)code);
        } else {
            error = "unknown script element " + n;
            return false;
        }
    }
    if (p.bytes.empty()) {
        error = "an empty script";
        return false;
    }
    return true;
}

std::string Pair(const uint8_t v[2], bool signedValues) {
    if (signedValues) {
        return Signed(v[0]) + " " + Signed(v[1]);
    }
    return Hex(v[0]) + " " + Hex(v[1]);
}

bool ReadPair(const tinyxml2::XMLElement* e, const char* name, uint8_t out[2], bool signedValues) {
    const char* v = e->Attribute(name);
    if (v == nullptr) {
        out[0] = out[1] = 0;
        return true;
    }
    char* end = nullptr;
    for (int i = 0; i < 2; i++) {
        long n = strtol(v, &end, signedValues ? 10 : 16);
        if (end == v || n < (signedValues ? -128 : 0) || n > 255) {
            return false;
        }
        out[i] = (uint8_t)n;
        v = end;
    }
    return true;
}

bool ReadSigned(const tinyxml2::XMLElement* e, const char* name, uint8_t& out) {
    const char* v = e->Attribute(name);
    if (v == nullptr) {
        out = 0;
        return true;
    }
    char* end = nullptr;
    long n = strtol(v, &end, 10);
    if (end == v || *end != 0 || n < -128 || n > 255) {
        return false;
    }
    out = (uint8_t)n;
    return true;
}

} // namespace

void ExtractEnemies(const uint8_t* bytes, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered) {
    Prg prg{ bytes };
    for (int bank = 1; bank <= 5; bank++) {
        EnemyArea e;
        e.bank = bank;
        auto get = [&](uint16_t a) { return prg.Get(bank, a); };
        for (int t = 0; t < kTypes; t++) {
            EnemyType& ty = e.types[t];
            ty.hitPoints = get((uint16_t)(kHitPointTbl + t));
            ty.delay = get((uint16_t)(kDelayTbl + t));
            ty.timer = get((uint16_t)(kTimerTbl + t));
            ty.flags = get((uint16_t)(kFlagsTbl + t));
            ty.turnAnimRewind = get((uint16_t)(kTurnRewindTbl + t));
            ty.distance = get((uint16_t)(kDistanceTbl + t));
            ty.flags2 = get((uint16_t)(kFlags2Tbl + t));
            ty.moveSet = get((uint16_t)(kMoveSetTbl + t));
            for (int d = 0; d < 2; d++) {
                ty.explodeAnim[d] = get((uint16_t)(kExplodeAnimTbl + 2 * t + d));
                ty.runAnim[d] = get((uint16_t)(kRunAnimTbl + 2 * t + d));
                ty.turnAnim[d] = get((uint16_t)(kTurnAnimTbl + 2 * t + d));
            }
        }
        // Scripts and paths, numbered by address.
        std::map<uint16_t, bool> targets; // address -> read as a movement script
        for (int m = 0; m < kMoves; m++) {
            for (int d = 0; d < 2; d++) {
                uint16_t p = prg.Get16(bank, (uint16_t)(kMovePtrTbl + 4 * m + 2 * d));
                if (p != 0) {
                    targets[p] = true;
                }
            }
        }
        for (int i = 0; i < kPaths; i++) {
            uint16_t p = prg.Get16(bank, (uint16_t)(kFireballPathTbl + 2 * i));
            if (p != 0) {
                targets.emplace(p, false);
            }
        }
        std::map<uint16_t, int> number;
        for (const auto& [address, script] : targets) {
            Piece p;
            p.address = address;
            p.bytes = DecodeScript(prg, bank, address, !script);
            p.size = (uint16_t)p.bytes.size();
            int n = (int)number.size();
            number[address] = n;
            e.scripts[n] = p;
            Cover(covered, bank, address, p.size);
        }
        for (int m = 0; m < kMoves; m++) {
            EnemyMove& mv = e.moves[m];
            for (int d = 0; d < 2; d++) {
                uint16_t p = prg.Get16(bank, (uint16_t)(kMovePtrTbl + 4 * m + 2 * d));
                mv.script[d] = p ? number[p] : -1;
            }
            mv.accelY = get((uint16_t)(kAccelYTbl + m));
            mv.accelX = get((uint16_t)(kAccelXTbl + m));
            mv.speedY = get((uint16_t)(kSpeedYTbl + m));
            mv.speedX = get((uint16_t)(kSpeedXTbl + m));
        }
        for (int i = 0; i < kProjectiles; i++) {
            EnemyProjectile& pr = e.projectiles[i];
            for (int d = 0; d < 2; d++) {
                pr.anim[d] = get((uint16_t)(kProjAnimTbl + 2 * i + d));
                pr.offsetX[d] = get((uint16_t)(kProjXTbl + 2 * i + d));
            }
            pr.offsetY = get((uint16_t)(kProjYTbl + i));
        }
        for (int i = 0; i < kPaths; i++) {
            uint16_t p = prg.Get16(bank, (uint16_t)(kFireballPathTbl + 2 * i));
            e.fireballPaths[i] = p ? number[p] : -1;
        }
        // Move sets: from $97D1 up to the first script. A set is a mask (entries - 1) and the move numbers.
        uint32_t end = kMoveSets + 0x100;
        for (const auto& [address, script] : targets) {
            if (address >= kMoveSets) {
                end = std::min<uint32_t>(end, address);
            }
        }
        e.moveSetArea = (int)(end - kMoveSets);
        for (int t = 0; t < kTypes; t++) {
            EnemyType& ty = e.types[t];
            uint16_t a = (uint16_t)(kMoveSets + ty.moveSet);
            if (a >= end) {
                continue;
            }
            int count = get(a) + 1;
            if ((count & (count - 1)) != 0 || a + 1 + (uint32_t)count > end) {
                continue; // not a move set (the type doesn't pick moves this way)
            }
            ty.haveMoves = true;
            for (int i = 0; i < count; i++) {
                ty.moves.push_back(get((uint16_t)(a + 1 + i)));
            }
            Cover(covered, bank, a, 1 + count);
        }
        Cover(covered, bank, kExplodeAnimTbl, kProjAnimTbl + 2 * kProjectiles - kExplodeAnimTbl);
        Cover(covered, bank, kProjXTbl, kFireballPathTbl + 2 * kPaths - kProjXTbl);
        files[std::string("metroid/enemies/") + AreaName(bank)] = WriteEnemies(e);
    }
}

std::string WriteEnemies(const EnemyArea& e) {
    std::string s = "<Enemies Version=\"0\" Area=\"" + std::string(AreaName(e.bank)) + "\" MoveSetArea=\"" +
                    std::to_string(e.moveSetArea) + "\">\n";
    s += "    <!-- Numbers are hex, except signed values (Accel, Speed, Offset, Step Y/X). Pairs are by direction\n"
         "         (0, then 1). Animations are indexes into metroid/sprites/<area>/enemies. -->\n";
    for (int t = 0; t < kTypes; t++) {
        const EnemyType& ty = e.types[t];
        s += "    <Enemy Type=\"" + Hex(t) + "\" HitPoints=\"" + Hex(ty.hitPoints) + "\" Delay=\"" + Hex(ty.delay) +
             "\" Timer=\"" + Hex(ty.timer) + "\" Distance=\"" + Hex(ty.distance) + "\" Flags=\"" + Hex(ty.flags) +
             "\" Flags2=\"" + Hex(ty.flags2) + "\"\n           ExplodeAnim=\"" + Pair(ty.explodeAnim, false) +
             "\" RunAnim=\"" + Pair(ty.runAnim, false) + "\" TurnAnim=\"" + Pair(ty.turnAnim, false) +
             "\" TurnAnimRewind=\"" + Hex(ty.turnAnimRewind) + "\" MoveSet=\"" + Hex(ty.moveSet) + "\"";
        if (ty.haveMoves) {
            s += " Moves=\"" + HexList(ty.moves) + "\"";
        }
        s += "/>\n";
    }
    for (int m = 0; m < kMoves; m++) {
        const EnemyMove& mv = e.moves[m];
        s += "    <Move Number=\"" + Hex(m) + "\"";
        for (int d = 0; d < 2; d++) {
            if (mv.script[d] >= 0) {
                s += " Script" + std::to_string(d) + "=\"" + Hex(mv.script[d]) + "\"";
            }
        }
        s += " AccelY=\"" + Signed(mv.accelY) + "\" AccelX=\"" + Signed(mv.accelX) + "\" SpeedY=\"" +
             Signed(mv.speedY) + "\" SpeedX=\"" + Signed(mv.speedX) + "\"/>\n";
    }
    for (int i = 0; i < kProjectiles; i++) {
        const EnemyProjectile& pr = e.projectiles[i];
        s += "    <Projectile Type=\"" + Hex(i) + "\" Anim=\"" + Pair(pr.anim, false) + "\" OffsetX=\"" +
             Pair(pr.offsetX, true) + "\" OffsetY=\"" + Signed(pr.offsetY) + "\"/>\n";
    }
    for (int i = 0; i < kPaths; i++) {
        if (e.fireballPaths[i] >= 0) {
            s += "    <FireballPath Number=\"" + Hex(i) + "\" Script=\"" + Hex(e.fireballPaths[i]) + "\"/>\n";
        }
    }
    for (const auto& [n, p] : e.scripts) {
        bool script = false;
        for (const EnemyMove& mv : e.moves) {
            script = script || mv.script[0] == n || mv.script[1] == n;
        }
        s += WriteScript(n, p, !script);
    }
    return s + "</Enemies>\n";
}

bool ReadEnemiesResource(const tinyxml2::XMLElement* root, EnemyAssets& out, std::string& error) {
    if (root == nullptr || strcmp(root->Name(), "Enemies") != 0) {
        error = "not an Enemies resource";
        return false;
    }
    EnemyArea e;
    e.bank = AreaBank(root->Attribute("Area"));
    if (e.bank == 0) {
        error = "Enemies: unknown Area";
        return false;
    }
    e.moveSetArea = root->IntAttribute("MoveSetArea");
    if (e.moveSetArea < 0 || e.moveSetArea > 0x100) {
        error = "Enemies: MoveSetArea is 0-256";
        return false;
    }
    for (auto c = root->FirstChildElement(); c; c = c->NextSiblingElement()) {
        std::string n = c->Name();
        if (n == "Enemy") {
            unsigned t = HexAttr(c, "Type");
            if (t >= kTypes) {
                error = "Enemies: Type is 00-0F";
                return false;
            }
            EnemyType& ty = e.types[t];
            ty.hitPoints = (uint8_t)HexAttr(c, "HitPoints");
            ty.delay = (uint8_t)HexAttr(c, "Delay");
            ty.timer = (uint8_t)HexAttr(c, "Timer");
            ty.distance = (uint8_t)HexAttr(c, "Distance");
            ty.flags = (uint8_t)HexAttr(c, "Flags");
            ty.flags2 = (uint8_t)HexAttr(c, "Flags2");
            ty.turnAnimRewind = (uint8_t)HexAttr(c, "TurnAnimRewind");
            ty.moveSet = (uint8_t)HexAttr(c, "MoveSet");
            if (!ReadPair(c, "ExplodeAnim", ty.explodeAnim, false) || !ReadPair(c, "RunAnim", ty.runAnim, false) ||
                !ReadPair(c, "TurnAnim", ty.turnAnim, false)) {
                error = "Enemies: type " + Hex(t) + ": animations are two hex numbers";
                return false;
            }
            ty.haveMoves = c->Attribute("Moves") != nullptr;
            ty.moves = HexBytes(c->Attribute("Moves"));
            size_t count = ty.moves.size();
            if (ty.haveMoves && (count == 0 || count > 0x80 || (count & (count - 1)) != 0)) {
                error = "Enemies: type " + Hex(t) + ": a move set has 1, 2, 4, 8... moves";
                return false;
            }
        } else if (n == "Move") {
            unsigned m = HexAttr(c, "Number");
            if (m >= kMoves) {
                error = "Enemies: moves are 00-13";
                return false;
            }
            EnemyMove& mv = e.moves[m];
            mv.script[0] = c->Attribute("Script0") ? (int)HexAttr(c, "Script0") : -1;
            mv.script[1] = c->Attribute("Script1") ? (int)HexAttr(c, "Script1") : -1;
            if (!ReadSigned(c, "AccelY", mv.accelY) || !ReadSigned(c, "AccelX", mv.accelX) ||
                !ReadSigned(c, "SpeedY", mv.speedY) || !ReadSigned(c, "SpeedX", mv.speedX)) {
                error = "Enemies: move " + Hex(m) + ": accelerations and speeds are -128 to 127";
                return false;
            }
        } else if (n == "Projectile") {
            unsigned i = HexAttr(c, "Type");
            if (i >= kProjectiles) {
                error = "Enemies: projectile types are 0-3";
                return false;
            }
            EnemyProjectile& pr = e.projectiles[i];
            if (!ReadPair(c, "Anim", pr.anim, false) || !ReadPair(c, "OffsetX", pr.offsetX, true) ||
                !ReadSigned(c, "OffsetY", pr.offsetY)) {
                error = "Enemies: projectile " + Hex(i) + " is malformed";
                return false;
            }
        } else if (n == "FireballPath") {
            unsigned i = HexAttr(c, "Number");
            if (i >= kPaths) {
                error = "Enemies: fireball paths are 0-3";
                return false;
            }
            e.fireballPaths[i] = (int)HexAttr(c, "Script");
        } else if (n == "Script") {
            Piece p;
            if (!ReadScript(c, p, error)) {
                error = "Enemies: script " + Hex(HexAttr(c, "Number")) + ": " + error;
                return false;
            }
            e.scripts[(int)HexAttr(c, "Number")] = p;
        } else {
            error = "Enemies: unknown element " + n;
            return false;
        }
    }
    auto known = [&](int s) { return s < 0 || e.scripts.count(s) != 0; };
    for (int m = 0; m < kMoves; m++) {
        if (!known(e.moves[m].script[0]) || !known(e.moves[m].script[1])) {
            error = "Enemies: move " + Hex(m) + " uses a script that isn't there";
            return false;
        }
    }
    for (int i = 0; i < kPaths; i++) {
        if (!known(e.fireballPaths[i])) {
            error = "Enemies: fireball path " + Hex(i) + " uses a script that isn't there";
            return false;
        }
    }
    out.areas[e.bank] = e;
    return true;
}

bool AssembleEnemies(const EnemyAssets& a, Image& image, std::string& error) {
    for (const auto& [bank, e] : a.areas) {
        const std::string area = AreaName(bank);
        // Move sets first: they must be within the 256 bytes from $97D1 the offsets reach. In the area they
        // had, else (bigger sets) over the scripts that follow it, which then move.
        uint32_t areaEnd = kMoveSets + (uint32_t)e.moveSetArea;
        std::vector<std::pair<uint16_t, uint16_t>> extents; // the scripts' original places
        for (const auto& [n, p] : e.scripts) {
            if (p.address) {
                extents.emplace_back(p.address, p.size);
            }
        }
        std::sort(extents.begin(), extents.end());
        uint32_t growEnd = areaEnd;
        for (const auto& [address, size] : extents) {
            if (address <= growEnd) {
                growEnd = std::max<uint32_t>(growEnd, address + (uint32_t)size);
            }
        }
        growEnd = std::min<uint32_t>(growEnd, kMoveSets + 0x100);
        std::vector<Piece> sets(kTypes);
        std::vector<const Piece*> pieces;
        std::vector<int> types;
        for (int t = 0; t < kTypes; t++) {
            const EnemyType& ty = e.types[t];
            if (!ty.haveMoves) {
                continue;
            }
            Piece& p = sets[t];
            p.address = (uint16_t)(kMoveSets + ty.moveSet);
            p.size = p.address < areaEnd ? (uint16_t)(areaEnd - p.address) : 0;
            p.bytes.push_back((uint8_t)(ty.moves.size() - 1));
            p.bytes.insert(p.bytes.end(), ty.moves.begin(), ty.moves.end());
            pieces.push_back(&p);
            types.push_back(t);
        }
        std::vector<bool> used(growEnd - kMoveSets, false);
        auto mark = [&](uint16_t address, size_t length) {
            for (size_t i = 0; i < length; i++) {
                size_t k = address + i - kMoveSets;
                if (address >= kMoveSets && k < used.size()) {
                    used[k] = true;
                }
            }
        };
        uint32_t setsEnd = areaEnd; // how far the sets reach
        auto allocateSet = [&](size_t length) -> uint16_t {
            for (size_t limit : { (size_t)e.moveSetArea, used.size() }) {
                size_t run = 0;
                for (size_t k = 0; k < limit; k++) {
                    run = used[k] ? 0 : run + 1;
                    if (run == length) {
                        setsEnd = std::max<uint32_t>(setsEnd, kMoveSets + (uint32_t)k + 1);
                        return (uint16_t)(kMoveSets + k + 1 - length);
                    }
                }
            }
            return 0;
        };
        std::vector<uint16_t> at;
        if (!PlacePieces(image, bank, pieces, at, allocateSet, "the " + area + " move sets", error, mark)) {
            return false;
        }
        std::vector<uint16_t> setAt = at;
        // Scripts and paths: where they were (unless the sets took that place), or in free space.
        std::vector<Piece> scripts;
        std::vector<int> numbers;
        for (const auto& [n, p] : e.scripts) {
            scripts.push_back(p);
            if (p.address < setsEnd && p.address + (uint32_t)p.size > areaEnd) {
                scripts.back().address = 0;
            }
            numbers.push_back(n);
        }
        pieces.clear();
        for (const Piece& p : scripts) {
            pieces.push_back(&p);
        }
        auto allocate = [&](size_t length) { return image.Allocate(bank, length); };
        if (!PlacePieces(image, bank, pieces, at, allocate, "the " + area + " enemy scripts", error)) {
            return false;
        }
        std::map<int, uint16_t> scriptAt;
        for (size_t i = 0; i < numbers.size(); i++) {
            scriptAt[numbers[i]] = at[i];
        }
        auto address = [&](int script) { return script < 0 ? (uint16_t)0 : scriptAt[script]; };
        uint8_t moveSet[kTypes];
        for (int t = 0; t < kTypes; t++) {
            moveSet[t] = e.types[t].moveSet;
        }
        for (size_t i = 0; i < types.size(); i++) {
            moveSet[types[i]] = (uint8_t)(setAt[i] - kMoveSets);
        }
        // The tables.
        auto table16 = [&](uint16_t base, auto field) {
            std::vector<uint8_t> bytes;
            for (int t = 0; t < kTypes; t++) {
                bytes.push_back(field(e.types[t], t));
            }
            image.Put(bank, base, bytes);
        };
        auto table32 = [&](uint16_t base, auto field) {
            std::vector<uint8_t> bytes;
            for (int t = 0; t < kTypes; t++) {
                bytes.push_back(field(e.types[t])[0]);
                bytes.push_back(field(e.types[t])[1]);
            }
            image.Put(bank, base, bytes);
        };
        table32(kExplodeAnimTbl, [](const EnemyType& t) { return t.explodeAnim; });
        table16(kHitPointTbl, [](const EnemyType& t, int) { return t.hitPoints; });
        table32(kRunAnimTbl, [](const EnemyType& t) { return t.runAnim; });
        table32(kTurnAnimTbl, [](const EnemyType& t) { return t.turnAnim; });
        table16(kTurnRewindTbl, [](const EnemyType& t, int) { return t.turnAnimRewind; });
        table16(kFlagsTbl, [](const EnemyType& t, int) { return t.flags; });
        table16(kTimerTbl, [](const EnemyType& t, int) { return t.timer; });
        table16(kDistanceTbl, [](const EnemyType& t, int) { return t.distance; });
        table16(kDelayTbl, [](const EnemyType& t, int) { return t.delay; });
        table16(kMoveSetTbl, [&](const EnemyType&, int t) { return moveSet[t]; });
        table16(kFlags2Tbl, [](const EnemyType& t, int) { return t.flags2; });
        for (int m = 0; m < kMoves; m++) {
            const EnemyMove& mv = e.moves[m];
            for (int d = 0; d < 2; d++) {
                image.Put16(bank, (uint16_t)(kMovePtrTbl + 4 * m + 2 * d), address(mv.script[d]));
            }
            image.Put(bank, (uint16_t)(kAccelYTbl + m), { mv.accelY });
            image.Put(bank, (uint16_t)(kAccelXTbl + m), { mv.accelX });
            image.Put(bank, (uint16_t)(kSpeedYTbl + m), { mv.speedY });
            image.Put(bank, (uint16_t)(kSpeedXTbl + m), { mv.speedX });
        }
        for (int i = 0; i < kProjectiles; i++) {
            const EnemyProjectile& pr = e.projectiles[i];
            image.Put(bank, (uint16_t)(kProjAnimTbl + 2 * i), { pr.anim[0], pr.anim[1] });
            image.Put(bank, (uint16_t)(kProjXTbl + 2 * i), { pr.offsetX[0], pr.offsetX[1] });
            image.Put(bank, (uint16_t)(kProjYTbl + i), { pr.offsetY });
        }
        for (int i = 0; i < kPaths; i++) {
            image.Put16(bank, (uint16_t)(kFireballPathTbl + 2 * i), address(e.fireballPaths[i]));
        }
    }
    return true;
}

} // namespace Game::Data
