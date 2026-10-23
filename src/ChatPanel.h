// D:\workspace\Fanan_web_hub\src\ChatPanel.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "FananServer.h"
#include "StellaAi.h"

#include <functional>
#include <vector>

//==============================================================================
/**
    The Build with AI tab, where everything starts: the conversation with Sonify AI, in one
    centred column.

    Header: the title, and the connection light with the credits on the right; under them
    the account box, with two yellow pill buttons beside it: Sign in / Sign out, and Buy
    credits (only once signed in).

    Signed out, the account box takes an email (passed to the site's sign-in). When the
    mouse moves into it, it suggests the email last signed in with; a click fills it in.
    Signed in, it shows the account's email.

    While a plugin is open (made or opened), its card sits at the left of the conversation:
    its name, its type and its developer (above the conversation when there's no room beside it).

    Below: until the user signs in, only "Sign in to start using Sonify AI". Once signed
    in, the conversation, the guide and the request box appear. New plugin and Open sit in
    one row as wide as the account row (from "Account" to the end of Buy credits): right
    under the guide while the conversation is empty, then just above the request box. Beside Send, the Type
    selector: every plugin starts with its exact type, chosen there (Instrument, Effect,
    MIDI FX or Widget, the rare things), never guessed from the request; with a plugin open
    it shows that plugin's type, locked. Beside it, the Look picker: Auto (Fanan AI chooses
    a look from its inspiration pool) or one of the pool's looks to design the panel in.
    When Sonify AI's plan waits
    for the user, a bar above the request box says its price, with Start, Change and Cancel
    (or, for a step that ran over its estimate, Go on and Pause).

    When the studio can't get online, a warning sits above the body (with Retry) until it
    can: no internet, or Fanan's server not answering, each with its own words.
*/
class ChatPanel final : public juce::Component,
                        public juce::FileDragAndDropTarget
{
public:
    struct Account
    {
        FananServer::Status status = FananServer::Status::checking;
        bool siteAvailable = false;
        bool signedIn = false;
        bool waitingForBrowser = false;   // sent to the browser to sign in, not back yet
        int credits = -1;
        juce::String email, computerCode;
        juce::String lastEmail;           // suggested when signing in again
    };

    ChatPanel();

    /** What's built: chosen beside Send before a new plugin starts (none until then), or
        locked to the open plugin's own type. */
    enum class BuildType { none = 0, instrument, effect, midiFx, widget };
    void setBuildType (BuildType type, bool locked);
    BuildType getBuildType() const noexcept;

    /** The look the panel is designed in, picked beside Send from Fanan AI's inspiration pool:
        a look's name, or empty for Auto (Fanan AI chooses one, and says which). */
    juce::String getPickedLook() const;
    std::function<void (const juce::String& look)> onLookPicked;

    void setConversation (const std::vector<StellaAi::Entry>& entries, bool busy);

    /** The plugin being made, for its card at the left of the conversation (name empty: none). */
    void setPluginInfo (const juce::String& name, const juce::String& type, const juce::String& developer);

    /** What the plan waits for (nothing hides the bar), the price line, and whether the
        credits may not last. */
    void setPlanBar (StellaAi::Waiting waiting, const juce::String& text, bool warning);

    /** Puts the cursor in the request box (when the tab is shown). */
    void focusInput();
    void setAccount (const Account& newAccount);

    /** The request, and the files attached to it (images, PDFs, text, and the user's own code:
        code files, zips and folders). */
    std::function<void (const juce::String& request, const juce::Array<juce::File>& files)> onSend;
    std::function<void()> onBuyCredits;
    std::function<void()> onRetry;
    std::function<void (const juce::String& email)> onSignIn;   // the email typed, if any
    std::function<void()> onSignOut;
    std::function<void()> onNewPlugin;
    std::function<void()> onOpenPlugin;
    std::function<void()> onStop;      // Sonify AI is working: Send turns into Stop

    // The plan bar.
    std::function<void()> onPlanStart, onPlanCancel;     // Change only asks for the change in the request box
    std::function<void()> onPlanGoOn, onPlanPause;       // a step over its estimate

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseEnter (const juce::MouseEvent&) override;

    // Files dropped on the panel are attached to the next request.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void paintOverChildren (juce::Graphics&) override;
    void fileDragExit (const juce::StringArray& files) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;


private:
    //==============================================================================
    /** A long, fully rounded yellow button with a black label. */
    class Pill final : public juce::Button
    {
    public:
        explicit Pill (const juce::String& label);
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    };

    enum class BoxMode { editing, showingAccount, waiting };

    void send();
    void signIn();
    void setBoxMode (BoxMode mode);
    void showSuggestion (bool show);
    bool isAccountBoxEvent (const juce::MouseEvent& e) const;
    bool canSuggest() const;
    bool showsWarning() const noexcept;
    juce::String cornerText() const;
    void rebuildTranscript (const std::vector<StellaAi::Entry>& entries);
    static int introHeight (int width);   // the guide's height in the conversation, at this width
    void appendAnswer (const juce::String& text);

    /** Round icon buttons inside the request box: attach (a paperclip), send (an arrow) and stop. */
    class IconButton final : public juce::Button
    {
    public:
        enum class Icon { attach, send, stop };

        explicit IconButton (Icon i) : juce::Button ({}), icon (i) {}
        void setIcon (Icon i)    { icon = i; repaint(); }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        Icon icon;
    };

    void addAttachment (const juce::File& file);
    void refreshChips();
    void updateSendButton();
    void chooseFiles();
    void askForType();   // Send with no type chosen: the selector opens, the request waits

    juce::TextEditor transcript, input;
    juce::TextButton retryButton { "Retry" };
    IconButton sendButton { IconButton::Icon::send }, attachButton { IconButton::Icon::attach };
    juce::ComboBox typeBox;          // the Type selector, beside Send
    juce::ComboBox lookBox;          // the Look picker beside it: Auto, or a look of the inspiration pool
    juce::StringArray lookNames;     // ...its looks, by item id - 2
    juce::Label typeHint;            // "First choose what you're building", until it's chosen
    bool typeLocked = false;
    juce::Array<juce::File> attachments;
    juce::OwnedArray<juce::TextButton> chips;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> inputBox;
    bool dragOver = false;
    Pill accountPill { "Sign in" }, buyPill { "Buy credits" };
    juce::TextButton newPluginButton, openButton;
    juce::TextEditor accountBox;

    Account account;
    bool busy = false;
    bool showingIntro = true;     // the conversation is empty: it shows the guide
    BoxMode boxMode = BoxMode::editing;
    juce::String typedEmail;   // kept while the box shows something else

    juce::Rectangle<int> titleRow, accountRow, warningArea, bodyArea;

    // The plugin's card: beside the column, or a line above the conversation.
    juce::String pluginName, pluginType, pluginDeveloper;
    juce::Rectangle<int> infoArea;
    bool infoInline = false;
    void paintPluginInfo (juce::Graphics&);

    StellaAi::Waiting planWaiting = StellaAi::Waiting::nothing;
    juce::String planBarText;
    bool planBarWarning = false;
    bool askingForChange = false;   // Change was pressed: the bar asks for the change until it's sent
    juce::TextButton planFirst, planSecond, planThird;   // Start / Change / Cancel, or Go on / Pause
    juce::Rectangle<int> planBarArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChatPanel)
};
