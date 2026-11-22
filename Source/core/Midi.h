#pragma once
#include "Types.h"

namespace mf
{
// Writes a type-0 MIDI file (96 PPQ) with tempo, 4/4 and a track name. Length is padded to `bars`.
std::vector<uint8_t> writeMidi (const Notes& notes, double bpm, int bars, const std::string& trackName, int channel);

// Reads any type-0/1 MIDI file into a flat note list (positions in beats). Returns false if it is not a MIDI file.
bool readMidi (const std::vector<uint8_t>& data, std::vector<RawNote>& out, double& tempoBpm);
}
