#pragma once
#include "Model.h"

namespace mf
{
struct StyleConfig
{
    std::string name, about, scale;
    int bpm = 150;
    std::vector<std::vector<int>> progs;     // chord roots as scale degrees (0 = i, 5 = VI, 4 = v ...)
    int size = 3;                            // notes per chord
    std::vector<std::string> chordRh, arp;
    std::vector<int> arpRate;                // ticks between arp notes
    int lo = 63, hi = 87;                    // lead range
    double ohMul = 1, percMul = 1;
};

struct Settings
{
    std::string style = "trap";
    int key = 1;                 // 0 = C
    std::string scale = "Minor";
    int bpm = 156;
    int bars = 8;
    int oct = 0;                 // 808 octave shift
    bool gmDrums = false;        // false = every drum on C5 (60)
    uint32_t song = 1;
    std::map<std::string, uint32_t> seeds;
};

struct PartInfo { std::string id, name; bool drum; std::string seedKey; };
const std::vector<PartInfo>& parts();
const std::vector<std::string>& scaleNames();
extern const char* const NOTE_NAMES[12];

class Generator
{
public:
    explicit Generator (const Model& m) : model (m) {}
    std::vector<std::string> styleNames() const;                 // built-ins + anything learned from training
    StyleConfig config (const std::string& style) const;
    std::vector<int> progression (const Settings& s) const;
    std::string chordName (const Settings& s, int degree) const;
    Notes generate (const std::string& partId, const Settings& s) const;
    std::string fileName (const std::string& partId, const Settings& s) const;

private:
    const Model& model;
};

std::string displayStyle (const std::string& s);   // "wlr" -> "WLR", "trap" -> "Trap"
}
