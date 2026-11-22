#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mf
{
constexpr int PPQ = 96;          // ticks per beat
constexpr int STEP = 24;         // one 16th note
constexpr int BAR = PPQ * 4;     // one 4/4 bar

struct Note { int t = 0, d = 0, n = 60, v = 100; };   // start tick, length, MIDI note, velocity
using Notes = std::vector<Note>;

struct RawNote { double beat = 0, len = 0; int pitch = 60, vel = 100; };  // as read from a MIDI file

// Small, fast, seedable random generator (mulberry32) so every pattern is reproducible from its seed.
struct Rng
{
    uint32_t a;
    explicit Rng (uint32_t seed) : a (seed) {}
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a;
        t = (t ^ (t >> 15)) * (1u | t);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
    int below (int n) { return n <= 0 ? 0 : (int) (next() * n) % n; }
    bool chance (double p) { return next() < p; }
    template <typename T> const T& pick (const std::vector<T>& v) { return v[(size_t) below ((int) v.size())]; }
    template <typename T> T wpick (const std::vector<T>& items, const std::vector<double>& w)
    {
        double total = 0; for (double x : w) total += x;
        if (total <= 0) return items.front();
        double x = next() * total;
        for (size_t i = 0; i < items.size(); ++i) { x -= w[i]; if (x <= 0) return items[i]; }
        return items.back();
    }
};

inline uint32_t hashStr (const std::string& s)
{
    uint32_t h = 7;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    return h;
}
} // namespace mf
