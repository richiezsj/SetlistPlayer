#pragma once
#include <JuceHeader.h>
#include "Theme.h"

// ===========================================================================
// AppLookAndFeel — a flat, macOS-style look applied app-wide. Draws buttons,
// combo boxes and text fields as flat rounded rectangles (no gradients or
// bevels), uses the system font, and pulls every colour from Theme. Set as the
// default LookAndFeel so all standard controls match without per-control code.
// ===========================================================================
class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AppLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, Theme::windowBg);

        setColour(juce::TextButton::buttonColourId,   Theme::controlBg);
        setColour(juce::TextButton::buttonOnColourId, Theme::accent);
        setColour(juce::TextButton::textColourOffId,  Theme::textPrimary);
        setColour(juce::TextButton::textColourOnId,   juce::Colours::white);

        setColour(juce::ComboBox::backgroundColourId, Theme::controlBg);
        setColour(juce::ComboBox::textColourId,       Theme::textPrimary);
        setColour(juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
        setColour(juce::ComboBox::arrowColourId,      Theme::textSecondary);

        setColour(juce::PopupMenu::backgroundColourId,            Theme::panelBg);
        setColour(juce::PopupMenu::textColourId,                  Theme::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accent);
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

        setColour(juce::TextEditor::backgroundColourId,     Theme::controlBg);
        setColour(juce::TextEditor::textColourId,           Theme::textPrimary);
        setColour(juce::TextEditor::outlineColourId,        juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
        setColour(juce::TextEditor::highlightColourId,      Theme::accent.withAlpha(0.30f));
        setColour(juce::CaretComponent::caretColourId,      Theme::accent);

        setColour(juce::Label::textColourId, Theme::textPrimary);

        setColour(juce::Slider::backgroundColourId,      Theme::controlBg);
        setColour(juce::Slider::trackColourId,           Theme::accent);
        setColour(juce::Slider::thumbColourId,           Theme::textPrimary);
        setColour(juce::Slider::textBoxTextColourId,     Theme::textPrimary);
        setColour(juce::Slider::textBoxBackgroundColourId, Theme::controlBg);
        setColour(juce::Slider::textBoxOutlineColourId,  juce::Colours::transparentBlack);

        setColour(juce::ScrollBar::thumbColourId, Theme::controlBgHi);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return Theme::font(juce::jmin(15.0f, buttonHeight * 0.5f + 3.0f));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawHighlighted, bool shouldDrawDown) override
    {
        auto bounds = b.getLocalBounds().toFloat().reduced(0.5f);
        auto c = backgroundColour;
        if (shouldDrawDown)             c = c.darker(0.18f);
        else if (shouldDrawHighlighted) c = c.brighter(0.08f);

        g.setColour(c);
        g.fillRoundedRectangle(bounds, Theme::radius);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, Theme::radius);

        // Chevron
        juce::Path p;
        const float cx = width - 15.0f, cy = height * 0.5f;
        p.startNewSubPath(cx - 4.0f, cy - 2.0f);
        p.lineTo(cx,        cy + 3.0f);
        p.lineTo(cx + 4.0f, cy - 2.0f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return Theme::font(13.0f); }
    juce::Font getPopupMenuFont() override               { return Theme::font(13.0f); }
    juce::Font getLabelFont(juce::Label& l) override
    {
        // Respect an explicitly-set font; otherwise use the system font.
        return l.getFont();
    }
};
