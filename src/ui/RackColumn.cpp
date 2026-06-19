#include "RackColumn.h"
#include "../modules/GainModule.h"

RackColumn::RackColumn (ProcessorRack& r) : rack (r)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&cardsContainer, false);
    viewport.setScrollBarsShown (true, false);

    addAndMakeVisible (addButton);
    addButton.onClick = [this]
    {
        rack.insertModule (rack.numModules(), std::make_unique<GainModule>());
        syncWithRack();
    };
}

RackColumn::~RackColumn() = default;

void RackColumn::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kBg));

    // Right divider
    g.setColour (juce::Colour (kDiv));
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void RackColumn::resized()
{
    auto b = getLocalBounds();
    addButton.setBounds (b.removeFromBottom (kAddH).reduced (8, 4));
    viewport.setBounds (b);

    const int nCards     = static_cast<int> (cards.size());
    const int containerH = juce::jmax (b.getHeight(), nCards * (kCardH + kCardGap));
    cardsContainer.setBounds (0, 0, b.getWidth(), containerH);

    for (int i = 0; i < nCards; ++i)
        cards[static_cast<size_t> (i)]->setBounds (0, i * (kCardH + kCardGap), b.getWidth(), kCardH);
}

void RackColumn::syncWithRack()
{
    // Rebuild card list to match current rack state.
    RackModule* prevSelected = nullptr;
    for (auto& c : cards)
        if (c->isSelected())
            prevSelected = c->getModule();

    for (auto& c : cards)
        cardsContainer.removeChildComponent (c.get());
    cards.clear();

    for (int i = 0; i < rack.numModules(); ++i)
    {
        auto card = std::make_unique<RackModuleCard> (rack.getModule (i), this);
        if (rack.getModule (i) == prevSelected)
            card->setSelected (true);
        cardsContainer.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }

    resized();
    repaint();
}

// ---------------------------------------------------------------------------
// Card callbacks
// ---------------------------------------------------------------------------

void RackColumn::cardSelected (RackModuleCard* card)
{
    for (auto& c : cards) c->setSelected (false);
    card->setSelected (true);
    if (onModuleSelected) onModuleSelected (card->getModule());
}

void RackColumn::cardRemoved (RackModuleCard* card)
{
    const int idx = cardIndex (card);
    if (idx >= 0)
    {
        rack.removeModule (idx);
        syncWithRack();

        // Notify main stage (selection lost)
        if (onModuleSelected) onModuleSelected (nullptr);
    }
}

void RackColumn::cardDragStarted (RackModuleCard* card, const juce::MouseEvent&)
{
    dragCard       = card;
    dragStartIndex = cardIndex (card);
}

void RackColumn::cardDragged (RackModuleCard* /*card*/, const juce::MouseEvent& e)
{
    if (dragCard == nullptr) return;

    // Convert event Y to cardsContainer coordinates
    auto relE = e.getEventRelativeTo (&cardsContainer);
    const int yInContainer = relE.getPosition().getY();
    const int insertIdx    = insertionIndexForY (yInContainer);
    applyInsertIndicators (insertIdx);
}

void RackColumn::cardDragEnded (RackModuleCard* /*card*/, const juce::MouseEvent& e)
{
    if (dragCard == nullptr) return;

    auto relE = e.getEventRelativeTo (&cardsContainer);
    const int yInContainer = relE.getPosition().getY();
    int toIdx = insertionIndexForY (yInContainer);

    clearInsertIndicators();

    if (dragStartIndex >= 0 && toIdx != dragStartIndex && toIdx != dragStartIndex + 1)
    {
        // Adjust destination for the gap left by the moving card
        if (toIdx > dragStartIndex) --toIdx;
        rack.moveModule (dragStartIndex, toIdx);
        syncWithRack();
    }

    dragCard       = nullptr;
    dragStartIndex = -1;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

int RackColumn::insertionIndexForY (int y) const
{
    const int stride = kCardH + kCardGap;
    if (stride == 0) return 0;
    int idx = (y + stride / 2) / stride;
    return juce::jlimit (0, static_cast<int> (cards.size()), idx);
}

void RackColumn::applyInsertIndicators (int insertIdx)
{
    const int nCards = static_cast<int> (cards.size());
    for (int i = 0; i < nCards; ++i)
    {
        cards[static_cast<size_t> (i)]->setInsertAbove (i == insertIdx);
        cards[static_cast<size_t> (i)]->setInsertBelow (i == nCards - 1 && insertIdx >= nCards);
    }
}

void RackColumn::clearInsertIndicators()
{
    for (auto& c : cards) { c->setInsertAbove (false); c->setInsertBelow (false); }
}

int RackColumn::cardIndex (RackModuleCard* card) const
{
    for (int i = 0; i < static_cast<int> (cards.size()); ++i)
        if (cards[static_cast<size_t> (i)].get() == card) return i;
    return -1;
}
