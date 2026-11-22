#pragma once
#include "Types.h"
#include <map>

namespace mf
{
// A Markov chain: for each context ("state"), how often each next token followed it in the training MIDI.
struct Markov
{
    std::map<std::string, std::map<int, double>> table;
    void add (const std::string& state, int token, double w = 1.0) { table[state][token] += w; }
    const std::map<int, double>* find (const std::string& state) const
    {
        auto it = table.find (state);
        return it == table.end() || it->second.empty() ? nullptr : &it->second;
    }
};

// Everything learned about one melody style. All values are raw counts, so training on more files just adds to them.
struct StyleCounts
{
    double files = 0, bars = 0, trip = 0, durSum = 0, durN = 0, bpmSum = 0;
    double deg[12] = {};     // time spent on each semitone above the tonic
    double pos[16] = {};     // note starts on each 16th of the bar
    std::map<int, double> iv;   // melodic leaps in semitones
    Markov rhythm;           // next gap (in 48ths of a beat) given previous gap + position in the beat
    Markov pitch;            // next scale-step move given previous move + current scale degree
};

// Where a drum part lands inside a 2-bar (32-step) cycle, and how long its notes are.
struct DrumCounts
{
    double cycles = 0, files = 0;
    double pos[32] = {};
    std::map<int, double> len;
};

struct Model
{
    std::map<std::string, StyleCounts> styles;
    std::map<std::string, DrumCounts> drums;     // "clap", "snare", "oh", "perc", "808"
    Markov hats;                                  // hi-hat gaps + velocity, including rolls
    double hatFiles = 0;

    std::string serialize() const;
    bool deserialize (const std::string& text);
    void merge (const Model& other);
    int totalFiles() const;
};

// ---------- training ----------
enum class Kind { Melody, HiHat, OpenHat, Clap, Snare, Perc, Bass808, Kick };
std::string canonicalStyle (std::string label);           // "digicore" -> "hyperpop", "slay" -> "trap" ...
Kind classifyFile (const std::string& fileName, std::string& styleOut);  // from names like "hh float.mid" or "(wlr) bees 155bpm.mid"
// Adds one MIDI file to the model. Returns a short description of what it was learned as.
std::string trainFile (Model& m, const std::string& fileName, const std::vector<RawNote>& notes, double tempo, const std::string& fallbackStyle);
}
