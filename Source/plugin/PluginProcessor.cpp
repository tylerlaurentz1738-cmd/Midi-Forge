#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../core/DefaultModel.h"
#include "../core/Midi.h"
#include <cmath>

namespace
{
enum Kind { kChords, kPad, kArp, kLead, kBrass, k808, kKick, kSnare, kClap, kHat, kOpenHat, kPerc };
int kindOf (const std::string& id)
{
    static const std::map<std::string, int> k = { { "chords", kChords }, { "pad", kPad }, { "arp", kArp }, { "lead", kLead },
        { "brass", kBrass }, { "808", k808 }, { "kick", kKick }, { "clap", kClap }, { "hihat", kHat }, { "openhat", kOpenHat }, { "perc", kPerc } };
    auto it = k.find (id);
    return it == k.end() ? kSnare : it->second;   // snare + snare roll
}
uint32_t freshSeed() { return (uint32_t) juce::Random::getSystemRandom().nextInt (0x7fffffff); }
double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
}

MidiForgeProcessor::MidiForgeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    auto f = userModelFile();
    if (f.existsAsFile()) userModel.deserialize (f.loadFileAsString().toStdString());
    rebuildModel();
    auto names = gen.styleNames();
    settings.style = names.front();
    settings.scale = gen.config (settings.style).scale;
    settings.bpm = gen.config (settings.style).bpm;
    newIdea();
}

// ---------------- model / training ----------------
juce::File MidiForgeProcessor::userModelFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("MIDI Forge").getChildFile ("user_model.txt");
}

void MidiForgeProcessor::rebuildModel()
{
    model = mf::Model();
    model.deserialize (mf::defaultModelText());
    model.merge (userModel);
}

juce::String MidiForgeProcessor::trainOn (const juce::Array<juce::File>& picked)
{
    juce::Array<juce::File> files;
    for (auto& f : picked)
    {
        if (f.isDirectory()) files.addArray (f.findChildFiles (juce::File::findFiles, true, "*.mid;*.midi;*.MID"));
        else if (f.hasFileExtension ("mid;midi")) files.add (f);
    }
    std::map<std::string, int> counts;
    int used = 0;
    for (auto& f : files)
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb)) continue;
        std::vector<uint8_t> data ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
        std::vector<mf::RawNote> notes; double tempo = 0;
        if (! mf::readMidi (data, notes, tempo) || notes.empty()) continue;
        auto what = mf::trainFile (userModel, f.getFileName().toStdString(), notes, tempo, settings.style);
        counts[what]++; ++used;
    }
    if (used == 0) return "No MIDI files found there.";
    auto out = userModelFile();
    out.getParentDirectory().createDirectory();
    out.replaceWithText (userModel.serialize());
    rebuildModel();
    regenerateAll();
    juce::StringArray parts;
    for (auto& [k, n] : counts) parts.add (juce::String (n) + " " + juce::String (k));
    return "Learned from " + juce::String (used) + " files: " + parts.joinIntoString (", ");
}

void MidiForgeProcessor::resetTraining()
{
    userModel = mf::Model();
    userModelFile().deleteFile();
    rebuildModel();
    auto names = gen.styleNames();
    if (std::find (names.begin(), names.end(), settings.style) == names.end()) setStyle (names.front());
    else regenerateAll();
}

// ---------------- generation ----------------
void MidiForgeProcessor::regenerateAll()
{
    for (auto& p : mf::parts()) pattern[p.id] = gen.generate (p.id, settings);
    pushToAudio();
}

void MidiForgeProcessor::regeneratePart (const std::string& id)
{
    std::string key = id;
    for (auto& p : mf::parts()) if (p.id == id) key = p.seedKey;
    settings.seeds[key] = freshSeed();
    for (auto& p : mf::parts()) if (p.seedKey == key) pattern[p.id] = gen.generate (p.id, settings);
    pushToAudio();
}

void MidiForgeProcessor::newIdea()
{
    settings.song = freshSeed();
    for (auto& p : mf::parts()) settings.seeds[p.seedKey] = freshSeed();
    regenerateAll();
}

void MidiForgeProcessor::setStyle (const std::string& style)
{
    settings.style = style;
    auto c = gen.config (style);
    settings.scale = c.scale;
    settings.bpm = c.bpm;
    regenerateAll();
}

std::vector<std::string> MidiForgeProcessor::mixIds() const
{
    std::vector<std::string> v;
    for (auto& p : mf::parts()) if (! mixOff.count (p.id)) v.push_back (p.id);
    return v;
}

static int midiChannelFor (const std::string& id, const mf::Settings& s)
{
    for (auto& p : mf::parts()) if (p.id == id) return (p.drum && s.gmDrums) ? 9 : 0;
    return 0;
}

juce::File MidiForgeProcessor::writePartToTemp (const std::string& id)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("MIDI Forge");
    dir.createDirectory();
    auto f = dir.getChildFile (gen.fileName (id, settings));
    auto bytes = mf::writeMidi (pattern[id], settings.bpm, settings.bars, gen.fileName (id, settings), midiChannelFor (id, settings));
    f.replaceWithData (bytes.data(), bytes.size());
    return f;
}

int MidiForgeProcessor::exportAll (const juce::File& folder)
{
    auto sub = folder.getNonexistentChildFile (juce::String (mf::displayStyle (settings.style)) + " MIDI Kit " + juce::String (mf::NOTE_NAMES[settings.key]) + " " + juce::String (settings.bpm) + "bpm", "", false);
    if (! sub.createDirectory()) return 0;
    int n = 0;
    for (auto& id : mixIds())
    {
        auto bytes = mf::writeMidi (pattern[id], settings.bpm, settings.bars, gen.fileName (id, settings), midiChannelFor (id, settings));
        if (sub.getChildFile (gen.fileName (id, settings)).replaceWithData (bytes.data(), bytes.size())) ++n;
    }
    sub.revealToUser();
    return n;
}

// ---------------- preview player ----------------
void MidiForgeProcessor::play (const std::vector<std::string>& ids)
{
    playIds = ids;
    pushToAudio();
    playing.store (true);
}

void MidiForgeProcessor::stop()
{
    playing.store (false);
    playIds.clear();
}

void MidiForgeProcessor::pushToAudio()
{
    auto s = std::make_shared<Shared>();
    s->bpm = settings.bpm;
    s->totalTicks = settings.bars * mf::BAR;
    for (auto& id : playIds)
    {
        auto it = pattern.find (id);
        if (it == pattern.end()) continue;
        int k = kindOf (id);
        for (auto& n : it->second) s->events.push_back ({ n.t, n.d, n.n, n.v, k });
    }
    std::sort (s->events.begin(), s->events.end(), [] (const Event& a, const Event& b) { return a.t < b.t; });
    const juce::SpinLock::ScopedLockType sl (lock);
    shared = s;
}

void MidiForgeProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
}

bool MidiForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MidiForgeProcessor::fire (const Shared& s, double from, double to)
{
    for (auto& e : s.events)
    {
        if (e.t >= to) break;
        if (e.t >= from) startVoice (e, s.bpm);
    }
}

void MidiForgeProcessor::startVoice (const Event& e, double bpm)
{
    Voice* v = nullptr;
    for (auto& x : voices) if (! x.on) { v = &x; break; }
    if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.age > v->age) v = &x; }
    *v = Voice();
    v->on = true; v->kind = e.kind;
    v->len = e.d * 60.0 / (bpm * mf::PPQ);
    v->amp = (float) e.v / 127.0f;
    double m = e.n;
    if (e.kind == k808) m -= 24;
    if (e.kind == kPerc) m = 79 + (e.n % 12);
    v->f = mtof (m);
}

float MidiForgeProcessor::renderVoice (Voice& v)
{
    const double dt = 1.0 / sr, a = v.age;
    auto tri = [] (double ph) { return (float) (4.0 * std::abs (ph - std::floor (ph + 0.5)) - 1.0); };
    auto saw = [] (double ph) { return (float) (2.0 * (ph - std::floor (ph)) - 1.0); };
    auto sus = [&] (double attack, double rel) -> float {
        if (a < attack) return (float) (a / attack);
        if (a < v.len) return 1.0f;
        double r = (a - v.len) / rel;
        if (r > 1.5) v.on = false;
        return (float) std::exp (-4.0 * r);
    };
    auto decay = [&] (double t) -> float { if (a > t * 2.5) v.on = false; return (float) std::exp (-a / t * 2.3); };
    float out = 0;
    switch (v.kind)
    {
        case kChords: case kPad:
        {
            bool pad = v.kind == kPad;
            float x = tri (v.ph) * 0.7f + saw (v.ph2) * 0.3f;
            v.lp += (pad ? 0.08f : 0.25f) * (x - v.lp);
            out = v.lp * sus (pad ? 0.3 : 0.005, pad ? 0.5 : 0.2) * (pad ? 0.07f : 0.09f);
            v.ph += v.f * dt; v.ph2 += v.f * 1.004 * dt; break;
        }
        case kArp:
            out = (tri (v.ph) + 0.3f * (float) std::sin (2 * juce::MathConstants<double>::pi * v.ph * 2)) * decay (0.12) * 0.12f;
            v.ph += v.f * dt; break;
        case kLead:
            out = ((float) std::sin (2 * juce::MathConstants<double>::pi * v.ph) + 0.35f * (float) std::sin (2 * juce::MathConstants<double>::pi * v.ph2))
                  * (float) std::exp (-a * 2.2) * sus (0.002, 0.15) * 0.13f;
            v.ph += v.f * dt; v.ph2 += v.f * 3.01 * dt; break;
        case kBrass:
        {
            float x = saw (v.ph) + saw (v.ph2);
            float cut = (float) juce::jlimit (0.05, 0.6, 0.08 + 0.5 * std::exp (-a * 8));
            v.lp += cut * (x - v.lp);
            out = v.lp * sus (0.01, 0.1) * 0.06f;
            v.ph += v.f * 0.997 * dt; v.ph2 += v.f * 1.003 * dt; break;
        }
        case k808:
        {
            double f = v.f * (1.0 + std::exp (-a / 0.015));
            out = (float) std::tanh (3.0 * std::sin (2 * juce::MathConstants<double>::pi * v.ph)) * sus (0.001, 0.03) * 0.33f;
            v.ph += f * dt; break;
        }
        case kKick:
        {
            double f = 48.0 + 120.0 * std::exp (-a / 0.03);
            out = (float) std::sin (2 * juce::MathConstants<double>::pi * v.ph) * decay (0.12) * 0.6f;
            v.ph += f * dt; break;
        }
        case kSnare: case kClap:
        {
            float n = rnd();
            v.lp += 0.5f * (n - v.lp);              // low-pass
            float band = v.lp - (v.lp2 += 0.08f * (v.lp - v.lp2));   // minus a slower low-pass = band-pass
            out = band * decay (v.kind == kClap ? 0.09 : 0.06) * 0.5f;
            break;
        }
        case kHat: case kOpenHat:
        {
            float n = rnd();
            v.lp += 0.6f * (n - v.lp);
            out = (n - v.lp) * decay (v.kind == kHat ? 0.018 : 0.14) * 0.28f;
            break;
        }
        case kPerc:
            out = tri (v.ph) * decay (0.035) * 0.18f;
            v.ph += v.f * dt; break;
        default: v.on = false;
    }
    v.age += dt;
    return out * v.amp;
}

void MidiForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    buffer.clear();
    std::shared_ptr<Shared> s;
    {
        const juce::SpinLock::ScopedTryLockType tl (lock);
        if (tl.isLocked()) s = shared;
    }
    const bool isOn = playing.load();
    if (isOn && ! wasPlaying) tickPos = 0;
    wasPlaying = isOn;

    const int n = buffer.getNumSamples();
    const int chans = buffer.getNumChannels();
    for (int start = 0; start < n; start += 32)
    {
        const int len = std::min (32, n - start);
        if (isOn && s && s->totalTicks > 0)
        {
            const double perSample = s->bpm * mf::PPQ / 60.0 / sr;
            double t0 = tickPos, t1 = tickPos + perSample * len;
            if (t1 >= s->totalTicks) { fire (*s, t0, s->totalTicks); t1 -= s->totalTicks; fire (*s, 0, t1); }
            else fire (*s, t0, t1);
            tickPos = t1;
            position.store (tickPos / s->totalTicks);
        }
        for (int i = 0; i < len; ++i)
        {
            float x = 0;
            for (auto& v : voices) if (v.on) x += renderVoice (v);
            x = std::tanh (x * 1.2f);
            for (int c = 0; c < chans; ++c) buffer.setSample (c, start + i, x);
        }
    }
}

// ---------------- state ----------------
void MidiForgeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement x ("MIDIFORGE");
    x.setAttribute ("style", settings.style); x.setAttribute ("key", settings.key); x.setAttribute ("scale", settings.scale);
    x.setAttribute ("bpm", settings.bpm); x.setAttribute ("bars", settings.bars); x.setAttribute ("oct", settings.oct);
    x.setAttribute ("gm", settings.gmDrums); x.setAttribute ("song", (int) settings.song);
    for (auto& [k, v] : settings.seeds) x.setAttribute ("seed_" + juce::String (k), (int) v);
    juce::StringArray off; for (auto& m : mixOff) off.add (m);
    x.setAttribute ("mixOff", off.joinIntoString (","));
    copyXmlToBinary (x, dest);
}

void MidiForgeProcessor::setStateInformation (const void* data, int size)
{
    auto x = getXmlFromBinary (data, size);
    if (x == nullptr || ! x->hasTagName ("MIDIFORGE")) return;
    settings.style = x->getStringAttribute ("style", settings.style).toStdString();
    settings.key = x->getIntAttribute ("key", settings.key);
    settings.scale = x->getStringAttribute ("scale", settings.scale).toStdString();
    settings.bpm = x->getIntAttribute ("bpm", settings.bpm);
    settings.bars = x->getIntAttribute ("bars", settings.bars);
    settings.oct = x->getIntAttribute ("oct", settings.oct);
    settings.gmDrums = x->getBoolAttribute ("gm", settings.gmDrums);
    settings.song = (uint32_t) x->getIntAttribute ("song", (int) settings.song);
    for (auto& p : mf::parts())
        settings.seeds[p.seedKey] = (uint32_t) x->getIntAttribute ("seed_" + juce::String (p.seedKey), (int) settings.seeds[p.seedKey]);
    mixOff.clear();
    for (auto& m : juce::StringArray::fromTokens (x->getStringAttribute ("mixOff"), ",", "")) if (m.isNotEmpty()) mixOff.insert (m.toStdString());
    regenerateAll();
    if (auto* ed = dynamic_cast<MidiForgeEditor*> (getActiveEditor())) ed->refreshAll();
}

juce::AudioProcessorEditor* MidiForgeProcessor::createEditor() { return new MidiForgeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MidiForgeProcessor(); }
