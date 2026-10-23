// D:\workspace\Fanan_web_hub\src\ProtectionPage.cpp

#include "ProtectionPage.h"
#include "StellaLookAndFeel.h"

namespace
{
    constexpr int onOffGroup = 4202;
    constexpr int margin = 24, gap = 18, rowHeight = 30, labelHeight = 20;
    constexpr int maxFormWidth = 980;
    constexpr int codeHeight = 250, formulaHeight = 96;
    constexpr const char* sampleId = "482-039-175";   // the worked example's computer ID
    constexpr double minSeconds = 1.0, maxSeconds = 600.0;

    const juce::Colour problemColour { 0xffff8a80 };
    const juce::Colour goodColour { 0xff69d391 };

    void styleHeading (juce::Label& label, const juce::String& text, float size)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Theme::font (size, true));
        label.setColour (juce::Label::textColourId, Theme::text);
        label.setJustificationType (juce::Justification::centredLeft);
    }

    void styleNote (juce::Label& label, const juce::String& text = {})
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Theme::font (13.5f));
        label.setColour (juce::Label::textColourId, Theme::muted);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setMinimumHorizontalScale (1.0f);
    }

    void styleSeconds (juce::TextEditor& editor)
    {
        editor.setFont (Theme::font (14.5f));
        editor.setInputRestrictions (5, "0123456789.");
        editor.setJustification (juce::Justification::centred);
        editor.setIndents (4, 7);
    }

    void styleCode (juce::TextEditor& editor)
    {
        editor.setMultiLine (true, false);
        editor.setReadOnly (true);
        editor.setScrollbarsShown (true);
        editor.setCaretVisible (false);
        editor.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain)));
        editor.setIndents (10, 8);
        editor.setColour (juce::TextEditor::backgroundColourId, Theme::inset);
    }

    void styleField (juce::TextEditor& editor, const juce::String& example)
    {
        editor.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.5f, juce::Font::plain)));
        editor.setInputRestrictions (16, "0123456789- ");   // digits, dashes and spaces: anything else typed or pasted is left out
        editor.setIndents (8, 7);
        editor.setTextToShowWhenEmpty (example, Theme::muted);
    }

    /** "10", "2.5": seconds as people type them. */
    juce::String secondsText (double seconds)
    {
        const auto tenths = juce::roundToInt (seconds * 10.0);
        return tenths % 10 == 0 ? juce::String (tenths / 10) : juce::String (tenths / 10.0, 1);
    }

    /** The plugin's name as code can use it: its letters and digits, each word capitalised. */
    juce::String codeName (const juce::String& name)
    {
        juce::String out;
        bool wordStart = true;

        for (auto c : name)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            {
                out << juce::String::charToString (wordStart ? juce::CharacterFunctions::toUpperCase (c) : c);
                wordStart = false;
            }
            else
            {
                wordStart = true;
            }
        }

        return out.substring (0, 40).isNotEmpty() ? out.substring (0, 40) : juce::String ("Product");
    }

    juce::String number (std::uint64_t value)
    {
        return juce::String ((juce::int64) value);
    }

    juce::String fromStd (const std::string& text)
    {
        return juce::String (juce::CharPointer_UTF8 (text.c_str()));
    }
}

//==============================================================================
void ProtectionPage::TextBlock::setText (const juce::String& text, float size, juce::Colour colour)
{
    const auto body = colour.isTransparent() ? Theme::text.withAlpha (0.82f) : colour;
    attributed = {};
    attributed.setWordWrap (juce::AttributedString::byWord);
    attributed.setLineSpacing (3.0f);

    const auto lines = juce::StringArray::fromLines (text);

    for (int i = 0; i < lines.size(); ++i)
    {
        const auto& line = lines[i];
        const bool heading = line.startsWith ("## ");
        const auto shown = (heading ? line.substring (3) : line) + (i < lines.size() - 1 ? "\n" : "");

        if (heading)
            attributed.append (shown, Theme::font (size + 1.5f, true), Theme::text);
        else
            attributed.append (shown, Theme::font (size), body);
    }

    repaint();
}

int ProtectionPage::TextBlock::heightFor (int width) const
{
    if (width <= 0 || attributed.getText().isEmpty())
        return 0;

    juce::TextLayout layout;
    layout.createLayout (attributed, (float) width);
    return (int) std::ceil (layout.getHeight()) + 4;
}

void ProtectionPage::TextBlock::paint (juce::Graphics& g)
{
    attributed.draw (g, getLocalBounds().toFloat());
}

//==============================================================================
ProtectionPage::Form::Form (ProtectionPage& owner)
    : page (owner)
{
    styleHeading (heading, "Protection", 22.0f);
    styleNote (productLabel);
    productLabel.setJustificationType (juce::Justification::centredRight);

    // On or off.
    styleHeading (protectionHeading, "Protect it", 16.0f);

    for (auto* button : { &offButton, &onButton })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (onOffGroup);
        button->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    }

    offButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    onButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    offButton.setTooltip ("Not protected: it plays fully on every computer");
    onButton.setTooltip ("Protected: until a buyer registers it on their computer with a serial from you, it plays its demo");
    offButton.onClick = [this] { if (offButton.getToggleState()) page.setOn (false); };
    onButton.onClick = [this] { if (onButton.getToggleState()) page.setOn (true); };
    styleNote (problemLabel);
    problemLabel.setColour (juce::Label::textColourId, problemColour);

    // The demo.
    styleHeading (demoHeading, "The demo", 16.0f);

    for (auto* label : { &playLabel, &playUnit, &silentLabel, &silentUnit })
    {
        styleNote (*label);
        label->setColour (juce::Label::textColourId, Theme::text.withAlpha (0.85f));
    }

    styleSeconds (playEditor);
    styleSeconds (silentEditor);
    playEditor.onTextChange = [this] { page.edited(); };
    silentEditor.onTextChange = [this] { page.edited(); };
    playEditor.onFocusLost = [this] { page.flush(); page.showProtection(); };
    silentEditor.onFocusLost = [this] { page.flush(); page.showProtection(); };

    // The formula.
    styleHeading (formulaHeading, "Your serial formula", 16.0f);
    styleHeading (jsHeading, "JavaScript (Node on your server, or a browser for testing)", 14.0f);
    styleHeading (phpHeading, "PHP (on your server)", 14.0f);
    styleHeading (excelHeading, "Excel (the buyer's ID in cell A2, this formula in B2)", 14.0f);
    styleCode (jsEditor);
    styleCode (phpEditor);
    styleCode (excelEditor);
    excelEditor.setMultiLine (true, true);   // one long line: wrapped to be seen whole
    styleNote (codeNote);

    jsSave.setButtonText (juce::String::fromUTF8 ("Save .js\xe2\x80\xa6"));
    phpSave.setButtonText (juce::String::fromUTF8 ("Save .php\xe2\x80\xa6"));
    excelSave.setButtonText (juce::String::fromUTF8 ("Save .txt\xe2\x80\xa6"));
    jsCopy.setTooltip ("Copies the JavaScript code");
    phpCopy.setTooltip ("Copies the PHP code");
    excelCopy.setTooltip ("Copies the Excel formula, to paste into cell B2");
    jsSave.setTooltip ("Saves it as a .js file, with the explanation above as comments at its top");
    phpSave.setTooltip ("Saves it as a .php file, with the explanation above as comments at its top");
    excelSave.setTooltip ("Saves the formula in a .txt file, with the explanation above at its top");
    jsCopy.onClick = [this] { page.copyCode (Code::js); };
    phpCopy.onClick = [this] { page.copyCode (Code::php); };
    excelCopy.onClick = [this] { page.copyCode (Code::excel); };
    jsSave.onClick = [this] { page.saveCode (Code::js); };
    phpSave.onClick = [this] { page.saveCode (Code::php); };
    excelSave.onClick = [this] { page.saveCode (Code::excel); };

    // Check a serial.
    styleHeading (checkHeading, "Check a serial", 16.0f);
    styleNote (idLabel, "Computer ID");
    styleNote (serialLabel, "Serial");
    styleField (idEditor, sampleId);
    styleField (serialEditor, "00000-00000");
    idEditor.onReturnKey = [this] { page.checkSerial(); };
    serialEditor.onReturnKey = [this] { page.checkSerial(); };
    idEditor.onTextChange = [this] { checkResult.setText ({}, juce::dontSendNotification); };
    serialEditor.onTextChange = [this] { checkResult.setText ({}, juce::dontSendNotification); };
    thisComputerButton.setTooltip ("Puts this computer's ID in, to try a serial for it");
    thisComputerButton.onClick = [this]
    {
        idEditor.setText (Licences::computerId(), true);
        serialEditor.grabKeyboardFocus();
    };
    checkButton.setColour (juce::TextButton::buttonColourId, Theme::accent);
    checkButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    checkButton.onClick = [this] { page.checkSerial(); };
    styleNote (checkResult);

    // license.soni, and a new formula.
    styleHeading (soniHeading, "license.soni", 16.0f);
    saveSoniButton.setButtonText (juce::String::fromUTF8 ("Save license.soni\xe2\x80\xa6"));
    saveSoniButton.setTooltip ("Saves this product's license.soni into a folder you choose (next to your plugin)");
    saveSoniButton.onClick = [this] { page.saveSoni(); };
    styleNote (soniStatus);

    newFormulaButton.setButtonText (juce::String::fromUTF8 ("Make a new formula\xe2\x80\xa6"));
    newFormulaButton.setTooltip ("Only before you release it, or if your formula got out: every serial already given stops working");
    newFormulaButton.onClick = [this] { page.makeNewFormula(); };

    for (auto* c : std::initializer_list<juce::Component*> {
             &heading, &productLabel, &protectionHeading, &offButton, &onButton, &stateNote, &problemLabel,
             &demoHeading, &playLabel, &playEditor, &playUnit, &silentLabel, &silentEditor, &silentUnit, &demoNote,
             &formulaHeading, &explanation, &jsHeading, &jsEditor, &jsCopy, &jsSave, &phpHeading, &phpEditor, &phpCopy, &phpSave,
             &excelHeading, &excelEditor, &excelCopy, &excelSave, &codeNote,
             &checkHeading, &checkNote, &idLabel, &idEditor, &thisComputerButton, &serialLabel, &serialEditor, &checkButton, &checkResult,
             &soniHeading, &soniNote, &saveSoniButton, &soniStatus, &newFormulaNote, &newFormulaButton })
        addAndMakeVisible (c);
}

void ProtectionPage::Form::layOut (int width)
{
    const auto formWidth = juce::jmin (maxFormWidth, width);
    auto area = juce::Rectangle<int> ((width - formWidth) / 2, 0, formWidth, 100000).reduced (margin, margin);
    cards.clear();

    auto top = area.removeFromTop (34);
    heading.setBounds (top.removeFromLeft (220));
    productLabel.setBounds (top);
    area.removeFromTop (gap);

    // One card: what fill() lays out inside it, then the card around it.
    auto card = [&] (auto&& fill)
    {
        const auto cardTop = area.getY();
        auto inner = area.reduced (gap, 0).withTrimmedTop (gap);
        fill (inner);
        const auto bottom = inner.getY() + gap;
        cards.push_back ({ area.getX(), cardTop, area.getWidth(), bottom - cardTop });
        area.setTop (bottom + gap);
    };

    auto block = [] (juce::Rectangle<int>& inner, TextBlock& text)
    {
        text.setBounds (inner.removeFromTop (text.heightFor (inner.getWidth())));
    };

    const bool on = onButton.getToggleState();

    // Protection on or off, and the demo.
    card ([&] (juce::Rectangle<int>& inner)
    {
        protectionHeading.setBounds (inner.removeFromTop (labelHeight + 4));
        inner.removeFromTop (8);
        auto row = inner.removeFromTop (rowHeight);
        offButton.setBounds (row.removeFromLeft (80));
        onButton.setBounds (row.removeFromLeft (80));
        inner.removeFromTop (10);
        block (inner, stateNote);
        problemLabel.setBounds (inner.removeFromTop (problemLabel.getText().isNotEmpty() ? labelHeight + 4 : 0));

        demoHeading.setVisible (on);
        playLabel.setVisible (on);
        playEditor.setVisible (on);
        playUnit.setVisible (on);
        silentLabel.setVisible (on);
        silentEditor.setVisible (on);
        silentUnit.setVisible (on);
        demoNote.setVisible (on);

        if (on)
        {
            inner.removeFromTop (gap);
            demoHeading.setBounds (inner.removeFromTop (labelHeight + 4));
            inner.removeFromTop (8);

            for (auto [label, editor, unit] : { std::tuple<juce::Label*, juce::TextEditor*, juce::Label*> { &playLabel, &playEditor, &playUnit },
                                                std::tuple<juce::Label*, juce::TextEditor*, juce::Label*> { &silentLabel, &silentEditor, &silentUnit } })
            {
                row = inner.removeFromTop (rowHeight);
                label->setBounds (row.removeFromLeft (200));
                editor->setBounds (row.removeFromLeft (64));
                row.removeFromLeft (8);
                unit->setBounds (row);
                inner.removeFromTop (6);
            }

            inner.removeFromTop (4);
            block (inner, demoNote);
        }
    });

    for (auto* c : std::initializer_list<juce::Component*> {
             &formulaHeading, &explanation, &jsHeading, &jsEditor, &jsCopy, &jsSave, &phpHeading, &phpEditor, &phpCopy, &phpSave,
             &excelHeading, &excelEditor, &excelCopy, &excelSave, &codeNote,
             &checkHeading, &checkNote, &idLabel, &idEditor, &thisComputerButton, &serialLabel, &serialEditor, &checkButton, &checkResult,
             &soniHeading, &soniNote, &saveSoniButton, &soniStatus, &newFormulaNote, &newFormulaButton })
        c->setVisible (on);

    if (on)
    {
        // The formula: how selling works with it, and its code.
        card ([&] (juce::Rectangle<int>& inner)
        {
            formulaHeading.setBounds (inner.removeFromTop (labelHeight + 4));
            inner.removeFromTop (10);
            block (inner, explanation);

            using CodeRow = std::tuple<juce::Label*, juce::TextEditor*, juce::Button*, juce::Button*, int>;

            for (auto [title, editor, copy, save, height] : { CodeRow { &jsHeading, &jsEditor, &jsCopy, &jsSave, codeHeight },
                                                              CodeRow { &phpHeading, &phpEditor, &phpCopy, &phpSave, codeHeight },
                                                              CodeRow { &excelHeading, &excelEditor, &excelCopy, &excelSave, formulaHeight } })
            {
                inner.removeFromTop (gap);
                auto row = inner.removeFromTop (rowHeight);
                save->setBounds (row.removeFromRight (120));
                row.removeFromRight (8);
                copy->setBounds (row.removeFromRight (80));
                row.removeFromRight (10);
                title->setBounds (row);
                inner.removeFromTop (6);
                editor->setBounds (inner.removeFromTop (height));
            }

            inner.removeFromTop (6);
            codeNote.setBounds (inner.removeFromTop (labelHeight + 2));
        });

        // Check a serial.
        card ([&] (juce::Rectangle<int>& inner)
        {
            checkHeading.setBounds (inner.removeFromTop (labelHeight + 4));
            inner.removeFromTop (6);
            block (inner, checkNote);
            inner.removeFromTop (10);

            auto row = inner.removeFromTop (rowHeight);
            idLabel.setBounds (row.removeFromLeft (100));
            thisComputerButton.setBounds (row.removeFromRight (170));
            row.removeFromRight (10);
            idEditor.setBounds (row.withWidth (juce::jmin (row.getWidth(), 300)));
            inner.removeFromTop (8);

            row = inner.removeFromTop (rowHeight);
            serialLabel.setBounds (row.removeFromLeft (100));
            checkButton.setBounds (row.removeFromRight (170));
            row.removeFromRight (10);
            serialEditor.setBounds (row.withWidth (juce::jmin (row.getWidth(), 360)));
            inner.removeFromTop (8);
            checkResult.setBounds (inner.removeFromTop (checkResult.getText().isNotEmpty() ? 40 : 0));
        });

        // license.soni, and a new formula.
        card ([&] (juce::Rectangle<int>& inner)
        {
            soniHeading.setBounds (inner.removeFromTop (labelHeight + 4));
            inner.removeFromTop (6);
            block (inner, soniNote);
            inner.removeFromTop (10);
            auto row = inner.removeFromTop (rowHeight);
            saveSoniButton.setBounds (row.removeFromLeft (190));
            row.removeFromLeft (12);
            soniStatus.setBounds (row);
            inner.removeFromTop (gap);
            block (inner, newFormulaNote);
            inner.removeFromTop (8);
            newFormulaButton.setBounds (inner.removeFromTop (rowHeight).withWidth (210));
        });
    }

    setSize (width, area.getY() - gap + margin);
}

void ProtectionPage::Form::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    for (const auto& card : cards)
    {
        g.setColour (Theme::raised.withAlpha (0.55f));
        g.fillRoundedRectangle (card.toFloat(), 10.0f);
        g.setColour (Theme::outline);
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }
}

//==============================================================================
ProtectionPage::ProtectionPage()
{
    viewport.setViewedComponent (&form, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    updateState();
}

ProtectionPage::~ProtectionPage()
{
    flush();
}

juce::String ProtectionPage::shownName() const
{
    return subject.name.trim().isNotEmpty() ? subject.name.trim() : juce::String (isMidi() ? "your MIDI FX" : "your plugin");
}

void ProtectionPage::setSubject (const Subject& newSubject)
{
    if (newSubject.key == subject.key && newSubject.kind == subject.kind && newSubject.folder == subject.folder)
    {
        // The same one: only its name or vendor may have changed (the code says them).
        subject = newSubject;
        updateState();
        return;
    }

    flush();
    subject = newSubject;
    protection = subject.key.isNotEmpty() ? Licences::loadProtection (subject.folder) : Licences::ProjectProtection();

    // Protected with a formula of the old shape (letters): it's gone, so a new one (three numbers) is made and kept.
    if (protection.on && ! protection.formula.isValid())
    {
        protection.formula = Licences::newFormula();
        keep();
    }

    form.problemLabel.setText ({}, juce::dontSendNotification);
    form.checkResult.setText ({}, juce::dontSendNotification);
    form.soniStatus.setText ({}, juce::dontSendNotification);
    form.codeNote.setText ({}, juce::dontSendNotification);
    form.idEditor.clear();
    form.serialEditor.clear();
    showProtection();
    updateState();
}

//==============================================================================
void ProtectionPage::showProtection()
{
    const juce::ScopedValueSetter<bool> quiet (showing, true);

    (protection.on ? form.onButton : form.offButton).setToggleState (true, juce::dontSendNotification);
    form.playEditor.setText (secondsText (isMidi() ? protection.passSeconds : protection.playSeconds), false);
    form.silentEditor.setText (secondsText (isMidi() ? protection.holdSeconds : protection.silentSeconds), false);
}

void ProtectionPage::readFields()
{
    auto read = [] (const juce::TextEditor& editor, double previous)
    {
        const auto value = editor.getText().trim().getDoubleValue();
        return value > 0.0 ? juce::jlimit (minSeconds, maxSeconds, value) : previous;
    };

    if (isMidi())
    {
        protection.passSeconds = read (form.playEditor, protection.passSeconds);
        protection.holdSeconds = read (form.silentEditor, protection.holdSeconds);
    }
    else
    {
        protection.playSeconds = read (form.playEditor, protection.playSeconds);
        protection.silentSeconds = read (form.silentEditor, protection.silentSeconds);
    }
}

void ProtectionPage::edited()
{
    if (showing || subject.key.isEmpty())
        return;

    pending = true;
    startTimer (600);
}

void ProtectionPage::timerCallback()
{
    flush();
}

void ProtectionPage::flush()
{
    stopTimer();

    if (! pending || subject.key.isEmpty())
        return;

    pending = false;
    readFields();

    if (keep())
    {
        updateState();

        if (onChanged != nullptr)
            onChanged();
    }
}

bool ProtectionPage::keep()
{
    if (Licences::saveProtection (subject.folder, protection))
    {
        form.problemLabel.setText ({}, juce::dontSendNotification);
        return true;
    }

    form.problemLabel.setText ("Couldn't save its protection in " + subject.folder.getFullPathName() + ". Is the folder read-only?",
                               juce::dontSendNotification);
    resized();
    return false;
}

void ProtectionPage::setOn (bool on)
{
    if (subject.key.isEmpty() || on == protection.on)
        return;

    flush();

    // The first time it's turned on, its product id and its formula are made, for good.
    if (on && protection.product.isEmpty())
        protection.product = Licences::newProductId();

    if (on && ! protection.formula.isValid())
        protection.formula = Licences::newFormula();

    protection.on = on;

    if (keep() && onChanged != nullptr)
        onChanged();

    showProtection();
    updateState();
}

//==============================================================================
juce::StringArray ProtectionPage::explanationLines() const
{
    const auto& f = protection.formula;
    const auto name = shownName();
    const auto times = juce::String::fromUTF8 (" \xc3\x97 ");

    // The worked example, with this product's own numbers (ID x A < 9 x 10^15: exact in 64 bits).
    const auto id = stella::licence::clean (sampleId);
    const auto idValue = (std::uint64_t) fromStd (id).getLargeIntValue();
    const auto product = idValue * f.a;
    const auto sum = product + f.b;
    const auto left = sum % f.m;
    const auto serial = f.serialFor (sampleId);
    const auto leftDigits = (int) number (left).length();
    const auto zeros = stella::licence::serialLength - leftDigits;
    static const char* const counts[] = { "no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };

    juce::StringArray lines;

    lines.add ("## Your three numbers");
    lines.add ("A = " + number (f.a));
    lines.add ("B = " + number (f.b));
    lines.add ("M = " + number (f.m));
    lines.add ("Keep these numbers private, and never change them after you start selling: every serial you already gave would "
               "stop working.");
    lines.add ({});
    lines.add ("## From your buyer's ID to their serial");
    lines.add ("Start with the computer ID your buyer gives you: 9 digits, like " + juce::String (sampleId) + ". Then:");
    lines.add ("1.  Take out the dashes (and any spaces): " + fromStd (id) + ".");
    lines.add ("2.  Multiply it by A: " + fromStd (id) + times + number (f.a) + " = " + number (product));
    lines.add ("3.  Add B: " + number (product) + " + " + number (f.b) + " = " + number (sum));
    lines.add ("4.  Divide that by M and keep only what's left over (the remainder): M goes into it " + number (sum / f.m)
               + " whole times, with " + number (left) + " left over.");

    if (zeros <= 0)
        lines.add ("5.  Write it with 10 digits, putting zeros in front when it's shorter. It has 10 already, so the serial is "
                   + fromStd (serial) + ".");
    else
        lines.add ("5.  Write it with 10 digits, putting zeros in front when it's shorter. It has " + juce::String (leftDigits) + ", so "
                   + counts[juce::jlimit (0, 10, zeros)] + (zeros == 1 ? " zero goes" : " zeros go") + " in front: the serial is "
                   + fromStd (serial) + ".");

    lines.add ("That's the serial: send it to your buyer. With or without a dash in the middle (" + fromStd (stella::licence::grouped (serial))
               + "), it works the same. The JavaScript, PHP and Excel below do exactly these steps for you.");
    lines.add ({});
    lines.add ("## One serial, one computer");
    lines.add ("Each serial unlocks " + name + " only on the one computer whose ID it was made from: on any other computer it doesn't "
               "work. A new computer has a new ID, so it needs a new serial: your buyer sends you the new ID, and you make its serial "
               "the same way.");
    return lines;
}

juce::String ProtectionPage::codeFor (Code kind, bool withExplanation) const
{
    const auto& f = protection.formula;
    const auto name = shownName();
    const auto code = codeName (name);
    const auto example = fromStd (f.serialFor (sampleId));
    const auto a = number (f.a), b = number (f.b), m = number (f.m);

    if (kind == Code::excel && ! withExplanation)
    {
        // The ID's digits (dashes and spaces taken out), checked to be 9 digits; then (ID x A + B) mod M
        // with A split in two (ID x A would pass Excel's 15 exact digits): every step stays exact.
        const juce::String id ("SUBSTITUTE(SUBSTITUTE(A2,\"-\",\"\"),\" \",\"\")");

        return "=IFERROR(IF(AND(LEN(" + id + ")=9," + id + "=TEXT(" + id + ",\"000000000\")),"
               "TEXT(MOD(MOD(" + id + "*INT(" + a + "/1000)," + m + ")*1000+" + id + "*MOD(" + a + ",1000)+" + b + "," + m + "),"
               "\"0000000000\"),\"\"),\"\")";
    }

    const juce::String comment (kind == Code::excel ? "" : "// ");
    const auto commentLine = comment.trimEnd();
    juce::String out;

    if (kind == Code::php)
        out << "<?php\n";

    out << comment << name << (subject.vendor.trim().isNotEmpty() ? " by " + subject.vendor.trim() : juce::String())
        << ": its serial formula, made by Sonify's Protection tab.\n"
        << comment << "Product: " << protection.product << "\n"
        << comment << "Keep this file private: anyone who has it can make serials for " << name << ".\n"
        << comment << "Never change these numbers after you start selling: every serial already given would stop working.\n";

    if (withExplanation)
    {
        out << commentLine << "\n";

        for (const auto& line : explanationLines())
        {
            if (line.startsWith ("## "))
                out << commentLine << "\n" << comment << line.substring (3).toUpperCase() << "\n";
            else if (line.isNotEmpty())
                out << comment << line << "\n";
        }

        out << commentLine << "\n";
    }

    out << commentLine << "\n";

    if (kind == Code::excel)
    {
        out << "HOW TO USE IT\n"
            << "Put your buyer's computer ID in cell A2 (with or without its dashes), and this formula in cell B2: B2 shows the serial.\n"
            << "For more buyers, fill it down: an ID in A3 gives its serial in B3, and so on. An ID that isn't 9 digits gives an empty cell.\n"
            << sampleId << " in A2 gives " << example << ".\n\n"
            << codeFor (Code::excel, false) << "\n";
    }
    else if (kind == Code::php)
    {
        auto function = code.toLowerCase() + "_serial_for";

        if (juce::CharacterFunctions::isDigit (function[0]))
            function = "p" + function;

        out << "// How to use it (64-bit PHP 7 or newer, as every web host has):\n"
            << "//     require '" << code.toLowerCase() << "-serial.php';\n"
            << "//     $serial = " << function << " ($_POST['computer_id']);   // '' when it isn't a computer ID\n"
            << "//     echo htmlspecialchars ($serial);                         // or email it to your buyer\n"
            << "// " << function << " ('" << sampleId << "') gives '" << example << "'.\n\n"
            << "function " << function << " ($computerId)\n"
            << "{\n"
            << "    $a = " << a << ";\n"
            << "    $b = " << b << ";\n"
            << "    $m = " << m << ";\n\n"
            << "    // The ID's 9 digits, without its dashes and spaces.\n"
            << "    $id = preg_replace ('/[\\s-]+/', '', (string) $computerId);\n\n"
            << "    if (PHP_INT_SIZE < 8 || ! preg_match ('/^[0-9]{9}$/D', $id))\n"
            << "        return '';\n\n"
            << "    // serial = (ID x A + B) mod M, written with 10 digits (zeros in front).\n"
            << "    return str_pad ((string) (((int) $id * $a + $b) % $m), 10, '0', STR_PAD_LEFT);\n"
            << "}\n";
    }
    else
    {
        const auto function = "serialFor" + code;
        const auto stem = code.toLowerCase() + "-serial";

        out << "// How to use it:\n"
            << "//     Node, on your server:   const { " << function << " } = require (\"./" << stem << ".js\");\n"
            << "//                             const serial = " << function << " (computerIdFromYourOrderForm);\n"
            << "//     A browser, for trying:  <script src=\"" << stem << ".js\"></script>, then " << function << " (\"...\")\n"
            << "// " << function << " (\"" << sampleId << "\") gives \"" << example << "\" (\"\" when it isn't a computer ID).\n\n"
            << "function " << function << " (computerId)\n"
            << "{\n"
            << "    const A = " << a << "n;\n"
            << "    const B = " << b << "n;\n"
            << "    const M = " << m << "n;\n\n"
            << "    // The ID's 9 digits, without its dashes and spaces.\n"
            << "    const id = String (computerId).replace (/[\\s-]+/g, \"\");\n\n"
            << "    if (! /^[0-9]{9}$/.test (id))\n"
            << "        return \"\";\n\n"
            << "    // serial = (ID x A + B) mod M, written with 10 digits (zeros in front). BigInt keeps it exact.\n"
            << "    return ((BigInt (id) * A + B) % M).toString().padStart (10, \"0\");\n"
            << "}\n\n"
            << "if (typeof module !== \"undefined\" && module.exports)\n"
            << "    module.exports = { " << function << " };\n";
    }

    return out;
}

//==============================================================================
void ProtectionPage::copyCode (Code kind)
{
    juce::SystemClipboard::copyTextToClipboard (codeFor (kind, false));
    form.codeNote.setText (kind == Code::php ? "Copied the PHP code."
                         : kind == Code::excel ? "Copied the Excel formula: paste it into cell B2."
                                               : "Copied the JavaScript code.",
                           juce::dontSendNotification);
    form.codeNote.setColour (juce::Label::textColourId, goodColour);
}

void ProtectionPage::saveCode (Code kind)
{
    if (! protection.isValid())
        return;

    const juce::String extension (kind == Code::php ? "php" : kind == Code::excel ? "txt" : "js");
    const auto stem = codeName (shownName()).toLowerCase() + (kind == Code::excel ? "-serial-excel" : "-serial");
    const auto suggested = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (stem + "." + extension);
    const auto title = kind == Code::php ? "Save the PHP code" : kind == Code::excel ? "Save the Excel formula" : "Save the JavaScript code";

    chooser = std::make_unique<juce::FileChooser> (title, suggested, "*." + extension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis = juce::Component::SafePointer<ProtectionPage> (this), kind, extension] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || fc.getResult() == juce::File())
                                  return;

                              const auto target = fc.getResult().withFileExtension (extension);
                              const bool saved = target.replaceWithText (safeThis->codeFor (kind, true));
                              auto& note = safeThis->form.codeNote;
                              note.setText (saved ? "Saved " + target.getFullPathName() + ". Keep it private."
                                                  : "Couldn't save " + target.getFullPathName() + ".",
                                            juce::dontSendNotification);
                              note.setColour (juce::Label::textColourId, saved ? goodColour : problemColour);
                          });
}

void ProtectionPage::checkSerial()
{
    const auto id = stella::licence::clean (form.idEditor.getText().toRawUTF8());
    const auto serial = stella::licence::clean (form.serialEditor.getText().toRawUTF8());
    juce::String text;
    bool good = false;

    if (id.size() != (size_t) stella::licence::idLength)
        text = "A computer ID has 9 digits (dashes and spaces don't matter).";
    else if (serial.size() != (size_t) stella::licence::serialLength)
        text = "A serial has 10 digits (dashes and spaces don't matter).";
    else if (protection.formula.fits (id.c_str(), serial.c_str()))
    {
        text = juce::String::fromUTF8 ("\xe2\x9c\x93  It fits: this serial registers ") + shownName() + " on the computer with that ID.";
        good = true;
    }
    else
        text = juce::String::fromUTF8 ("\xe2\x9c\x97  It doesn't fit that ID. Check your site uses this product's three numbers (A, B and M) "
                                       "as they are above, and that the whole ID was copied.");

    form.checkResult.setText (text, juce::dontSendNotification);
    form.checkResult.setColour (juce::Label::textColourId, good ? goodColour : problemColour);
    resized();
}

void ProtectionPage::saveSoni()
{
    if (! protection.isValid())
        return;

    chooser = std::make_unique<juce::FileChooser> ("Choose the folder license.soni goes into (the plugin's own folder)",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safeThis = juce::Component::SafePointer<ProtectionPage> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || fc.getResult() == juce::File())
                                  return;

                              auto& page = *safeThis;
                              const auto target = fc.getResult().getChildFile (stella::licence::fileName);
                              stella::licence::Soni there;
                              juce::String text;
                              bool saved = false;

                              // Another product's license.soni stays: each plugin has a folder of its own.
                              if (Licences::readSoni (target, there) && juce::String (there.product) != page.protection.product)
                                  text = "That folder has another product's license.soni (" + juce::String (juce::CharPointer_UTF8 (there.name.c_str()))
                                       + "): each plugin needs a folder of its own.";
                              else if (Licences::writeSoni (target, Licences::soniFor (page.protection, page.subject.name, page.subject.vendor)))
                              {
                                  text = "Saved " + target.getFullPathName();
                                  saved = true;
                              }
                              else
                                  text = "Couldn't save " + target.getFullPathName() + ".";

                              page.form.soniStatus.setText (text, juce::dontSendNotification);
                              page.form.soniStatus.setColour (juce::Label::textColourId, saved ? goodColour : problemColour);
                          });
}

void ProtectionPage::makeNewFormula()
{
    if (! protection.isValid())
        return;

    confirmWindow = std::make_unique<juce::AlertWindow> ("Make a new formula?",
        "Every serial you've already given for " + shownName() + " stops working with the new formula, and every copy already out "
        "there keeps the old one (only copies with the new license.soni use the new one).\n\n"
        "Do this only before you release it, or if your formula got out.",
        juce::MessageBoxIconType::WarningIcon, this);
    confirmWindow->addButton ("Make a new formula", 1);
    confirmWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    confirmWindow->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<ProtectionPage> (this)] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& page = *safeThis;

            if (result == 1 && page.subject.key.isNotEmpty())
            {
                page.flush();
                page.protection.formula = Licences::newFormula();

                if (page.keep() && page.onChanged != nullptr)
                    page.onChanged();

                page.form.checkResult.setText ({}, juce::dontSendNotification);
                page.form.soniStatus.setText ("A new formula is made: update your site's code, and ship the new license.soni.",
                                              juce::dontSendNotification);
                page.form.soniStatus.setColour (juce::Label::textColourId, Theme::muted);
                page.updateState();
            }

            juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->confirmWindow = nullptr; });
        }), false);
}

//==============================================================================
void ProtectionPage::updateState()
{
    const bool open = subject.key.isNotEmpty();
    const bool midi = isMidi();
    const auto name = shownName();

    viewport.setVisible (open);

    if (! open)
    {
        repaint();
        return;
    }

    form.productLabel.setText (protection.product.isNotEmpty() ? "Product id: " + protection.product
                                                               : juce::String ("Its product id is made when you turn protection on"),
                               juce::dontSendNotification);

    if (protection.on)
        form.stateNote.setText ("Protected. Until a buyer registers " + name + " on their computer with a serial from you, it plays its demo. "
                                "Registered, it plays fully on that computer, online or not. You sell it on your own site: "
                                "Sonify and Fanan take no part and no share.");
    else
        form.stateNote.setText ("Not protected: " + name + " plays fully on every computer. Turn protection on to sell it with serials "
                                "from your own site.");

    form.playLabel.setText (midi ? "Notes pass for" : "It plays sound for", juce::dontSendNotification);
    form.playUnit.setText ("seconds,", juce::dontSendNotification);
    form.silentLabel.setText (midi ? "then they're held back for" : "then it's silent for", juce::dontSendNotification);
    form.silentUnit.setText ("seconds, again and again.", juce::dontSendNotification);
    form.demoNote.setText (midi ? "While notes are held back, the notes it was playing end (nothing hangs). Registered, it plays fully."
                                : "Each change fades over 10 milliseconds, so it never clicks. Registered, it plays fully.",
                           13.0f, Theme::muted);

    if (protection.isValid())
    {
        form.explanation.setText (explanationLines().joinIntoString ("\n"));
        form.jsEditor.setText (codeFor (Code::js, false), false);
        form.phpEditor.setText (codeFor (Code::php, false), false);
        form.excelEditor.setText (codeFor (Code::excel, false), false);
        form.checkNote.setText ("Test your site: paste a computer ID and the serial your site made for it. (Sonify never makes serials: "
                                "only your site does.)", 13.5f);
        form.soniNote.setText ("The file with " + name + "'s formula (scrambled), as it ships with it. "
                               + juce::String (midi ? "Sonify puts it inside the MIDI FX's file for you."
                                                    : "Export Plugin and Add to Sonify put it in place for you; here's a copy for anywhere else.")
                               , 13.5f);
        form.newFormulaNote.setText ("## Make a new formula\nOnly before you release " + name + ", or if your formula got out: every serial "
                                     "already given stops working.", 13.5f);
    }

    form.layOut (juce::jmax (320, viewport.getMaximumVisibleWidth()));
    repaint();
}

//==============================================================================
void ProtectionPage::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    if (subject.key.isNotEmpty())
        return;

    auto area = getLocalBounds().withSizeKeepingCentre (juce::jmin (600, getWidth() - 40), 120);

    g.setColour (Theme::text);
    g.setFont (Theme::font (20.0f, true));
    g.drawText ("Nothing to protect yet", area.removeFromTop (32), juce::Justification::centred, false);

    g.setColour (Theme::muted);
    g.setFont (Theme::font (14.5f));
    g.drawFittedText ("Open a plugin or a MIDI FX (or ask Fanan AI for one in Build with AI). Here you protect it with a serial "
                      "formula of its own, and sell it with serials from your own site.",
                      area, juce::Justification::centredTop, 4, 1.0f);
}

void ProtectionPage::resized()
{
    viewport.setBounds (getLocalBounds());
    form.layOut (juce::jmax (320, viewport.getMaximumVisibleWidth()));
}
