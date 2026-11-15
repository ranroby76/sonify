// D:\workspace\Fanan_web_hub\src\ColourSwatch.cpp
// From KnobMaker's StyleEditorPanel; with the standard colour it stands for when see-through,
// and Copy and Paste on a right-click.

#include "ColourSwatch.h"

//==============================================================================
namespace
{
    constexpr const char* dropperSourceProperty = "km2ColourDropperSource";

    /** The marked component under a point on the screen (or one it's in). */
    juce::Component* dropperSourceAt (juce::Point<int> screen)
    {
        for (auto* c = juce::Desktop::getInstance().findComponentAt (screen); c != nullptr; c = c->getParentComponent())
            if ((bool) c->getProperties()[dropperSourceProperty])
                return c;

        return nullptr;
    }

    /** The colour showing there, as it's drawn. */
    std::optional<juce::Colour> colourOnScreenAt (juce::Point<int> screen)
    {
        auto* source = dropperSourceAt (screen);

        if (source == nullptr)
            return std::nullopt;

        const auto local = source->getLocalPoint (nullptr, screen);

        if (! source->getLocalBounds().contains (local))
            return std::nullopt;

        const auto pixel = source->createComponentSnapshot ({ local.x, local.y, 1, 1 }, true, 1.0f);

        if (! pixel.isValid())
            return std::nullopt;

        return pixel.getPixelAt (0, 0);
    }

    /** The picker, and what the dropper does, said under it. */
    struct DropperHolder final : public juce::Component
    {
        explicit DropperHolder (std::unique_ptr<juce::ColourSelector> s)
            : selector (std::move (s))
        {
            addAndMakeVisible (*selector);
            hint.setText ("Or click the knob (or the panel) to take a colour from it", juce::dontSendNotification);
            hint.setFont (juce::FontOptions (12.0f));
            hint.setJustificationType (juce::Justification::centred);
            hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
            addAndMakeVisible (hint);
            setSize (juce::jmax (200, selector->getWidth()), selector->getHeight() + hintHeight);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            hint.setBounds (area.removeFromBottom (hintHeight));
            selector->setBounds (area);
        }

        static constexpr int hintHeight = 24;
        std::unique_ptr<juce::ColourSelector> selector;
        juce::Label hint;
    };

    /** A call-out a click outside doesn't close when it lands on a dropper source: that click takes
        the colour there (its see-through-ness kept as the picker has it). */
    class DropperCallOut final : public juce::CallOutBox
    {
    public:
        DropperCallOut (DropperHolder& holder, juce::Rectangle<int> area)
            : juce::CallOutBox (holder, area, nullptr), selector (*holder.selector)
        {
        }

        void inputAttemptWhenModal() override
        {
            if (const auto taken = colourOnScreenAt (juce::Desktop::getMousePosition()))
            {
                selector.setCurrentColour (taken->withAlpha (selector.getCurrentColour().getAlpha()), juce::sendNotification);
                return;
            }

            juce::CallOutBox::inputAttemptWhenModal();
        }

    private:
        juce::ColourSelector& selector;
    };

    /** As CallOutBox::launchAsynchronously: the call-out modal, deleted (with the picker) when it closes. */
    class DropperLaunch final : public juce::ModalComponentManager::Callback,
                                private juce::Timer
    {
    public:
        DropperLaunch (std::unique_ptr<juce::ColourSelector> selector, juce::Rectangle<int> area)
            : holder (std::make_unique<DropperHolder> (std::move (selector))),
              callout (*holder, area)
        {
            callout.setVisible (true);
            callout.enterModalState (true, this);
            startTimer (200);
        }

        void modalStateFinished (int) override {}

        void timerCallback() override
        {
            if (! juce::Process::isForegroundProcess())
                callout.dismiss();
        }

    private:
        std::unique_ptr<DropperHolder> holder;
        DropperCallOut callout;
    };
}

void ColourDropper::markAsSource (juce::Component& c)
{
    c.getProperties().set (dropperSourceProperty, true);
}

void ColourDropper::launch (std::unique_ptr<juce::ColourSelector> selector, juce::Rectangle<int> area)
{
    jassert (selector != nullptr);
    new DropperLaunch (std::move (selector), area);   // (the modal manager deletes it when the call-out closes)
}

//==============================================================================
ColourSwatchButton::ColourSwatchButton (const juce::String& name)
    : juce::Button (name)
{
}

ColourSwatchButton::~ColourSwatchButton()
{
}

void ColourSwatchButton::setColourValue (juce::Colour c, juce::NotificationType notify)
{
    if (c == colour)
        return;

    colour = c;
    repaint();

    if (notify != juce::dontSendNotification && onColourChange != nullptr)
        onColourChange();
}

void ColourSwatchButton::setStandardColour (juce::Colour c)
{
    standard = c;
    repaint();
}

juce::Colour ColourSwatchButton::getShownColour() const noexcept
{
    return colour.isTransparent() && ! standard.isTransparent() ? standard : colour;
}

void ColourSwatchButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    // Checkerboard so alpha is visible.
    const int cell = 6;
    g.setColour (juce::Colour (0xff555555));
    g.fillRoundedRectangle (r, 3.0f);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 3.0f);
        g.reduceClipRegion (clip);

        g.setColour (juce::Colour (0xff777777));

        for (int y = 0; y < getHeight(); y += cell)
            for (int x = 0; x < getWidth(); x += cell)
                if (((x / cell) + (y / cell)) % 2 == 0)
                    g.fillRect (x, y, cell, cell);
    }

    g.setColour (getShownColour());
    g.fillRoundedRectangle (r, 3.0f);

    g.setColour (juce::Colours::black.withAlpha (down ? 0.8f : (highlighted ? 0.5f : 0.3f)));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void ColourSwatchButton::clicked (const juce::ModifierKeys& modifiers)
{
    if (modifiers.isPopupMenu())
        showCopyPasteMenu();
    else
        openPicker();
}

void ColourSwatchButton::openPicker()
{
    auto selector = std::make_unique<juce::ColourSelector> (
        juce::ColourSelector::showColourAtTop
      | juce::ColourSelector::showSliders
      | juce::ColourSelector::showColourspace
      | juce::ColourSelector::showAlphaChannel);

    // A see-through colour starts from the one it shows, solid: otherwise every colour picked
    // would stay see-through, and nothing would change.
    const auto start = colour.isTransparent() ? (standard.isTransparent() ? colour.withAlpha (1.0f) : standard.withAlpha (1.0f)) : colour;

    selector->setName (getName());
    selector->setCurrentColour (start, juce::dontSendNotification);
    selector->setSize (300, 380);
    selector->addChangeListener (this);

    ColourDropper::launch (std::move (selector), getScreenBounds());
}

void ColourSwatchButton::showCopyPasteMenu()
{
    const auto pasted = parse (juce::SystemClipboard::getTextFromClipboard());

    juce::PopupMenu menu;
    menu.addItem (1, "Copy");
    menu.addItem (2, "Paste", pasted.has_value());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safeThis = juce::Component::SafePointer<ColourSwatchButton> (this), pasted] (int choice)
                        {
                            if (safeThis == nullptr)
                                return;

                            if (choice == 1)
                                juce::SystemClipboard::copyTextToClipboard ("#" + safeThis->getShownColour().toDisplayString (true));
                            else if (choice == 2 && pasted.has_value())
                                safeThis->setColourValue (*pasted, juce::sendNotification);
                        });
}

std::optional<juce::Colour> ColourSwatchButton::parse (const juce::String& text)
{
    auto hex = text.trim().trimCharactersAtStart ("#");

    if (hex.length() == 6)
        hex = "ff" + hex;

    if (hex.length() != 8 || ! hex.containsOnly ("0123456789abcdefABCDEF"))
        return std::nullopt;

    return juce::Colour ((juce::uint32) hex.getHexValue64());
}

void ColourSwatchButton::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (auto* selector = dynamic_cast<juce::ColourSelector*> (source))
        setColourValue (selector->getCurrentColour(), juce::sendNotification);
}
