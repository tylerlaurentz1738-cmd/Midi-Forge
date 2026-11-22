#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Simple white + grey look.
namespace ui
{
const juce::Colour bg     { 0xfff2f2f0 };   // window
const juce::Colour panel  { 0xffffffff };   // cards, controls
const juce::Colour soft   { 0xfff6f6f4 };   // piano roll background, hovers
const juce::Colour line   { 0xffe2e2de };   // borders, grid
const juce::Colour ink    { 0xff1d1d1f };   // main text, notes
const juce::Colour mute   { 0xff8d8d89 };   // secondary text
const juce::Colour accent { 0xff2b2b2d };   // primary button

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, ink);
        setColour (juce::ComboBox::arrowColourId, mute);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, soft.darker (0.03f));
        setColour (juce::PopupMenu::highlightedTextColourId, ink);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOffId, ink);
        setColour (juce::TextButton::textColourOnId, ink);
        setColour (juce::Label::textColourId, ink);
        setColour (juce::ToggleButton::textColourId, mute);
        setColour (juce::Slider::textBoxTextColourId, ink);
        setColour (juce::Slider::textBoxBackgroundColourId, panel);
        setColour (juce::Slider::textBoxOutlineColourId, line);
        setColour (juce::Slider::trackColourId, soft.darker (0.06f));
        setColour (juce::Slider::backgroundColourId, panel);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = base;
        if (down) c = c.darker (0.08f); else if (over) c = c.darker (0.04f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 8.0f);
        if (base != accent) { g.setColour (line); g.drawRoundedRectangle (r, 8.0f, 1.0f); }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (13.5f, true); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (panel); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (line);  g.drawRoundedRectangle (r, 8.0f, 1.0f);
        juce::Path p;
        float cx = (float) w - 15.0f, cy = (float) h * 0.5f;
        p.startNewSubPath (cx - 4, cy - 2); p.lineTo (cx, cy + 2); p.lineTo (cx + 4, cy - 2);
        g.setColour (mute); g.strokePath (p, juce::PathStrokeType (1.5f));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (14.0f, true); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
    {
        l.setBounds (8, 0, box.getWidth() - 28, box.getHeight());
        l.setFont (getComboBoxFont (box));
    }
    juce::Font getPopupMenuFont() override { return font (14.0f); }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto track = juce::Rectangle<float> (r.getRight() - 30, r.getCentreY() - 8, 28, 16);
        g.setColour (b.getToggleState() ? accent : line);
        g.fillRoundedRectangle (track, 8.0f);
        float x = b.getToggleState() ? track.getRight() - 14 : track.getX() + 2;
        g.setColour (panel);
        g.fillEllipse (x, track.getY() + 2, 12, 12);
        g.setColour (mute);
        g.setFont (font (12.0f));
        g.drawText (b.getButtonText(), r.withTrimmedRight (34), juce::Justification::centredRight);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
        g.setColour (panel); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (soft.darker (0.03f));
        g.fillRoundedRectangle (r.withRight (juce::jmax (r.getX() + 8.0f, pos)), 8.0f);
        g.setColour (line); g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (14.0f, true));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, ink);
        l->setColour (juce::Label::textWhenEditingColourId, ink);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return l;
    }
};
} // namespace ui
