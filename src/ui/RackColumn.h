#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>
#include "RackModuleCard.h"
#include "../engine/ProcessorRack.h"

/**
 * The left rack column — a scrollable list of RackModuleCards plus an "+ Add" button.
 * Handles drag-to-reorder by acting as the RackModuleCard::Listener and translating
 * pointer movements into ProcessorRack::moveModule calls.
 */
class RackColumn : public juce::Component, public RackModuleCard::Listener
{
  public:
    explicit RackColumn (ProcessorRack& rack);
    ~RackColumn () override;

    void paint (juce::Graphics& g) override;
    void resized () override;

    // Call after any structural change to regenerate cards.
    void syncWithRack ();

    std::function<void (RackModule*)> onModuleSelected;

    // RackModuleCard::Listener
    void cardSelected (RackModuleCard* card) override;
    void cardRemoved (RackModuleCard* card) override;
    void cardDragStarted (RackModuleCard* card, const juce::MouseEvent& e) override;
    void cardDragged (RackModuleCard* card, const juce::MouseEvent& e) override;
    void cardDragEnded (RackModuleCard* card, const juce::MouseEvent& e) override;

    static constexpr int kCardH = RackModuleCard::kHeight;
    static constexpr int kCardGap = 1;
    static constexpr int kAddH = 36;

  private:
    int insertionIndexForY (int yInContainer) const;
    void applyInsertIndicators (int insertIdx);
    void clearInsertIndicators ();
    int cardIndex (RackModuleCard* card) const;

    ProcessorRack& rack;

    juce::Viewport viewport;
    juce::Component cardsContainer;
    std::vector<std::unique_ptr<RackModuleCard>> cards;

    juce::TextButton addButton{"+ Add"};

    // Drag state
    RackModuleCard* dragCard{nullptr};
    int dragStartIndex{-1};

    static constexpr juce::uint32 kBg = 0xff16181d;
    static constexpr juce::uint32 kAccent = 0xff28e0c8;
    static constexpr juce::uint32 kDiv = 0xff2a2e37;
};
