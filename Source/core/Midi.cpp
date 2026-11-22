#include "Midi.h"
#include <algorithm>
#include <map>

namespace mf
{
static void vlq (std::vector<uint8_t>& o, uint32_t n)
{
    uint8_t buf[5]; int k = 0;
    buf[k++] = n & 127;
    while (n >>= 7) buf[k++] = (uint8_t) ((n & 127) | 128);
    while (k) o.push_back (buf[--k]);
}

std::vector<uint8_t> writeMidi (const Notes& notes, double bpm, int bars, const std::string& name, int ch)
{
    struct Ev { int t, on; uint8_t a, b, c; };
    std::vector<Ev> ev;
    for (auto& n : notes)
    {
        int t = std::max (0, n.t), d = std::max (1, n.d);
        uint8_t k = (uint8_t) std::clamp (n.n, 0, 127), v = (uint8_t) std::clamp (n.v, 1, 127);
        ev.push_back ({ t, 1, (uint8_t) (0x90 | ch), k, v });
        ev.push_back ({ t + d, 0, (uint8_t) (0x80 | ch), k, 0 });
    }
    std::stable_sort (ev.begin(), ev.end(), [] (const Ev& x, const Ev& y) { return x.t != y.t ? x.t < y.t : x.on < y.on; });

    std::vector<uint8_t> tr;
    tr.insert (tr.end(), { 0, 0xFF, 0x03 }); vlq (tr, (uint32_t) name.size()); tr.insert (tr.end(), name.begin(), name.end());
    uint32_t mp = (uint32_t) (60000000.0 / std::max (1.0, bpm) + 0.5);
    tr.insert (tr.end(), { 0, 0xFF, 0x51, 3, (uint8_t) (mp >> 16), (uint8_t) (mp >> 8), (uint8_t) mp });
    tr.insert (tr.end(), { 0, 0xFF, 0x58, 4, 4, 2, 24, 8 });
    int last = 0;
    for (auto& e : ev) { vlq (tr, (uint32_t) (e.t - last)); tr.insert (tr.end(), { e.a, e.b, e.c }); last = e.t; }
    vlq (tr, (uint32_t) std::max (0, bars * BAR - last)); tr.insert (tr.end(), { 0xFF, 0x2F, 0 });

    std::vector<uint8_t> f = { 'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0, (uint8_t) PPQ, 'M','T','r','k' };
    uint32_t L = (uint32_t) tr.size();
    f.insert (f.end(), { (uint8_t) (L >> 24), (uint8_t) (L >> 16), (uint8_t) (L >> 8), (uint8_t) L });
    f.insert (f.end(), tr.begin(), tr.end());
    return f;
}

static bool rdVlq (const std::vector<uint8_t>& d, size_t& i, size_t end, uint32_t& v)
{
    v = 0;
    for (int k = 0; k < 4 && i < end; ++k) { uint8_t b = d[i++]; v = (v << 7) | (b & 127); if (! (b & 128)) return true; }
    return false;
}

bool readMidi (const std::vector<uint8_t>& d, std::vector<RawNote>& out, double& tempo)
{
    out.clear(); tempo = 0;
    if (d.size() < 14 || d[0] != 'M' || d[1] != 'T' || d[2] != 'h' || d[3] != 'd') return false;
    auto u16 = [&] (size_t i) { return (int) (d[i] << 8 | d[i + 1]); };
    auto u32 = [&] (size_t i) { return (uint32_t) d[i] << 24 | (uint32_t) d[i + 1] << 16 | (uint32_t) d[i + 2] << 8 | d[i + 3]; };
    int ntr = u16 (10), div = u16 (12);
    if (div <= 0 || (div & 0x8000)) return false;   // SMPTE timing not supported
    size_t i = 8 + u32 (4);
    for (int t = 0; t < ntr && i + 8 <= d.size(); ++t)
    {
        if (! (d[i] == 'M' && d[i + 1] == 'T' && d[i + 2] == 'r' && d[i + 3] == 'k')) { i += 8 + u32 (i + 4); continue; }
        size_t end = std::min (d.size(), i + 8 + u32 (i + 4)), j = i + 8;
        uint64_t tick = 0; uint8_t rs = 0;
        std::map<int, std::vector<std::pair<uint64_t, int>>> on;
        while (j < end)
        {
            uint32_t dt; if (! rdVlq (d, j, end, dt)) break; tick += dt;
            if (j >= end) break;
            uint8_t b = d[j];
            if (b == 0xFF)
            {
                if (j + 2 > end) break;
                uint8_t ty = d[j + 1]; size_t k = j + 2; uint32_t ln; if (! rdVlq (d, k, end, ln)) break;
                if (ty == 0x51 && ln == 3 && k + 3 <= end && tempo == 0)
                    tempo = 60000000.0 / (double) ((uint32_t) d[k] << 16 | (uint32_t) d[k + 1] << 8 | d[k + 2]);
                j = k + ln; continue;
            }
            if (b == 0xF0 || b == 0xF7) { size_t k = j + 1; uint32_t ln; if (! rdVlq (d, k, end, ln)) break; j = k + ln; continue; }
            if (b & 0x80) { rs = b; ++j; }
            uint8_t st = rs & 0xF0;
            if (st == 0xC0 || st == 0xD0) { ++j; continue; }
            if (j + 2 > end) break;
            uint8_t a = d[j], c = d[j + 1]; j += 2;
            int key = ((rs & 15) << 8) | a;
            if (st == 0x90 && c > 0) on[key].push_back ({ tick, c });
            else if (st == 0x80 || (st == 0x90 && c == 0))
            {
                auto& q = on[key];
                if (! q.empty()) { auto s = q.front(); q.erase (q.begin());
                    out.push_back ({ (double) s.first / div, (double) (tick - s.first) / div, a, s.second }); }
            }
        }
        i = end;
    }
    std::sort (out.begin(), out.end(), [] (const RawNote& x, const RawNote& y) { return x.beat != y.beat ? x.beat < y.beat : x.pitch < y.pitch; });
    if (tempo <= 0) tempo = 140;
    return true;
}
} // namespace mf
