#include "AnalogPalette.h"

// ─────────────────────────────────────────────────────────────────────────────
// Theme 0 — Amber  (warm analog hardware; the original default)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ColorTheme themeAmber {
    "Amber",
    0xff14100c, 0xff1c1812, 0xff23201a, 0xff2a2620, 0xff302c24, 0xff2c2010, // bg
    0xff3d3428, 0xff524638,                                                   // div
    0xffc8922a, 0xffe8b040, 0xff7a5818,                                       // accent
    0xffede0c4, 0xffb09870, 0xff8a7860,                                       // text
    0xff4aaa30, 0xffc8a030, 0xffcc3322,                                       // vu
    0xff2a2418, 0xff403830, 0xff3a3428, 0xff161210, 0xfffae8c0,               // knob
    0xff5598ee, 0xff28c8a0, 0xffddaa20, 0xffdd6030                            // bands
};

// ─────────────────────────────────────────────────────────────────────────────
// Theme 1 — Silver  (brushed-aluminium studio rack)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ColorTheme themeSilver {
    "Silver",
    0xff0c0d0f, 0xff131518, 0xff1b1e22, 0xff222428, 0xff2c2e34, 0xff1e2030, // bg
    0xff353840, 0xff484c58,                                                   // div
    0xff8899b0, 0xffc8d8e8, 0xff485868,                                       // accent
    0xffeef2f8, 0xff9ab0c8, 0xff687888,                                       // text
    0xff40b040, 0xffc09030, 0xffcc3322,                                       // vu
    0xff242830, 0xff404858, 0xff363e4c, 0xff0e1018, 0xfffaf8ff,               // knob
    0xff5598ee, 0xff28c8a0, 0xffddaa20, 0xffdd6030                            // bands
};

// ─────────────────────────────────────────────────────────────────────────────
// Theme 2 — Sapphire  (electric blue mixing console)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ColorTheme themeSapphire {
    "Sapphire",
    0xff080c18, 0xff0e1422, 0xff141c2e, 0xff1a2238, 0xff222c44, 0xff1a2850, // bg
    0xff283450, 0xff3a4868,                                                   // div
    0xff2878e0, 0xff50a8ff, 0xff184888,                                       // accent
    0xffdeeeff, 0xff7898c8, 0xff486090,                                       // text
    0xff40c840, 0xffc09030, 0xffcc3322,                                       // vu
    0xff101828, 0xff243050, 0xff1e2c48, 0xff060810, 0xff88c8ff,               // knob
    0xff5598ee, 0xff28c8a0, 0xffddaa20, 0xffdd6030                            // bands
};

// ─────────────────────────────────────────────────────────────────────────────
// Theme 3 — Phosphor  (green oscilloscope / vintage CRT)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ColorTheme themePhosphor {
    "Phosphor",
    0xff060a06, 0xff0c120c, 0xff101810, 0xff161e16, 0xff1e281e, 0xff142014, // bg
    0xff243824, 0xff385038,                                                   // div
    0xff38b838, 0xff68e848, 0xff1e6020,                                       // accent
    0xffc8f0c0, 0xff78b070, 0xff488040,                                       // text
    0xff68e848, 0xffa8e020, 0xffcc4420,                                       // vu
    0xff0e1a0e, 0xff204820, 0xff1a2e1a, 0xff060a06, 0xff88ff60,               // knob
    0xff40d040, 0xff28c8a0, 0xffc8e020, 0xffe06828                            // bands
};

// ─────────────────────────────────────────────────────────────────────────────

const ColorTheme AP::kThemes[AP::kNumThemes] = { themeAmber, themeSilver, themeSapphire, themePhosphor };


static int gCurrentTheme = 0;

int AP::getTheme () noexcept { return gCurrentTheme; }

void AP::setTheme (int index)
{
    gCurrentTheme = juce::jlimit (0, kNumThemes - 1, index);
    const ColorTheme& t = kThemes[gCurrentTheme];

    kBgDeep   = t.kBgDeep;   kBgBase   = t.kBgBase;
    kBgPanel  = t.kBgPanel;  kBgCard   = t.kBgCard;
    kBgHover  = t.kBgHover;  kBgSelect = t.kBgSelect;
    kDiv      = t.kDiv;      kDivHi    = t.kDivHi;
    kAccent   = t.kAccent;   kAccentBr = t.kAccentBr;  kAccentDm = t.kAccentDm;
    kTxtHi    = t.kTxtHi;    kTxtMid   = t.kTxtMid;    kTxtLo    = t.kTxtLo;
    kGreen    = t.kGreen;    kAmber    = t.kAmber;      kRed      = t.kRed;
    kKnobBody = t.kKnobBody; kKnobRim  = t.kKnobRim;
    kKnobHi   = t.kKnobHi;  kKnobLo   = t.kKnobLo;    kKnobDot  = t.kKnobDot;
    kBand0    = t.kBand0;    kBand1    = t.kBand1;
    kBand2    = t.kBand2;    kBand3    = t.kBand3;

    // Persist
    juce::PropertiesFile::Options opts;
    opts.applicationName    = "AuClear";
    opts.filenameSuffix     = ".xml";
    opts.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile prefs (opts);
    prefs.setValue ("colorTheme", gCurrentTheme);
    prefs.saveIfNeeded ();
}

void AP::loadSavedTheme ()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName    = "AuClear";
    opts.filenameSuffix     = ".xml";
    opts.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile prefs (opts);
    setTheme (prefs.getIntValue ("colorTheme", 0));
}
