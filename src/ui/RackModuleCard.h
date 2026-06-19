#pragma once
#include <JuceHeader.h>
#include "../engine/RackModule.h"

/**
 * A collapsed card in the rack column representing one module.
 * Shows: drag handle, enable toggle, module name, current param summary, remove button.
 * Fires drag callbacks so RackColumn can manage reorder without any locking.
 */
class RackModuleCard : public juce::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void cardSelected    (RackModuleCard*) = 0;
        virtual void cardRemoved     (RackModuleCard*) = 0;
        virtual void cardDragStarted (RackModuleCard*, const juce::MouseEvent&) = 0;
        virtual void cardDragged     (RackModuleCard*, const juce::MouseEvent&) = 0;
        virtual void cardDragEnded   (RackModuleCard*, const juce::MouseEvent&) = 0;
    };

    RackModuleCard (RackModule* module, Listener* listener);

    void paint    (juce::Graphics& g) override;
    void resized  () override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;
    void mouseUp    (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit  (const juce::MouseEvent& e) override;

    RackModule* getModule() const { return moduleRef; }
    void setSelected (bool s)     { selected = s; repaint(); }
    bool isSelected()  const      { return selected; }
    void setInsertAbove (bool v)  { insertAbove = v; repaint(); }
    void setInsertBelow (bool v)  { insertBelow = v; repaint(); }

    static constexpr int kHeight = 56;

private:
    void drawDragHandle (juce::Graphics& g, juce::Rectangle<int> area);

    RackModule* moduleRef;
    Listener*   listener;

    bool selected    { false };
    bool hovered     { false };
    bool dragging    { false };
    bool insertAbove { false };
    bool insertBelow { false };

    juce::TextButton enableButton { "" };
    juce::TextButton removeButton { "x" };

    juce::Point<int> mouseDownPos;
    static constexpr int kDragThreshold = 5;

    static constexpr juce::uint32 kBg       = 0xff1e2128;
    static constexpr juce::uint32 kBgHover  = 0xff252931;
    static constexpr juce::uint32 kBgSelect = 0xff1e2d3a;
    static constexpr juce::uint32 kAccent   = 0xff28e0c8;
    static constexpr juce::uint32 kTextHi   = 0xffe8eaed;
    static constexpr juce::uint32 kTextLo   = 0xff9aa0ab;
    static constexpr juce::uint32 kDivider  = 0xff2a2e37;
    static constexpr juce::uint32 kInsert   = 0xff28e0c8;
};
