#include "game/data/Audio.h"

#include "game/data/Data.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <tinyxml2.h>

namespace Game::Data {

namespace {

// The engine's tables (the same addresses in banks 0-5; Sound.cpp reads them there).
constexpr uint16_t kMultiInitRow = 0xB2A7; // ChooseNextSFXRoutineTbl row for the multi channel SFX / music 8-11
constexpr uint16_t kMusicInitRow = 0xB2B6; // its row for music 0-7
constexpr uint16_t kInitMusicIndexTbl = 0xBBFA;
constexpr uint16_t kVolCntrlAddrTbl = 0xBCB0;
constexpr uint16_t kInitMusicTbl = 0xBD31;
constexpr uint16_t kMusicNotesTbl = 0xBE77;
constexpr uint16_t kNoteLengthsTbl = 0xBEF7;
constexpr int kSongs = 12;
constexpr int kNotes = 64;
constexpr int kLengths = 34;
constexpr int kEnvelopes = 5;
constexpr int kSongHeader = 13; // InitMusicTbl entry: 5 settings and 4 channel addresses

const char* const kSongNames[kSongs] = { "ridley",       "tourian",  "item_room", "kraid",    "norfair", "escape",
                                         "mother_brain", "brinstar", "fade_in",   "power_up", "ending",  "intro" };
const char* const kChannelNames[4] = { "SQ1", "SQ2", "Triangle", "Noise" };

// Note names of the note indexes (even bytes $00-$7E), after the pitch the original's table gives them
// on the square channels. $02 has no period: a rest.
const char* const kNoteNames[kNotes] = { "A1",  "-",   "C#2", "D2",  "E2",  "F2",  "F#2", "G2",  "G#2", "A2",  "A#2",
                                         "B2",  "C3",  "C#3", "D3",  "D#3", "E3",  "F3",  "F#3", "G3",  "G#3", "A3",
                                         "A#3", "B3",  "C4",  "C#4", "D4",  "D#4", "E4",  "F4",  "F#4", "G4",  "G#4",
                                         "A4",  "A#4", "B4",  "C5",  "C#5", "D5",  "D#5", "E5",  "F5",  "F#5", "G5",
                                         "G#5", "A5",  "A#5", "B5",  "C6",  "C#6", "D6",  "D#6", "E6",  "F6",  "F#6",
                                         "G6",  "G#6", "A6",  "A#6", "B6",  "C7",  "C#7", "D7",  "F7" };

// Drum beats: the noise channel's "notes" are offsets of the register entries at $B200.
const uint8_t kDrums[4] = { 0x01, 0x04, 0x07, 0x0A };

// The init routines songs start with (Music00Init-Music05Init) and the SQ1/SQ2 duty/volume each sets.
struct DutyPreset {
    uint16_t routine;
    uint8_t sq1, sq2;
};
const DutyPreset kDutyPresets[] = { { 0xBC77, 0x92, 0x96 }, { 0xBC7A, 0xB6, 0xF6 }, { 0xBC7D, 0xF4, 0xF4 },
                                    { 0xBC80, 0x34, 0x34 }, { 0xBC83, 0xB3, 0xB3 }, { 0xBC86, 0xF5, 0xF6 } };

// --- Sound effects --------------------------------------------------------------------------------------

enum class Kind { Drum, Square, Triangle, Noise };

struct Field {
    const char* name;
    uint8_t byte, shift, bits, valueShift;
};

const Field kSquareFields[] = { { "Duty", 0, 6, 2, 0 },           { "Halt", 0, 5, 1, 0 },
                                { "ConstantVolume", 0, 4, 1, 0 }, { "Volume", 0, 0, 4, 0 },
                                { "Sweep", 1, 7, 1, 0 },          { "SweepPeriod", 1, 4, 3, 0 },
                                { "SweepNegate", 1, 3, 1, 0 },    { "SweepShift", 1, 0, 3, 0 },
                                { "Period", 2, 0, 8, 0 },         { "Period", 3, 0, 3, 8 },
                                { "Length", 3, 3, 5, 0 },         { nullptr, 0, 0, 0, 0 } };
const Field kTriangleFields[] = { { "Control", 0, 7, 1, 0 },    { "LinearCounter", 0, 0, 7, 0 },
                                  { "Unused4009", 1, 0, 8, 0 }, { "Period", 2, 0, 8, 0 },
                                  { "Period", 3, 0, 3, 8 },     { "Length", 3, 3, 5, 0 },
                                  { nullptr, 0, 0, 0, 0 } };
const Field kNoiseFields[] = { { "Unused400C", 0, 6, 2, 0 },     { "Halt", 0, 5, 1, 0 },
                               { "ConstantVolume", 0, 4, 1, 0 }, { "Volume", 0, 0, 4, 0 },
                               { "Unused400D", 1, 0, 8, 0 },     { "Mode", 2, 7, 1, 0 },
                               { "Unused400E", 2, 4, 3, 0 },     { "Period", 2, 0, 4, 0 },
                               { "Length", 3, 3, 5, 0 },         { "Unused400F", 3, 0, 3, 0 },
                               { nullptr, 0, 0, 0, 0 } };
// Drum beats have no $400D byte.
const Field kDrumFields[] = { { "Unused400C", 0, 6, 2, 0 }, { "Halt", 0, 5, 1, 0 },   { "ConstantVolume", 0, 4, 1, 0 },
                              { "Volume", 0, 0, 4, 0 },     { "Mode", 1, 7, 1, 0 },   { "Unused400E", 1, 4, 3, 0 },
                              { "Period", 1, 0, 4, 0 },     { "Length", 2, 3, 5, 0 }, { "Unused400F", 2, 0, 3, 0 },
                              { nullptr, 0, 0, 0, 0 } };

const Field* FieldsOf(Kind k) {
    switch (k) {
        case Kind::Drum:
            return kDrumFields;
        case Kind::Square:
            return kSquareFields;
        case Kind::Triangle:
            return kTriangleFields;
        default:
            return kNoiseFields;
    }
}

const char* KindName(Kind k) {
    static const char* const kNames[] = { "Drum", "Square", "Triangle", "Noise" };
    return kNames[(int)k];
}

const char* KindComment(Kind k) {
    switch (k) {
        case Kind::Drum:
            return "Noise channel registers $400C, $400E, $400F for a drum beat of the music (drum0-drum3).";
        case Kind::Square:
            return "Square channel registers ($4000-$4003 or $4004-$4007) the sound starts with.";
        case Kind::Triangle:
            return "Triangle channel registers ($4008-$400B) the sound starts with.";
        default:
            return "Noise channel registers ($400C-$400F) the sound starts with.";
    }
}

struct EffectSpec {
    uint16_t address;
    const char* name;
    Kind kind;
};

// The register entries at $B201-$B288 (the engine loads them by their low address byte).
const EffectSpec kEffects[] = {
    { 0xB201, "DrumBeat0", Kind::Drum },
    { 0xB204, "DrumBeat1", Kind::Drum },
    { 0xB207, "DrumBeat2", Kind::Drum },
    { 0xB20A, "DrumBeat3", Kind::Drum },
    { 0xB20D, "GamePaused", Kind::Square },
    { 0xB211, "ScrewAttack", Kind::Noise },
    { 0xB215, "MissileLaunch", Kind::Noise },
    { 0xB219, "BombExplode", Kind::Noise },
    { 0xB21D, "SamusWalk", Kind::Noise },
    { 0xB221, "SpitFlame", Kind::Noise },
    { 0xB225, "SamusHit", Kind::Square },
    { 0xB229, "BossHitSQ2", Kind::Square },
    { 0xB22D, "BossHitSQ1", Kind::Square },
    { 0xB231, "IncorrectPasswordSQ1", Kind::Square },
    { 0xB235, "IncorrectPasswordSQ2", Kind::Square },
    { 0xB239, "TimeBombTick", Kind::Square },
    { 0xB23D, "EnergyPickup", Kind::Square },
    { 0xB241, "MissilePickup", Kind::Square },
    { 0xB245, "Metal", Kind::Square },
    { 0xB249, "LongRangeShot", Kind::Square },
    { 0xB24D, "ShortRangeShot", Kind::Square },
    { 0xB251, "Jump", Kind::Square },
    { 0xB255, "EnemyHit", Kind::Square },
    { 0xB259, "BugOutOfHole", Kind::Square },
    { 0xB25D, "WaveBeam", Kind::Square },
    { 0xB261, "IceBeam", Kind::Square },
    { 0xB265, "BombLaunch1", Kind::Triangle },
    { 0xB269, "BombLaunch2", Kind::Triangle },
    { 0xB26D, "SamusToBall", Kind::Triangle },
    { 0xB271, "MetroidHit", Kind::Triangle },
    { 0xB275, "SamusDie", Kind::Triangle },
    { 0xB279, "SamusBeep", Kind::Triangle },
    { 0xB27D, "BigEnemyHit", Kind::Triangle },
    { 0xB281, "StatueRaise", Kind::Triangle },
    { 0xB285, "Door", Kind::Triangle },
};
constexpr int kEffectCount = sizeof(kEffects) / sizeof(kEffects[0]);

int EffectSize(Kind k) {
    return k == Kind::Drum ? 3 : 4;
}

struct TableSpec {
    uint16_t address;
    uint16_t size;
    const char* name;
    const char* comment;
};

// Values some sound effects step through, frame by frame (addresses fixed in Sound.cpp).
const TableSpec kTables[] = {
    { 0xB4FB, 20, "SpitFlamesTable", "Noise volume ($400C), one value per frame." },
    { 0xB63C, 20, "BossHitTable", "SQ1 and SQ2 duty/volume ($4000/$4004), one value per frame from the second on." },
    { 0xB6A9, 7, "MissilePickupTable", "SQ1 period low ($4002), one value per step; 00 ends the sound." },
    { 0xB778, 2, "IceBeamTable", "SQ1 period low ($4002), alternating every frame." },
    { 0xB7A7, 2, "WaveBeamPeriodTable", "SQ1 period low ($4002), alternating every frame." },
    { 0xB7A9, 3, "WaveBeamVolumeTable", "SQ1 duty/volume ($4000), one value per step; 00 ends the sound." },
};
constexpr int kTableCount = sizeof(kTables) / sizeof(kTables[0]);

// --- Helpers ----------------------------------------------------------------------------------------------

struct Prg {
    const uint8_t* p;
    uint8_t Get(int bank, uint16_t a) const {
        return p[(size_t)bank * 0x4000 + (a - BankBase(bank))];
    }
    uint16_t Get16(int bank, uint16_t a) const {
        return Get(bank, a) | (Get(bank, (uint16_t)(a + 1)) << 8);
    }
};

uint16_t ImageGet16(const Image& image, int bank, uint16_t a) {
    return image.Get(bank, a) | (image.Get(bank, (uint16_t)(a + 1)) << 8);
}

size_t Offset(int bank, uint16_t address) {
    return (size_t)bank * 0x4000 + (address - BankBase(bank));
}

void Cover(std::vector<uint8_t>& covered, int bank, uint16_t address, size_t length) {
    for (size_t i = 0; i < length; i++) {
        covered[Offset(bank, address) + i] = 1;
    }
}

bool AnyCovered(const std::vector<uint8_t>& covered, int bank, uint16_t address, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (covered[Offset(bank, address) + i]) {
            return true;
        }
    }
    return false;
}

std::string Hex(unsigned v, int digits) {
    char b[16];
    snprintf(b, sizeof(b), "%0*X", digits, v);
    return b;
}

unsigned HexAttr(const tinyxml2::XMLElement* e, const char* name, unsigned def = 0) {
    const char* v = e->Attribute(name);
    return v ? (unsigned)strtoul(v, nullptr, 16) : def;
}

std::vector<std::string> Tokens(const char* text) {
    std::vector<std::string> out;
    std::string cur;
    for (const char* p = text ? text : ""; *p; p++) {
        if (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur += *p;
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

bool HexByte(const std::string& s, uint8_t& out) {
    if (s.empty() || s.size() > 2) {
        return false;
    }
    char* end = nullptr;
    unsigned long v = strtoul(s.c_str(), &end, 16);
    if (*end != 0) {
        return false;
    }
    out = (uint8_t)v;
    return true;
}

std::vector<uint8_t> HexList(const char* text, bool& ok) {
    std::vector<uint8_t> out;
    ok = true;
    for (const std::string& t : Tokens(text)) {
        uint8_t b = 0;
        if (!HexByte(t, b)) {
            ok = false;
        }
        out.push_back(b);
    }
    return out;
}

int BankOf(const char* name) {
    for (int b = 0; b <= 5; b++) {
        if (name && strcmp(name, BankFolder(b)) == 0) {
            return b;
        }
    }
    return -1;
}

std::string Head(const char* type, int bank) {
    return std::string("<") + type + " Version=\"0\" Bank=\"" + BankFolder(bank) + "\"";
}

// --- Songs -------------------------------------------------------------------------------------------------

// Pitch of a note name (C4 = 48), or -1.
int Pitch(const std::string& s) {
    static const int kSemis[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    if (s.size() < 2 || s[0] < 'A' || s[0] > 'G') {
        return -1;
    }
    int semi = kSemis[s[0] - 'A'];
    size_t i = 1;
    if (s[i] == '#') {
        semi++;
        i++;
    } else if (s[i] == 'b') {
        semi--;
        i++;
    }
    if (i >= s.size() || s[i] < '0' || s[i] > '9' || i + 1 != s.size()) {
        return -1;
    }
    return (s[i] - '0') * 12 + semi;
}

// The byte for a song token, false if it isn't one.
bool SongByte(const std::string& t, uint8_t& out) {
    if (t == "end") {
        out = 0x00;
    } else if (t == ":|") {
        out = 0xFF;
    } else if (t.rfind("|:", 0) == 0) {
        char* end = nullptr;
        long n = strtol(t.c_str() + 2, &end, 10);
        if (t.size() == 2 || *end != 0 || n < 0 || n > 62) {
            return false;
        }
        out = (uint8_t)(0xC0 + n);
    } else if (t.size() == 2 && t[0] == 'L' && isxdigit((unsigned char)t[1])) {
        out = (uint8_t)(0xB0 + strtoul(t.c_str() + 1, nullptr, 16));
    } else if (t[0] == '$') {
        return HexByte(t.substr(1), out);
    } else if (t.size() == 5 && t.rfind("drum", 0) == 0 && t[4] >= '0' && t[4] <= '3') {
        out = kDrums[t[4] - '0'];
    } else if (t == "-") {
        out = 0x02;
    } else {
        int pitch = Pitch(t);
        for (int i = 0; i < kNotes && pitch >= 0; i++) {
            if (Pitch(kNoteNames[i]) == pitch) {
                out = (uint8_t)(i * 2);
                return true;
            }
        }
        return false;
    }
    return true;
}

// A note byte as a token: a note name (a drum for the noise channel), else the raw byte.
std::string NoteToken(uint8_t b, bool noise) {
    if (noise) {
        for (int i = 0; i < 4; i++) {
            if (b == kDrums[i]) {
                return "drum" + std::to_string(i);
            }
        }
    } else if ((b & 1) == 0 && b < kNotes * 2) {
        return kNoteNames[b / 2];
    }
    return "$" + Hex(b, 2);
}

std::string PartText(const std::vector<uint8_t>& bytes, bool noise) {
    std::string out, line;
    bool afterLength = false;
    auto flush = [&]() {
        if (!line.empty()) {
            out += "        " + line + "\n";
            line.clear();
        }
    };
    for (uint8_t b : bytes) {
        std::string t;
        bool breakAfter = false;
        if (afterLength) {
            t = NoteToken(b, noise); // the byte after a length is always played as a note
            afterLength = false;
        } else if (b == 0x00) {
            t = "end";
            breakAfter = true;
        } else if (b == 0xFF) {
            t = ":|";
            breakAfter = true;
        } else if (b >= 0xC0) {
            t = "|:" + std::to_string(b & 0x3F);
        } else if ((b & 0xF0) == 0xB0) {
            t = "L" + Hex(b & 0x0F, 1);
            afterLength = true;
        } else {
            t = NoteToken(b, noise);
        }
        if (t.rfind("|:", 0) == 0 && line.size() > 60) {
            flush();
        }
        line += (line.empty() ? "" : " ") + t;
        if (breakAfter || (line.size() > 100 && !afterLength)) {
            flush();
        }
    }
    flush();
    return out;
}

uint16_t SongRoutineEntry(uint16_t musicTbl, uint16_t multiTbl, int index) {
    return index < 8 ? (uint16_t)(musicTbl + 2 * index) : (uint16_t)(multiTbl + 2 * (index - 8));
}

std::string WriteSong(const Song& s, const uint8_t* lengths) {
    std::string x = Head("Song", s.bank) + " Name=\"" + kSongNames[s.index] + "\"";
    if (s.address) {
        x += " Address=\"" + Hex(s.address, 4) + "\" Size=\"" + std::to_string(s.size) + "\"";
    }
    x += " Repeat=\"" + Hex(s.repeat, 2) + "\" NoteLengths=\"" + Hex(s.noteLengths, 2) + "\" TriangleCounter=\"" +
         Hex(s.triangle, 2) + "\" SQ1Duty=\"" + Hex(s.sq1Duty, 2) + "\" SQ2Duty=\"" + Hex(s.sq2Duty, 2) +
         "\" SQ1Envelope=\"" + Hex(s.sq1Envelope, 1) + "\" SQ2Envelope=\"" + Hex(s.sq2Envelope, 1) + "\">\n";
    if (lengths) {
        x += "    <!-- L0-LF with NoteLengths " + Hex(s.noteLengths, 2) + ", in frames:";
        for (int i = 0; i < 16 && s.noteLengths + i < kLengths; i++) {
            x += " " + std::to_string(lengths[s.noteLengths + i]);
        }
        x += " -->\n";
    }
    for (const SongPart& p : s.parts) {
        x += "    <Part Channel=\"";
        bool first = true;
        for (int c = 0; c < 4; c++) {
            if (p.channels & (1 << c)) {
                x += std::string(first ? "" : " ") + kChannelNames[c];
                first = false;
            }
        }
        x += "\">\n" + PartText(p.bytes, p.channels == 8) + "    </Part>\n";
    }
    return x + "</Song>\n";
}

// Where a channel's data ends: after its end ($00), or at `limit`.
uint16_t ScanChannel(const Prg& prg, int bank, uint16_t start, uint16_t limit) {
    bool afterLength = false;
    for (uint16_t a = start; a < limit; a++) {
        uint8_t b = prg.Get(bank, a);
        if (afterLength) {
            afterLength = false;
        } else if (b == 0x00) {
            return (uint16_t)(a + 1);
        } else if ((b & 0xF0) == 0xB0) {
            afterLength = true;
        }
    }
    return limit;
}

// --- Other XML ----------------------------------------------------------------------------------------------

std::string WriteEffect(const SoundEffect& e) {
    const EffectSpec& spec = kEffects[e.spec];
    std::string x = Head("SoundEffect", e.bank) + " Name=\"" + spec.name + "\" Address=\"" + Hex(spec.address, 4) +
                    "\" Channel=\"" + KindName(spec.kind) + "\"";
    std::vector<std::pair<std::string, unsigned>> values;
    for (const Field* f = FieldsOf(spec.kind); f->name; f++) {
        unsigned v = ((e.bytes[f->byte] >> f->shift) & ((1u << f->bits) - 1)) << f->valueShift;
        auto it = std::find_if(values.begin(), values.end(), [&](const auto& p) { return p.first == f->name; });
        if (it == values.end()) {
            values.push_back({ f->name, v });
        } else {
            it->second |= v;
        }
    }
    for (const auto& [name, v] : values) {
        if (name.rfind("Unused", 0) == 0 && v == 0) {
            continue;
        }
        bool wide = name == "Period" && (spec.kind == Kind::Square || spec.kind == Kind::Triangle);
        x += "\n    " + name + "=\"" + Hex(v, wide ? 3 : 1) + "\"";
    }
    return x + ">\n    <!-- " + KindComment(spec.kind) + " -->\n</SoundEffect>\n";
}

std::string HexLine(const std::vector<uint8_t>& bytes) {
    std::string s;
    for (size_t i = 0; i < bytes.size(); i++) {
        s += (i ? " " : "") + Hex(bytes[i], 2);
    }
    return s;
}

std::string WriteEffectTable(const SoundEffect& e) {
    const TableSpec& spec = kTables[e.spec];
    return Head("SoundEffectTable", e.bank) + " Name=\"" + spec.name + "\" Address=\"" + Hex(spec.address, 4) +
           "\">\n    <!-- " + spec.comment + " -->\n    " + HexLine(e.bytes) + "\n</SoundEffectTable>\n";
}

std::string WriteNotes(const NoteTable& t) {
    std::string x = Head("NoteTable", t.bank) + " Address=\"" + Hex(kMusicNotesTbl, 4) + "\">\n";
    x += "    <!-- Timer period of each note the songs play: the frequency is 1789773 / 16 / (Period + 1) on the\n"
         "         square channels, an octave lower on the triangle. Names are the songs' note names. -->\n";
    for (size_t i = 0; i < t.periods.size(); i++) {
        x += "    <Note Name=\"" + std::string(kNoteNames[i]) + "\" Period=\"" + Hex(t.periods[i], 3) + "\"/>\n";
    }
    return x + "</NoteTable>\n";
}

std::string WriteLengths(const NoteLengths& t) {
    std::string x = Head("NoteLengths", t.bank) + " Address=\"" + Hex(kNoteLengthsTbl, 4) + "\">\n";
    x += "    <!-- Note lengths in frames (decimal). A song's L0 is the length at its NoteLengths offset. -->\n";
    const int groups[3][2] = { { 0, 11 }, { 11, 11 }, { 22, 12 } };
    for (const auto& g : groups) {
        x += "    <Lengths Offset=\"" + Hex((unsigned)g[0], 2) + "\">";
        for (int i = 0; i < g[1]; i++) {
            x += (i ? " " : "") + std::to_string(t.frames[g[0] + i]);
        }
        x += "</Lengths>\n";
    }
    return x + "</NoteLengths>\n";
}

std::string WriteEnvelopes(const VolumeEnvelopes& v) {
    std::string x = Head("VolumeEnvelopes", v.bank) + " Address=\"" + Hex(kVolCntrlAddrTbl, 4) + "\">\n";
    x += "    <!-- Square channel volume per frame of a note (a song's SQ1Envelope / SQ2Envelope picks one). Each\n"
         "         note starts at the second value. FF: the channel's duty/volume from then on, F0: silent. -->\n";
    for (size_t i = 0; i < v.envelopes.size(); i++) {
        const auto& e = v.envelopes[i];
        x += "    <Envelope Number=\"" + std::to_string(i + 1) + "\"";
        if (e.address) {
            x += " Address=\"" + Hex(e.address, 4) + "\" Size=\"" + std::to_string(e.size) + "\"";
        }
        x += ">" + HexLine(e.volumes) + "</Envelope>\n";
    }
    return x + "</VolumeEnvelopes>\n";
}

} // namespace

std::vector<uint8_t> Song::Data() const {
    std::vector<uint8_t> out;
    for (const SongPart& p : parts) {
        out.insert(out.end(), p.bytes.begin(), p.bytes.end());
    }
    return out;
}

void Audio::Merge(const Audio& other) {
    for (const auto& [bank, list] : other.songs) {
        for (const auto& [i, s] : list) {
            songs[bank][i] = s;
        }
    }
    for (const auto& [bank, list] : other.effects) {
        for (const auto& [i, e] : list) {
            effects[bank][i] = e;
        }
    }
    for (const auto& [bank, list] : other.effectTables) {
        for (const auto& [i, e] : list) {
            effectTables[bank][i] = e;
        }
    }
    for (const auto& [bank, t] : other.notes) {
        notes[bank] = t;
    }
    for (const auto& [bank, t] : other.lengths) {
        lengths[bank] = t;
    }
    for (const auto& [bank, t] : other.envelopes) {
        envelopes[bank] = t;
    }
}

// --- Extraction ------------------------------------------------------------------------------------------------

void ExtractAudio(const uint8_t* prgBytes, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered) {
    Prg prg{ prgBytes };
    for (int bank = 0; bank <= 5; bank++) {
        std::string folder = BankFolder(bank);
        std::string music = "metroid/music/" + folder + "/";
        std::string sfx = "metroid/sfx/" + folder + "/";

        for (int i = 0; i < kEffectCount; i++) {
            const EffectSpec& spec = kEffects[i];
            int size = EffectSize(spec.kind);
            if (AssetBlockLength(bank, spec.address) != (size_t)size || AnyCovered(covered, bank, spec.address, size)) {
                continue;
            }
            SoundEffect e{ bank, i, {} };
            for (int k = 0; k < size; k++) {
                e.bytes.push_back(prg.Get(bank, (uint16_t)(spec.address + k)));
            }
            Cover(covered, bank, spec.address, size);
            files[sfx + spec.name] = WriteEffect(e);
        }
        for (int i = 0; i < kTableCount; i++) {
            const TableSpec& spec = kTables[i];
            if (AssetBlockLength(bank, spec.address) != spec.size ||
                AnyCovered(covered, bank, spec.address, spec.size)) {
                continue;
            }
            SoundEffect e{ bank, i, {} };
            for (int k = 0; k < spec.size; k++) {
                e.bytes.push_back(prg.Get(bank, (uint16_t)(spec.address + k)));
            }
            Cover(covered, bank, spec.address, spec.size);
            files[sfx + spec.name] = WriteEffectTable(e);
        }

        NoteTable notes{ bank, {} };
        for (int i = 0; i < kNotes; i++) {
            notes.periods.push_back((uint16_t)((prg.Get(bank, (uint16_t)(kMusicNotesTbl + 2 * i)) << 8) |
                                               prg.Get(bank, (uint16_t)(kMusicNotesTbl + 2 * i + 1))));
        }
        Cover(covered, bank, kMusicNotesTbl, kNotes * 2);
        files[music + "notes"] = WriteNotes(notes);

        NoteLengths lengths{ bank, {} };
        for (int i = 0; i < kLengths; i++) {
            lengths.frames.push_back(prg.Get(bank, (uint16_t)(kNoteLengthsTbl + i)));
        }
        Cover(covered, bank, kNoteLengthsTbl, kLengths);
        files[music + "lengths"] = WriteLengths(lengths);

        VolumeEnvelopes env{ bank, {} };
        bool envOk = !AnyCovered(covered, bank, kVolCntrlAddrTbl, kEnvelopes * 2);
        for (int i = 0; i < kEnvelopes && envOk; i++) {
            VolumeEnvelopes::Envelope e;
            e.address = prg.Get16(bank, (uint16_t)(kVolCntrlAddrTbl + 2 * i));
            if (e.address < 0x8000 || e.address >= 0xBFC0) {
                envOk = false;
                break;
            }
            for (uint16_t a = e.address;; a++) {
                uint8_t b = prg.Get(bank, a);
                e.volumes.push_back(b);
                if (b == 0xFF || b == 0xF0 || e.volumes.size() == 64) {
                    break;
                }
            }
            e.size = (uint16_t)e.volumes.size();
            envOk = !AnyCovered(covered, bank, e.address, e.size);
            env.envelopes.push_back(e);
        }
        if (envOk) {
            Cover(covered, bank, kVolCntrlAddrTbl, kEnvelopes * 2);
            for (const auto& e : env.envelopes) {
                Cover(covered, bank, e.address, e.size);
            }
            files[music + "envelopes"] = WriteEnvelopes(env);
        }

        // Songs: those whose data is in this bank (the others' entries point into RAM).
        uint16_t musicTbl = prg.Get16(bank, kMusicInitRow);
        uint16_t multiTbl = prg.Get16(bank, kMultiInitRow);
        for (int index = 0; index < kSongs; index++) {
            uint16_t header = (uint16_t)(kInitMusicTbl + prg.Get(bank, (uint16_t)(kInitMusicIndexTbl + index)));
            uint16_t ptr[4];
            bool inBank = true, any = false;
            uint16_t start = 0xFFFF, unionEnd = 0;
            for (int c = 0; c < 4; c++) {
                ptr[c] = prg.Get16(bank, (uint16_t)(header + 5 + 2 * c));
                if (ptr[c] == 0) {
                    continue;
                }
                any = true;
                if (ptr[c] < 0x8000 || ptr[c] >= 0xC000) {
                    inBank = false;
                    break;
                }
                start = std::min(start, ptr[c]);
                // The channel's data block in the asset map bounds the song: what follows is other data.
                size_t len = std::max<size_t>(AssetBlockLength(bank, ptr[c]), 1);
                unionEnd = (uint16_t)std::max<size_t>(unionEnd, ptr[c] + len);
            }
            if (!any || !inBank) {
                continue;
            }
            uint16_t end = start;
            for (int c = 0; c < 4; c++) {
                if (ptr[c]) {
                    end = std::max(end, ScanChannel(prg, bank, ptr[c], unionEnd));
                }
            }
            uint16_t routineEntry = SongRoutineEntry(musicTbl, multiTbl, index);
            uint16_t routine = prg.Get16(bank, routineEntry);
            const DutyPreset* preset = nullptr;
            for (const DutyPreset& p : kDutyPresets) {
                if (p.routine == routine) {
                    preset = &p;
                }
            }
            // Data shared with anything else stays untyped.
            if (preset == nullptr || AnyCovered(covered, bank, start, (size_t)(end - start)) ||
                AnyCovered(covered, bank, header, kSongHeader) || AnyCovered(covered, bank, routineEntry, 2)) {
                continue;
            }
            Song s;
            s.bank = bank;
            s.index = (uint8_t)index;
            s.address = start;
            s.size = (uint16_t)(end - start);
            s.noteLengths = prg.Get(bank, header);
            s.repeat = prg.Get(bank, (uint16_t)(header + 1));
            s.triangle = prg.Get(bank, (uint16_t)(header + 2));
            s.sq1Envelope = prg.Get(bank, (uint16_t)(header + 3));
            s.sq2Envelope = prg.Get(bank, (uint16_t)(header + 4));
            s.sq1Duty = preset->sq1;
            s.sq2Duty = preset->sq2;
            std::vector<uint16_t> starts;
            for (int c = 0; c < 4; c++) {
                if (ptr[c]) {
                    starts.push_back(ptr[c]);
                }
            }
            std::sort(starts.begin(), starts.end());
            starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
            for (size_t k = 0; k < starts.size(); k++) {
                SongPart p;
                for (int c = 0; c < 4; c++) {
                    if (ptr[c] == starts[k]) {
                        p.channels |= (uint8_t)(1 << c);
                    }
                }
                uint16_t to = k + 1 < starts.size() ? starts[k + 1] : end;
                for (uint16_t a = starts[k]; a < to; a++) {
                    p.bytes.push_back(prg.Get(bank, a));
                }
                s.parts.push_back(p);
            }
            Cover(covered, bank, start, s.size);
            Cover(covered, bank, header, kSongHeader);
            Cover(covered, bank, routineEntry, 2);
            files[music + kSongNames[index]] = WriteSong(s, lengths.frames.data());
        }
    }
}

// --- XML reading ------------------------------------------------------------------------------------------------

bool IsAudioResource(const tinyxml2::XMLElement* root) {
    if (root == nullptr) {
        return false;
    }
    static const char* const kTypes[] = { "Song",      "SoundEffect", "SoundEffectTable",
                                          "NoteTable", "NoteLengths", "VolumeEnvelopes" };
    for (const char* t : kTypes) {
        if (strcmp(root->Name(), t) == 0) {
            return true;
        }
    }
    return false;
}

bool ReadAudioResource(const tinyxml2::XMLElement* root, Audio& audio, std::string& error) {
    std::string kind = root->Name();
    int bank = BankOf(root->Attribute("Bank"));
    if (bank < 0) {
        error = kind + ": unknown Bank";
        return false;
    }
    const char* nameAttr = root->Attribute("Name");
    std::string name = nameAttr ? nameAttr : "";

    if (kind == "Song") {
        Song s;
        s.bank = bank;
        int index = -1;
        for (int i = 0; i < kSongs; i++) {
            if (name == kSongNames[i]) {
                index = i;
            }
        }
        if (index < 0) {
            error = "Song: unknown Name " + name;
            return false;
        }
        s.index = (uint8_t)index;
        s.address = (uint16_t)HexAttr(root, "Address");
        s.size = (uint16_t)root->IntAttribute("Size");
        s.repeat = (uint8_t)HexAttr(root, "Repeat");
        s.noteLengths = (uint8_t)HexAttr(root, "NoteLengths");
        s.triangle = (uint8_t)HexAttr(root, "TriangleCounter");
        s.sq1Duty = (uint8_t)HexAttr(root, "SQ1Duty");
        s.sq2Duty = (uint8_t)HexAttr(root, "SQ2Duty");
        s.sq1Envelope = (uint8_t)HexAttr(root, "SQ1Envelope");
        s.sq2Envelope = (uint8_t)HexAttr(root, "SQ2Envelope");
        uint8_t seen = 0;
        for (auto e = root->FirstChildElement("Part"); e; e = e->NextSiblingElement("Part")) {
            SongPart p;
            for (const std::string& c : Tokens(e->Attribute("Channel"))) {
                int ch = -1;
                for (int k = 0; k < 4; k++) {
                    if (c == kChannelNames[k]) {
                        ch = k;
                    }
                }
                if (ch < 0 || (seen & (1 << ch))) {
                    error = "Song " + name + ": bad or repeated Channel " + c;
                    return false;
                }
                seen |= (uint8_t)(1 << ch);
                p.channels |= (uint8_t)(1 << ch);
            }
            for (const std::string& t : Tokens(e->GetText())) {
                uint8_t b = 0;
                if (!SongByte(t, b)) {
                    error = "Song " + name + ": unknown note or command '" + t + "'";
                    return false;
                }
                p.bytes.push_back(b);
            }
            s.parts.push_back(std::move(p));
        }
        audio.songs[bank][s.index] = std::move(s);
        return true;
    }
    if (kind == "SoundEffect") {
        int spec = -1;
        for (int i = 0; i < kEffectCount; i++) {
            if (name == kEffects[i].name) {
                spec = i;
            }
        }
        if (spec < 0) {
            error = "SoundEffect: unknown Name " + name;
            return false;
        }
        SoundEffect e{ bank, spec, std::vector<uint8_t>(EffectSize(kEffects[spec].kind), 0) };
        for (const Field* f = FieldsOf(kEffects[spec].kind); f->name; f++) {
            unsigned v = HexAttr(root, f->name);
            e.bytes[f->byte] |= (uint8_t)(((v >> f->valueShift) & ((1u << f->bits) - 1)) << f->shift);
        }
        audio.effects[bank][spec] = std::move(e);
        return true;
    }
    if (kind == "SoundEffectTable") {
        int spec = -1;
        for (int i = 0; i < kTableCount; i++) {
            if (name == kTables[i].name) {
                spec = i;
            }
        }
        if (spec < 0) {
            error = "SoundEffectTable: unknown Name " + name;
            return false;
        }
        bool ok;
        SoundEffect e{ bank, spec, HexList(root->GetText(), ok) };
        if (!ok || e.bytes.size() != kTables[spec].size) {
            error = "SoundEffectTable " + name + " needs " + std::to_string(kTables[spec].size) + " hex bytes";
            return false;
        }
        audio.effectTables[bank][spec] = std::move(e);
        return true;
    }
    if (kind == "NoteTable") {
        NoteTable t{ bank, {} };
        for (auto e = root->FirstChildElement("Note"); e; e = e->NextSiblingElement("Note")) {
            unsigned p = HexAttr(e, "Period");
            if (p > 0xFFFF) {
                error = "NoteTable: bad Period";
                return false;
            }
            t.periods.push_back((uint16_t)p);
        }
        if (t.periods.size() != kNotes) {
            error = "NoteTable needs 64 notes";
            return false;
        }
        audio.notes[bank] = std::move(t);
        return true;
    }
    if (kind == "NoteLengths") {
        NoteLengths t{ bank, {} };
        for (auto e = root->FirstChildElement("Lengths"); e; e = e->NextSiblingElement("Lengths")) {
            for (const std::string& v : Tokens(e->GetText())) {
                char* end = nullptr;
                long n = strtol(v.c_str(), &end, 10);
                if (*end != 0 || n < 0 || n > 255) {
                    error = "NoteLengths: bad length " + v;
                    return false;
                }
                t.frames.push_back((uint8_t)n);
            }
        }
        if (t.frames.size() != kLengths) {
            error = "NoteLengths needs 34 lengths";
            return false;
        }
        audio.lengths[bank] = std::move(t);
        return true;
    }
    if (kind == "VolumeEnvelopes") {
        VolumeEnvelopes v{ bank, {} };
        for (auto e = root->FirstChildElement("Envelope"); e; e = e->NextSiblingElement("Envelope")) {
            VolumeEnvelopes::Envelope env;
            env.address = (uint16_t)HexAttr(e, "Address");
            env.size = (uint16_t)e->IntAttribute("Size");
            bool ok;
            env.volumes = HexList(e->GetText(), ok);
            if (!ok || env.volumes.empty()) {
                error = "VolumeEnvelopes: bad Envelope";
                return false;
            }
            v.envelopes.push_back(std::move(env));
        }
        if (v.envelopes.size() != kEnvelopes) {
            error = "VolumeEnvelopes needs 5 envelopes";
            return false;
        }
        audio.envelopes[bank] = std::move(v);
        return true;
    }
    error = "not an audio resource";
    return false;
}

// --- Assembly ------------------------------------------------------------------------------------------------------

bool AssembleAudio(const Audio& audio, Image& image, std::string& error) {
    for (const auto& [bank, list] : audio.effects) {
        for (const auto& [spec, e] : list) {
            image.Put(bank, kEffects[spec].address, e.bytes);
        }
    }
    for (const auto& [bank, list] : audio.effectTables) {
        for (const auto& [spec, e] : list) {
            image.Put(bank, kTables[spec].address, e.bytes);
        }
    }
    for (const auto& [bank, t] : audio.notes) {
        std::vector<uint8_t> bytes;
        for (uint16_t p : t.periods) {
            bytes.push_back((uint8_t)(p >> 8));
            bytes.push_back((uint8_t)p);
        }
        image.Put(bank, kMusicNotesTbl, bytes);
    }
    for (const auto& [bank, t] : audio.lengths) {
        image.Put(bank, kNoteLengthsTbl, t.frames);
    }
    for (const auto& [bank, v] : audio.envelopes) {
        for (size_t i = 0; i < v.envelopes.size(); i++) {
            const auto& e = v.envelopes[i];
            uint16_t at = e.address;
            if (at == 0 || e.volumes.size() > e.size) {
                at = image.Allocate(bank, e.volumes.size());
                if (at == 0) {
                    error = "no room left in the bank for volume envelope " + std::to_string(i + 1);
                    return false;
                }
            }
            image.Put(bank, at, e.volumes);
            image.Put16(bank, (uint16_t)(kVolCntrlAddrTbl + 2 * i), at);
        }
    }
    for (const auto& [bank, list] : audio.songs) {
        uint16_t musicTbl = ImageGet16(image, bank, kMusicInitRow);
        uint16_t multiTbl = ImageGet16(image, bank, kMultiInitRow);
        for (const auto& [index, s] : list) {
            std::string what = std::string("song ") + kSongNames[index] + " (" + BankFolder(bank) + ")";
            const DutyPreset* preset = nullptr;
            for (const DutyPreset& p : kDutyPresets) {
                if (p.sq1 == s.sq1Duty && p.sq2 == s.sq2Duty) {
                    preset = &p;
                }
            }
            if (preset == nullptr) {
                error = what + ": SQ1Duty/SQ2Duty must be one of the engine's pairs (92/96, B6/F6, F4/F4, 34/34, "
                               "B3/B3, F5/F6)";
                return false;
            }
            std::vector<uint8_t> data = s.Data();
            uint16_t at = s.address;
            if (at == 0 || data.size() > s.size) {
                at = data.empty() ? 0 : image.Allocate(bank, data.size());
                if (at == 0 && !data.empty()) {
                    error = "no room left in the bank for the " + what;
                    return false;
                }
            }
            image.Put(bank, at, data);
            std::vector<uint8_t> header{ s.noteLengths, s.repeat, s.triangle, s.sq1Envelope, s.sq2Envelope };
            uint16_t ptr[4] = { 0, 0, 0, 0 };
            size_t offset = 0;
            for (const SongPart& p : s.parts) {
                for (int c = 0; c < 4; c++) {
                    if (p.channels & (1 << c)) {
                        ptr[c] = (uint16_t)(at + offset);
                    }
                }
                offset += p.bytes.size();
            }
            for (uint16_t p : ptr) {
                header.push_back((uint8_t)p);
                header.push_back((uint8_t)(p >> 8));
            }
            uint16_t headerAt = (uint16_t)(kInitMusicTbl + image.Get(bank, (uint16_t)(kInitMusicIndexTbl + index)));
            image.Put(bank, headerAt, header);
            image.Put16(bank, SongRoutineEntry(musicTbl, multiTbl, index), preset->routine);
        }
    }
    return true;
}

} // namespace Game::Data
