#pragma once
#include <JuceHeader.h>

// Shared warm-analog colour palette — include everywhere UI colours are needed.
// All values are 0xAARRGGBB.
namespace AP
{
    // ── Backgrounds ──────────────────────────────────────────────────────────
    constexpr juce::uint32 kBgDeep   = 0xff14100c;  // deepest backdrop
    constexpr juce::uint32 kBgBase   = 0xff1c1812;  // default panel bg
    constexpr juce::uint32 kBgPanel  = 0xff23201a;  // raised sections
    constexpr juce::uint32 kBgCard   = 0xff2a2620;  // rack cards, columns
    constexpr juce::uint32 kBgHover  = 0xff302c24;  // hover
    constexpr juce::uint32 kBgSelect = 0xff2c2010;  // selected (warm tint)

    // ── Structure ─────────────────────────────────────────────────────────────
    constexpr juce::uint32 kDiv      = 0xff3d3428;
    constexpr juce::uint32 kDivHi    = 0xff524638;

    // ── Accent — amber/gold ───────────────────────────────────────────────────
    constexpr juce::uint32 kAccent   = 0xffc8922a;  // primary amber
    constexpr juce::uint32 kAccentBr = 0xffe8b040;  // bright / active
    constexpr juce::uint32 kAccentDm = 0xff7a5818;  // dim / inactive

    // ── Text ─────────────────────────────────────────────────────────────────
    constexpr juce::uint32 kTxtHi    = 0xffede0c4;  // warm cream
    constexpr juce::uint32 kTxtMid   = 0xffb09870;
    constexpr juce::uint32 kTxtLo    = 0xff8a7860;

    // ── VU-meter colours ─────────────────────────────────────────────────────
    constexpr juce::uint32 kGreen    = 0xff4aaa30;
    constexpr juce::uint32 kAmber    = 0xffc8a030;
    constexpr juce::uint32 kRed      = 0xffcc3322;

    // ── Knob internals ────────────────────────────────────────────────────────
    constexpr juce::uint32 kKnobBody = 0xff2a2418;
    constexpr juce::uint32 kKnobRim  = 0xff403830;
    constexpr juce::uint32 kKnobHi   = 0xff3a3428;
    constexpr juce::uint32 kKnobLo   = 0xff161210;
    constexpr juce::uint32 kKnobDot  = 0xfffae8c0;  // indicator line (warm white)

    // ── Band colours (for multi-band panels) ─────────────────────────────────
    constexpr juce::uint32 kBand0    = 0xff5598ee;  // mid blue
    constexpr juce::uint32 kBand1    = 0xff28c8a0;  // warm teal
    constexpr juce::uint32 kBand2    = 0xffddaa20;  // amber
    constexpr juce::uint32 kBand3    = 0xffdd6030;  // warm orange
}
