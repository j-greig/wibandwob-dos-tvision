// The tracker: music written in rows, channels across and rows down, in the period
// form (Soundtracker, 1987): each cell a note, an instrument and an effect,
// "C-4 01 A37". Channels are FM (one of the synth's patches), the PC speaker, or the
// TR-808's drum voices. A song is plain text both ways (songText, parseSong). No Turbo
// Vision dependency. Format and commands: docs/machine-sound.md.
#pragma once

#include <string>
#include <vector>

#include "synth.h"

namespace sound {

enum class ChanKind { Fm, Speaker, Drums };

struct TrackChannel {
    ChanKind kind = ChanKind::Fm;
    int patch = 0;   // FM: the default patch (index into fmPatches())
};

// note: -1 empty ("---"), -2 note off ("==="), else a MIDI note, or for a drum channel a
// TR-808 voice index 0-12 ("BD-" ... "CH-"). instr: -1 empty (".."), else 0-255.
// fx: 0 none ("..."), else 'A' (arpeggio, two semitone offsets), 'V' (velocity 00-7F),
// 'G' (gate in sixteenths of a row: G10 is one row).
struct TrackCell {
    int note = -1;
    int instr = -1;
    char fx = 0;
    int fxv = 0;
    bool empty() const { return note == -1 && instr == -1 && fx == 0; }
};

struct Song {
    std::string title = "untitled";
    int tempo = 120;     // beats per minute
    int rpb = 4;         // rows per beat (4: a row is a sixteenth)
    std::vector<TrackChannel> chans;
    std::vector<std::vector<TrackCell>> grid;   // [row][channel]
    int rows() const { return (int)grid.size(); }
};

const char* drumId(int v);                  // 0 -> "bd" ... 12 -> "ch"
int drumIndex(const std::string& s);        // "bd"/"BD"/"BD-" -> 0, else -1
std::string chanName(const TrackChannel& c);   // "fm:bass", "speaker", "drums"
bool parseChan(const std::string& s, TrackChannel& out);

std::string cellText(const TrackChannel& c, const TrackCell& cell);    // "C-4 01 A37", "--- .. ..."
bool parseCell(const TrackChannel& c, const std::string& text, TrackCell& out, std::string& err);
// One row as the window and the ears print it: "03 C-2 01 ... | --- .. ... | BD- 7F ...".
std::string rowText(const Song& s, int row);

Song newSong(int rows, const std::vector<TrackChannel>& chans);
std::string songText(const Song& s);
bool parseSong(const std::string& text, Song& out, std::string& err);   // err: "line N: ..."

double rowMs(const Song& s);

// What a row sounds: synth notes (atMs from the row's start) and drum hits.
struct RowSound {
    int chan = 0;
    bool drum = false;
    Note note;           // synth note (voice, midi, vel, dur, patch)
    int drum_voice = -1; // drum voice index
    double gain = 1.0;   // drum level 0-1
    double atMs = 0;
    std::string cell;    // the cell's text
};
std::vector<RowSound> rowSounds(const Song& s, int row);

}  // namespace sound
