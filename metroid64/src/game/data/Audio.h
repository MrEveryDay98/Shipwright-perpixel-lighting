#pragma once

// Music and sound effects as resources. The sound engine is in every bank 0-5 (Sound.cpp reads its tables
// from whichever bank is mapped), so each bank has its own copy of the tables and of the songs it plays.
//
//   metroid/music/<bank>/<song>      <Song>             one song: its settings and its channels' notes
//   metroid/music/<bank>/notes       <NoteTable>        the period of each note
//   metroid/music/<bank>/lengths     <NoteLengths>      the note lengths, in frames
//   metroid/music/<bank>/envelopes   <VolumeEnvelopes>  the square channels' volume envelopes
//   metroid/sfx/<bank>/<name>        <SoundEffect>      the APU registers a sound effect starts with
//   metroid/sfx/<bank>/<name>        <SoundEffectTable> the per-frame values some sound effects step through
//
// <bank> is the bank's folder (title, brinstar, norfair, tourian, kraid, ridley).

#include "game/data/Image.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tinyxml2 {
class XMLElement;
}

namespace Game::Data {

// A run of a song's data starting where one or more channels start (bit 0 SQ1, 1 SQ2, 2 triangle,
// 3 noise). A channel that doesn't end there reads on into the next part, as in the original.
struct SongPart {
    uint8_t channels = 0;
    std::vector<uint8_t> bytes;
};

struct Song {
    int bank = 0;
    uint8_t index = 0;    // MusicInitIndex: 0 Ridley ... 7 Brinstar, 8 fade in ... 11 intro
    uint16_t address = 0; // where its data was in the ROM (0: new)
    uint16_t size = 0;
    uint8_t noteLengths = 0; // offset of L0 in the note lengths
    uint8_t repeat = 0;      // 0: plays once
    uint8_t triangle = 0;    // TriCounterCntrl
    uint8_t sq1Envelope = 0, sq2Envelope = 0;
    uint8_t sq1Duty = 0, sq2Duty = 0; // the duty/volume the song's init routine sets
    std::vector<SongPart> parts;

    std::vector<uint8_t> Data() const;
};

// One of the engine's SFX register entries ($B201-$B288), or one of its per-frame tables.
struct SoundEffect {
    int bank = 0;
    int spec = 0; // entry in the engine's list (fixes the address and the meaning)
    std::vector<uint8_t> bytes;
};

struct NoteTable {
    int bank = 0;
    std::vector<uint16_t> periods; // (period high << 8) | period low, 64 notes
};

struct NoteLengths {
    int bank = 0;
    std::vector<uint8_t> frames; // 34 lengths from $BEF7
};

struct VolumeEnvelopes {
    int bank = 0;
    struct Envelope {
        uint16_t address = 0;
        uint16_t size = 0;
        std::vector<uint8_t> volumes; // up to and including the FF / F0 end
    };
    std::vector<Envelope> envelopes; // 5
};

struct Audio {
    std::map<int, std::map<uint8_t, Song>> songs;           // [bank][index]
    std::map<int, std::map<int, SoundEffect>> effects;      // [bank][spec]
    std::map<int, std::map<int, SoundEffect>> effectTables; // [bank][spec]
    std::map<int, NoteTable> notes;                         // [bank]
    std::map<int, NoteLengths> lengths;                     // [bank]
    std::map<int, VolumeEnvelopes> envelopes;               // [bank]

    void Merge(const Audio& other);
};

// Reads the music and sound effects out of the PRG ROM into resource files; marks the bytes it covers.
void ExtractAudio(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered);

// True if the root element is an audio resource.
bool IsAudioResource(const tinyxml2::XMLElement* root);
bool ReadAudioResource(const tinyxml2::XMLElement* root, Audio& audio, std::string& error);

// Puts the music and sound effects into the image (pointers included).
bool AssembleAudio(const Audio& audio, Image& image, std::string& error);

} // namespace Game::Data
