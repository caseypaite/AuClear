#pragma once
#include <JuceHeader.h>
#include "../engine/RackModule.h"
#include "AnalogPalette.h"

/**
 * A collapsed card in the rack column representing one module.
 * Shows: drag handle, enable toggle, module name, wet/dry mix slider,
 * remove button.  Fires drag + context-menu callbacks to RackColumn.
 */
class RackModuleCard : public juce::Component
{
  public:
    struct Listener
    {
        virtual ~Listener () = default;
        virtual void cardSelected   (RackModuleCard*) = 0;
        virtual void cardRemoved    (RackModuleCard*) = 0;
        virtual void cardDuplicated (RackModuleCard*) = 0;
        virtual void cardDragStarted (RackModuleCard*, const juce::MouseEvent&) = 0;
        virtual void cardDragged    (RackModuleCard*, const juce::MouseEvent&) = 0;
        virtual void cardDragEnded  (RackModuleCard*, const juce::MouseEvent&) = 0;
    };

    RackModuleCard (RackModule* module, Listener* listener);

    void paint       (juce::Graphics& g) override;
    void resized     () override;
    void mouseDown   (const juce::MouseEvent& e) override;
    void mouseDrag   (const juce::MouseEvent& e) override;
    void mouseUp     (const juce::MouseEvent& e) override;
    void mouseEnter  (const juce::MouseEvent& e) override;
    void mouseExit   (const juce::MouseEvent& e) override;

    RackModule* getModule () const { return moduleRef; }

    void setSelected (bool s) { selected = s; repaint (); }
    bool isSelected  () const { return selected; }
    void setInsertAbove (bool v) { insertAbove = v; repaint (); }
    void setInsertBelow (bool v) { insertBelow = v; repaint (); }

    static constexpr int kHeight = 70; // 56 top-row + 14 mix-row

  private:
    void drawDragHandle (juce::Graphics& g, juce::Rectangle<int> area);
    void showContextMenu ();

    RackModule* moduleRef;
    Listener*   listener;

    bool selected{false};
    bool hovered{false};
    bool dragging{false};
    bool insertAbove{false};
    bool insertBelow{false};

    juce::TextButton enableButton{""};
    juce::TextButton removeButton{"×"};
    juce::Slider     mixSlider;

    juce::Point<int> mouseDownPos;
    static constexpr int kDragThreshold = 5;
    static constexpr int kTopRowH = 56;  // height of main row
    static constexpr int kMixRowH = 14;  // height of mix strip

};
