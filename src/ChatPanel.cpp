// D:\workspace\Fanan_web_hub\src\ChatPanel.cpp

#include "ChatPanel.h"
#include "Skills.h"   // the inspiration pool's looks, for the Look picker
#include "StellaLookAndFeel.h"

namespace
{
    constexpr int columnWidth      = 820;  // the chat sits in one centred column
    constexpr int headerHeight     = 100;
    constexpr int accountLabelGap  = 58;   // room for the word "Account"
    constexpr int pillWidth        = 150;
    constexpr int warningHeight    = 110;
    constexpr int inputHeight      = 176;
    constexpr int infoWidth        = 236;  // the plugin's card, beside the column
    constexpr int infoHeight       = 150;
    constexpr int infoLineHeight   = 30;   // ...or its line above the conversation

    const juce::Colour pillYellow { 0xffffcc00 };

    /** Whether text is written right to left (Hebrew, Arabic, Persian...): its first letter that
        has a direction decides, as a web page's dir="auto" does. */
    bool isRightToLeft (const juce::String& text)
    {
        for (auto p = text.getCharPointer(); ! p.isEmpty();)
        {
            const auto c = (juce::uint32) p.getAndAdvance();

            if ((c >= 0x0590 && c <= 0x08ff) || (c >= 0xfb1d && c <= 0xfdff) || (c >= 0xfe70 && c <= 0xfeff))
                return true;

            if (juce::CharacterFunctions::isLetter ((juce::juce_wchar) c))
                return false;
        }

        return false;
    }

    juce::String welcomeText()
    {
        return juce::String::fromUTF8 (
            "Choose what you're building in Type, beside Send, then describe it: Fanan AI builds it, "
            "playing live while you shape it.\n\n"
            "For example:\n"
            "  \xe2\x80\xa2 Instrument: a three-oscillator bass synth with a ladder filter\n"
            "  \xe2\x80\xa2 Effect: a warm tape delay with wow and flutter\n"
            "  \xe2\x80\xa2 MIDI FX: an arpeggiator that follows the chord\n"
            "  \xe2\x80\xa2 Widget: anything else, like a tuner or a spectrum display\n\n"
            "Then reshape its panel in Edit UI, and play it in your songs.");
    }
}

//==============================================================================
ChatPanel::Pill::Pill (const juce::String& label)
    : juce::Button (label)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ChatPanel::Pill::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    auto fill = pillYellow;

    if (! isEnabled())
        fill = fill.withMultipliedAlpha (0.35f);
    else if (isButtonDown)
        fill = fill.darker (0.2f);
    else if (isMouseOver)
        fill = fill.brighter (0.18f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    g.setColour (juce::Colours::black.withAlpha (isEnabled() ? 1.0f : 0.55f));
    g.setFont (Theme::font (14.5f, true));
    g.drawText (getButtonText(), bounds, juce::Justification::centred, false);
}

//==============================================================================
ChatPanel::ChatPanel()
{
    transcript.setMultiLine (true, true);
    transcript.setReadOnly (true);
    transcript.setCaretVisible (false);
    transcript.setScrollbarsShown (true);
    transcript.setColour (juce::TextEditor::backgroundColourId, Theme::panel);
    transcript.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);

    input.setMultiLine (true, true);
    input.setReturnKeyStartsNewLine (false);
    input.setFont (Theme::font (15.0f));
    input.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Describe your plugin\xe2\x80\xa6"), Theme::muted);
    input.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    input.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    input.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    input.onReturnKey = [this] { send(); };
    input.onTextChange = [this]
    {
        // Typed right to left: it sits on the right, as it reads.
        const auto rightToLeft = isRightToLeft (input.getText());

        if (rightToLeft != input.getJustificationType().testFlags (juce::Justification::right))
            input.setJustification (rightToLeft ? juce::Justification::topRight : juce::Justification::topLeft);

        updateSendButton();
    };

    attachButton.setTooltip ("Attach files for Fanan AI: images, PDFs, or your own code (files, a zip or a whole folder). Or drop them here");
    attachButton.onClick = [this] { chooseFiles(); };
    addChildComponent (attachButton);

    sendButton.onClick = [this]
    {
        if (busy)
        {
            if (onStop != nullptr) onStop();
        }
        else
        {
            send();
        }
    };

    retryButton.setTooltip ("Try to connect again now");
    retryButton.onClick = [this] { if (onRetry != nullptr) onRetry(); };

    accountPill.onClick = [this]
    {
        if (account.signedIn)
        {
            if (onSignOut != nullptr) onSignOut();
        }
        else
        {
            signIn();
        }
    };

    buyPill.setTooltip ("Buy credits on the Fanan site (the same credits all of Sonify uses)");
    buyPill.onClick = [this] { if (onBuyCredits != nullptr) onBuyCredits(); };

    accountBox.setMultiLine (false);
    accountBox.setFont (Theme::font (14.0f));
    accountBox.setJustification (juce::Justification::centredLeft);
    accountBox.setIndents (8, 0);
    accountBox.setSelectAllWhenFocused (false);
    accountBox.setColour (juce::TextEditor::backgroundColourId, Theme::inset);
    accountBox.setTextToShowWhenEmpty ("Your email", Theme::muted);
    accountBox.onReturnKey = [this] { signIn(); };
    accountBox.addMouseListener (this, true);   // the hover suggestion

    // The Type selector: what's built, chosen before anything is.
    typeBox.addItem ("Instrument", (int) BuildType::instrument);
    typeBox.addItem ("Effect", (int) BuildType::effect);
    typeBox.addItem ("MIDI FX", (int) BuildType::midiFx);
    typeBox.addItem ("Widget", (int) BuildType::widget);
    typeBox.setTextWhenNothingSelected (juce::String::fromUTF8 ("Type\xe2\x80\xa6"));
    typeBox.setTextWhenNoChoicesAvailable ("Type");
    typeBox.onChange = [this]
    {
        typeHint.setVisible (false);
        updateSendButton();
    };

    // The Look picker: Auto, or a look of Fanan AI's inspiration pool (the inspiration-looks skill's).
    lookBox.addItem ("Look: Auto", 1);
    lookNames = Skills::headings ("inspiration-looks");

    if (! lookNames.isEmpty())
    {
        lookBox.addSectionHeading ("Inspiration pool");

        for (int i = 0; i < lookNames.size(); ++i)
            lookBox.addItem (lookNames[i], i + 2);
    }

    lookBox.setSelectedId (1, juce::dontSendNotification);
    lookBox.setTooltip ("The look Fanan AI designs the plugin's panel in: Auto lets it choose one from its inspiration pool "
                        "(it says which), or pick one yourself");
    lookBox.onChange = [this]
    {
        if (onLookPicked != nullptr)
            onLookPicked (getPickedLook());
    };

    typeHint.setText ("First choose what you're building", juce::dontSendNotification);
    typeHint.setFont (Theme::font (13.0f, true));
    typeHint.setColour (juce::Label::textColourId, pillYellow);
    typeHint.setJustificationType (juce::Justification::centredLeft);
    typeHint.setInterceptsMouseClicks (false, false);

    setBuildType (BuildType::none, false);

    addChildComponent (transcript);
    addChildComponent (input);
    addChildComponent (typeBox);
    addChildComponent (lookBox);
    addChildComponent (typeHint);
    addChildComponent (sendButton);
    addChildComponent (retryButton);
    addChildComponent (buyPill);
    addAndMakeVisible (accountPill);
    addAndMakeVisible (accountBox);

    // Everything starts here: a new plugin, or one to open.
    newPluginButton.setButtonText (juce::String::fromUTF8 ("New plugin\xe2\x80\xa6"));
    newPluginButton.setColour (juce::TextButton::buttonColourId, Theme::accent);
    newPluginButton.setTooltip ("Start a new plugin");
    newPluginButton.onClick = [this] { if (onNewPlugin != nullptr) onNewPlugin(); };
    openButton.setButtonText (juce::String::fromUTF8 ("Open\xe2\x80\xa6"));
    openButton.setTooltip ("Open a plugin you're working on");
    openButton.onClick = [this] { if (onOpenPlugin != nullptr) onOpenPlugin(); };
    addAndMakeVisible (newPluginButton);
    addAndMakeVisible (openButton);

    // The plan bar's buttons: what they do depends on what the plan waits for.
    planFirst.setColour (juce::TextButton::buttonColourId, Theme::accent);
    planFirst.onClick = [this]
    {
        if (planWaiting == StellaAi::Waiting::approval) { if (onPlanStart != nullptr) onPlanStart(); }
        else if (planWaiting == StellaAi::Waiting::overrun) { if (onPlanGoOn != nullptr) onPlanGoOn(); }
    };
    planSecond.onClick = [this]
    {
        if (planWaiting == StellaAi::Waiting::approval)
        {
            askingForChange = true;
            input.setTextToShowWhenEmpty (juce::String::fromUTF8 ("What should change in the plan?\xe2\x80\xa6"), Theme::muted);
            input.grabKeyboardFocus();
            repaint();
        }
        else if (planWaiting == StellaAi::Waiting::overrun)
        {
            if (onPlanPause != nullptr) onPlanPause();
        }
    };
    planThird.onClick = [this] { if (planWaiting == StellaAi::Waiting::approval && onPlanCancel != nullptr) onPlanCancel(); };
    planFirst.setTooltip ("Start building this plan");
    planThird.setTooltip ("Drop this plan: nothing is built");
    addChildComponent (planFirst);
    addChildComponent (planSecond);
    addChildComponent (planThird);

    rebuildTranscript ({});
    setAccount ({});
}

void ChatPanel::setPlanBar (StellaAi::Waiting waiting, const juce::String& text, bool warning)
{
    if (waiting == planWaiting && text == planBarText && warning == planBarWarning)
        return;

    const bool changed = waiting != planWaiting;

    if (changed)
        askingForChange = false;

    planWaiting = waiting;
    planBarText = text;
    planBarWarning = warning;

    if (changed)
    {
        const bool approval = waiting == StellaAi::Waiting::approval;
        planFirst.setButtonText (approval ? "Start" : "Go on");
        planSecond.setButtonText (approval ? "Change" : "Pause");
        planThird.setButtonText ("Cancel");
        planSecond.setTooltip (approval ? "Type what should change: Fanan AI makes a new plan" : "Stop here: say \"continue\" later to go on");

        // The request box asks for the change only while there's a plan to change.
        if (! approval)
            input.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Describe your plugin\xe2\x80\xa6"), Theme::muted);

        resized();
    }

    repaint();
}

//==============================================================================
void ChatPanel::setPluginInfo (const juce::String& name, const juce::String& type, const juce::String& developer)
{
    if (name == pluginName && type == pluginType && developer == pluginDeveloper)
        return;

    pluginName = name;
    pluginType = type;
    pluginDeveloper = developer;
    resized();
    repaint();
}

void ChatPanel::paintPluginInfo (juce::Graphics& g)
{
    if (pluginName.isEmpty() || infoArea.isEmpty())
        return;

    const auto developer = pluginDeveloper.isNotEmpty() ? pluginDeveloper : juce::String ("Made in Sonify");

    if (infoInline)
    {
        // No room beside the column: one line above the conversation.
        auto line = infoArea;
        g.setColour (Theme::muted);
        g.setFont (Theme::font (12.5f, true));
        g.drawText ("PLUGIN", line.removeFromLeft (58), juce::Justification::centredLeft, false);

        juce::AttributedString text;
        text.append (pluginName, Theme::font (14.0f, true), Theme::text);
        text.append (juce::String::fromUTF8 ("  \xc2\xb7  ") + pluginType + juce::String::fromUTF8 ("  \xc2\xb7  ") + developer,
                     Theme::font (13.5f), Theme::text.withAlpha (0.75f));
        text.setJustification (juce::Justification::centredLeft);
        text.setWordWrap (juce::AttributedString::none);
        text.draw (g, line.toFloat());
        return;
    }

    const auto box = infoArea.toFloat();
    g.setColour (Theme::raised.withAlpha (0.55f));
    g.fillRoundedRectangle (box, 8.0f);
    g.setColour (Theme::outline);
    g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, 1.0f);

    auto area = infoArea.reduced (14, 12);
    g.setColour (Theme::accent);
    g.setFont (Theme::font (12.0f, true));
    g.drawText ("PLUGIN", area.removeFromTop (16), juce::Justification::centredLeft, false);
    area.removeFromTop (6);

    const auto row = [&g, &area] (const juce::String& label, const juce::String& value, bool bold)
    {
        g.setColour (Theme::muted);
        g.setFont (Theme::font (12.0f));
        g.drawText (label, area.removeFromTop (15), juce::Justification::centredLeft, false);
        g.setColour (Theme::text.withAlpha (bold ? 1.0f : 0.88f));
        g.setFont (Theme::font (bold ? 15.0f : 14.0f, bold));
        g.drawFittedText (value, area.removeFromTop (20), juce::Justification::centredLeft, 1, 0.8f);
        area.removeFromTop (4);
    };

    row ("Name", pluginName, true);
    row ("Type", pluginType, false);
    row ("Developer", developer, false);
}

void ChatPanel::setConversation (const std::vector<StellaAi::Entry>& entries, bool isBusy)
{
    busy = isBusy;
    rebuildTranscript (entries);

    // Typing ahead is fine while Sonify AI works; the button stops it instead of sending.
    updateSendButton();
}

void ChatPanel::updateSendButton()
{
    sendButton.setIcon (busy ? IconButton::Icon::stop : IconButton::Icon::send);
    sendButton.setEnabled (busy || input.getText().trim().isNotEmpty() || ! attachments.isEmpty());
    sendButton.setTooltip (busy ? "Stop Fanan AI after the step it's doing" : juce::String ("Send (Enter)"));
}

//==============================================================================
void ChatPanel::IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto circle = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre()).reduced (1.0f);
    const auto centre = circle.getCentre();
    const auto on = isEnabled();

    if (icon == Icon::attach)
    {
        g.setColour (highlighted ? Theme::raised.brighter (0.1f) : juce::Colours::transparentBlack);
        g.fillEllipse (circle);

        // A paperclip: a tall loop with a shorter one inside, tilted.
        juce::Path clip;
        const auto h = size * 0.56f, w = size * 0.24f;
        clip.addRoundedRectangle (-w * 0.5f, -h * 0.5f, w, h, w * 0.5f);
        clip.startNewSubPath (0.0f, -h * 0.28f);
        clip.lineTo (0.0f, h * 0.22f);
        clip.applyTransform (juce::AffineTransform::rotation (0.6f).translated (centre));

        g.setColour (down ? Theme::accent : (highlighted ? Theme::text : Theme::muted));
        g.strokePath (clip, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    g.setColour (on ? (down ? Theme::accent.darker (0.2f) : (highlighted ? Theme::accent.brighter (0.15f) : Theme::accent))
                    : Theme::raised);
    g.fillEllipse (circle);

    g.setColour (on ? juce::Colours::white : Theme::muted);

    if (icon == Icon::stop)
    {
        g.fillRoundedRectangle (juce::Rectangle<float> (size * 0.32f, size * 0.32f).withCentre (centre), 2.0f);
        return;
    }

    // An arrow pointing up: send.
    juce::Path arrow;
    const auto a = size * 0.22f;
    arrow.startNewSubPath (centre.x, centre.y + a);
    arrow.lineTo (centre.x, centre.y - a);
    arrow.startNewSubPath (centre.x - a * 0.8f, centre.y - a * 0.2f);
    arrow.lineTo (centre.x, centre.y - a);
    arrow.lineTo (centre.x + a * 0.8f, centre.y - a * 0.2f);
    g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void ChatPanel::chooseFiles()
{
    // Files (pictures, PDFs, code, a zip), or a whole folder of code: a chooser can't offer both.
    juce::PopupMenu menu;
    menu.addItem (1, juce::String::fromUTF8 ("Files\xe2\x80\xa6"));
    menu.addItem (2, juce::String::fromUTF8 ("A folder of code\xe2\x80\xa6"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&attachButton),
                        [safeThis = juce::Component::SafePointer<ChatPanel> (this)] (int chosen)
                        {
                            if (safeThis == nullptr || chosen == 0)
                                return;

                            const bool folder = chosen == 2;
                            safeThis->chooser = std::make_unique<juce::FileChooser> (folder ? "A folder of your code for Fanan AI" : "Attach files for Fanan AI",
                                                                                     juce::File(), folder ? juce::String() : StellaAi::attachableFiles());
                            safeThis->chooser->launchAsync (folder ? juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories
                                                                   : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                                                         | juce::FileBrowserComponent::canSelectMultipleItems,
                                                            [safeThis] (const juce::FileChooser& fc)
                                                            {
                                                                if (safeThis != nullptr)
                                                                    for (const auto& file : fc.getResults())
                                                                        safeThis->addAttachment (file);
                                                            });
                        });
}

void ChatPanel::addAttachment (const juce::File& file)
{
    if (! StellaAi::canAttach (file) || attachments.contains (file) || attachments.size() >= StellaAi::maxFilesAtOnce)
        return;

    attachments.add (file);
    refreshChips();
}

void ChatPanel::refreshChips()
{
    chips.clear();

    for (const auto& file : attachments)
    {
        auto* chip = chips.add (new juce::TextButton (file.getFileName() + (file.isDirectory() ? "/" : "") + juce::String::fromUTF8 ("  \xc3\x97")));
        chip->setTooltip (file.getFullPathName() + "\nClick to remove");
        chip->setColour (juce::TextButton::buttonColourId, Theme::raised);
        chip->onClick = [this, file]
        {
            attachments.removeFirstMatchingValue (file);
            juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<ChatPanel> (this)]
            {
                if (safeThis != nullptr)
                    safeThis->refreshChips();
            });
        };
        addAndMakeVisible (chip);
    }

    updateSendButton();
    resized();
    repaint();
}

bool ChatPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (! account.signedIn)
        return false;

    for (const auto& path : files)
        if (StellaAi::canAttach (juce::File (path)))
            return true;

    return false;
}

void ChatPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;

    for (const auto& path : files)
        addAttachment (juce::File (path));

    repaint();
}

void ChatPanel::paintOverChildren (juce::Graphics& g)
{
    if (! dragOver || inputBox.isEmpty())
        return;

    g.setColour (Theme::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (inputBox.toFloat().reduced (2.0f), 7.0f);
    g.setColour (Theme::accent);
    g.drawRoundedRectangle (inputBox.toFloat().reduced (1.0f), 8.0f, 2.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (15.0f, true));
    g.drawText ("Drop to attach", inputBox, juce::Justification::centred, false);
}

void ChatPanel::fileDragMove (const juce::StringArray&, int, int)
{
    if (! dragOver)
    {
        dragOver = true;
        repaint();
    }
}

void ChatPanel::fileDragEnter (const juce::StringArray&, int, int)    { dragOver = true; repaint(); }
void ChatPanel::fileDragExit (const juce::StringArray&)               { dragOver = false; repaint(); }

void ChatPanel::setAccount (const Account& newAccount)
{
    account = newAccount;

    const bool online = account.status == FananServer::Status::online;

    // The account box: the signed-in email, the wait for the browser, or an email to sign in with.
    setBoxMode (account.signedIn ? BoxMode::showingAccount
              : account.waitingForBrowser ? BoxMode::waiting
                                          : BoxMode::editing);

    if (boxMode == BoxMode::showingAccount)
        accountBox.setText (account.email.isNotEmpty() ? account.email : juce::String ("Signed in"), juce::dontSendNotification);

    accountBox.setTooltip (account.computerCode.isNotEmpty() ? "This computer: " + account.computerCode : juce::String());

    // The pills: Sign in alone, or Sign out and Buy credits side by side.
    accountPill.setButtonText (account.signedIn ? "Sign out" : "Sign in");
    accountPill.setTooltip (account.signedIn ? "Sign this computer out. Your credits stay with your account."
                                             : "Sign in on the Fanan site: your account and credits work on every computer, and all of Sonify");
    accountPill.setEnabled (online && account.siteAvailable);

    buyPill.setVisible (account.signedIn && account.siteAvailable);
    buyPill.setEnabled (online);

    // Sonify AI itself is there only once signed in.
    transcript.setVisible (account.signedIn);
    input.setVisible (account.signedIn);
    typeBox.setVisible (account.signedIn);
    lookBox.setVisible (account.signedIn && ! lookNames.isEmpty());
    sendButton.setVisible (account.signedIn);
    attachButton.setVisible (account.signedIn);

    if (! account.signedIn)
        typeHint.setVisible (false);

    for (auto* chip : chips)
        chip->setVisible (account.signedIn);

    retryButton.setVisible (showsWarning());

    resized();
    repaint();
}

void ChatPanel::setBoxMode (BoxMode mode)
{
    if (mode == boxMode)
        return;   // checks run every few minutes: what the user typed is left alone

    if (boxMode == BoxMode::editing)
        typedEmail = accountBox.getText().trim();

    boxMode = mode;

    const bool editing = mode == BoxMode::editing;

    accountBox.setReadOnly (! editing);
    accountBox.setCaretVisible (editing);
    accountBox.setMouseCursor (editing ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor);

    switch (mode)
    {
        case BoxMode::editing:
            accountBox.setText (typedEmail, juce::dontSendNotification);
            accountBox.setColour (juce::TextEditor::textColourId, Theme::text);
            accountBox.setColour (juce::TextEditor::outlineColourId, Theme::outline);
            break;

        case BoxMode::showingAccount:
            typedEmail.clear();   // after signing out, the box starts empty (with the suggestion)
            accountBox.setColour (juce::TextEditor::textColourId, Theme::text);
            accountBox.setColour (juce::TextEditor::outlineColourId, pillYellow.withAlpha (0.55f));
            break;

        case BoxMode::waiting:
            accountBox.setText (juce::String::fromUTF8 ("Finish in your browser\xe2\x80\xa6"), juce::dontSendNotification);
            accountBox.setColour (juce::TextEditor::textColourId, Theme::hot);
            accountBox.setColour (juce::TextEditor::outlineColourId, Theme::outline);
            break;
    }

    // TextEditor applies a new text colour to text added from now on; recolour what's there.
    accountBox.applyColourToAllText (accountBox.findColour (juce::TextEditor::textColourId), true);
    showSuggestion (false);
}

bool ChatPanel::canSuggest() const
{
    return boxMode == BoxMode::editing && accountBox.isEmpty() && account.lastEmail.isNotEmpty();
}

void ChatPanel::showSuggestion (bool show)
{
    // The email last signed in with, shown in the empty box while the mouse is over it.
    if (show && canSuggest())
        accountBox.setTextToShowWhenEmpty (account.lastEmail, pillYellow.withAlpha (0.75f));
    else
        accountBox.setTextToShowWhenEmpty ("Your email", Theme::muted);

    accountBox.repaint();
}

bool ChatPanel::isAccountBoxEvent (const juce::MouseEvent& e) const
{
    return e.eventComponent == &accountBox || accountBox.isParentOf (e.eventComponent);
}

void ChatPanel::mouseEnter (const juce::MouseEvent& e)
{
    if (isAccountBoxEvent (e))
        showSuggestion (true);
}

void ChatPanel::mouseExit (const juce::MouseEvent& e)
{
    if (isAccountBoxEvent (e) && ! accountBox.getScreenBounds().contains (e.getScreenPosition()))
        showSuggestion (false);
}

void ChatPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! isAccountBoxEvent (e) || ! canSuggest())
        return;

    // A click takes the suggestion: filled in and selected, so typing replaces it.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<ChatPanel> (this)]
    {
        if (safeThis == nullptr || ! safeThis->canSuggest())
            return;

        safeThis->accountBox.setText (safeThis->account.lastEmail, juce::dontSendNotification);
        safeThis->accountBox.selectAll();
        safeThis->showSuggestion (false);
    });
}

void ChatPanel::signIn()
{
    if (account.signedIn || ! accountPill.isEnabled())
        return;

    const auto typed = boxMode == BoxMode::editing ? accountBox.getText().trim() : typedEmail;

    if (onSignIn != nullptr)
        onSignIn (typed);
}

bool ChatPanel::showsWarning() const noexcept
{
    return account.status == FananServer::Status::noInternet || account.status == FananServer::Status::serverDown
        || account.status == FananServer::Status::blocked;
}

juce::String ChatPanel::cornerText() const
{
    switch (account.status)
    {
        case FananServer::Status::checking:    return juce::String::fromUTF8 ("Connecting\xe2\x80\xa6");
        case FananServer::Status::noInternet:  return "Offline";
        case FananServer::Status::serverDown:  return "Server not responding";
        case FananServer::Status::notSetUp:    return "Not set up";
        case FananServer::Status::blocked:     return "Internet blocked";
        case FananServer::Status::online:
            if (! account.signedIn || account.credits < 0)
                return "Online";

            return juce::String::fromUTF8 ("Online \xc2\xb7 ") + juce::String (account.credits)
                 + (account.credits == 1 ? " credit" : " credits");
    }

    return {};
}

void ChatPanel::setBuildType (BuildType type, bool locked)
{
    typeLocked = locked;
    typeBox.setSelectedId ((int) type, juce::dontSendNotification);
    typeBox.setEnabled (! locked);
    typeBox.setTooltip (locked ? "The type of the plugin that's open. For another type, start a new plugin (New plugin)."
                               : "What you're building, chosen first so it's built exactly as that: an instrument, an effect, "
                                 "a MIDI FX, or a widget, for anything else");

    if (locked || type != BuildType::none)
        typeHint.setVisible (false);
}

ChatPanel::BuildType ChatPanel::getBuildType() const noexcept
{
    return (BuildType) juce::jlimit (0, (int) BuildType::widget, typeBox.getSelectedId());
}

juce::String ChatPanel::getPickedLook() const
{
    const auto index = lookBox.getSelectedId() - 2;
    return juce::isPositiveAndBelow (index, lookNames.size()) ? lookNames[index] : juce::String();
}

void ChatPanel::askForType()
{
    typeHint.setVisible (true);
    resized();

    if (typeBox.isShowing())
        typeBox.showPopup();
}

void ChatPanel::send()
{
    const auto text = input.getText().trim();

    if (busy || ! account.signedIn || (text.isEmpty() && attachments.isEmpty()))
        return;

    // Every new plugin starts with its exact type: the request waits until it's chosen.
    if (! typeLocked && getBuildType() == BuildType::none)
    {
        askForType();
        return;
    }

    const auto files = attachments;
    askingForChange = false;
    input.clear();
    input.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Describe your plugin\xe2\x80\xa6"), Theme::muted);
    attachments.clear();
    refreshChips();

    if (onSend != nullptr)
        onSend (text, files);
}

//==============================================================================
//==============================================================================
namespace
{
    /** A stretch of an answer in one style. */
    struct Run
    {
        juce::String text;
        bool bold = false, italic = false, code = false, ask = false;
    };

    /** Inline marks, each only when it's closed on the same line: **bold**, *italic*,
        `code`, and ==a question or a task for the user== (shown in yellow). */
    juce::Array<Run> parseLine (const juce::String& line, bool bold)
    {
        juce::Array<Run> runs;
        bool italic = false, code = false, ask = false;
        Run current;
        current.bold = bold;

        auto flush = [&]
        {
            if (current.text.isNotEmpty())
                runs.add (current);

            current = Run();
            current.bold = bold;
            current.italic = italic;
            current.code = code;
            current.ask = ask;
        };

        const auto n = line.length();

        for (int i = 0; i < n;)
        {
            const auto rest = line.substring (i);
            auto closedLater = [&line] (const char* mark, int from) { return line.indexOf (from, mark) >= 0; };

            if (! code && rest.startsWith ("**") && (bold || closedLater ("**", i + 2)))
            {
                flush();
                bold = ! bold;
                current.bold = bold;
                i += 2;
            }
            else if (! code && rest.startsWith ("==") && (ask || closedLater ("==", i + 2)))
            {
                flush();
                ask = ! ask;
                current.ask = ask;
                i += 2;
            }
            else if (line[i] == '`' && (code || closedLater ("`", i + 1)))
            {
                flush();
                code = ! code;
                current.code = code;
                i += 1;
            }
            else if (! code && line[i] == '*' && (italic || (i + 1 < n && line[i + 1] != ' ' && closedLater ("*", i + 1))))
            {
                flush();
                italic = ! italic;
                current.italic = italic;
                i += 1;
            }
            else
            {
                current.text += juce::String::charToString (line[i]);
                ++i;
            }
        }

        flush();
        return runs;
    }

    /** When an answer marks nothing for the user, its questions are found by their "?". */
    juce::String markQuestions (const juce::String& line)
    {
        juce::String result, sentence;

        for (int i = 0; i < line.length(); ++i)
        {
            sentence += juce::String::charToString (line[i]);
            const auto end = line[i] == '.' || line[i] == '!' || line[i] == '?';

            if (end && (i + 1 == line.length() || line[i + 1] == ' '))
            {
                const auto trimmed = sentence.trim();
                result += line[i] == '?' && trimmed.isNotEmpty()
                              ? sentence.substring (0, sentence.length() - trimmed.length()) + "==" + trimmed + "=="
                              : sentence;
                sentence.clear();
            }
        }

        return result + sentence;
    }
}

void ChatPanel::appendAnswer (const juce::String& text)
{
    // Markdown-lite: bold and italic as fonts, bullets as dots, headings bold, and what
    // the user should answer or do in yellow. The marks themselves never show.
    // Code never shows: a fenced block is left out, whatever Sonify AI wrote.
    juce::StringArray lines;
    bool inCode = false;

    for (const auto& line : juce::StringArray::fromLines (text))
    {
        if (line.trimStart().startsWith ("```"))
            inCode = ! inCode;
        else if (! inCode)
            lines.add (line);
    }

    while (! lines.isEmpty() && lines[lines.size() - 1].trim().isEmpty())
        lines.remove (lines.size() - 1);

    const bool marked = text.contains ("==");

    for (int l = 0; l < lines.size(); ++l)
    {
        auto line = lines[l];
        const auto trimmed = line.trimStart();
        juce::String prefix;
        bool heading = false;

        if (trimmed == "---" || trimmed == "***" || trimmed == "___")
        {
            line.clear();
        }
        else if (trimmed.startsWith ("#"))
        {
            line = trimmed.trimCharactersAtStart ("#").trimStart();
            heading = true;
        }
        else if (trimmed.startsWith ("- ") || trimmed.startsWith ("* ") || (trimmed[0] == (juce::juce_wchar) 0x2022 && trimmed[1] == ' '))
        {
            prefix = juce::String::repeatedString (" ", juce::jmin (6, line.length() - trimmed.length())) + juce::String::fromUTF8 ("\xe2\x80\xa2 ");
            line = trimmed.substring (2);
        }
        else if (trimmed.startsWith ("> "))
        {
            line = trimmed.substring (2);
        }

        if (! marked)
            line = markQuestions (line);

        if (prefix.isNotEmpty())
        {
            transcript.setFont (Theme::font (15.0f));
            transcript.setColour (juce::TextEditor::textColourId, Theme::muted);
            transcript.insertTextAtCaret (prefix);
        }

        for (const auto& run : parseLine (line, heading))
        {
            auto font = Theme::font (15.0f, run.bold);   // `marked` words read as plain words

            if (run.italic)
                font = font.italicised();

            transcript.setFont (font);
            transcript.setColour (juce::TextEditor::textColourId, run.ask ? pillYellow : Theme::text.withAlpha (0.9f));
            transcript.insertTextAtCaret (run.text);
        }

        if (l + 1 < lines.size())
            transcript.insertTextAtCaret ("\n");
    }
}

int ChatPanel::introHeight (int width)
{
    // Laid out as the conversation lays it out (its font, its margins), with a little room spare.
    juce::AttributedString text;
    text.setWordWrap (juce::AttributedString::byWord);
    text.append (welcomeText(), Theme::font (15.0f), Theme::text);

    juce::TextLayout layout;
    layout.createLayout (text, (float) juce::jmax (1, width - 16));
    return (int) std::ceil (layout.getHeight()) + 20;
}

void ChatPanel::rebuildTranscript (const std::vector<StellaAi::Entry>& entries)
{
    transcript.clear();

    // The guide shows while the conversation is empty, with New plugin and Open right under it.
    if (showingIntro != entries.empty())
    {
        showingIntro = entries.empty();
        resized();
    }

    auto add = [this] (const juce::String& text, juce::Colour colour, bool bold)
    {
        transcript.setFont (Theme::font (15.0f, bold));
        transcript.setColour (juce::TextEditor::textColourId, colour);
        transcript.insertTextAtCaret (text);
    };

    if (entries.empty())
        add (welcomeText(), Theme::text.withAlpha (0.88f), false);

    // A conversation in a right-to-left language sits on the right: the user's latest request
    // says which (Fanan AI answers in the user's language).
    bool rightToLeft = false;

    for (auto it = entries.rbegin(); it != entries.rend(); ++it)
    {
        if (it->kind == StellaAi::Entry::Kind::user)
        {
            rightToLeft = isRightToLeft (it->text);
            break;
        }
    }

    transcript.setJustification (rightToLeft ? juce::Justification::topRight : juce::Justification::topLeft);

    for (const auto& entry : entries)
    {
        switch (entry.kind)
        {
            case StellaAi::Entry::Kind::user:
                add ("You\n", Theme::accent, true);
                add (entry.text + "\n\n", Theme::text, false);
                break;

            case StellaAi::Entry::Kind::ai:
                add ("Fanan AI\n", Theme::text, true);
                appendAnswer (entry.text);
                add ("\n\n", Theme::text, false);
                break;

            case StellaAi::Entry::Kind::notice:
                add (entry.text + "\n\n", Theme::hot, false);
                break;

            case StellaAi::Entry::Kind::activity:
                transcript.setFont (Theme::font (13.0f));
                transcript.setColour (juce::TextEditor::textColourId, Theme::muted);
                transcript.insertTextAtCaret (juce::String::fromUTF8 ("\xe2\x80\xba ") + entry.text + "\n\n");
                break;

            case StellaAi::Entry::Kind::plan:
            {
                // The plan's card (StellaAi::planText): the title, what it builds, the steps as a
                // checklist with their credits (done ones ticked, the one under way bright), later
                // phases and notes, and a warning when the credits may not last.
                for (const auto& line : juce::StringArray::fromLines (entry.text))
                {
                    const auto rest = line.substring (2);

                    if (line.startsWith ("# "))
                        add (rest + "\n", Theme::text, true);
                    else if (line.startsWith ("~ "))
                        add (rest + "\n", Theme::text.withAlpha (0.88f), false);
                    else if (line.startsWith ("+ ") || line.startsWith ("= "))
                        add ("   " + rest + "\n", Theme::muted, false);
                    else if (line.startsWith ("! "))
                        add ("   " + rest + "\n", Theme::hot, false);
                    else if (line[0] == (juce::juce_wchar) 0x2713)   // ✓ done (JUCE's startsWith compares bytes, so by character)
                        add ("   " + line + "\n", Theme::safe, false);
                    else if (line[0] == (juce::juce_wchar) 0x25b8)   // ▸ under way
                        add ("   " + line + "\n", Theme::text, true);
                    else
                        add ("   " + line + "\n", Theme::text.withAlpha (0.72f), false);
                }

                add ("\n", Theme::text, false);
                break;
            }
        }
    }

    if (busy)
        add (juce::String::fromUTF8 ("Fanan AI is working\xe2\x80\xa6\n"), Theme::muted, false);

    transcript.moveCaretToEnd();
}

//==============================================================================
void ChatPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    if (account.signedIn && ! inputBox.isEmpty())
    {
        g.setColour (Theme::inset);
        g.fillRoundedRectangle (inputBox.toFloat(), 8.0f);
        g.setColour (input.hasKeyboardFocus (true) ? Theme::accent.withAlpha (0.6f) : Theme::outline);
        g.drawRoundedRectangle (inputBox.toFloat().reduced (0.5f), 8.0f, 1.0f);
    }

    if (! planBarArea.isEmpty())
    {
        // The plan waits for the user: its price, beside the buttons.
        const auto box = planBarArea.toFloat();
        const auto edge = planBarWarning ? Theme::hot : Theme::accent;

        g.setColour (edge.withAlpha (0.10f));
        g.fillRoundedRectangle (box, 8.0f);
        g.setColour (edge.withAlpha (0.65f));
        g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, 1.0f);

        auto textArea = planBarArea.reduced (12, 0);
        textArea.setRight (planFirst.getX() - 10);

        g.setColour (planBarWarning && ! askingForChange ? Theme::hot : Theme::text);
        g.setFont (Theme::font (14.0f, true));
        g.drawFittedText (askingForChange ? juce::String ("Type below what should change in the plan, then press Enter.") : planBarText,
                          textArea, juce::Justification::centredLeft, 2, 0.9f);
    }

    paintPluginInfo (g);

    if (dragOver && ! inputBox.isEmpty())
    {
        // Files dragged over the chat: the request box says where they go.
        g.setColour (Theme::accent.withAlpha (0.16f));
        g.fillRoundedRectangle (inputBox.toFloat(), 8.0f);
        g.setColour (Theme::accent);
        g.drawRoundedRectangle (inputBox.toFloat().reduced (1.0f), 8.0f, 2.0f);
    }

    // Title on the left; the connection light and the credits in the top right corner.
    g.setColour (Theme::text);
    g.setFont (Theme::font (16.0f, true));
    g.drawText ("Fanan AI", titleRow, juce::Justification::centredLeft, false);

    {
        const auto text = cornerText();
        const auto font = Theme::font (13.0f);

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, 0.0f);
        const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (Theme::font (16.0f, true), "Fanan AI");
        const auto textWidth = juce::jmin ((float) (titleRow.getWidth() - titleWidth) - 40.0f,
                                           glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), true).getWidth() + 2.0f);

        auto corner = titleRow.toFloat();
        const auto textArea = corner.removeFromRight (textWidth);
        corner.removeFromRight (6.0f);
        const auto light = corner.removeFromRight (8.0f).withSizeKeepingCentre (8.0f, 8.0f);

        const auto lightColour = account.status == FananServer::Status::online   ? Theme::safe
                               : account.status == FananServer::Status::notSetUp ? Theme::hot
                               : showsWarning()                                  ? Theme::clip
                                                                                 : Theme::muted;
        g.setColour (lightColour.withAlpha (0.35f));
        g.fillEllipse (light.expanded (2.5f));   // a soft glow, like a real LED
        g.setColour (lightColour);
        g.fillEllipse (light);

        g.setColour (showsWarning() ? Theme::clip.brighter (0.4f) : Theme::text.withAlpha (0.85f));
        g.setFont (font);
        g.drawText (text, textArea, juce::Justification::centredRight, true);
    }

    g.setColour (Theme::muted);
    g.setFont (Theme::font (13.0f));
    g.drawText ("Account", accountRow.withWidth (accountLabelGap), juce::Justification::centredLeft, false);

    g.setColour (Theme::outline);
    g.fillRect (0, headerHeight, getWidth(), 1);

    // The warning: it stays until the studio is back online.
    if (showsWarning() && ! warningArea.isEmpty())
    {
        const auto box = warningArea.toFloat();

        g.setColour (Theme::clip.withAlpha (0.14f));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (Theme::clip.withAlpha (0.75f));
        g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);

        auto text = warningArea.reduced (12, 9);
        text.removeFromBottom (retryButton.getHeight() + 4);

        const bool noInternet = account.status == FananServer::Status::noInternet;
        const bool blocked = account.status == FananServer::Status::blocked;

        g.setColour (Theme::text);
        g.setFont (Theme::font (14.5f, true));
        g.drawText (blocked ? "The internet is blocked" : noInternet ? "No internet connection" : "Fanan's server isn't responding",
                    text.removeFromTop (18), juce::Justification::centredLeft, true);

        g.setColour (Theme::text.withAlpha (0.8f));
        g.setFont (Theme::font (13.0f));
        g.drawFittedText (blocked    ? "Fanan AI works through the internet: turn it on with the switch at the top left. Everything else keeps working."
                        : noInternet ? "Fanan AI needs the internet. Everything else keeps working."
                                     : "Your internet works. Sonify keeps trying on its own.",
                          text, juce::Justification::topLeft, 2, 1.0f);
    }

    // Until the user signs in, this is all the body shows.
    if (! account.signedIn && ! bodyArea.isEmpty())
    {
        g.setColour (Theme::text.withAlpha (0.85f));
        g.setFont (Theme::font (16.0f));
        g.drawFittedText ("Sign in to start using Fanan AI", bodyArea.reduced (20), juce::Justification::centred, 2, 1.0f);
    }
}

void ChatPanel::focusInput()
{
    if (input.isShowing())
        input.grabKeyboardFocus();
}

void ChatPanel::resized()
{
    // One centred column, like a chat page: the header, the conversation, the request box.
    auto area = getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 32, columnWidth), getHeight());

    auto header = area.removeFromTop (headerHeight);
    header.removeFromTop (14);
    titleRow = header.removeFromTop (30);
    header.removeFromTop (10);

    // The account box, with Sign in / Sign out and Buy credits beside it.
    auto row = header.removeFromTop (32);
    auto pills = row.removeFromRight (buyPill.isVisible() ? pillWidth * 2 + 10 : pillWidth);

    if (buyPill.isVisible())
    {
        accountPill.setBounds (pills.removeFromLeft (pillWidth));
        pills.removeFromLeft (10);
        buyPill.setBounds (pills);
    }
    else
    {
        accountPill.setBounds (pills);
    }

    row.removeFromRight (16);
    accountRow = row.withSizeKeepingCentre (row.getWidth(), 26);
    accountBox.setBounds (accountRow.withTrimmedLeft (accountLabelGap));

    // New plugin and Open: one row as wide as the account row, from the word "Account" to the
    // end of the last pill (Buy credits).
    const auto startsLeft = accountRow.getX();
    const auto startsRight = (buyPill.isVisible() ? buyPill : accountPill).getRight();
    constexpr int startsHeight = 34, startsGap = 10;

    const auto placeStarts = [&] (int y)
    {
        auto starts = juce::Rectangle<int> (startsLeft, y, juce::jmax (0, startsRight - startsLeft), startsHeight);
        const auto half = (starts.getWidth() - startsGap) / 2;
        newPluginButton.setBounds (starts.removeFromLeft (half));
        starts.removeFromLeft (startsGap);
        openButton.setBounds (starts);
    };

    area.removeFromTop (1);   // the line under the header

    // The plugin's card: beside the column when there's room, otherwise a line above the conversation.
    infoArea = {};
    infoInline = false;

    if (pluginName.isNotEmpty())
    {
        const auto room = area.getX() - 16 - 16;

        if (room >= 170)
        {
            infoArea = juce::Rectangle<int> (16 + juce::jmax (0, room - infoWidth), area.getY() + 16, juce::jmin (infoWidth, room), infoHeight);
        }
        else
        {
            infoInline = true;
            infoArea = area.removeFromTop (infoLineHeight).withTrimmedTop (6);
        }
    }

    if (showsWarning())
    {
        warningArea = area.removeFromTop (warningHeight).reduced (0, 10);
        retryButton.setBounds (warningArea.reduced (10, 8).removeFromBottom (26).removeFromRight (76));
    }
    else
    {
        warningArea = {};
    }

    bodyArea = area;
    planBarArea = {};

    if (! account.signedIn)
    {
        planFirst.setVisible (false);
        planSecond.setVisible (false);
        planThird.setVisible (false);

        // Under "Sign in to start using Sonify AI": a plugin can still be started or opened.
        placeStarts (bodyArea.getCentreY() + 28);
    }

    if (account.signedIn)
    {
        // Attached files as chips above the request box, in as many rows as they need; the
        // attach and send buttons inside it.
        const auto chipWidth = [rowWidth = area.getWidth()] (const juce::TextButton* chip)
        {
            return juce::jmin (170, rowWidth, juce::GlyphArrangement::getStringWidthInt (Theme::font (13.0f), chip->getButtonText()) + 24);
        };

        int chipRows = 0;

        for (int i = 0, x = 0; i < chips.size(); ++i)
        {
            const auto width = chipWidth (chips[i]);

            if (i == 0 || x + width > area.getWidth())
            {
                ++chipRows;
                x = 0;
            }

            x += width + 6;
        }

        const auto chipRow = chipRows * 30;
        auto bottom = area.removeFromBottom (inputHeight + chipRow).withTrimmedTop (8).withTrimmedBottom (18);

        // The plan bar, above it all, while the plan waits for the user.
        const bool showBar = planWaiting != StellaAi::Waiting::nothing;
        planFirst.setVisible (showBar);
        planSecond.setVisible (showBar);
        planThird.setVisible (showBar && planWaiting == StellaAi::Waiting::approval);

        if (showBar)
        {
            planBarArea = area.removeFromBottom (56).withTrimmedTop (8);
            auto buttons = planBarArea.reduced (10, 9);

            if (planThird.isVisible())
            {
                planThird.setBounds (buttons.removeFromRight (78));
                buttons.removeFromRight (6);
            }

            planSecond.setBounds (buttons.removeFromRight (78));
            buttons.removeFromRight (6);
            planFirst.setBounds (buttons.removeFromRight (86));
        }
        else
        {
            planBarArea = {};
        }

        if (chipRows > 0)
        {
            auto chipArea = bottom.removeFromTop (chipRow - 6);
            auto chipLine = chipArea.removeFromTop (24);

            for (auto* chip : chips)
            {
                const auto width = chipWidth (chip);

                if (width > chipLine.getWidth() && chipLine.getX() > chipArea.getX())
                {
                    chipArea.removeFromTop (6);
                    chipLine = chipArea.removeFromTop (24);
                }

                chip->setBounds (chipLine.removeFromLeft (juce::jmin (width, chipLine.getWidth())));
                chipLine.removeFromLeft (6);
            }

            bottom.removeFromTop (6);
        }

        inputBox = bottom;
        auto buttons = bottom.removeFromBottom (40).reduced (8, 4);
        attachButton.setBounds (buttons.removeFromLeft (32));
        sendButton.setBounds (buttons.removeFromRight (32));
        buttons.removeFromRight (8);
        typeBox.setBounds (buttons.removeFromRight (128).reduced (0, 2));
        buttons.removeFromRight (8);

        if (lookBox.isVisible())
        {
            lookBox.setBounds (buttons.removeFromRight (juce::jlimit (120, 200, buttons.getWidth() / 3)).reduced (0, 2));
            buttons.removeFromRight (10);
        }

        typeHint.setBounds (buttons.withTrimmedLeft (10));
        input.setBounds (bottom.reduced (8, 6));

        // The conversation, with New plugin and Open between it and the request box: right
        // under the guide while it's empty (the room left is below them), just above the request
        // box once it has started.
        const auto conversation = area.withTrimmedTop (10).withTrimmedBottom (4);
        const auto room = juce::jmax (0, conversation.getHeight() - startsHeight - startsGap);

        if (showingIntro)
        {
            const auto guide = juce::jmin (room, introHeight (conversation.getWidth()));
            transcript.setBounds (conversation.withHeight (guide));
            placeStarts (conversation.getY() + guide + startsGap);
        }
        else
        {
            transcript.setBounds (conversation.withHeight (room));
            placeStarts (conversation.getY() + room + startsGap);
        }

        // A warning or the plan bar appearing shrinks the conversation: keep its newest lines in
        // view (again once the editor has laid itself out at the new size).
        transcript.moveCaretToTop (false);
        transcript.moveCaretToEnd();

        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<ChatPanel> (this)]
        {
            if (safeThis != nullptr)
            {
                safeThis->transcript.moveCaretToTop (false);
                safeThis->transcript.moveCaretToEnd();
            }
        });
    }
}
