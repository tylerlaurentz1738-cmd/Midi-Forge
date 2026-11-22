#include "Generator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <set>

namespace mf
{
const char* const NOTE_NAMES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

static const std::map<std::string, std::vector<int>>& scales()
{
    static const std::map<std::string, std::vector<int>> s = {
        { "Minor", { 0,2,3,5,7,8,10 } }, { "Harmonic Minor", { 0,2,3,5,7,8,11 } }, { "Phrygian", { 0,1,3,5,7,8,10 } },
        { "Dorian", { 0,2,3,5,7,9,10 } }, { "Major", { 0,2,4,5,7,9,11 } } };
    return s;
}
const std::vector<std::string>& scaleNames()
{
    static const std::vector<std::string> n = { "Minor", "Harmonic Minor", "Phrygian", "Dorian", "Major" };
    return n;
}
static std::string scaleShort (const std::string& s)
{
    if (s == "Harmonic Minor") return "hmin";
    if (s == "Phrygian") return "phryg";
    if (s == "Dorian") return "dor";
    if (s == "Major") return "maj";
    return "min";
}

const std::vector<PartInfo>& parts()
{
    static const std::vector<PartInfo> p = {
        { "chords", "Chords", false, "chords" }, { "arp", "Arp", false, "arp" }, { "lead", "Lead", false, "lead" },
        { "brass", "Brass", false, "brass" }, { "pad", "Pad", false, "pad" }, { "808", "808", false, "groove" },
        { "kick", "Kick", true, "groove" }, { "hihat", "Hi-Hat", true, "hihat" }, { "clap", "Clap", true, "clap" },
        { "snare", "Snare", true, "snare" }, { "roll", "Snare Roll", true, "roll" }, { "openhat", "Open Hat", true, "openhat" },
        { "perc", "Perc", true, "perc" } };
    return p;
}

std::string displayStyle (const std::string& s)
{
    if (s == "wlr") return "WLR";
    std::string o = s; if (! o.empty()) o[0] = (char) std::toupper ((unsigned char) o[0]);
    return o;
}

// ---------- built-in style settings (the learned parts come from the Model) ----------
StyleConfig Generator::config (const std::string& style) const
{
    StyleConfig c; c.name = style;
    c.scale = "Minor"; c.progs = { { 0,5,0,5 }, { 0,5,0,4 }, { 0,0,5,5 }, { 0,3,5,4 }, { 0,5,3,4 } };
    c.chordRh = { "sustain", "half" }; c.arp = { "up", "updown" }; c.arpRate = { 24 }; c.lo = 62; c.hi = 86; c.bpm = 156;
    c.about = "Learned from your MIDI";
    if (style == "trap") c.about = "Trap, slay, kan, baton rouge";
    else if (style == "wlr") { c.about = "Whole Lotta Red style"; c.scale = "Harmonic Minor"; c.progs = { { 0,5,0,4 }, { 0,0,5,4 }, { 0,5,3,4 }, { 0,3,0,4 } };
        c.chordRh = { "bounce", "sustain", "half" }; c.arp = { "updown", "pinky", "random" }; c.arpRate = { 16, 24, 12 }; c.lo = 64; c.hi = 88; c.ohMul = 1.1; c.percMul = 1.1; }
    else if (style == "yeat") { c.about = "Yeat bells: big stacked chords"; c.progs = { { 0,5,0,5 }, { 0,5,3,5 }, { 0,5,0,4 }, { 5,5,0,0 } }; c.size = 5;
        c.arpRate = { 24, 48 }; c.ohMul = .8; c.percMul = .8; }
    else if (style == "hyperpop") { c.about = "Hyperpop and digicore"; c.progs = { { 0,5,2,6 }, { 0,6,5,6 }, { 5,6,0,0 }, { 0,3,5,6 } };
        c.chordRh = { "stabs", "bounce" }; c.arp = { "up", "updown", "random" }; c.arpRate = { 12, 16 }; c.lo = 66; c.hi = 90; c.percMul = 1.2; }
    else if (style == "melodic") { c.about = "Melodic trap"; c.progs = { { 0,5,2,6 }, { 0,3,5,4 }, { 0,5,3,6 }, { 5,3,0,4 } }; c.size = 4;
        c.chordRh = { "sustain", "syncop" }; c.arp = { "updown", "pinky" }; c.arpRate = { 24, 16 }; c.lo = 63; c.hi = 87; }
    else if (style == "memphis") { c.about = "Memphis: straight 8ths, dark phrygian"; c.scale = "Phrygian"; c.progs = { { 0,0,1,0 }, { 0,5,0,5 }, { 0,1,0,4 }, { 0,0,5,1 } };
        c.chordRh = { "eighths", "sustain" }; c.arp = { "up", "pinky" }; c.arpRate = { 48, 24 }; c.lo = 60; c.hi = 84; c.ohMul = .8; c.percMul = 1.3; }
    else if (style == "plugg") { c.about = "Plugg: sparse, long airy notes"; c.progs = { { 0,5,2,6 }, { 0,3,6,2 }, { 5,6,0,0 } }; c.size = 5;
        c.chordRh = { "sustain" }; c.arp = { "updown", "up" }; c.arpRate = { 24, 32 }; c.ohMul = .9; c.percMul = .8; }
    auto it = model.styles.find (style);
    if (it != model.styles.end() && it->second.files > 0) c.bpm = (int) std::lround (it->second.bpmSum / it->second.files);
    return c;
}

std::vector<std::string> Generator::styleNames() const
{
    std::vector<std::string> order = { "trap", "wlr", "yeat", "hyperpop", "melodic", "memphis", "plugg" }, out;
    for (auto& s : order) if (model.styles.count (s) && model.styles.at (s).files > 0) out.push_back (s);
    for (auto& [s, c] : model.styles) if (c.files > 0 && std::find (out.begin(), out.end(), s) == out.end()) out.push_back (s);
    if (out.empty()) out.push_back ("trap");
    return out;
}

// ---------- music helpers ----------
namespace
{
struct Ctx
{
    const Model& model; const Settings& s; StyleConfig cfg; std::vector<int> prog; const std::vector<int>& sc;
    int noteAt (int deg, int base) const { int o = (int) std::floor (deg / 7.0), i = ((deg % 7) + 7) % 7; return base + s.key + sc[(size_t) i] + 12 * o; }
    int chordDeg (int bar) const { return prog[(size_t) (((bar % (int) prog.size()) + (int) prog.size()) % (int) prog.size())]; }
    int pcOf (int m) const { return ((m - s.key) % 12 + 12) % 12; }
    int degIdx (int m) const { auto it = std::find (sc.begin(), sc.end(), pcOf (m)); return it == sc.end() ? -1 : (int) (it - sc.begin()); }
    int dn (int gm) const { return s.gmDrums ? gm : 60; }
    int total() const { return s.bars * BAR; }
};
int fold (int n, int lo) { while (n < lo) n += 12; while (n >= lo + 12) n -= 12; return n; }

const StyleCounts* styleCounts (const Model& m, const std::string& style)
{
    auto it = m.styles.find (style);
    if (it != m.styles.end() && it->second.files > 0) return &it->second;
    for (auto& [k, v] : m.styles) if (v.files > 0) return &v;
    return nullptr;
}
const DrumCounts* drum (const Model& m, const std::string& k)
{
    auto it = m.drums.find (k);
    return it == m.drums.end() || it->second.cycles <= 0 ? nullptr : &it->second;
}
int sampleMap (Rng& r, const std::map<int, double>& m, int fallback)
{
    if (m.empty()) return fallback;
    std::vector<int> k; std::vector<double> w;
    for (auto& [a, b] : m) { k.push_back (a); w.push_back (b); }
    return r.wpick (k, w);
}
int drumLen (Rng& r, const DrumCounts* d, int fallback) { return d ? std::max (1, sampleMap (r, d->len, fallback)) : fallback; }

// ---------- chords / pad / arp / brass ----------
std::vector<std::array<int, 3>> chordRhythm (const std::string& n)
{
    if (n == "half") return { { 0,8,100 }, { 8,8,90 } };
    if (n == "stabs") return { { 0,2,100 }, { 3,2,90 }, { 6,2,95 }, { 10,2,90 }, { 12,2,95 } };
    if (n == "syncop") return { { 0,6,100 }, { 6,4,90 }, { 10,6,95 } };
    if (n == "bounce") return { { 0,3,100 }, { 3,3,90 }, { 6,4,95 }, { 10,6,90 } };
    if (n == "eighths") { std::vector<std::array<int, 3>> v; for (int s = 0; s < 16; s += 2) v.push_back ({ s, 2, s % 4 ? 85 : 100 }); return v; }
    return { { 0,16,100 } };
}
std::vector<int> voicing (const Ctx& c, int d, int size)
{
    std::set<int> o;
    o.insert (fold (c.noteAt (d, 0), 48));
    int k = std::clamp (size, 3, 5);
    for (int i = 0; i < k; ++i) o.insert (fold (c.noteAt (d + 2 * i, 0), 60));
    if (size >= 5) o.insert (fold (c.noteAt (d, 0), 72));
    return { o.begin(), o.end() };
}
Notes genChords (const Ctx& c, Rng& r)
{
    Notes out; auto rh = chordRhythm (r.pick (c.cfg.chordRh));
    for (int b = 0; b < c.s.bars; ++b)
        for (int n : voicing (c, c.chordDeg (b), c.cfg.size))
            for (auto& h : rh) out.push_back ({ b * BAR + h[0] * STEP, h[1] * STEP - 4, n, h[2] });
    return out;
}
Notes genPad (const Ctx& c, Rng&)
{
    Notes out; int prevDeg = -999; size_t prevStart = 0;
    for (int b = 0; b < c.s.bars; ++b)
    {
        int d = c.chordDeg (b);
        if (d == prevDeg) { for (size_t i = prevStart; i < out.size(); ++i) out[i].d += BAR; continue; }
        prevStart = out.size(); prevDeg = d;
        out.push_back ({ b * BAR, BAR - 4, fold (c.noteAt (d, 0), 48), 80 });
        for (int x : { d + 2, d + 4, d + 6, d + 8 }) out.push_back ({ b * BAR, BAR - 4, fold (c.noteAt (x, 0), 60), 80 });
    }
    return out;
}
Notes genArp (const Ctx& c, Rng& r)
{
    Notes out; std::string mode = r.pick (c.cfg.arp); int sp = r.pick (c.cfg.arpRate);
    for (int b = 0; b < c.s.bars; ++b)
    {
        int d = c.chordDeg (b);
        std::vector<int> t1; for (int x : { d, d + 2, d + 4, d + 6 }) t1.push_back (fold (c.noteAt (x, 0), 60));
        std::sort (t1.begin(), t1.end());
        std::vector<int> t2 = t1; for (int x : t1) t2.push_back (x + 12);
        std::vector<int> seq;
        if (mode == "up") seq = t2;
        else if (mode == "updown") { seq = t2; for (int i = (int) t2.size() - 2; i >= 1; --i) seq.push_back (t2[(size_t) i]); }
        else if (mode == "pinky") for (size_t i = 1; i < t2.size(); ++i) { seq.push_back (t2[0]); seq.push_back (t2[i]); }
        int i = 0;
        for (int t = 0; t < BAR; t += sp, ++i)
            out.push_back ({ b * BAR + t, (int) std::lround (sp * .85), seq.empty() ? r.pick (t2) : seq[(size_t) i % seq.size()], t % PPQ == 0 ? 100 : 88 });
    }
    return out;
}
Notes genBrass (const Ctx& c, Rng& r)
{
    static const std::vector<std::vector<std::vector<int>>> pats = { { { 0,3,6 }, { 0,3,6,12 } }, { { 0,10,12 }, { 0,2,4 } }, { { 0 }, { 8,11,14 } }, { { 0,3,6,10 }, { 0,3,6 } } };
    const auto& pat = r.pick (pats); Notes out;
    for (int b = 0; b < c.s.bars; ++b)
    {
        int d = c.chordDeg (b);
        std::vector<int> v = { fold (c.noteAt (d, 0), 55), fold (c.noteAt (d + 2, 0), 55), fold (c.noteAt (d + 4, 0), 55), fold (c.noteAt (d, 0), 67) };
        for (int s : pat[(size_t) b % 2]) for (int n : v) out.push_back ({ b * BAR + s * STEP, 2 * STEP - 4, n, 110 });
    }
    return out;
}

// ---------- lead: Markov chains trained on the melodies ----------
Notes genLead (const Ctx& c, Rng& r)
{
    const StyleCounts* sc = styleCounts (c.model, c.cfg.name);
    std::vector<int> cands;
    for (int m = c.cfg.lo; m <= c.cfg.hi; ++m) if (c.degIdx (m) >= 0) cands.push_back (m);
    auto absDegOf = [&] (int m) { int rel = m - c.s.key - 60; return (int) std::floor (rel / 12.0) * 7 + c.degIdx (m); };
    auto isCT = [&] (int m, int ch) { int k = ((c.degIdx (m) - ch) % 7 + 7) % 7; return k == 0 || k == 2 || k == 4; };
    double meanDur = sc && sc->durN > 0 ? sc->durSum / sc->durN : .5;
    int maxLen48 = std::clamp ((int) std::lround (meanDur * 48 * 1.5), 12, 96);

    // rhythm: walk the gap chain across 2 bars (384 48ths)
    std::vector<int> on;
    for (int tries = 0; tries < 8 && on.size() < 5; ++tries)
    {
        on.clear(); int t = 0, prev = 0;
        while (t < 384)
        {
            on.push_back (t);
            const std::map<int, double>* dist = nullptr;
            if (sc) { dist = sc->rhythm.find (std::to_string (prev) + "|" + std::to_string (t % 48)); if (! dist) dist = sc->rhythm.find ("*|" + std::to_string (t % 48)); }
            int gap = dist ? sampleMap (r, *dist, 24) : 24;
            if (gap <= 0) gap = 24;
            prev = gap; t += gap;
        }
    }
    // pitch: walk the scale-step chain, favouring chord tones on beats and staying in range
    int repeats = 0;
    auto pickMove = [&] (int prevMove, int cur, int onset48) -> int
    {
        int curDeg = absDegOf (cur), ch = c.chordDeg (onset48 / 192);
        const std::map<int, double>* dist = nullptr;
        std::string dg = std::to_string (((curDeg % 7) + 7) % 7);
        if (sc) { dist = sc->pitch.find (std::to_string (prevMove) + "|" + dg); if (! dist) dist = sc->pitch.find ("*|" + dg); }
        std::vector<int> items; std::vector<double> w;
        for (int m : cands)
        {
            int move = absDegOf (m) - curDeg;
            if (move < -9 || move > 9) continue;
            double p = .02;
            if (dist) { auto it = dist->find (move); if (it != dist->end()) p += it->second; }
            else p += std::max (0.0, 4.0 - std::abs (move));
            bool strong = onset48 % 48 == 0;
            if (isCT (m, ch)) p *= strong ? 3.0 : 1.2;
            if (move == 0 && repeats >= 2) p *= .15;          // avoid droning on one note
            items.push_back (m); w.push_back (p);
        }
        int n = items.empty() ? cur : r.wpick (items, w);
        repeats = n == cur ? repeats + 1 : 0;
        return n;
    };
    struct LN { int t48, len48, n; };
    auto makeHalf = [&] (int offset48, int startNote, int prevMove, std::vector<LN>* copyFrom, int keepUntil) {
        std::vector<LN> v; int cur = startNote, pm = prevMove;
        for (size_t i = 0; i < on.size(); ++i)
        {
            int nx = i + 1 < on.size() ? on[i + 1] : 384;
            int n = (copyFrom && (int) i < keepUntil) ? (*copyFrom)[i].n : pickMove (pm, cur, on[i] + offset48);
            if (copyFrom && (int) i < keepUntil && on[i] % 48 == 0 && ! isCT (n, c.chordDeg ((on[i] + offset48) / 192))) n = pickMove (pm, cur, on[i] + offset48);
            pm = std::clamp (absDegOf (n) - absDegOf (cur), -9, 9); cur = n;
            v.push_back ({ on[i] + offset48, std::min (nx - on[i], maxLen48), n });
        }
        return v;
    };
    std::vector<int> start; for (int m : cands) if (isCT (m, c.chordDeg (0))) start.push_back (m);
    int first = start.empty() ? cands[cands.size() / 2] : start[start.size() / 2];
    auto call = makeHalf (0, first, 0, nullptr, 0);
    int keep = std::max (0, (int) on.size() - 3);
    auto answer = makeHalf (384, call.back().n, 0, &call, keep);
    auto answer2 = makeHalf (384, call.back().n, 0, &call, keep);
    Notes out;
    for (int chunk = 0; chunk * 4 < c.s.bars; ++chunk)
        for (auto* half : { &call, chunk % 2 ? &answer2 : &answer })
            for (auto& x : *half)
            {
                int t = chunk * BAR * 4 + x.t48 * 2;
                if (t < c.total()) out.push_back ({ t, std::max (6, x.len48 * 2 - 4), x.n, 100 });
            }
    return out;
}

// ---------- drums: 2-bar probability cells learned from the drum MIDI ----------
std::vector<int> phraseHits (const Ctx& c, Rng& r, const DrumCounts* d, double mul, std::vector<int> must, double fillMul)
{
    double p[32] = {};
    if (d) for (int i = 0; i < 32; ++i) p[i] = d->pos[i] / d->cycles;
    auto cell = [&] (double fm, bool fillOnly, const std::vector<int>& base) {
        std::vector<int> v;
        for (int s = 0; s < 32; ++s)
        {
            bool isMust = std::find (must.begin(), must.end(), s) != must.end();
            if (fillOnly && s < 24) { if (std::find (base.begin(), base.end(), s) != base.end()) v.push_back (s); continue; }
            if (isMust || r.chance (std::min (.97, p[s] * mul * (s >= 24 ? fm : 1.0)))) v.push_back (s);
        }
        return v;
    };
    auto A = cell (1, false, {}), B = cell (fillMul, true, A), B2 = cell (fillMul * 1.3, true, A);
    std::vector<int> hits;
    for (int b = 0; b < c.s.bars; b += 2)
    {
        const auto& cl = (b % 4 == 2) ? ((b % 8 == 6) ? B2 : B) : A;
        for (int s : cl) if (b * 16 + s < c.s.bars * 16) hits.push_back (b * 16 + s);
    }
    return hits;
}
std::vector<int> grooveSteps (const Ctx& c, Rng& r) { return phraseHits (c, r, drum (c.model, "808"), 1, { 0 }, 1.2); }

Notes genKick (const Ctx& c, Rng& r) { Notes o; for (int s : grooveSteps (c, r)) o.push_back ({ s * STEP, 2 * STEP, c.dn (36), 100 }); return o; }

Notes gen808 (const Ctx& c, Rng& r)
{
    auto on = grooveSteps (c, r);
    Rng r2 (c.s.seeds.count ("groove") ? c.s.seeds.at ("groove") + 999u : 999u);
    int lo = 57 + 12 * c.s.oct; Notes out;
    for (size_t i = 0; i < on.size(); ++i)
    {
        int s = on[i], nx = i + 1 < on.size() ? on[i + 1] : c.s.bars * 16, bar = s / 16, nextBar = nx / 16;
        int n = fold (c.noteAt (c.chordDeg (bar), 0), lo);
        if (nextBar != bar && nextBar < c.s.bars && s % 16 >= 12 && r2.chance (.15)) n = fold (c.noteAt (c.chordDeg (nextBar), 0), lo);
        else if (r2.chance (.08)) n += 12;
        else if (r2.chance (.05)) n += 7;
        out.push_back ({ s * STEP, std::min (nx - s, 20) * STEP - 2, n, 127 });
    }
    // slides: octave drops at the end of 2-bar phrases, velocity 100 (~79% in FL), overlapping the held note
    for (int cyc = 0; cyc < c.s.bars; cyc += 2)
    {
        if (! r2.chance (.55)) continue;
        int s = cyc * 16 + r2.wpick (std::vector<int> { 28, 30, 26 }, { .6, .4, .15 });
        Note* under = nullptr;
        for (auto& x : out) if (x.v == 127 && x.t <= s * STEP && x.t + x.d > s * STEP) under = &x;
        if (! under) continue;
        int len = std::min (r2.wpick (std::vector<int> { 2, 4 }, { .6, .4 }), (cyc + 2) * 16 - s);
        if (len <= 0) continue;
        under->d = std::max (under->d, (s + len) * STEP - under->t - 2);
        int pitch = under->n + (r2.chance (.8) ? -12 : 12);
        out.push_back ({ s * STEP, len * STEP - 2, pitch, 100 });
    }
    return out;
}

Notes genClap (const Ctx& c, Rng& r)
{
    auto* d = drum (c.model, "clap"); Notes o;
    for (int s : phraseHits (c, r, d, 1, { 8, 24 }, 1.3)) o.push_back ({ s * STEP, drumLen (r, d, 2) * STEP, c.dn (39), 100 });
    return o;
}
Notes dedupe (Notes v, int minGap)
{
    std::sort (v.begin(), v.end(), [] (const Note& a, const Note& b) { return a.t < b.t; });
    Notes o; for (auto& n : v) if (o.empty() || n.t - o.back().t >= minGap) o.push_back (n);
    return o;
}
Notes genSnare (const Ctx& c, Rng& r)
{
    Notes o;
    for (int s : phraseHits (c, r, drum (c.model, "snare"), .85, {}, 1.3)) o.push_back ({ s * STEP, 12, c.dn (38), 100 });
    for (int cyc = 0; cyc < c.s.bars; cyc += 2)
    {
        if (! r.chance (.6)) continue;
        int target = cyc * 16 + r.pick (std::vector<int> { 8, 14, 24, 30, 32 });
        int sp = r.pick (std::vector<int> { 12, 8, 12 }), cnt = r.pick (std::vector<int> { 3, 4, 4, 6 });
        for (int k = cnt; k >= 1; --k) { int t = target * STEP - k * sp; if (t >= 0 && t < c.total()) o.push_back ({ t, std::max (6, sp - 2), c.dn (38), 100 }); }
    }
    return dedupe (o, 4);
}
Notes genRoll (const Ctx& c, Rng& r)
{
    Notes o;
    if (r.chance (.45))
    {
        int rb = c.s.bars >= 2 && r.chance (.6) ? 2 : 1, start = (c.s.bars - rb) * BAR;
        static const std::vector<std::vector<int>> ladders = { { 2,2,4,4,4,4,8,8 }, { 3,3,6,6,6,6,12,12 }, { 4,4,4,4,8,8,8,8 }, { 2,4,4,6,6,8,8,16 } };
        auto lad = r.pick (ladders); lad.erase (lad.begin(), lad.begin() + (8 - rb * 4));
        int total = 0; for (int h : lad) total += h;
        int rise = ! c.s.gmDrums && r.chance (.6) ? r.pick (std::vector<int> { 7, 12 }) : 0, k = 0;
        for (size_t beat = 0; beat < lad.size(); ++beat)
        {
            double sp = 96.0 / lad[beat];
            for (int i = 0; i < lad[beat]; ++i)
            {
                double p = (double) k++ / std::max (1, total - 1);
                o.push_back ({ start + (int) beat * 96 + (int) std::lround (i * sp), std::max (6, (int) std::lround (sp * .8)), c.dn (38) + (int) std::lround (p * rise), (int) std::lround (45 + p * 82) });
            }
        }
    }
    else
    {
        for (int b = 3; b < c.s.bars; b += 4)
        {
            int bursts = r.pick (std::vector<int> { 2, 3 });
            for (int j = 0; j < bursts; ++j)
            {
                int target = b * 16 + r.pick (std::vector<int> { 8, 12, 14, 16 });
                int sp = r.pick (std::vector<int> { 12, 8, 16, 6 }), cnt = r.pick (std::vector<int> { 3, 4, 6, 8 });
                for (int k = cnt; k >= 1; --k)
                {
                    int t = target * STEP - k * sp;
                    if (t >= b * BAR && t < c.total()) o.push_back ({ t, std::max (5, sp - 2), c.dn (38), (int) std::lround (127 - (k - 1) * 50.0 / cnt) });
                }
            }
        }
    }
    return dedupe (o, 4);
}
Notes genOpen (const Ctx& c, Rng& r)
{
    auto* d = drum (c.model, "oh"); Notes o;
    for (int s : phraseHits (c, r, d, c.cfg.ohMul, {}, 1.2)) o.push_back ({ s * STEP, drumLen (r, d, 2) * STEP, c.dn (46), 100 });
    return o;
}
Notes genPerc (const Ctx& c, Rng& r)
{
    std::vector<int> gm = { r.pick (std::vector<int> { 37, 56, 75 }), r.pick (std::vector<int> { 63, 64, 70 }) };
    Notes o;
    for (int s : phraseHits (c, r, drum (c.model, "perc"), c.cfg.percMul, {}, 1.2))
        o.push_back ({ s * STEP, 2 * STEP, c.s.gmDrums ? r.pick (gm) : 60, r.chance (.15) ? 68 : 100 });
    return o;
}

// ---------- hi-hats: Markov chain over gaps + velocity, trained on the hi-hat MIDI (rolls included) ----------
Notes genHats (const Ctx& c, Rng& r)
{
    const Markov& mk = c.model.hats;
    auto walk = [&] (int bars) {
        std::vector<std::pair<int, int>> hits;
        int t = 0, prev = 0, end = bars * 192, vel = 100;
        while (t < end)
        {
            hits.push_back ({ t, vel });
            const std::map<int, double>* dist = mk.find (std::to_string (prev) + "|" + std::to_string (t % 48));
            if (! dist) dist = mk.find ("*|" + std::to_string (t % 48));
            int tok = dist ? sampleMap (r, *dist, 24 * 4 + 2) : 24 * 4 + 2;
            int vb = tok % 4;
            vel = vb == 0 ? 30 : vb == 1 ? 65 : 100;
            prev = tok; t += std::max (1, tok / 4);
        }
        return hits;
    };
    auto A = walk (4), B = walk (4);
    Notes o;
    for (int b = 0; b < c.s.bars; ++b)
    {
        const auto& src = (b % 8 == 7) ? B : A;
        int srcBar = b % 4;
        for (auto& [t48, v] : src)
            if (t48 >= srcBar * 192 && t48 < (srcBar + 1) * 192)
                o.push_back ({ b * BAR + (t48 - srcBar * 192) * 2, 10, c.dn (42), v });
    }
    return o;
}
} // namespace

// ---------- public ----------
std::vector<int> Generator::progression (const Settings& s) const
{
    Rng r (s.song);
    return r.pick (config (s.style).progs);
}

std::string Generator::chordName (const Settings& s, int d) const
{
    const auto& sc = scales().at (scales().count (s.scale) ? s.scale : "Minor");
    auto at = [&] (int deg) { int o = (int) std::floor (deg / 7.0), i = ((deg % 7) + 7) % 7; return s.key + sc[(size_t) i] + 12 * o; };
    int r0 = at (d), t = at (d + 2) - r0, f = at (d + 4) - r0;
    std::string q = (t == 4 && f == 7) ? "" : (t == 3 && f == 7) ? "m" : (t == 3 && f == 6) ? "dim" : (t == 4 && f == 8) ? "aug" : "";
    return std::string (NOTE_NAMES[((r0 % 12) + 12) % 12]) + q;
}

Notes Generator::generate (const std::string& id, const Settings& s) const
{
    const auto& sc = scales().at (scales().count (s.scale) ? s.scale : "Minor");
    Ctx c { model, s, config (s.style), progression (s), sc };
    std::string seedKey = id;
    for (auto& p : parts()) if (p.id == id) seedKey = p.seedKey;
    uint32_t seed = s.seeds.count (seedKey) ? s.seeds.at (seedKey) : 1u;
    uint32_t mix = seedKey == "groove" ? (seed ^ (s.song * 2654435761u)) : (seed ^ hashStr (id) ^ (s.song * 2654435761u));
    Rng r (mix);
    Notes n;
    if (id == "chords") n = genChords (c, r);
    else if (id == "pad") n = genPad (c, r);
    else if (id == "arp") n = genArp (c, r);
    else if (id == "lead") n = genLead (c, r);
    else if (id == "brass") n = genBrass (c, r);
    else if (id == "808") n = gen808 (c, r);
    else if (id == "kick") n = genKick (c, r);
    else if (id == "hihat") n = genHats (c, r);
    else if (id == "clap") n = genClap (c, r);
    else if (id == "snare") n = genSnare (c, r);
    else if (id == "roll") n = genRoll (c, r);
    else if (id == "openhat") n = genOpen (c, r);
    else if (id == "perc") n = genPerc (c, r);
    Notes o; for (auto& x : n) if (x.t >= 0 && x.t < c.total()) o.push_back (x);
    return o;
}

std::string Generator::fileName (const std::string& id, const Settings& s) const
{
    std::string name = id;
    for (auto& p : parts()) if (p.id == id) name = p.name;
    std::string clean; for (char ch : name) if (ch != ' ' && ch != '-') clean += ch;
    std::string key = NOTE_NAMES[s.key]; if (key.size() > 1) key = key.substr (0, 1) + "s";
    return displayStyle (s.style) + "_" + clean + "_" + key + scaleShort (s.scale) + "_" + std::to_string (s.bpm) + "bpm.mid";
}
} // namespace mf
