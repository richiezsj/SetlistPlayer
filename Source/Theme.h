#pragma once
#include <JuceHeader.h>

// ===========================================================================
// Theme — a small design-token system aligned with the macOS Human Interface
// Guidelines: neutral dark greys, a single system-blue accent, hairline
// separators, the system font, and an 8-pt spacing rhythm. Every component
// pulls its colours, metrics and fonts from here so the UI stays consistent.
// ===========================================================================
namespace Theme
{
    // --- Surfaces (Apple dark system backgrounds) ---
    inline const juce::Colour windowBg   { 0xFF1C1C1E }; // window
    inline const juce::Colour panelBg     { 0xFF2C2C2E }; // elevated card
    inline const juce::Colour panelBgAlt  { 0xFF252527 }; // alternate row
    inline const juce::Colour controlBg   { 0xFF3A3A3C }; // buttons / fields
    inline const juce::Colour controlBgHi { 0xFF48484A }; // hover / pressed
    inline const juce::Colour separator   { 0x1AFFFFFF }; // white @ 10%

    // --- Text (Apple label hierarchy) ---
    inline const juce::Colour textPrimary   { 0xFFF2F2F7 };
    inline const juce::Colour textSecondary { 0x99EBEBF5 }; // ~60%
    inline const juce::Colour textTertiary  { 0x4DEBEBF5 }; // ~30%

    // --- Accent + semantic ---
    inline const juce::Colour accent        { 0xFF0A84FF }; // system blue (dark)
    inline const juce::Colour accentPressed { 0xFF0060DF };
    inline const juce::Colour destructive   { 0xFFFF453A }; // system red
    inline const juce::Colour audioAccent   { 0xFF0A84FF }; // Audio channel
    inline const juce::Colour clickAccent    { 0xFFFF9F0A }; // Click channel (orange)

    // --- VU meter (system greens/yellows/reds) ---
    inline const juce::Colour meterGreen { 0xFF30D158 };
    inline const juce::Colour meterAmber { 0xFFFFD60A };
    inline const juce::Colour meterRed   { 0xFFFF453A };

    // --- Metrics (8-pt rhythm) ---
    constexpr float radius       = 7.0f;
    constexpr float radiusLarge  = 10.0f;
    constexpr int   pad          = 16;   // panel inset
    constexpr int   gap          = 8;    // between controls
    constexpr int   controlH     = 28;   // standard control height

    // --- Fonts (system UI font via FontOptions) ---
    inline juce::Font font(float height)
    {
        return juce::Font(juce::FontOptions().withHeight(height));
    }
    inline juce::Font fontBold(float height)
    {
        return juce::Font(juce::FontOptions().withHeight(height).withStyle("Bold"));
    }
    // Uppercase section header: bold, small, slightly tracked, tertiary colour.
    inline juce::Font sectionFont()
    {
        return fontBold(11.0f).withExtraKerningFactor(0.08f);
    }
}
