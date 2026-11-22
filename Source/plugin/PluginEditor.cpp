#include "PluginEditor.h"

// ---------- one part: name, mix switch, piano roll (drag it into FL), play / new / save ----------
class RollView : public juce::Component
{
public:
    RollView (MidiForgeProcessor& p, std::string partId, bool isDrum) : proc (p), id (std::move (partId)), drum (isDrum)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (ui::soft); g.fillRoundedRectangle (r, 8.0f);
        const auto& notes = proc.pattern[id];
        const int bars = proc.settings.bars, total = bars * mf::BAR;
        g.setColour (ui::line);
        for (int b = 1; b < bars; ++b) g.fillRect ((float) b / bars * r.getWidth(), 4.0f, b % 4 ? 0.5f : 1.0f, r.getHeight() - 8);
        if (! notes.empty())
        {
            int lo = 127, hi = 0;
            for (auto& n : notes) { lo = std::min (lo, n.n); hi = std::max (hi, n.n); }
            if (hi - lo < 12) { int m = (hi + lo) / 2; lo = m - 6; hi = m + 6; }
            const float rowH = (r.getHeight() - 12) / (float) (hi - lo + 1);
            for (auto& n : notes)
            {
                float x = (float) n.t / total * r.getWidth(), w = std::max (2.0f, (float) n.d / total * r.getWidth() - 1);
                float y = r.getHeight() - 6 - (n.n - lo + 1) * rowH;
                g.setColour ((drum ? ui::mute : ui::ink).withAlpha (0.35f + 0.65f * n.v / 127.0f));
                g.fillRoundedRectangle (x, y, w, std::max (2.0f, rowH - 1), 1.0f);
            }
        }
        if (proc.isPlaying())
        {
            auto& ids = proc.playingIds();
            if (std::find (ids.begin(), ids.end(), id) != ids.end())
            {
                g.setColour (ui::ink.withAlpha (0.6f));
                g.fillRect ((float) proc.playPosition() * r.getWidth(), 0.0f, 1.5f, r.getHeight());
            }
        }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 5) return;
        dragging = true;
        auto f = proc.writePartToTemp (id);
        juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, this);
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    MidiForgeProcessor& proc;
    std::string id;
    bool drum, dragging = false;
};

class PartTile : public juce::Component
{
public:
    PartTile (MidiForgeProcessor& p, MidiForgeEditor& ed, const mf::PartInfo& info)
        : proc (p), editor (ed), part (info), roll (p, info.id, info.drum)
    {
        addAndMakeVisible (roll);
        for (auto* b : { &playB, &newB, &saveB }) addAndMakeVisible (b);
        addAndMakeVisible (mix);
        mix.setToggleState (! proc.mixOff.count (part.id), juce::dontSendNotification);
        mix.onClick = [this] {
            if (mix.getToggleState()) proc.mixOff.erase (part.id); else proc.mixOff.insert (part.id);
            if (proc.isPlaying() && proc.playingIds().size() > 1) proc.play (proc.mixIds());
            repaint();
        };
        playB.onClick = [this] {
            bool me = proc.isPlaying() && proc.playingIds().size() == 1 && proc.playingIds()[0] == part.id;
            if (me) proc.stop(); else proc.play ({ part.id });
        };
        newB.onClick = [this] { proc.regeneratePart (part.id); editor.refreshAll(); };
        saveB.onClick = [this] {
            auto f = proc.writePartToTemp (part.id);
            chooser = std::make_unique<juce::FileChooser> ("Save MIDI", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (f.getFileName()), "*.mid");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [this, f] (const juce::FileChooser& fc) {
                    auto dest = fc.getResult();
                    if (dest == juce::File()) return;
                    if (f.copyFileTo (dest.withFileExtension ("mid"))) editor.setStatus ("Saved " + dest.withFileExtension ("mid").getFileName());
                });
        };
    }
    void refresh()
    {
        mix.setToggleState (! proc.mixOff.count (part.id), juce::dontSendNotification);
        bool me = proc.isPlaying() && proc.playingIds().size() == 1 && proc.playingIds()[0] == part.id;
        playB.setButtonText (me ? "Stop" : "Play");
        roll.repaint();
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (ui::panel); g.fillRoundedRectangle (r, 12.0f);
        g.setColour (ui::line);  g.drawRoundedRectangle (r, 12.0f, 1.0f);
        g.setColour (mix.getToggleState() ? ui::ink : ui::mute);
        g.setFont (ui::font (15.0f, true));
        g.drawText (part.name, 14, 10, getWidth() - 100, 20, juce::Justification::centredLeft);
        g.setColour (ui::mute);
        g.setFont (ui::font (11.5f));
        g.drawText (juce::String ((int) proc.pattern[part.id].size()) + " notes", 14, getHeight() - 52, getWidth() - 28, 14, juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (22);
        mix.setBounds (top.removeFromRight (70));
        r.removeFromTop (6);
        auto btns = r.removeFromBottom (30);
        r.removeFromBottom (22);
        roll.setBounds (r);
        int w = (btns.getWidth() - 12) / 3;
        playB.setBounds (btns.removeFromLeft (w)); btns.removeFromLeft (6);
        newB.setBounds (btns.removeFromLeft (w));  btns.removeFromLeft (6);
        saveB.setBounds (btns);
    }

private:
    MidiForgeProcessor& proc;
    MidiForgeEditor& editor;
    mf::PartInfo part;
    RollView roll;
    juce::TextButton playB { "Play" }, newB { "New" }, saveB { "Save" };
    juce::ToggleButton mix { "Mix" };
    std::unique_ptr<juce::FileChooser> chooser;
};

// ---------- editor ----------
MidiForgeEditor::MidiForgeEditor (MidiForgeProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    for (int i = 0; i < 12; ++i) key.addItem (mf::NOTE_NAMES[i], i + 1);
    int id = 1; for (auto& s : mf::scaleNames()) scale.addItem (s, id++);
    for (int b : { 4, 8, 16 }) bars.addItem (juce::String (b) + " bars", b);
    oct.addItem ("808 low", 1); oct.addItem ("808 mid", 2); oct.addItem ("808 high", 3);
    drums.addItem ("Drums on C5", 1); drums.addItem ("GM drum map", 2);
    bpm.setSliderStyle (juce::Slider::LinearBar);
    bpm.setRange (60, 200, 1);
    bpm.setTextValueSuffix (" bpm");
    for (juce::Component* c : std::initializer_list<juce::Component*> { &style, &key, &scale, &bars, &oct, &drums, &bpm, &newIdea, &playAll, &exportAll, &train, &reset })
        addAndMakeVisible (c);
    newIdea.setColour (juce::TextButton::buttonColourId, ui::accent);
    newIdea.setColour (juce::TextButton::textColourOffId, ui::panel);

    style.onChange = [this] {
        if (updating) return;
        int i = style.getSelectedItemIndex();
        if (i >= 0 && i < (int) styleIds.size()) { proc.setStyle (styleIds[(size_t) i]); refreshAll(); }
    };
    key.onChange   = [this] { if (! updating) { proc.settings.key = key.getSelectedId() - 1; afterChange(); } };
    scale.onChange = [this] { if (! updating) { proc.settings.scale = scale.getText().toStdString(); afterChange(); } };
    bars.onChange  = [this] { if (! updating) { proc.settings.bars = bars.getSelectedId(); afterChange(); } };
    oct.onChange   = [this] { if (! updating) { proc.settings.oct = oct.getSelectedId() - 2; afterChange(); } };
    drums.onChange = [this] { if (! updating) { proc.settings.gmDrums = drums.getSelectedId() == 2; afterChange(); } };
    bpm.onValueChange = [this] { if (! updating) { proc.settings.bpm = (int) bpm.getValue(); afterChange(); } };

    newIdea.onClick = [this] { proc.newIdea(); refreshAll(); };
    playAll.onClick = [this] {
        if (proc.isPlaying() && proc.playingIds().size() > 1) proc.stop(); else proc.play (proc.mixIds());
        refreshAll();
    };
    exportAll.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the MIDI kit", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc) {
            auto dir = fc.getResult();
            if (dir == juce::File()) return;
            int n = proc.exportAll (dir);
            setStatus (n > 0 ? "Exported " + juce::String (n) + " MIDI files to " + dir.getFullPathName() : "Could not write to that folder.");
        });
    };
    train.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Pick MIDI files or a folder to learn from", juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.mid;*.midi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories
                              | juce::FileBrowserComponent::canSelectMultipleItems, [this] (const juce::FileChooser& fc) {
            auto files = fc.getResults();
            if (files.isEmpty()) return;
            setStatus ("Learning...");
            auto msg = proc.trainOn (files);
            populateStyles();
            refreshAll();
            setStatus (msg);
        });
    };
    reset.onClick = [this] {
        proc.resetTraining();
        populateStyles();
        refreshAll();
        setStatus ("Back to the factory model.");
    };

    for (auto& part : mf::parts()) addAndMakeVisible (tiles.add (new PartTile (proc, *this, part)));

    populateStyles();
    sendLookAndFeelChange();      // children created before being added need the white/grey look applied
    refreshAll();
    setResizable (true, true);
    setResizeLimits (900, 680, 1800, 1300);
    setSize (1100, 820);
    startTimerHz (30);
}

MidiForgeEditor::~MidiForgeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MidiForgeEditor::populateStyles()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    style.clear (juce::dontSendNotification);
    styleIds = proc.gen.styleNames();
    int i = 1;
    for (auto& s : styleIds) style.addItem (mf::displayStyle (s), i++);
}

void MidiForgeEditor::afterChange()
{
    proc.regenerateAll();
    refreshAll();
}

void MidiForgeEditor::refreshAll()
{
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        auto it = std::find (styleIds.begin(), styleIds.end(), proc.settings.style);
        if (it != styleIds.end()) style.setSelectedItemIndex ((int) (it - styleIds.begin()), juce::dontSendNotification);
        key.setSelectedId (proc.settings.key + 1, juce::dontSendNotification);
        int si = 1; for (auto& s : mf::scaleNames()) { if (s == proc.settings.scale) scale.setSelectedId (si, juce::dontSendNotification); ++si; }
        bars.setSelectedId (proc.settings.bars, juce::dontSendNotification);
        oct.setSelectedId (proc.settings.oct + 2, juce::dontSendNotification);
        drums.setSelectedId (proc.settings.gmDrums ? 2 : 1, juce::dontSendNotification);
        bpm.setValue (proc.settings.bpm, juce::dontSendNotification);
    }
    playAll.setButtonText (proc.isPlaying() && proc.playingIds().size() > 1 ? "Stop" : "Play all");
    for (auto* t : tiles) t->refresh();
    repaint();
}

void MidiForgeEditor::timerCallback()
{
    if (proc.isPlaying() || wasPlaying)
    {
        for (auto* t : tiles) t->refresh();
        playAll.setButtonText (proc.isPlaying() && proc.playingIds().size() > 1 ? "Stop" : "Play all");
    }
    wasPlaying = proc.isPlaying();
}

void MidiForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::ink);
    g.setFont (ui::font (26.0f, true));
    g.drawText ("MIDI Forge", 24, 16, 300, 32, juce::Justification::centredLeft);
    g.setColour (ui::mute);
    g.setFont (ui::font (13.0f));
    auto cfg = proc.gen.config (proc.settings.style);
    g.drawText (juce::String (cfg.about) + "  |  model trained on " + juce::String (proc.model.totalFiles()) + " MIDI files",
                24, 46, getWidth() - 48, 18, juce::Justification::centredLeft);

    // progression chips
    auto prog = proc.gen.progression (proc.settings);
    float x = 24; const float y = 146;
    g.setFont (ui::font (12.5f));
    g.drawText ("Chords", (int) x, (int) y, 60, 26, juce::Justification::centredLeft);
    x += 56;
    g.setFont (ui::font (13.5f, true));
    for (int d : prog)
    {
        juce::String name = proc.gen.chordName (proc.settings, d);
        juce::GlyphArrangement ga;
        ga.addLineOfText (ui::font (13.5f, true), name, 0.0f, 0.0f);
        float w = ga.getBoundingBox (0, -1, true).getWidth() + 24;
        g.setColour (ui::panel); g.fillRoundedRectangle (x, y, w, 26, 13);
        g.setColour (ui::line);  g.drawRoundedRectangle (x, y, w, 26, 13, 1);
        g.setColour (ui::ink);   g.drawText (name, (int) x, (int) y, (int) w, 26, juce::Justification::centred);
        x += w + 6;
    }
    // status line
    g.setColour (ui::mute);
    g.setFont (ui::font (12.5f));
    g.drawText (status.isEmpty() ? juce::String ("Drag any piano roll into FL Studio, or Export all. Train on my MIDI makes it learn from your own files.") : status,
                24, getHeight() - 30, getWidth() - 48, 20, juce::Justification::centredLeft);
}

void MidiForgeEditor::resized()
{
    auto r = getLocalBounds().reduced (24, 0);
    auto row = r.withY (82).withHeight (40);
    style.setBounds (row.removeFromLeft (150)); row.removeFromLeft (8);
    key.setBounds (row.removeFromLeft (70)); row.removeFromLeft (8);
    scale.setBounds (row.removeFromLeft (150)); row.removeFromLeft (8);
    bpm.setBounds (row.removeFromLeft (110)); row.removeFromLeft (8);
    bars.setBounds (row.removeFromLeft (96)); row.removeFromLeft (8);
    oct.setBounds (row.removeFromLeft (104)); row.removeFromLeft (8);
    drums.setBounds (row.removeFromLeft (140));
    auto row2 = r.withY (140).withHeight (36);
    reset.setBounds (row2.removeFromRight (120)); row2.removeFromRight (8);
    train.setBounds (row2.removeFromRight (150)); row2.removeFromRight (8);
    exportAll.setBounds (row2.removeFromRight (110)); row2.removeFromRight (8);
    playAll.setBounds (row2.removeFromRight (96)); row2.removeFromRight (8);
    newIdea.setBounds (row2.removeFromRight (110));

    auto grid = getLocalBounds().reduced (24, 0).withTop (192).withTrimmedBottom (40);
    const int cols = getWidth() > 1300 ? 5 : 4, gap = 12;
    const int rows = ((int) tiles.size() + cols - 1) / cols;
    const int w = (grid.getWidth() - gap * (cols - 1)) / cols, h = (grid.getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (grid.getX() + (i % cols) * (w + gap), grid.getY() + (i / cols) * (h + gap), w, h);
}
