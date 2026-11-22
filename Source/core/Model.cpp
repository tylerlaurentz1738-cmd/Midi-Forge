#include "Model.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace mf
{
// ---------- text format (one record per line) ----------
static void writeMap (std::ostringstream& o, const std::map<int, double>& m)
{
    for (auto& [k, v] : m) o << ' ' << k << ':' << v;
}
static void writeMarkov (std::ostringstream& o, const std::string& tag, const std::string& style, const Markov& mk)
{
    for (auto& [state, m] : mk.table)
    {
        o << tag;
        if (! style.empty()) o << ' ' << style;
        o << ' ' << state;
        writeMap (o, m);
        o << '\n';
    }
}

std::string Model::serialize() const
{
    std::ostringstream o;
    o.precision (6);
    o << "MIDIFORGE 1\n";
    for (auto& [name, s] : styles)
    {
        o << "S " << name << ' ' << s.files << ' ' << s.bars << ' ' << s.trip << ' ' << s.durSum << ' ' << s.durN << ' ' << s.bpmSum << '\n';
        o << "SD " << name; for (double x : s.deg) o << ' ' << x; o << '\n';
        o << "SP " << name; for (double x : s.pos) o << ' ' << x; o << '\n';
        o << "SI " << name; writeMap (o, s.iv); o << '\n';
        writeMarkov (o, "SR", name, s.rhythm);
        writeMarkov (o, "SM", name, s.pitch);
    }
    for (auto& [kind, d] : drums)
    {
        o << "D " << kind << ' ' << d.files << ' ' << d.cycles; for (double x : d.pos) o << ' ' << x; o << '\n';
        o << "DL " << kind; writeMap (o, d.len); o << '\n';
    }
    o << "HF " << hatFiles << '\n';
    writeMarkov (o, "H", "", hats);
    return o.str();
}

static void readMap (std::istringstream& in, std::map<int, double>& m)
{
    std::string kv;
    while (in >> kv)
    {
        auto c = kv.find (':');
        if (c == std::string::npos) continue;
        m[std::stoi (kv.substr (0, c))] += std::stod (kv.substr (c + 1));
    }
}

bool Model::deserialize (const std::string& text)
{
    std::istringstream all (text);
    std::string line;
    if (! std::getline (all, line) || line.rfind ("MIDIFORGE", 0) != 0) return false;
    try
    {
        while (std::getline (all, line))
        {
            std::istringstream in (line);
            std::string tag, name; in >> tag;
            if (tag == "S") { in >> name; auto& s = styles[name]; in >> s.files >> s.bars >> s.trip >> s.durSum >> s.durN >> s.bpmSum; }
            else if (tag == "SD") { in >> name; for (double& x : styles[name].deg) in >> x; }
            else if (tag == "SP") { in >> name; for (double& x : styles[name].pos) in >> x; }
            else if (tag == "SI") { in >> name; readMap (in, styles[name].iv); }
            else if (tag == "SR" || tag == "SM") { std::string st; in >> name >> st; readMap (in, (tag == "SR" ? styles[name].rhythm : styles[name].pitch).table[st]); }
            else if (tag == "D") { in >> name; auto& d = drums[name]; in >> d.files >> d.cycles; for (double& x : d.pos) in >> x; }
            else if (tag == "DL") { in >> name; readMap (in, drums[name].len); }
            else if (tag == "HF") in >> hatFiles;
            else if (tag == "H") { std::string st; in >> st; readMap (in, hats.table[st]); }
        }
    }
    catch (...) { return false; }
    return true;
}

static void mergeMarkov (Markov& a, const Markov& b)
{
    for (auto& [s, m] : b.table) for (auto& [k, v] : m) a.table[s][k] += v;
}

void Model::merge (const Model& o)
{
    for (auto& [name, s] : o.styles)
    {
        auto& t = styles[name];
        t.files += s.files; t.bars += s.bars; t.trip += s.trip; t.durSum += s.durSum; t.durN += s.durN; t.bpmSum += s.bpmSum;
        for (int i = 0; i < 12; ++i) t.deg[i] += s.deg[i];
        for (int i = 0; i < 16; ++i) t.pos[i] += s.pos[i];
        for (auto& [k, v] : s.iv) t.iv[k] += v;
        mergeMarkov (t.rhythm, s.rhythm); mergeMarkov (t.pitch, s.pitch);
    }
    for (auto& [kind, d] : o.drums)
    {
        auto& t = drums[kind];
        t.files += d.files; t.cycles += d.cycles;
        for (int i = 0; i < 32; ++i) t.pos[i] += d.pos[i];
        for (auto& [k, v] : d.len) t.len[k] += v;
    }
    mergeMarkov (hats, o.hats); hatFiles += o.hatFiles;
}

int Model::totalFiles() const
{
    double n = hatFiles;
    for (auto& [k, s] : styles) n += s.files;
    for (auto& [k, d] : drums) n += d.files;
    return (int) n;
}

// ---------- training ----------
static std::string lower (std::string s) { for (auto& c : s) c = (char) std::tolower ((unsigned char) c); return s; }

std::string canonicalStyle (std::string s)
{
    s = lower (s);
    std::string out;
    for (char c : s) if (std::isalnum ((unsigned char) c)) out += c;
    if (out == "digicore") return "hyperpop";
    if (out == "slay" || out == "kan" || out == "batonrouge" || out == "rage") return out == "rage" ? "rage" : "trap";
    if (out == "wholelottared") return "wlr";
    return out.empty() ? "trap" : out;
}

Kind classifyFile (const std::string& fileName, std::string& style)
{
    std::string f = lower (fileName);
    auto slash = f.find_last_of ("/\\");
    if (slash != std::string::npos) f = f.substr (slash + 1);
    auto starts = [&] (const char* p) { return f.rfind (p, 0) == 0; };
    auto has = [&] (const char* p) { return f.find (p) != std::string::npos; };
    style.clear();
    if (! f.empty() && f[0] == '(')
    {
        auto e = f.find (')');
        if (e != std::string::npos) style = canonicalStyle (f.substr (1, e - 1));
        return Kind::Melody;
    }
    if (starts ("808") || has (" 808") || has ("bass")) return Kind::Bass808;
    if (starts ("oh") || has ("open hat") || has ("openhat") || has ("open hh")) return Kind::OpenHat;
    if (starts ("hh") || has ("hihat") || has ("hi hat") || has ("hi-hat") || starts ("hat")) return Kind::HiHat;
    if (starts ("clap")) return Kind::Clap;
    if (starts ("snare") || starts ("rim")) return Kind::Snare;
    if (starts ("perc") || has ("shaker") || has ("bell")) return Kind::Perc;
    if (starts ("kick")) return Kind::Kick;
    return Kind::Melody;
}

static int tonicOf (const double h[12])
{
    static const double MIN[12] = { 6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17 };
    static const double MAJ[12] = { 6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88 };
    double best = -1; int tonic = 0;
    for (int r = 0; r < 12; ++r)
        for (int mi = 0; mi < 2; ++mi)
        {
            double s = 0;
            for (int i = 0; i < 12; ++i) s += h[(r + i) % 12] * (mi ? MIN[i] : MAJ[i]);
            if (s > best) { best = s; tonic = mi ? r : (r + 9) % 12; }   // major keys -> relative minor tonic
        }
    return tonic;
}

static const int DEG_OF_PC[12] = { 0,1,1,2,2,3,3,4,5,5,6,6 };   // semitone above tonic -> minor-scale degree

static void trainMelody (StyleCounts& s, const std::vector<RawNote>& notes, double tempo)
{
    double h[12] = {}, len = 0;
    for (auto& n : notes) { h[((n.pitch % 12) + 12) % 12] += n.len; len = std::max (len, n.beat + n.len); }
    int T = tonicOf (h);
    // top voice per onset, on a 48ths-of-a-beat grid
    std::map<int, std::pair<int, double>> top;
    for (auto& n : notes)
    {
        // snap to the nearest 16th or 16th-triplet so slightly-off timing does not create fake rhythms
        double x = n.beat * 48;
        int a12 = (int) std::lround (x / 12) * 12, a8 = (int) std::lround (x / 8) * 8;
        int o = std::abs (x - a12) <= std::abs (x - a8) ? a12 : a8;
        auto it = top.find (o);
        if (it == top.end() || n.pitch > it->second.first) top[o] = { n.pitch, std::max (n.len, it == top.end() ? 0.0 : it->second.second) };
        s.deg[((n.pitch - T) % 12 + 12) % 12] += n.len;
    }
    if (top.size() < 2) return;
    s.files += 1; s.bars += std::max (1.0, len / 4); s.bpmSum += tempo;
    std::vector<int> on; std::vector<int> pitch;
    for (auto& [o, p] : top)
    {
        if (! on.empty() && o - on.back() < 8) continue;   // layered parts: ignore onsets closer than a 16th-triplet
        on.push_back (o); pitch.push_back (p.first);
        if (o % 12 == 0) s.pos[(o / 12) % 16] += 1; else s.trip += 1;
        s.durSum += p.second; s.durN += 1;
    }
    int prevGap = 0, prevMove = 0;
    auto absDeg = [&] (int p) { int rel = p - T; int oct = (int) std::floor (rel / 12.0); return oct * 7 + DEG_OF_PC[((rel % 12) + 12) % 12]; };
    for (size_t i = 0; i + 1 < on.size(); ++i)
    {
        int gap = std::min (96, on[i + 1] - on[i]);
        int p48 = ((on[i] % 48) + 48) % 48;
        s.rhythm.add (std::to_string (prevGap) + "|" + std::to_string (p48), gap);
        s.rhythm.add ("*|" + std::to_string (p48), gap);
        prevGap = gap;
        int semis = std::clamp (pitch[i + 1] - pitch[i], -12, 12);
        s.iv[semis] += 1;
        int d0 = absDeg (pitch[i]), move = std::clamp (absDeg (pitch[i + 1]) - d0, -9, 9);
        std::string dg = std::to_string (((d0 % 7) + 7) % 7);
        s.pitch.add (std::to_string (prevMove) + "|" + dg, move);
        s.pitch.add ("*|" + dg, move);
        prevMove = move;
    }
}

static void trainDrum (DrumCounts& d, const std::vector<RawNote>& notes, bool is808)
{
    double len = 0; bool hasLoud = false;
    for (auto& n : notes) { len = std::max (len, n.beat + n.len); if (n.vel >= 120) hasLoud = true; }
    d.files += 1; d.cycles += std::max (8.0, len) / 8.0;
    for (auto& n : notes)
    {
        if (is808 && hasLoud && n.vel >= 95 && n.vel <= 105) continue;   // slide notes (velocity ~80%) are not rhythm hits
        double st = n.beat * 4;
        if (std::abs (st - std::round (st)) > 0.01) continue;
        d.pos[((int) std::lround (st)) % 32] += 1;
        d.len[std::clamp ((int) std::lround (n.len * 4), 1, 16)] += 1;
    }
}

static int velBucket (int v) { return v < 40 ? 0 : v < 80 ? 1 : 2; }

static void trainHats (Model& m, const std::vector<RawNote>& notes)
{
    std::map<int, int> hits;   // onset in 48ths -> loudest velocity
    for (auto& n : notes) { int o = (int) std::lround (n.beat * 48); hits[o] = std::max (hits[o], n.vel); }
    if (hits.size() < 4) return;
    m.hatFiles += 1;
    std::vector<std::pair<int, int>> h (hits.begin(), hits.end());
    int prevTok = 0;
    for (size_t i = 0; i + 1 < h.size(); ++i)
    {
        int gap = std::min (96, h[i + 1].first - h[i].first);
        int tok = gap * 4 + velBucket (h[i + 1].second);
        int p48 = ((h[i].first % 48) + 48) % 48;
        m.hats.add (std::to_string (prevTok) + "|" + std::to_string (p48), tok);
        m.hats.add ("*|" + std::to_string (p48), tok);
        prevTok = tok;
    }
}

std::string trainFile (Model& m, const std::string& fileName, const std::vector<RawNote>& notes, double tempo, const std::string& fallbackStyle)
{
    if (notes.empty()) return "skipped (no notes)";
    std::string style;
    switch (classifyFile (fileName, style))
    {
        case Kind::HiHat:   trainHats (m, notes); return "hi-hat";
        case Kind::OpenHat: trainDrum (m.drums["oh"], notes, false); return "open hat";
        case Kind::Clap:    trainDrum (m.drums["clap"], notes, false); return "clap";
        case Kind::Snare:   trainDrum (m.drums["snare"], notes, false); return "snare";
        case Kind::Perc:    trainDrum (m.drums["perc"], notes, false); return "perc";
        case Kind::Bass808: trainDrum (m.drums["808"], notes, true); return "808";
        case Kind::Kick:    trainDrum (m.drums["808"], notes, false); return "kick (808 groove)";
        case Kind::Melody:
        default:
        {
            if (style.empty()) style = canonicalStyle (fallbackStyle);
            trainMelody (m.styles[style], notes, tempo);
            return "melody (" + style + ")";
        }
    }
}
} // namespace mf
