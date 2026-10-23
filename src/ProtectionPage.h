// D:\workspace\Fanan_web_hub\src\ProtectionPage.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Licences.h"

#include <functional>
#include <memory>

//==============================================================================
/**
    The Plugins Creator Studio's Protection tab: the open plugin or MIDI FX, protected with a
    serial formula of its own (Licences, stella_licence.h). Its ad in the Store is the Store
    tab's.

        Protection   on or off; the product's id (made once, the first time it's turned on)
        The demo     what it does until it's registered: a plugin plays its sound for a while,
                     then goes silent for a while; a MIDI FX lets notes through, then holds
                     them back
        Your serial formula
                     its three numbers (A, B, M), how a buyer's ID becomes their serial (in
                     plain words, worked through once with these numbers), and the formula as
                     ready JavaScript (Node or a browser), PHP and Excel, each with Copy and Save
        Check a serial
                     a computer ID and a serial: whether they fit (to test the creator's site)
        license.soni the file that ships with the plugin: Save license.soni..., and Make a new
                     formula (behind a clear warning)

    Kept as it's changed, in protection.json in the project's folder (a MIDI FX's: its own
    folder in My Devices, see Devices::protectionFolderOf). Made the first time it's turned on.
*/
class ProtectionPage final : public juce::Component,
                             private juce::Timer
{
public:
    /** What's open in the studio. */
    struct Subject
    {
        juce::String key;          // the project's id or the MIDI FX's (empty: nothing open)
        juce::String kind;         // "instrument", "effect", "widget" or "midifx"
        juce::String name;         // what it's called (license.soni and the code say it)
        juce::String vendor;       // who makes it
        juce::File folder;         // where its protection.json is kept
    };

    ProtectionPage();
    ~ProtectionPage() override;

    /** The open plugin or MIDI FX changed: its protection as it was left. */
    void setSubject (const Subject& subject);
    const Subject& getSubject() const noexcept                              { return subject; }

    /** Its protection as it is now (kept). */
    const Licences::ProjectProtection& getProtection() const noexcept       { return protection; }

    /** The protection was changed and kept (turned on or off, new demo times, a new formula). */
    std::function<void()> onChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    /** Paragraphs of plain text that wrap to their width: a line starting "## " is a heading. */
    class TextBlock final : public juce::Component
    {
    public:
        void setText (const juce::String& text, float size = 13.5f, juce::Colour colour = {});
        int heightFor (int width) const;
        void paint (juce::Graphics&) override;

    private:
        juce::AttributedString attributed;
    };

    //==========================================================================
    class Form final : public juce::Component
    {
    public:
        explicit Form (ProtectionPage& owner);

        void layOut (int width);   // sets its own height
        void paint (juce::Graphics&) override;
        void resized() override    {}

        ProtectionPage& page;

        juce::Label heading, productLabel;

        // Protection on or off
        juce::Label protectionHeading;
        juce::TextButton offButton { "Off" }, onButton { "On" };
        TextBlock stateNote;
        juce::Label problemLabel;

        // The demo
        juce::Label demoHeading, playLabel, playUnit, silentLabel, silentUnit;
        juce::TextEditor playEditor, silentEditor;
        TextBlock demoNote;

        // The formula
        juce::Label formulaHeading;
        TextBlock explanation;
        juce::Label jsHeading, phpHeading, excelHeading, codeNote;
        juce::TextEditor jsEditor, phpEditor, excelEditor;
        juce::TextButton jsCopy { "Copy" }, jsSave, phpCopy { "Copy" }, phpSave, excelCopy { "Copy" }, excelSave;

        // Check a serial
        juce::Label checkHeading, idLabel, serialLabel, checkResult;
        TextBlock checkNote;
        juce::TextEditor idEditor, serialEditor;
        juce::TextButton thisComputerButton { "This computer's ID" }, checkButton { "Check" };

        // license.soni and a new formula
        juce::Label soniHeading, soniStatus;
        TextBlock soniNote, newFormulaNote;
        juce::TextButton saveSoniButton, newFormulaButton;

        std::vector<juce::Rectangle<int>> cards;
    };

    /** The formula's ready code: JavaScript, PHP, or an Excel formula. */
    enum class Code { js, php, excel };

    void timerCallback() override;
    bool isMidi() const noexcept                     { return subject.kind == "midifx"; }
    juce::String shownName() const;
    void showProtection();       // the protection into the fields
    void readFields();           // the fields into the protection
    void edited();               // typed: kept in a moment
    void flush();                // kept now
    bool keep();                 // saved now (true when it was)
    void setOn (bool on);
    void updateState();          // what shows, what it says
    void copyCode (Code kind);
    void saveCode (Code kind);
    void checkSerial();
    void saveSoni();
    void makeNewFormula();
    juce::StringArray explanationLines() const;          // "## " starts a heading
    juce::String codeFor (Code kind, bool withExplanation) const;

    Subject subject;
    Licences::ProjectProtection protection;
    bool showing = false, pending = false;

    Form form { *this };
    juce::Viewport viewport;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> confirmWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProtectionPage)
};
