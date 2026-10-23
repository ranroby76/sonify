// D:\workspace\Fanan_web_hub\src\Licences.cpp

#include <juce_gui_basics/juce_gui_basics.h>

// The computer's part of stella_licence.h (its ID, the key files): after JUCE, so windows.h comes
// after it, as in every exported plugin. Sonify and the exports then agree exactly.
#define STELLA_LICENCE_NATIVE 1
#include "Licences.h"

#include "PluginLibrary.h"   // the protected blob libraries in use: one Registration window for them too
#include "StellaLookAndFeel.h"

#include <map>
#include <random>
#include <vector>

namespace
{
    constexpr size_t maxSoniSize = 64 * 1024;

    const juce::Colour problemColour { 0xffff8a80 };
    const juce::Colour demoColour { 0xffffc940 };

    //==========================================================================
    struct State
    {
        juce::CriticalSection lock;
        std::map<juce::String, std::shared_ptr<std::atomic<bool>>> flags;   // by product
        std::map<juce::String, bool> registered;                           // what was last found, by product
    };

    State& state()
    {
        static State s;
        return s;
    }

    /** What was found for a product: kept, and its flag follows. */
    void setRegistered (const juce::String& product, bool isRegistered)
    {
        auto& s = state();
        const juce::ScopedLock sl (s.lock);
        s.registered[product] = isRegistered;

        if (const auto found = s.flags.find (product); found != s.flags.end() && found->second != nullptr)
            found->second->store (isRegistered);
    }

    juce::String fromStd (const std::string& text)
    {
        return juce::String (juce::CharPointer_UTF8 (text.c_str()));
    }

    /** Good randomness: the system's (std::random_device), mixed with JUCE's and the clock. */
    void randomBytes (std::uint8_t* out, size_t size)
    {
        std::random_device device;
        juce::Random random;
        random.setSeedRandomly();

        for (size_t i = 0; i < size; ++i)
        {
            const auto ticks = (std::uint64_t) juce::Time::getHighResolutionTicks();
            out[i] = (std::uint8_t) ((std::uint32_t) device() ^ (std::uint32_t) random.nextInt() ^ (std::uint32_t) (ticks >> (i % 8)));
        }
    }

    /** One of the formula's numbers as protection.json keeps it (a number, or its digits as text). */
    std::uint64_t toNumber (const juce::var& value)
    {
        const auto n = value.isString() ? value.toString().trim().getLargeIntValue() : (juce::int64) value;
        return n < 0 ? 0u : (std::uint64_t) n;
    }

    /** A serial as typed or pasted, with only its digits left: grouped 12345-67890 when it's
        all of a serial, as it was otherwise (16 digits at most). */
    juce::String serialShown (const juce::String& text)
    {
        const auto digits = stella::licence::clean (text.substring (0, 200).toRawUTF8()).substr (0, 16);

        if (digits.size() == (size_t) stella::licence::serialLength)
            return fromStd (stella::licence::grouped (digits));

        return fromStd (digits);
    }

    /** "10", "2.5": seconds as people read them. */
    juce::String secondsText (double seconds)
    {
        const auto tenths = juce::roundToInt (seconds * 10.0);
        return tenths % 10 == 0 ? juce::String (tenths / 10) : juce::String (tenths / 10.0, 1);
    }

    double secondsFrom (const juce::var& value, double fallback)
    {
        const auto seconds = (double) value;
        return seconds > 0.0 && seconds <= 3600.0 ? seconds : fallback;
    }

    //==========================================================================
    /** The Registration window's content. */
    class RegistrationContent final : public juce::Component
    {
    public:
        RegistrationContent (const stella::licence::Soni& soniToRegister, std::function<void()> afterRegistering, bool isMidiFx)
            : soni (soniToRegister), after (std::move (afterRegistering)), midi (isMidiFx)
        {
            const auto name = shownName();
            const auto vendor = fromStd (soni.vendor).trim();

            title.setText (name, juce::dontSendNotification);
            title.setFont (Theme::font (21.0f, true));
            title.setColour (juce::Label::textColourId, Theme::text);
            addAndMakeVisible (title);

            byLine.setText (vendor.isNotEmpty() ? "by " + vendor : juce::String(), juce::dontSendNotification);
            byLine.setFont (Theme::font (13.5f));
            byLine.setColour (juce::Label::textColourId, Theme::muted);
            addAndMakeVisible (byLine);

            status.setFont (Theme::font (13.5f, true));
            status.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (status);

            const auto seller = vendor.isNotEmpty() ? vendor : juce::String ("its seller");
            juce::String stepsText = "To register it: copy your computer ID and give it to " + seller + " where you bought " + name
                                   + " (their site asks for it). You get a serial back: type or paste it below and press Register. "
                                     "It's kept on this computer, so you only do this once here.";

            // Sample libraries in their demo: their serials go in here too.
            juce::StringArray libraries;

            for (const auto& library : PluginLibrary::licencesInUse())
                if (! library.registered && library.product != soni.product)
                    libraries.add (fromStd (library.name).trim().isNotEmpty() ? fromStd (library.name).trim() : fromStd (library.product));

            if (! libraries.isEmpty())
                stepsText << " A sample library's serial goes in here too (in its demo now: " << libraries.joinIntoString (", ") << ").";

            steps.setText (stepsText, juce::dontSendNotification);
            steps.setFont (Theme::font (13.5f));
            steps.setColour (juce::Label::textColourId, Theme::text.withAlpha (0.85f));
            steps.setJustificationType (juce::Justification::topLeft);
            steps.setMinimumHorizontalScale (1.0f);
            addAndMakeVisible (steps);

            for (auto* label : { &idHeading, &serialHeading })
            {
                label->setFont (Theme::font (12.5f, true));
                label->setColour (juce::Label::textColourId, Theme::muted);
                addAndMakeVisible (label);
            }

            idHeading.setText ("YOUR COMPUTER ID", juce::dontSendNotification);
            serialHeading.setText ("YOUR SERIAL", juce::dontSendNotification);

            const auto id = Licences::computerId();
            idText.setText (id.isNotEmpty() ? id : juce::String ("Can't be read on this computer"), juce::dontSendNotification);
            idText.setFont (id.isNotEmpty() ? juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::bold))
                                            : Theme::font (14.0f));
            idText.setColour (juce::Label::textColourId, id.isNotEmpty() ? Theme::text : problemColour);
            idText.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (idText);

            copyButton.setTooltip ("Copies your computer ID, to paste on the seller's site");
            copyButton.setEnabled (id.isNotEmpty());
            copyButton.onClick = [this, id]
            {
                juce::SystemClipboard::copyTextToClipboard (id);
                say ("Your computer ID is copied. Paste it on the seller's site.", false, false);
            };
            addAndMakeVisible (copyButton);

            serialEditor.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::plain)));
            serialEditor.setInputRestrictions (16, "0123456789-");   // digits (and the dash): anything else typed or pasted is left out
            serialEditor.setIndents (10, 8);
            serialEditor.setJustification (juce::Justification::centredLeft);
            serialEditor.setTextToShowWhenEmpty ("00000-00000", Theme::muted);
            serialEditor.onReturnKey = [this] { registerNow(); };
            serialEditor.onTextChange = [this] { message.setText ({}, juce::dontSendNotification); };
            addAndMakeVisible (serialEditor);

            pasteButton.setTooltip ("Pastes the serial you copied");
            pasteButton.onClick = [this]
            {
                serialEditor.setText (serialShown (juce::SystemClipboard::getTextFromClipboard()), true);
                serialEditor.grabKeyboardFocus();
            };
            addAndMakeVisible (pasteButton);

            registerButton.setColour (juce::TextButton::buttonColourId, Theme::accent);
            registerButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            registerButton.onClick = [this] { registerNow(); };
            addAndMakeVisible (registerButton);

            closeButton.onClick = [this]
            {
                if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
                    window->closeButtonPressed();
            };
            addAndMakeVisible (closeButton);

            message.setFont (Theme::font (13.5f));
            message.setJustificationType (juce::Justification::topLeft);
            message.setMinimumHorizontalScale (1.0f);
            addAndMakeVisible (message);

            showStatus (soni.isValid() && Licences::isRegistered (soni));
            setSize (540, 440);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Theme::panel);

            g.setColour (Theme::inset);
            g.fillRoundedRectangle (idText.getBounds().toFloat(), 8.0f);
            g.setColour (Theme::outline);
            g.drawRoundedRectangle (idText.getBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

            const auto pill = status.getBounds().toFloat().expanded (0.0f, 1.0f);
            g.setColour (status.findColour (juce::Label::textColourId).withAlpha (0.12f));
            g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (24, 20);

            title.setBounds (area.removeFromTop (30));
            byLine.setBounds (area.removeFromTop (byLine.getText().isNotEmpty() ? 20 : 0));
            area.removeFromTop (10);
            status.setBounds (area.removeFromTop (28));
            area.removeFromTop (12);
            steps.setBounds (area.removeFromTop (76));
            area.removeFromTop (12);

            idHeading.setBounds (area.removeFromTop (18));
            area.removeFromTop (4);
            auto row = area.removeFromTop (40);
            copyButton.setBounds (row.removeFromRight (104).reduced (0, 4));
            row.removeFromRight (10);
            idText.setBounds (row);
            area.removeFromTop (14);

            serialHeading.setBounds (area.removeFromTop (18));
            area.removeFromTop (4);
            row = area.removeFromTop (38);
            pasteButton.setBounds (row.removeFromRight (104).reduced (0, 3));
            row.removeFromRight (10);
            serialEditor.setBounds (row);
            area.removeFromTop (16);

            row = area.removeFromTop (36);
            closeButton.setBounds (row.removeFromRight (100));
            row.removeFromRight (10);
            registerButton.setBounds (row.removeFromRight (140));
            area.removeFromTop (10);
            message.setBounds (area);
        }

    private:
        juce::String shownName() const
        {
            const auto name = fromStd (soni.name).trim();
            return name.isNotEmpty() ? name : juce::String ("This plugin");
        }

        void showStatus (bool registered)
        {
            juce::String text;

            if (! soni.isValid())
                text = "Its license.soni file is missing: it plays its demo";
            else if (registered)
                text = juce::String::fromUTF8 ("\xe2\x9c\x93  Registered on this computer: it plays fully");
            else if (midi)
                text = "Demo: it lets notes through for " + secondsText (soni.passSeconds) + " seconds, then holds them back for "
                     + secondsText (soni.holdSeconds) + " seconds";
            else
                text = "Demo: " + secondsText (soni.playSeconds) + " seconds of sound, then " + secondsText (soni.silentSeconds)
                     + " seconds of silence";

            status.setText ("  " + text, juce::dontSendNotification);
            status.setColour (juce::Label::textColourId, ! soni.isValid() ? problemColour : registered ? Theme::accent : demoColour);
            repaint();
        }

        void say (const juce::String& text, bool problem, bool good)
        {
            message.setText (text, juce::dontSendNotification);
            message.setColour (juce::Label::textColourId, problem ? problemColour : good ? Theme::accent : Theme::muted);
        }

        void registerNow()
        {
            const auto typed = serialEditor.getText().trim();

            if (typed.isEmpty())
            {
                say ("Type or paste your serial first.", true, false);
                serialEditor.grabKeyboardFocus();
                return;
            }

            juce::String why;

            if (soni.isValid() && Licences::isRegistered (soni))
                why = shownName() + " is registered on this computer already.";
            else
                why = Licences::registerSerial (soni, typed);

            if (why.isEmpty())
            {
                PluginLibrary::recheckLicences();   // a library of the same product's demo ends at once too
                showStatus (true);
                say ("Thank you! " + shownName() + " is registered on this computer and plays fully from now on.", false, true);
                done();
                return;
            }

            // Not this product's serial: maybe a sample library's in use (its demo ends at once).
            bool librariesWaiting = false;

            for (const auto& library : PluginLibrary::licencesInUse())
                librariesWaiting = librariesWaiting || ! library.registered;

            if (librariesWaiting && PluginLibrary::registerSerial (typed).isEmpty())
            {
                say ("Thank you! The sample library that serial is for is registered on this computer and plays fully from now on.", false, true);
                done();
                return;
            }

            say (why, true, false);
        }

        void done()
        {
            if (after != nullptr)
                juce::MessageManager::callAsync ([call = after] { call(); });
        }

        stella::licence::Soni soni;
        std::function<void()> after;
        bool midi = false;

        juce::Label title, byLine, status, steps, idHeading, serialHeading, idText, message;
        juce::TextButton copyButton { "Copy ID" }, pasteButton { "Paste" }, registerButton { "Register" }, closeButton { "Close" };
        juce::TextEditor serialEditor;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegistrationContent)
    };

    /** The window: one at a time, gone when it's closed (or when Sonify quits). */
    class RegistrationWindow final : public juce::DocumentWindow,
                                     private juce::DeletedAtShutdown
    {
    public:
        RegistrationWindow (const stella::licence::Soni& soni, std::function<void()> after, bool midi)
            : DocumentWindow ("Registration", Theme::panel, DocumentWindow::closeButton),
              content (soni, std::move (after), midi)
        {
            setUsingNativeTitleBar (true);
            setContentNonOwned (&content, true);
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
        }

        ~RegistrationWindow() override
        {
            clearContentComponent();   // the content is a member: let go of it before it goes
        }

        void closeButtonPressed() override
        {
            setVisible (false);
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RegistrationWindow> (this)]
            {
                delete safe.getComponent();
            });
        }

    private:
        RegistrationContent content;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegistrationWindow)
    };

    juce::Component::SafePointer<RegistrationWindow>& openWindow()
    {
        static juce::Component::SafePointer<RegistrationWindow> window;
        return window;
    }
}

//==============================================================================
juce::String Licences::computerId()
{
    // Read once a run: it never changes while Sonify runs.
    static const juce::String id = []
    {
        const auto raw = stella::licence::native::computerId();
        return raw.empty() ? juce::String() : fromStd (stella::licence::grouped (raw));
    }();

    return id;
}

stella::licence::Formula Licences::newFormula()
{
    std::uint8_t random[64];

    for (;;)
    {
        randomBytes (random, sizeof (random));

        if (const auto formula = stella::licence::Formula::make (random, sizeof (random)); formula.isValid())
            return formula;
    }
}

juce::String Licences::newProductId()
{
    std::uint8_t random[16];
    randomBytes (random, sizeof (random));

    juce::String id ("sp-");

    static const char* const characters = "0123456789abcdefghjkmnpqrstvwxyz";   // 32: no i, l, o or u

    for (auto byte : random)
        id << juce::String::charToString ((juce::juce_wchar) characters[byte % 32]);

    return id;
}

//==============================================================================
bool Licences::readSoni (const void* data, size_t size, stella::licence::Soni& soni)
{
    return data != nullptr && size > 0 && size <= maxSoniSize && stella::licence::read (static_cast<const std::uint8_t*> (data), size, soni);
}

bool Licences::readSoni (const juce::File& file, stella::licence::Soni& soni)
{
    if (! file.existsAsFile() || file.getSize() > (juce::int64) maxSoniSize)
        return false;

    juce::MemoryBlock data;
    return file.loadFileAsData (data) && readSoni (data.getData(), data.getSize(), soni);
}

juce::MemoryBlock Licences::soniData (const stella::licence::Soni& soni)
{
    std::uint8_t noise[256];
    randomBytes (noise, sizeof (noise));

    const auto bytes = stella::licence::write (soni, noise, sizeof (noise));
    return juce::MemoryBlock (bytes.data(), bytes.size());
}

bool Licences::writeSoni (const juce::File& file, const stella::licence::Soni& soni)
{
    if (! soni.isValid())
        return false;

    const auto data = soniData (soni);
    return file.getParentDirectory().createDirectory() && file.replaceWithData (data.getData(), data.getSize());
}

//==============================================================================
bool Licences::isRegistered (const stella::licence::Soni& soni)
{
    if (! soni.isValid())
        return false;

    const auto registered = stella::licence::native::isRegistered (soni);
    setRegistered (fromStd (soni.product), registered);
    return registered;
}

juce::String Licences::registerSerial (const stella::licence::Soni& soni, const juce::String& serial)
{
    const auto why = stella::licence::native::registerSerial (soni, serial.toStdString());

    if (! why.empty())
        return fromStd (why);

    setRegistered (fromStd (soni.product), true);
    return {};
}

std::shared_ptr<std::atomic<bool>> Licences::flagFor (const juce::String& productId)
{
    auto& s = state();
    const juce::ScopedLock sl (s.lock);
    const auto id = productId.trim();
    auto& flag = s.flags[id];

    if (flag == nullptr)
    {
        const auto known = s.registered.find (id);
        flag = std::make_shared<std::atomic<bool>> (known != s.registered.end() && known->second);
    }

    return flag;
}

std::uint32_t Licences::runtimeToken (const juce::String& productId)
{
    return stella::licence::tokenFor (productId.trim().toRawUTF8());
}

void Licences::showRegistration (const stella::licence::Soni& soni, std::function<void()> afterRegistering, bool isMidiFx)
{
    // One at a time: another product's (or the same one's) window takes the place of one open.
    auto& window = openWindow();

    if (window != nullptr)
        delete window.getComponent();

    window = new RegistrationWindow (soni, std::move (afterRegistering), isMidiFx);
    window->setVisible (true);
    window->toFront (true);
}

//==============================================================================
bool Licences::ProjectProtection::isValid() const
{
    return on && product.isNotEmpty() && formula.isValid();
}

bool Licences::hasProtection (const juce::File& projectFolder)
{
    return projectFolder != juce::File() && projectFolder.getChildFile (protectionFileName).existsAsFile();
}

Licences::ProjectProtection Licences::loadProtection (const juce::File& projectFolder)
{
    ProjectProtection p;

    if (! hasProtection (projectFolder))
        return p;

    const auto json = juce::JSON::parse (projectFolder.getChildFile (protectionFileName));

    if (! json.isObject())
        return p;

    p.on = (bool) json.getProperty ("on", false);
    p.product = json.getProperty ("product", {}).toString().trim().substring (0, 80);

    // The formula's three numbers (an older formula, of another shape, isn't valid: a new one is made).
    const auto formula = json.getProperty ("formula", {});
    p.formula.a = toNumber (formula.getProperty ("a", {}));
    p.formula.b = toNumber (formula.getProperty ("b", {}));
    p.formula.m = toNumber (formula.getProperty ("m", {}));

    p.playSeconds = secondsFrom (json.getProperty ("playSeconds", {}), stella::licence::playSeconds);
    p.silentSeconds = secondsFrom (json.getProperty ("silentSeconds", {}), stella::licence::silentSeconds);
    p.passSeconds = secondsFrom (json.getProperty ("passSeconds", {}), stella::licence::midiPassSeconds);
    p.holdSeconds = secondsFrom (json.getProperty ("holdSeconds", {}), stella::licence::midiHoldSeconds);
    return p;
}

bool Licences::saveProtection (const juce::File& projectFolder, const ProjectProtection& protection)
{
    if (projectFolder == juce::File() || ! projectFolder.createDirectory())
        return false;

    // serial = (ID x a + b) mod m: each number below 10^10, so a JSON number keeps it exactly.
    juce::DynamicObject::Ptr formula (new juce::DynamicObject());
    formula->setProperty ("a", (juce::int64) protection.formula.a);
    formula->setProperty ("b", (juce::int64) protection.formula.b);
    formula->setProperty ("m", (juce::int64) protection.formula.m);

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("on", protection.on);
    item->setProperty ("product", protection.product);
    item->setProperty ("formula", juce::var (formula.get()));
    item->setProperty ("playSeconds", protection.playSeconds);
    item->setProperty ("silentSeconds", protection.silentSeconds);
    item->setProperty ("passSeconds", protection.passSeconds);
    item->setProperty ("holdSeconds", protection.holdSeconds);

    return projectFolder.getChildFile (protectionFileName).replaceWithText (juce::JSON::toString (juce::var (item.get())));
}

stella::licence::Soni Licences::soniFor (const ProjectProtection& protection, const juce::String& name, const juce::String& vendor)
{
    stella::licence::Soni soni;
    soni.product = protection.product.trim().toStdString();
    soni.name = name.trim().toStdString();
    soni.vendor = vendor.trim().toStdString();
    soni.formula = protection.formula;
    soni.playSeconds = (float) protection.playSeconds;
    soni.silentSeconds = (float) protection.silentSeconds;
    soni.passSeconds = (float) protection.passSeconds;
    soni.holdSeconds = (float) protection.holdSeconds;
    return soni;
}
