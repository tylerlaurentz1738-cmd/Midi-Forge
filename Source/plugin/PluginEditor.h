#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Look.h"

class PartTile;

class MidiForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MidiForgeEditor (MidiForgeProcessor&);
    ~MidiForgeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshAll();                       // re-read everything from the processor
    void setStatus (const juce::String& s) { status = s; repaint(); }

private:
    void timerCallback() override;
    void populateStyles();
    void afterChange();                      // regenerate + refresh

    MidiForgeProcessor& proc;
    ui::Look look;
    juce::ComboBox style, key, scale, bars, oct, drums;
    juce::Slider bpm;
    juce::TextButton newIdea { "New idea" }, playAll { "Play all" }, exportAll { "Export all" },
                     train { "Train on my MIDI" }, reset { "Reset training" };
    juce::OwnedArray<PartTile> tiles;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;
    std::vector<std::string> styleIds;
    bool updating = false, wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiForgeEditor)
};
