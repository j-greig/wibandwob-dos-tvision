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

enum class ChanKind { Fm, Speaker, Drums, Voice };

struct TrackChannel {
    ChanKind kind = ChanKind::Fm;
    int patch = 0;           // FM: the default patch (index into fmPatches())
    std::string singer;      // Voice: wib or wob
};

// note: -1 empty ("---"), -2 note off ("==="), else a MIDI note, or for a drum channel a
// TR-808 voice index 0-12 ("BD-" ... "CH-"). instr: -1 empty (".."), else 0-255.
// fx: 0 none ("..."), else 'A' (arpeggio, two semitone offsets), 'V' (velocity 00-7F),
// 'G' (gate in sixteenths of a row: G10 is one row), 'F' (as ProTracker: F01-F1F speed, ticks of
// a 24-tick beat per row, so F06 is 4 rows a beat; F20-FF tempo in beats per minute).
// A voice channel's cell has a word in place of the instrument: "C-4 hel    ...".
struct TrackCell {
    int note = -1;
    int instr = -1;
    char fx = 0;
    int fxv = 0;
    std::string word;
    bool empty() const { return note == -1 && instr == -1 && fx == 0 && word.empty(); }
};

struct TrackPattern {
    std::vector<std::vector<TrackCell>> grid;   // [row][channel]
    int rows() const { return (int)grid.size(); }
};

// A song: patterns of their own length, and the order they play in.
struct Song {
    std::string title = "untitled";
    int tempo = 120;     // beats per minute at the start
    int rpb = 4;         // rows per beat at the start (4: a row is a sixteenth)
    std::vector<TrackChannel> chans;
    std::vector<TrackPattern> pats;   // at least one
    std::vector<int> order;           // pattern numbers, at least one
    int rows(int pat) const { return (pat >= 0 && pat < (int)pats.size()) ? pats[pat].rows() : 0; }
    TrackCell& cell(int pat, int row, int ch) { return pats[pat].grid[row][ch]; }
    const TrackCell& cell(int pat, int row, int ch) const { return pats[pat].grid[row][ch]; }
};

const char* drumId(int v);                  // 0 -> "bd" ... 12 -> "ch"
int drumIndex(const std::string& s);        // "bd"/"BD"/"BD-" -> 0, else -1
std::string chanName(const TrackChannel& c);   // "fm:bass", "speaker", "drums"
bool parseChan(const std::string& s, TrackChannel& out);

std::string cellText(const TrackChannel& c, const TrackCell& cell);    // "C-4 01 A37", "--- .. ..."
bool parseCell(const TrackChannel& c, const std::string& text, TrackCell& out, std::string& err);
// One row as the window and the ears print it: "03 C-2 01 ... | --- .. ... | BD- 7F ...".
std::string rowText(const Song& s, int pat, int row);
// A pattern with every cell empty, rows long.
TrackPattern emptyPattern(const Song& s, int rows);

Song newSong(int rows, const std::vector<TrackChannel>& chans);
// Text: the header, "order 0 1 0 2", then "pattern <n>" and its rows for each pattern. A file
// with rows and no pattern line (the first format) loads as pattern 0 with order 0.
std::string songText(const Song& s);
bool parseSong(const std::string& text, Song& out, std::string& err);   // err: "line N: ..."

// Row length: speed is ticks of a 24-tick beat per row (24 / rpb).
double rowMs(int tempo, int speed);
double rowMs(const Song& s);   // at the song's starting tempo and rpb
int speedOf(int rpb);          // 24 / rpb
// A row's tempo and speed changes (F effects); -1 where the row has none.
struct RowFx { int tempo = -1, speed = -1; };
RowFx rowEffects(const Song& s, int pat, int row);

// What a row sounds: synth notes (atMs from the row's start) and drum hits.
struct RowSound {
    int chan = 0;
    bool drum = false;
    Note note;           // synth note (voice, midi, vel, dur, patch)
    int drum_voice = -1; // drum voice index
    double gain = 1.0;   // drum level 0-1
    bool sing = false;   // a voice cell: word sung by singer on note.midi for note.durMs
    std::string word, singer;
    double atMs = 0;
    std::string cell;    // the cell's text
};
// rowLenMs: the row's length at the tempo and speed playing (gates and arpeggios use it).
std::vector<RowSound> rowSounds(const Song& s, int pat, int row, double rowLenMs);

}  // namespace sound
