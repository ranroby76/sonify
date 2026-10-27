// D:\workspace\Fanan_web_hub\src\TomahawkScreen.cpp

#include "TomahawkScreen.h"
#include "PluginHeader.h"
#include "tomahawk/TomahawkVoice.h"
#include "tomahawk/GrexSoundsParts.h"
#include "tomahawk/SynthesisPanel.h"
#include "tomahawk/ModulationPanel.h"
#include "tomahawk/EffectsPanel.h"

namespace
{
    using Tomahawk::GrexSounds;
    using Tomahawk::Library;
    using Tomahawk::SlotParams;
    namespace Betel = Tomahawk::Betel;
    namespace Style = Tomahawk::InstrEditStyle;

    const juce::Colour accent (Betel::Pal::kAccent);
    const juce::Colour pageYellow (Betel::Pal::kNav);
    const juce::Colour savedGreen (0xFF57D46A);

    // Grex's instrument editor is drawn at this size (a little taller than Grex's, for the
    // header) and shown at 80 % of it.
    constexpr int designW = 1280, designH = 840;
    constexpr float shownScale = 0.8f;

    juce::Image namedImage (const char* name)
    {
        int size = 0;

        if (const char* data = TomahawkImages::getNamedResource (name, size); data != nullptr && size > 0)
            return juce::ImageFileFormat::loadFrom (data, static_cast<size_t> (size));

        return {};
    }

    /** A family's icon (assets/instruments icons, 600x600) by the name of the sound's category
        folder, both packs' names; none for a family without one. */
    juce::Image familyIcon (const juce::String& category)
    {
        static const std::map<juce::String, const char*> icons
        {
            { "accordion", "accordion_png" },
            { "brass",     "brass_png" },
            { "choir",     "choir_png" },
            { "guitar",    "guitar_png" },
            { "organ",     "organ_png" },
            { "pad",       "pads_png" },
            { "pads",      "pads_png" },
            { "piano",     "piano_png" },
            { "string",    "strings_png" },
            { "strings",   "strings_png" },
            { "synth",     "synth_png" },
            { "wind",      "wind_png" },
            { "woodwind",  "wind_png" },
            { "keyboard",  "piano_png" }       // ORIENTAL's keyboard
        };

        const auto it = icons.find (category.trim().toLowerCase().replaceCharacter (' ', '_'));
        return it != icons.end() ? namedImage (it->second) : juce::Image();
    }

    /** A sound's picture: its family's icon, else its pack's (SP_CLASSIC, else Grex's
        SP_WORLD; SP_ORIENTAL). */
    juce::Image pictureFor (const Library::Sound* sound)
    {
        if (sound == nullptr)
            return {};

        if (auto icon = familyIcon (sound->category); icon.isValid())
            return icon;

        if (sound->pack == Tomahawk::packOriental)
            return namedImage ("SP_ORIENTAL_png");

        auto image = namedImage ("SP_CLASSIC_png");
        return image.isValid() ? image : namedImage ("SP_WORLD_png");
    }

    //==============================================================================
    /** The SOUND ENGINE tab: Grex's SOUNDS tab for one slot (PACK, CATEGORY and INSTRUMENT,
        eight to a page, a button per page, the pack's picture), the library's state, and Grex's
        two global settings under the columns. */
    class SoundsPage final : public juce::Component
    {
    public:
        std::function<void (int flag)> onPicked;
        std::function<void (float lowVelocity, float bendRange)> onGlobalsMoved;

        SoundsPage()
        {
            colPack.setTitle ("PACK");
            colPack.setItems ({ Library::packName (Tomahawk::packClassic), Library::packName (Tomahawk::packOriental) });
            colPack.setMaxRowH (30);
            colPack.setDrawRule (true);
            colPack.onRowClicked = [this] (int r) { setPack (r); };
            addAndMakeVisible (colPack);

            colCategory.setTitle ("CATEGORY");
            colCategory.setMaxRowH (26);
            colCategory.setDrawRule (true);
            colCategory.onRowClicked = [this] (int r) { category = r; page = 0; refreshColumns(); };
            addAndMakeVisible (colCategory);

            colInstr.setTitle ("INSTRUMENT");
            colInstr.setMaxRowH (30);
            colInstr.setDrawRule (false);
            colInstr.onRowClicked = [this] (int r) { pick (r); };
            addAndMakeVisible (colInstr);

            // The lists' names half as big again as Grex's (their rows as they are).
            for (auto* c : { &colPack, &colCategory, &colInstr })
                c->setTextScale (1.5f);

            pageCaption.setText ("PAGE", juce::dontSendNotification);
            pageCaption.setColour (juce::Label::textColourId, pageYellow);
            pageCaption.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            addChildComponent (pageCaption);

            addAndMakeVisible (picture);

            soundName.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
            soundName.setColour (juce::Label::textColourId, juce::Colours::white);
            soundName.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (soundName);

            soundInfo.setFont (juce::Font (juce::FontOptions (14.0f)));
            soundInfo.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
            soundInfo.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (soundInfo);

            status.setFont (juce::Font (juce::FontOptions (13.0f)));
            status.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
            status.setJustificationType (juce::Justification::centred);
            status.setMinimumHorizontalScale (0.6f);
            addAndMakeVisible (status);

            // Grex's global settings, horizontal as its OCTAVE slider.
            lowVelocity.setHorizontal (true);
            lowVelocity.setStep (1.0f);
            bendRange.setHorizontal (true);
            bendRange.setStep (1.0f);
            bendRange.displayFn = [] (float v) { return juce::String (juce::roundToInt (v)) + " st"; };

            for (auto* s : { &lowVelocity, &bendRange })
            {
                s->onChange = [this] (float) { if (onGlobalsMoved) onGlobalsMoved (lowVelocity.getValue(), bendRange.getValue()); };
                addAndMakeVisible (*s);
            }
        }

        /** What's playing and the library as it is now. */
        void show (int newFlag, const juce::String& name, const juce::String& statusText)
        {
            const auto library = Library::get().contents();
            const auto* sound = library != nullptr ? library->find (newFlag) : nullptr;
            const bool installed = library != nullptr && library->isInstalled();

            if (newFlag != flag)
            {
                flag = newFlag;
                followSound();
            }

            soundName.setText (newFlag >= 0 ? (name.isNotEmpty() ? name : "Sound " + juce::String (newFlag))
                                            : juce::String ("Pick a sound"), juce::dontSendNotification);
            soundInfo.setText (sound != nullptr ? juce::String (Library::packName (sound->pack)) + "   " + sound->category + "   " + juce::String (newFlag)
                                                : juce::String(), juce::dontSendNotification);

            const auto family = sound != nullptr ? juce::String (sound->pack) + "/" + sound->category : juce::String();

            if (newFlag != shownFlag || family != shownFamily)
            {
                shownFlag = newFlag;
                shownFamily = family;
                picture.set (pictureFor (sound));
            }

            picture.setVisible (installed && newFlag >= 0);
            status.setText (statusText, juce::dontSendNotification);
            refreshColumns();
            repaint();
        }

        /** Grex's two global settings, as the plugin holds them (automation moves them). */
        void showGlobals (float lowVelocityValue, float bendRangeValue)
        {
            if (! lowVelocity.isMouseButtonDown()) lowVelocity.setValue (lowVelocityValue);
            if (! bendRange.isMouseButtonDown())   bendRange.setValue (bendRangeValue);
        }

        void libraryChanged()
        {
            followSound();
            refreshColumns();
        }

        void paint (juce::Graphics& g) override
        {
            Style::paintComponentFrame (g, soundsFrame, "SOUND");
            Style::paintComponentFrame (g, globalFrame, "GLOBAL");

            if (! picture.isVisible())
            {
                const auto library = Library::get().contents();
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.setFont (juce::Font (juce::FontOptions (16.0f)));
                g.drawFittedText (library == nullptr || ! library->isInstalled()
                                      ? "The Tomahawk library isn't located.\n\nGET THE LIBRARY downloads it (one zip, about 12 GB). "
                                        "Unzip it anywhere, then LOCATE LIBRARY and choose the Tomahawk_Sampler folder."
                                      : "Pick a sound: a pack, a category, an instrument.",
                                  pictureArea.reduced (24), juce::Justification::centred, 8);
            }
        }

        void resized() override
        {
            auto area = getLocalBounds();
            auto left = area.removeFromLeft (juce::jmin (780, area.getWidth() * 62 / 100));
            area.removeFromLeft (16);

            globalFrame = left.removeFromBottom (128);
            left.removeFromBottom (10);
            soundsFrame = left;

            auto columns = Style::componentFrameContent (soundsFrame).reduced (6, 0);
            colPack.setBounds (columns.removeFromLeft (150));
            columns.removeFromLeft (10);
            colCategory.setBounds (columns.removeFromLeft (230));
            columns.removeFromLeft (10);

            auto pager = columns.removeFromBottom (32);
            colInstr.setBounds (columns);
            pageCaption.setBounds (pager.removeFromLeft (52));
            pagerArea = pager;
            layoutPageButtons();

            auto globals = Style::componentFrameContent (globalFrame).reduced (10, 0);
            const int half = (globals.getWidth() - 20) / 2;
            lowVelocity.setBounds (globals.removeFromLeft (half));
            globals.removeFromLeft (20);
            bendRange.setBounds (globals);

            status.setBounds (area.removeFromBottom (24));
            area.removeFromBottom (6);
            soundInfo.setBounds (area.removeFromBottom (22));
            soundName.setBounds (area.removeFromBottom (30));
            area.removeFromBottom (4);
            pictureArea = area;
            picture.setBounds (area);
        }

    private:
        static constexpr int perPage = 8;

        struct Row { int flag; juce::String name; };

        std::vector<Row> rows() const
        {
            std::vector<Row> list;
            const auto library = Library::get().contents();

            if (library == nullptr || ! juce::isPositiveAndBelow (category, library->categories[(size_t) pack].size()))
                return list;

            for (const auto* sound : library->soundsIn (pack, library->categories[(size_t) pack][category]))
                list.push_back ({ sound->flag, sound->name });

            return list;
        }

        int pageCount() const
        {
            return juce::jmax (1, (static_cast<int> (rows().size()) + perPage - 1) / perPage);
        }

        /** The columns show where the sound playing lives (Grex's syncTimbreToSlot). */
        void followSound()
        {
            const auto library = Library::get().contents();

            if (library == nullptr)
                return;

            if (const auto* sound = library->find (flag))
            {
                pack = sound->pack;
                category = juce::jmax (0, library->categories[(size_t) pack].indexOf (sound->category));

                const auto list = rows();

                for (size_t i = 0; i < list.size(); ++i)
                    if (list[i].flag == flag)
                        page = static_cast<int> (i) / perPage;
            }
            else if (! library->hasPack (pack))
            {
                for (int p = 0; p < Tomahawk::numPacks; ++p)
                    if (library->hasPack (p)) { pack = p; break; }
            }
        }

        void setPack (int p)
        {
            pack = juce::jlimit (0, Tomahawk::numPacks - 1, p);
            category = 0;
            page = 0;

            if (const auto library = Library::get().contents())
                if (const auto* sound = library->find (flag); sound != nullptr && sound->pack == pack)
                    followSound();

            refreshColumns();
        }

        void selectPage (int want)
        {
            if (want < 0 || want >= pageCount())
                return;

            page = want;
            refreshColumns();
        }

        /** A button per page, in a row: as many as there are pages. */
        void layoutPageButtons()
        {
            const int count = pageCount() > 1 ? pageCount() : 0;

            while (pageButtons.size() > count)
                pageButtons.removeLast();

            while (pageButtons.size() < count)
            {
                const int index = pageButtons.size();
                auto* b = pageButtons.add (new juce::TextButton (juce::String (index + 1)));
                b->onClick = [this, index] { selectPage (index); };
                addAndMakeVisible (b);
            }

            auto row = pagerArea;

            for (int i = 0; i < pageButtons.size(); ++i)
            {
                Style::styleSquareButton (*pageButtons[i], i == page);
                pageButtons[i]->setBounds (row.removeFromLeft (36).reduced (2));
                row.removeFromLeft (4);
            }
        }

        void pick (int row)
        {
            const auto list = rows();
            const auto index = static_cast<size_t> (page * perPage + row);

            if (index < list.size() && onPicked)
                onPicked (list[index].flag);
        }

        void refreshColumns()
        {
            const auto library = Library::get().contents();

            colPack.setSelected (pack);

            juce::StringArray categories;

            if (library != nullptr)
                categories = library->categories[(size_t) pack];

            colCategory.setItems (categories);
            colCategory.setSelected (categories.isEmpty() ? -1 : juce::jlimit (0, categories.size() - 1, category));

            const auto list = rows();
            page = juce::jlimit (0, pageCount() - 1, page);

            juce::StringArray names;
            juce::Array<int> rowFlags;
            int selected = -1;

            for (int i = 0; i < perPage; ++i)
            {
                const auto index = static_cast<size_t> (page * perPage + i);

                if (index >= list.size())
                    break;

                names.add (list[index].name);
                rowFlags.add (list[index].flag);

                if (list[index].flag == flag)
                    selected = i;
            }

            colInstr.setItems (names);

            for (int i = 0; i < rowFlags.size(); ++i)
                colInstr.setNumber (i, rowFlags[i]);

            colInstr.setSelected (selected);

            pageCaption.setVisible (pageCount() > 1);
            layoutPageButtons();
        }

        GrexSounds::SelectorColumn colPack, colCategory, colInstr;
        juce::Label pageCaption, soundName, soundInfo, status;
        juce::OwnedArray<juce::TextButton> pageButtons;
        juce::Rectangle<int> pagerArea;
        Tomahawk::InstrImageDisplay picture;
        Tomahawk::GoldSlider lowVelocity { "LOW VELOCITY RESPONSE", 0.0f, 100.0f, 0.0f, "" };
        Tomahawk::GoldSlider bendRange { "PITCH BEND RANGE", 1.0f, 12.0f, 2.0f, "" };
        juce::Rectangle<int> soundsFrame, globalFrame, pictureArea;

        int flag = -2, pack = Tomahawk::packClassic, category = 0, page = 0;
        int shownFlag = -2;
        juce::String shownFamily;
    };

    //==============================================================================
    /** The Tomahawk's window: Grex's instrument editor, the one place for everything. Its
        header: the sound, SAVE PRESET, LOCATE LIBRARY and GET THE LIBRARY; its tabs: SOUND
        ENGINE (the sounds), SYNTHESIS, MODULATION and EFFECTS (every effect in one row); Grex's
        keyboard along the bottom. */
    class TomahawkEditor final : public juce::Component,
                                 private juce::Timer,
                                 private Library::Listener
    {
    public:
        explicit TomahawkEditor (TomahawkSampler& p)
            : plugin (p), voice (p.getVoice())
        {
            setOpaque (true);

            title.setText ("TOMAHAWK", juce::dontSendNotification);
            title.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
            title.setColour (juce::Label::textColourId, accent);
            addAndMakeVisible (title);

            slotLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
            slotLabel.setColour (juce::Label::textColourId, accent);
            addAndMakeVisible (slotLabel);

            savedLabel.setJustificationType (juce::Justification::centred);
            savedLabel.setColour (juce::Label::textColourId, savedGreen);
            savedLabel.setInterceptsMouseClicks (false, false);
            addChildComponent (savedLabel);

            saveButton.setButtonText ("SAVE PRESET");
            saveButton.setTooltip ("This sound's settings become its own, written into its preset (in the presets' folders beside Sonify): it comes with them every time it's picked");
            saveButton.onClick = [this] { savePreset(); };

            locateButton.setButtonText ("LOCATE LIBRARY");
            locateButton.setTooltip ("The Tomahawk_Sampler folder, wherever you unzipped the library");
            locateButton.onClick = [this] { locate(); };

            getButton.setButtonText ("GET THE LIBRARY");
            getButton.setTooltip ("Downloads the whole library (one zip): unzip it anywhere, then Locate Library");
            getButton.onClick = [] { juce::URL (Library::downloadLink).launchInDefaultBrowser(); };

            for (auto* b : { &saveButton, &locateButton, &getButton })
            {
                Style::styleSquareButton (*b, false);
                addAndMakeVisible (*b);
            }

            const juce::String tabLabels[numTabs] { "SOUND ENGINE", "SYNTHESIS", "MODULATION", "EFFECTS" };

            for (int i = 0; i < numTabs; ++i)
            {
                tabs[i].setButtonText (tabLabels[i]);
                tabs[i].onClick = [this, i] { selectTab (i); };
                addAndMakeVisible (tabs[i]);
            }

            // The sounds
            sounds.onPicked = [this] (int flag)
            {
                voice.selectInstrument (flag);
                plugin.markChanged();
                refreshAll();
            };

            sounds.onGlobalsMoved = [this] (float lowVelocity, float bendRange)
            {
                plugin.set (TomahawkSampler::lowVelocity, lowVelocity);
                plugin.set (TomahawkSampler::bendRange, bendRange);
            };

            // Grex's panels, as SoundsTab sets them up for a solo slot.
            synthPanel.setBandMode (true);
            synthPanel.setGainVisible (true);
            synthPanel.setNoteRangeMode (0);
            synthPanel.setEnergyOffset (0.0f);
            synthPanel.onBaseUnityRequested = [this] { askBaseGain(); };
            synthPanel.onBaseOctaveRequested = [this] { askBaseOctave(); };

            modPanel.setSoloMode (true);
            modPanel.setBandMode (true);

            effectsPanel.setSoloMode (true);
            effectsPanel.setSelectorsVisible ({});
            effectsPanel.setSelectorsInRow (true);

            synthPanel.onAnythingChanged = [this] { collect(); };
            modPanel.onAnythingChanged = [this] { collect(); };
            effectsPanel.onAnythingChanged = [this] { collect(); };

            addChildComponent (sounds);
            addChildComponent (synthPanel);
            addChildComponent (modPanel);
            addChildComponent (effectsPanel);

            pianoStrip.onNoteOn = [this] (int n, int v) { voice.editorNoteOn (n, v); };
            pianoStrip.onNoteOff = [this] (int n) { voice.editorNoteOff (n); };
            pianoStrip.onQuerySounding = [this] (uint32_t* mask) { if (mask != nullptr) voice.getSoundingNotes (mask); };
            addAndMakeVisible (pianoStrip);

            voice.onChanged = [this] { refreshAll(); };
            Library::get().addListener (this);

            selectTab (0);
            pushParams();
            refreshAll();
            setSize (designW, designH);
            startTimerHz (30);
        }

        ~TomahawkEditor() override
        {
            stopTimer();
            Library::get().removeListener (this);
            voice.onChanged = nullptr;
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xFF101010));
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawHorizontalLine (headerBottom, 0.0f, (float) getWidth());
            g.drawHorizontalLine (tabsBottom, 0.0f, (float) getWidth());
        }

        void resized() override
        {
            const int pad = 16, W = getWidth();
            const int rowY = 16, rowH = 30;

            // The header: the name and SAVE PRESET on the left, the library's two on the right.
            int right = W - pad;
            getButton.setBounds (right - 170, rowY, 170, rowH);
            right -= 170 + 8;
            locateButton.setBounds (right - 170, rowY, 170, rowH);
            right -= 170 + 16;

            title.setBounds (pad, rowY - 2, 150, rowH + 4);
            int x = pad + 150;

            const int textW = [this]
            {
                juce::GlyphArrangement ga;
                ga.addLineOfText (slotLabel.getFont(), slotLabel.getText(), 0.0f, 0.0f);
                return (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + 8;
            }();

            const int labelW = juce::jlimit (60, juce::jmax (60, right - x - 150 - 8), textW);
            slotLabel.setBounds (x, rowY + 2, labelW, rowH - 4);
            x += labelW + 8;
            saveButton.setBounds (x, rowY, 140, rowH);
            x += 140 + 12;
            savedLabel.setBounds (x, rowY, juce::jmax (0, right - x), rowH);

            const int tabY = headerBottom + 10, tabH = 42, gap = 8;
            const int tabW = (W - 2 * pad - (numTabs - 1) * gap) / numTabs;

            for (int i = 0; i < numTabs; ++i)
                tabs[i].setBounds (pad + i * (tabW + gap), tabY, tabW, tabH);

            pianoStrip.setBounds (0, getHeight() - keyboardH, W, keyboardH);

            const int contentY = tabsBottom + 8;
            const juce::Rectangle<int> content (pad, contentY, W - 2 * pad, getHeight() - contentY - 12 - keyboardH - 8);

            for (auto* panel : std::initializer_list<juce::Component*> { &sounds, &synthPanel, &modPanel, &effectsPanel })
                panel->setBounds (content);
        }

    private:
        static constexpr int numTabs = 4, headerBottom = 62, tabsBottom = 122, keyboardH = 64;

        void selectTab (int index)
        {
            activeTab = juce::jlimit (0, numTabs - 1, index);

            for (int i = 0; i < numTabs; ++i)
                Style::styleSquareButton (tabs[i], i == activeTab);

            sounds.setVisible (activeTab == 0);
            synthPanel.setVisible (activeTab == 1);
            modPanel.setVisible (activeTab == 2);
            effectsPanel.setVisible (activeTab == 3);
        }

        /** The sound's settings into Grex's panels (seeding them: they don't notify). */
        void pushParams()
        {
            currentParams = voice.getParams();
            synthPanel.loadParams (currentParams);
            modPanel.loadParams (currentParams);
            effectsPanel.loadParams (currentParams);
            shownParamsFlag = voice.getFlag();
        }

        /** The panels' settings back into the sound (Grex's collectInto, then the commit). */
        void collect()
        {
            auto p = currentParams;
            synthPanel.readInto (p);
            modPanel.readInto (p);
            effectsPanel.readInto (p);
            currentParams = p;
            voice.setParams (p);
            plugin.markChanged();
        }

        void refreshAll()
        {
            const auto flag = voice.getFlag();

            // A sound picked, read, or put in from a song: the panels show its settings.
            if (flag != shownParamsFlag || voice.getParams().baseUnityDb != currentParams.baseUnityDb
                || voice.getParams().baseOctave != currentParams.baseOctave)
                pushParams();

            const auto name = voice.getInstrumentName();
            slotLabel.setText (flag >= 0 ? "Editing: " + (name.isNotEmpty() ? name : "Sound " + juce::String (flag)) : juce::String(),
                               juce::dontSendNotification);
            sounds.show (flag, name, statusText());
            resized();
        }

        juce::String statusText() const
        {
            const auto library = Library::get().contents();

            if (library == nullptr || ! library->isInstalled())
                return "The Tomahawk library isn't located.";

            if (voice.isLoading())
                return "Reading " + voice.getInstrumentName() + "...";

            if (const auto problem = voice.getProblem(); problem.isNotEmpty())
                return problem;

            juce::String text;

            for (int p = 0; p < Tomahawk::numPacks; ++p)
            {
                int count = 0;

                for (const auto& entry : library->sounds)
                    if (entry.second.pack == p)
                        ++count;

                if (count > 0)
                    text << Library::packName (p) << " " << count << "    ";
            }

            return text + library->root.getFullPathName();
        }

        void libraryChanged() override
        {
            sounds.libraryChanged();
            refreshAll();
        }

        void timerCallback() override
        {
            // The sweetener's meters (Grex's UI timer), the global settings as automation moves
            // them, and the status while a sound is read.
            float gr[4] {};
            voice.getSweetenerGr (gr);
            effectsPanel.setSweetenerGr (gr[0], gr[1], gr[2], gr[3]);

            sounds.showGlobals ((float) plugin.get (TomahawkSampler::lowVelocity), (float) plugin.get (TomahawkSampler::bendRange));

            if (const bool loadingNow = voice.isLoading(); loadingNow != wasLoading)
            {
                wasLoading = loadingNow;
                refreshAll();
            }
        }

        //==============================================================================
        void flashSaved (const juce::String& text)
        {
            savedLabel.setText (text, juce::dontSendNotification);
            savedLabel.setVisible (true);

            const int token = ++savedToken;
            juce::Component::SafePointer<TomahawkEditor> safe (this);

            juce::Timer::callAfterDelay (2000, [safe, token]
            {
                if (auto* e = safe.getComponent())
                    if (e->savedToken == token)
                        e->savedLabel.setVisible (false);
            });
        }

        void savePreset()
        {
            collect();
            juce::String why;

            if (voice.savePreset (why) != juce::File())
            {
                flashSaved ("PRESET SAVED");
                refreshAll();
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Tomahawk", why);
            }
        }

        /** Grex's BASE UNITY dialog (SoundsTab::showBaseUnityDialog). */
        void askBaseGain()
        {
            if (voice.getFlag() < 0)
                return;

            auto* w = new juce::AlertWindow ("BASE GAIN",
                                             "Base level for this instrument, in dB.\n"
                                             "The GAIN handle's middle (100) will mean exactly this.\n"
                                             "Saved in the instrument's preset. Range -40 to +40.",
                                             juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("db", juce::String (voice.getParams().baseUnityDb, 2), "dB:");
            w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            showCalibration (w, [this] (juce::AlertWindow& dialog, juce::String& why)
            {
                return voice.setBaseGain (dialog.getTextEditorContents ("db").getFloatValue(), why) != juce::File();
            });
        }

        /** The same for the base octave: where OCTAVE's 0 plays. */
        void askBaseOctave()
        {
            if (voice.getFlag() < 0)
                return;

            auto* w = new juce::AlertWindow ("BASE OCTAVE",
                                             "The octave this instrument plays in when OCTAVE is at 0.\n"
                                             "Saved in the instrument's preset.",
                                             juce::MessageBoxIconType::NoIcon);
            w->addComboBox ("octave", { "-3", "-2", "-1", "0", "+1", "+2", "+3" }, "Base octave:");

            if (auto* box = w->getComboBoxComponent ("octave"))
                box->setSelectedItemIndex (juce::jlimit (-3, 3, voice.getParams().baseOctave) + 3, juce::dontSendNotification);

            w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            showCalibration (w, [this] (juce::AlertWindow& dialog, juce::String& why)
            {
                const auto* box = dialog.getComboBoxComponent ("octave");
                return voice.setBaseOctave ((box != nullptr ? box->getSelectedItemIndex() : 3) - 3, why) != juce::File();
            });
        }

        void showCalibration (juce::AlertWindow* w, std::function<bool (juce::AlertWindow&, juce::String&)> save)
        {
            juce::Component::SafePointer<TomahawkEditor> safe (this);

            w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, save] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owned (w);
                auto* editor = safe.getComponent();

                if (result != 1 || editor == nullptr)
                    return;

                juce::String why;

                if (save (*w, why))
                {
                    editor->plugin.markChanged();
                    editor->pushParams();
                    editor->refreshAll();
                    editor->flashSaved ("SAVED IN THE PRESET");
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Tomahawk", why);
                }
            }), false);
        }

        void locate()
        {
            const auto library = Library::get().contents();
            const auto start = library != nullptr && library->root.isDirectory() ? library->root.getParentDirectory()
                                                                                  : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

            chooser = std::make_unique<juce::FileChooser> ("Locate the Tomahawk library (the Tomahawk_Sampler folder)", start);

            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [] (const juce::FileChooser& fc)
            {
                const auto folder = fc.getResult();

                if (folder == juce::File())
                    return;

                juce::String problem;

                if (! Library::get().locate (folder, problem))
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Locate Library", problem);
            });
        }

        //==============================================================================
        TomahawkSampler& plugin;
        Tomahawk::Voice& voice;

        juce::Label title, slotLabel, savedLabel;
        juce::TextButton saveButton, locateButton, getButton;
        juce::TextButton tabs[numTabs];

        SoundsPage sounds;
        Tomahawk::SynthesisPanel synthPanel;
        Tomahawk::ModulationPanel modPanel;
        Tomahawk::EffectsPanel effectsPanel;
        Tomahawk::PianoStrip pianoStrip;

        SlotParams currentParams;
        int activeTab = 0, shownParamsFlag = -2, savedToken = 0;
        bool wasLoading = false;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    //==============================================================================
    /** The editor at its design size, scaled to the window (Grex's Content does the same). */
    class ScaledEditor final : public juce::Component
    {
    public:
        explicit ScaledEditor (TomahawkSampler& plugin) : editor (plugin)
        {
            setLookAndFeel (&grexLook);
            addAndMakeVisible (editor);
            setSize (juce::roundToInt (designW * shownScale), juce::roundToInt (designH * shownScale));
        }

        ~ScaledEditor() override
        {
            setLookAndFeel (nullptr);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xFF080808));
        }

        void resized() override
        {
            const float s = juce::jmin ((float) getWidth() / (float) designW, (float) getHeight() / (float) designH);
            const float dx = ((float) getWidth() - designW * s) * 0.5f;
            const float dy = ((float) getHeight() - designH * s) * 0.5f;

            editor.setBounds (0, 0, designW, designH);
            editor.setTransform (juce::AffineTransform::scale (s).translated (dx, dy));
        }

    private:
        juce::LookAndFeel_V4 grexLook;   // Grex's editor is drawn on JUCE's own look
        TomahawkEditor editor;
    };

    class TomahawkWindow final : public juce::DocumentWindow
    {
    public:
        explicit TomahawkWindow (TomahawkSampler& plugin)
            : juce::DocumentWindow ("Tomahawk", juce::Colour (0xFF101010), juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            PluginHeader::attach (*this, plugin);    // its constrainer: the editor's shape, below the header
            setContentOwned (new ScaledEditor (plugin), true);
            setResizable (true, true);

            // The editor keeps its shape, from half its design size to full.
            getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
            getConstrainer()->setSizeLimits (designW / 2, designH / 2, designW, designH);
            centreWithSize (getWidth(), getHeight());
        }

        void closeButtonPressed() override
        {
            setVisible (false);   // kept for next time, where it was
        }
    };

    class WindowScreen final : public Tomahawks::Screen
    {
    public:
        explicit WindowScreen (TomahawkSampler& p) : plugin (p) {}

        void show (const juce::String& title) override
        {
            // Presets dropped into the presets' folders since (shared ones) come in now.
            Library::get().reloadPresets();

            if (window == nullptr)
                window = std::make_unique<TomahawkWindow> (plugin);

            window->setName (title.isNotEmpty() ? title : juce::String ("Tomahawk"));
            window->setVisible (true);
            window->toFront (true);
        }

        void hide() override
        {
            if (window != nullptr)
                window->setVisible (false);
        }

        bool isShowing() const override   { return window != nullptr && window->isVisible(); }

    private:
        TomahawkSampler& plugin;
        std::unique_ptr<TomahawkWindow> window;
    };
}

//==============================================================================
void TomahawkScreen::install()
{
    Tomahawks::setScreenFactory ([] (TomahawkSampler& plugin) -> std::unique_ptr<Tomahawks::Screen>
    {
        return std::make_unique<WindowScreen> (plugin);
    });
}
