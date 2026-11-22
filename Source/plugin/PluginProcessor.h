#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <set>
#include "../core/Generator.h"

class MidiForgeProcessor : public juce::AudioProcessor
{
public:
    MidiForgeProcessor();
    ~MidiForgeProcessor() override = default;

    // ---- juce::AudioProcessor ----
    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MIDI Forge"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- engine (message thread only) ----
    mf::Model model;                 // factory model + everything you trained
    mf::Model userModel;             // only what you trained (saved to disk)
    mf::Generator gen { model };
    mf::Settings settings;
    std::map<std::string, mf::Notes> pattern;
    std::set<std::string> mixOff;

    void regenerateAll();
    void regeneratePart (const std::string& id);
    void newIdea();
    void setStyle (const std::string& style);
    juce::String trainOn (const juce::Array<juce::File>& files);
    void resetTraining();
    juce::File writePartToTemp (const std::string& id);
    int exportAll (const juce::File& folder);
    std::vector<std::string> mixIds() const;

    // ---- preview player ----
    void play (const std::vector<std::string>& ids);
    void stop();
    bool isPlaying() const { return playing.load(); }
    const std::vector<std::string>& playingIds() const { return playIds; }
    double playPosition() const { return position.load(); }

private:
    void rebuildModel();
    juce::File userModelFile() const;
    void pushToAudio();

    struct Event { int t, d, n, v, kind; };
    struct Shared { std::vector<Event> events; int totalTicks = 0; double bpm = 120; };
    juce::SpinLock lock;
    std::shared_ptr<Shared> shared;          // swapped under `lock`
    std::atomic<bool> playing { false };
    std::atomic<double> position { 0.0 };
    std::vector<std::string> playIds;

    // audio thread
    struct Voice
    {
        bool on = false; int kind = 0;
        double age = 0, len = 0, f = 0, ph = 0, ph2 = 0;
        float amp = 0, lp = 0, lp2 = 0;
    };
    std::array<Voice, 96> voices;
    double sr = 44100.0, tickPos = 0.0;
    uint32_t noise = 22222;
    bool wasPlaying = false;
    void fire (const Shared& s, double from, double to);
    void startVoice (const Event& e, double bpm);
    float renderVoice (Voice& v);
    float rnd() { noise = noise * 1664525u + 1013904223u; return (float) ((noise >> 8) & 0xFFFF) / 32768.0f - 1.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiForgeProcessor)
};
