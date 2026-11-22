// Command-line trainer: builds the factory model from folders of MIDI files.
//   train <out_model.txt> <folder> [folder...]
#include "../Source/core/Model.h"
#include "../Source/core/Midi.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
namespace fs = std::filesystem;
int main (int argc, char** argv)
{
    if (argc < 3) { std::cerr << "usage: train out.txt folder...\n"; return 1; }
    mf::Model m; int n = 0;
    for (int a = 2; a < argc; ++a)
        for (auto& e : fs::recursive_directory_iterator (argv[a]))
        {
            auto ext = e.path().extension().string();
            if (ext != ".mid" && ext != ".midi" && ext != ".MID") continue;
            std::ifstream f (e.path(), std::ios::binary);
            std::vector<uint8_t> d ((std::istreambuf_iterator<char> (f)), {});
            std::vector<mf::RawNote> notes; double tempo;
            if (! mf::readMidi (d, notes, tempo)) continue;
            mf::trainFile (m, e.path().filename().string(), notes, tempo, "trap"); ++n;
        }
    std::ofstream (argv[1]) << m.serialize();
    std::cout << "trained on " << n << " files\n";
    for (auto& [k, s] : m.styles) std::cout << "  style " << k << ": " << s.files << " files, rhythm states " << s.rhythm.table.size() << ", pitch states " << s.pitch.table.size() << "\n";
    for (auto& [k, d] : m.drums) std::cout << "  drum " << k << ": " << d.files << " files\n";
    std::cout << "  hats: " << m.hatFiles << " files, " << m.hats.table.size() << " states\n";
}
