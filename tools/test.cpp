// Stress test: every style x part x many seeds; checks ranges, writes and re-reads MIDI.
#include "../Source/core/Generator.h"
#include "../Source/core/Midi.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iterator>
int main (int argc, char** argv)
{
    std::ifstream f (argv[1]); std::stringstream ss; ss << f.rdbuf();
    mf::Model m; if (! m.deserialize (ss.str())) { std::cerr << "bad model\n"; return 1; }
    mf::Model m2; m2.deserialize (m.serialize()); if (m2.serialize() != m.serialize()) std::cerr << "WARN roundtrip differs\n";
    mf::Generator g (m);
    int bad = 0, total = 0; long notes = 0;
    for (auto& st : g.styleNames())
        for (int bars : { 4, 8, 16 })
            for (int seed = 1; seed <= 60; ++seed)
            {
                mf::Settings s; s.style = st; s.scale = g.config (st).scale; s.bpm = g.config (st).bpm; s.bars = bars; s.key = seed % 12;
                s.gmDrums = seed % 2; s.oct = seed % 3 - 1; s.song = seed * 7919u;
                for (auto& p : mf::parts()) s.seeds[p.seedKey] = seed * 104729u + (uint32_t) p.seedKey.size() * 31u;
                for (auto& p : mf::parts())
                {
                    auto n = g.generate (p.id, s); ++total; notes += (long) n.size();
                    bool ok = ! n.empty() || p.id == "perc" || p.id == "openhat" || p.id == "snare" || p.id == "roll";
                    for (auto& x : n) if (x.t < 0 || x.t >= bars * mf::BAR || x.d <= 0 || x.n < 0 || x.n > 127 || x.v < 1 || x.v > 127) ok = false;
                    auto bytes = mf::writeMidi (n, s.bpm, bars, p.name, 0);
                    std::vector<mf::RawNote> back; double tp;
                    if (! mf::readMidi (bytes, back, tp) || back.size() != n.size()) ok = false;
                    if (! ok) { ++bad; if (bad < 6) std::cerr << "BAD " << st << " " << p.id << " bars " << bars << " seed " << seed << " n=" << n.size() << "\n"; }
                }
            }
    std::cout << "styles:"; for (auto& s : g.styleNames()) std::cout << " " << mf::displayStyle (s) << "(" << g.config (s).bpm << ")"; std::cout << "\n";
    std::cout << "patterns " << total << " bad " << bad << " notes " << notes << "\n";
    // show one example per style for the lead, and one hat/808 pattern
    for (auto& st : g.styleNames())
    {
        mf::Settings s; s.style = st; s.scale = g.config (st).scale; s.song = 42; for (auto& p : mf::parts()) s.seeds[p.seedKey] = 1234;
        auto n = g.generate ("lead", s);
        std::cout << mf::displayStyle (st) << " lead (" << n.size() << "): ";
        for (size_t i = 0; i < std::min<size_t> (n.size(), 18); ++i) std::cout << n[i].t / 96.0 << ":" << n[i].n << " ";
        std::cout << "\n";
        if (st == "wlr") for (std::string id : { "hihat", "808", "clap" })
        {
            auto h = g.generate (id, s); std::cout << "  " << id << " (" << h.size() << "): ";
            for (size_t i = 0; i < std::min<size_t> (h.size(), 30); ++i) std::cout << h[i].t / 96.0 << (h[i].v != 100 ? "v" + std::to_string (h[i].v) : "") << (id == "808" ? ":" + std::to_string (h[i].n) : "") << " ";
            std::cout << "\n";
            std::ofstream (std::string ("/tmp/wlr_") + id + ".mid", std::ios::binary).write ((const char*) mf::writeMidi (h, 154, 8, id, 0).data(), (long) mf::writeMidi (h, 154, 8, id, 0).size());
        }
    }
    return bad != 0;
}
