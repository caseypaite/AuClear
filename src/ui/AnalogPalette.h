#pragma once
#include <JuceHeader.h>

/**
 * Runtime colour palette with 4 built-in themes.
 * All components read AP:: values at paint time so a single call to
 * AP::setTheme() + a full repaint switches the entire UI.
 *
 * C++17 inline variables — defined here, no separate .cpp needed for the
 * variable storage.  setTheme() / loadSavedTheme() are defined in
 * AnalogPalette.cpp.
 */

struct ColorTheme
{
    const char* name;
    juce::uint32 kBgDeep, kBgBase, kBgPanel, kBgCard, kBgHover, kBgSelect;
    juce::uint32 kDiv, kDivHi;
    juce::uint32 kAccent, kAccentBr, kAccentDm;
    juce::uint32 kTxtHi, kTxtMid, kTxtLo;
    juce::uint32 kGreen, kAmber, kRed;
    juce::uint32 kKnobBody, kKnobRim, kKnobHi, kKnobLo, kKnobDot;
    juce::uint32 kBand0, kBand1, kBand2, kBand3;
};

namespace AP
{
    // ── theme table ────────────────────────────────────────────────────────────
    static constexpr int kNumThemes = 4;
    extern const ColorTheme kThemes[kNumThemes]; // defined in AnalogPalette.cpp

    // ── theme control ──────────────────────────────────────────────────────────
    void setTheme (int index);   // copies theme → live vars, saves pref
    int  getTheme () noexcept;
    void loadSavedTheme ();      // reads saved pref and applies at startup

    // ── live colour variables (all paint() paths read these directly) ──────────
    // Initialised to Amber (theme 0) — updated by setTheme().
    inline juce::uint32 kBgDeep   = 0xff14100c;
    inline juce::uint32 kBgBase   = 0xff1c1812;
    inline juce::uint32 kBgPanel  = 0xff23201a;
    inline juce::uint32 kBgCard   = 0xff2a2620;
    inline juce::uint32 kBgHover  = 0xff302c24;
    inline juce::uint32 kBgSelect = 0xff2c2010;
    inline juce::uint32 kDiv      = 0xff3d3428;
    inline juce::uint32 kDivHi    = 0xff524638;
    inline juce::uint32 kAccent   = 0xffc8922a;
    inline juce::uint32 kAccentBr = 0xffe8b040;
    inline juce::uint32 kAccentDm = 0xff7a5818;
    inline juce::uint32 kTxtHi    = 0xffede0c4;
    inline juce::uint32 kTxtMid   = 0xffb09870;
    inline juce::uint32 kTxtLo    = 0xff8a7860;
    inline juce::uint32 kGreen    = 0xff4aaa30;
    inline juce::uint32 kAmber    = 0xffc8a030;
    inline juce::uint32 kRed      = 0xffcc3322;
    inline juce::uint32 kKnobBody = 0xff2a2418;
    inline juce::uint32 kKnobRim  = 0xff403830;
    inline juce::uint32 kKnobHi   = 0xff3a3428;
    inline juce::uint32 kKnobLo   = 0xff161210;
    inline juce::uint32 kKnobDot  = 0xfffae8c0;
    inline juce::uint32 kBand0    = 0xff5598ee;
    inline juce::uint32 kBand1    = 0xff28c8a0;
    inline juce::uint32 kBand2    = 0xffddaa20;
    inline juce::uint32 kBand3    = 0xffdd6030;
}
