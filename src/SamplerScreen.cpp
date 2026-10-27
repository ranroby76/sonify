// D:\workspace\Fanan_web_hub\src\SamplerScreen.cpp

#include "SamplerScreen.h"
#include "PluginHeader.h"

namespace
{
    namespace Look
    {
        const juce::Colour page (0xff232427);
        const juce::Colour panel (0xff2b2c30);
        const juce::Colour panel2 (0xff313337);
        const juce::Colour inset (0xff1d1e21);
        const juce::Colour line (0xff3b3d42);
        const juce::Colour text (0xffd6d7d9);
        const juce::Colour bright (0xffeceded);
        const juce::Colour muted (0xff8e939a);
        const juce::Colour accent (0xff25984d);
        const juce::Colour problem (0xffff7b7b);

        juce::Font font (float size, bool bold = false)
        {
            return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
        }

        constexpr int sliderWidth = 46;
        constexpr int labelHeight = 18;
        constexpr int groupGap = 6;
        constexpr int groupTitle = 18;
    }

    /** Where the choosers start: the last folder used this run, else the Samples folder. */
    juce::File& lastFolder()
    {
        static juce::File folder;
        return folder;
    }

    juce::File startFolder()
    {
        if (lastFolder().isDirectory())
            return lastFolder();

        auto samples = Samplers::folder();
        samples.createDirectory();
        return samples;
    }

    //==============================================================================
    /** Sampler Arena's sliders: a thin track filled from the bottom, its value on the thumb. */
    class SamplerLook final : public juce::LookAndFeel_V4
    {
    public:
        SamplerLook()
        {
            setColour (juce::ComboBox::backgroundColourId, Look::inset);
            setColour (juce::ComboBox::outlineColourId, Look::line);
            setColour (juce::ComboBox::textColourId, Look::text);
            setColour (juce::ComboBox::arrowColourId, Look::muted);
            setColour (juce::PopupMenu::backgroundColourId, Look::panel2);
            setColour (juce::PopupMenu::textColourId, Look::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, Look::accent);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TextButton::buttonColourId, Look::panel2);
            setColour (juce::TextButton::textColourOffId, Look::text);
            setColour (juce::TextButton::textColourOnId, Look::bright);
            setColour (juce::Label::textColourId, Look::text);
            setColour (juce::GroupComponent::textColourId, Look::text);
            setColour (juce::GroupComponent::outlineColourId, Look::line);
            setColour (juce::TooltipWindow::backgroundColourId, Look::panel2);
            setColour (juce::TooltipWindow::textColourId, Look::text);
            setColour (juce::TooltipWindow::outlineColourId, Look::line);
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minPos, float maxPos,
                               juce::Slider::SliderStyle style, juce::Slider& slider) override
        {
            if (style != juce::Slider::LinearVertical)
            {
                LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
                return;
            }

            const auto accent = slider.findColour (juce::Slider::trackColourId);
            const auto centre = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
            const auto trackWidth = 6.0f;
            const juce::Rectangle<float> track (centre - trackWidth * 0.5f, static_cast<float> (y), trackWidth, static_cast<float> (height));

            g.setColour (Look::inset);
            g.fillRoundedRectangle (track, 3.0f);

            if (const auto fill = track.getBottom() - sliderPos; fill > 0.0f)
            {
                g.setColour (accent.withAlpha (slider.isEnabled() ? 0.55f : 0.25f));
                g.fillRoundedRectangle (track.withTop (sliderPos), 3.0f);
            }

            const auto thumb = juce::Rectangle<float> (static_cast<float> (width) - 4.0f, 20.0f).withCentre ({ centre, sliderPos });
            g.setColour (slider.isMouseButtonDown() ? Look::panel2.brighter (0.15f) : Look::panel2);
            g.fillRoundedRectangle (thumb, 4.0f);
            g.setColour (accent.withAlpha (slider.isEnabled() ? 0.9f : 0.4f));
            g.drawRoundedRectangle (thumb.reduced (0.5f), 4.0f, 1.0f);

            g.setColour (slider.isEnabled() ? Look::bright : Look::muted);
            g.setFont (Look::font (10.5f, true));
            g.drawFittedText (slider.getTextFromValue (slider.getValue()), thumb.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
        }

        int getSliderThumbRadius (juce::Slider&) override   { return 10; }

        void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) override
        {
            const auto area = button.getLocalBounds().toFloat().reduced (0.5f);
            auto fill = button.findColour (juce::TextButton::buttonColourId);

            if (down)       fill = fill.brighter (0.18f);
            else if (over)  fill = fill.brighter (0.08f);

            g.setColour (button.isEnabled() ? fill : fill.withAlpha (0.5f));
            g.fillRoundedRectangle (area, 4.0f);
            g.setColour (Look::line);
            g.drawRoundedRectangle (area, 4.0f, 1.0f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int) override   { return Look::font (12.5f); }
        juce::Font getComboBoxFont (juce::ComboBox&) override             { return Look::font (12.5f); }
        juce::Font getPopupMenuFont() override                            { return Look::font (13.0f); }
    };

    //==============================================================================
    /** A slider for one of the sampler's parameters: its own range (skewed round a middle
        value for times and frequencies), its value as the sampler writes it. */
    class ParamSlider final : public juce::Slider
    {
    public:
        ParamSlider (SonifySampler& s, SonifySampler::Param p, juce::Colour accent, double middle, const juce::String& shortName)
            : sampler (s), param (p), label (shortName)
        {
            const auto& parameter = sampler.getParameters()[static_cast<size_t> (sampler.findParameter (param))];
            juce::NormalisableRange<double> range (parameter.minValue, parameter.maxValue, parameter.stepped ? 1.0 : 0.0);

            if (middle > parameter.minValue && middle < parameter.maxValue)
                range.setSkewForCentre (middle);

            setSliderStyle (juce::Slider::LinearVertical);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setNormalisableRange (range);
            setDoubleClickReturnValue (true, sampler.getDefault (param));
            setColour (juce::Slider::trackColourId, accent);
            setTooltip (parameter.name + " (double-click: back to " + sampler.getParameterText (sampler.findParameter (param), sampler.getDefault (param)) + ")");
            setValue (sampler.get (param), juce::dontSendNotification);
            onValueChange = [this] { sampler.set (param, getValue()); };
        }

        juce::String getTextFromValue (double value) override
        {
            return sampler.getParameterText (sampler.findParameter (param), value);
        }

        /** The sampler's value, unless the slider is being dragged (automation, a preset, an SFZ). */
        void refresh()
        {
            if (isMouseButtonDown())
                return;

            if (const auto value = sampler.get (param); std::abs (value - getValue()) > 1.0e-9)
                setValue (value, juce::dontSendNotification);
        }

        SonifySampler& sampler;
        const SonifySampler::Param param;
        const juce::String label;
    };

    //==============================================================================
    class SamplerPage final : public juce::Component,
                              private juce::Timer
    {
    public:
        explicit SamplerPage (SonifySampler& s) : sampler (s)
        {
            setLookAndFeel (&look);

            const auto button = [this] (juce::TextButton& b, const juce::String& text, const juce::String& tip, std::function<void()> click)
            {
                b.setButtonText (text);
                b.setTooltip (tip);
                b.onClick = std::move (click);
                addAndMakeVisible (b);
            };

            button (loadButton, "Load SFZ...", "Open an SFZ instrument", [this] { chooseInstrument(); });
            button (latestButton, "Latest", "The instrument sampled last in the Sampling Studio", [this]
            {
                if (const auto latest = Samplers::latest(); latest.existsAsFile())
                    sampler.loadInstrument (latest, true);
            });
            button (clearButton, "Clear", "No instrument", [this] { sampler.clearInstrument(); });
            button (saveButton, "Save SFZ", "Write the SFZ again with these settings in it", [this]
            {
                if (const auto file = sampler.getInstrumentFile(); file.existsAsFile())
                    sampler.saveInstrument (file);
            });
            button (saveAsButton, "Save As...", "The SFZ with these settings in it, as a new file", [this] { chooseSaveInstrument(); });
            button (loadPresetButton, "Load Preset...", "Settings saved before (Sampler Arena's .sfzp too)", [this] { choosePreset (false); });
            button (savePresetButton, "Save Preset...", "These settings and the instrument, as a preset (.sfzp)", [this] { choosePreset (true); });

            patch.setFont (Look::font (13.5f, true));
            patch.setJustificationType (juce::Justification::centredLeft);
            patch.setColour (juce::Label::textColourId, juce::Colour (0xffd4a843));
            addAndMakeVisible (patch);

            using P = SonifySampler;
            const juce::Colour ampColour (120, 200, 120), filterColour (120, 140, 220), filterEnvColour (220, 160, 80),
                               playColour (200, 200, 100), volumeColour (140, 220, 220), pitchEnvColour (200, 120, 220),
                               pitchColour (160, 200, 160), lfoColour (200, 160, 220);

            amp = addGroup ("Amp Envelope", ampColour, {
                { P::ampAttack, 100.0, "A" }, { P::ampHold, 200.0, "H" }, { P::ampDecay, 300.0, "D" }, { P::ampSustain, 0.0, "S" },
                { P::ampRelease, 600.0, "R" } });
            filter = addGroup ("Filter", filterColour, {
                { P::cutoff, 1000.0, "Cut" }, { P::resonance, 0.0, "Res" }, { P::filterKeytrack, 0.0, "Trk" } });
            filterEnv = addGroup ("Filter Envelope", filterEnvColour, {
                { P::filterAttack, 100.0, "A" }, { P::filterHold, 200.0, "H" }, { P::filterDecay, 300.0, "D" },
                { P::filterSustain, 0.0, "S" }, { P::filterRelease, 600.0, "R" }, { P::filterAmount, 0.0, "Amt" } });
            play = addGroup ("Play", playColour, { { P::octave, 0.0, "Oct" }, { P::glide, 200.0, "Glide" } });
            vol = addGroup ("Volume", volumeColour, { { P::volume, 0.0, "Vol" } });
            pitchEnv = addGroup ("Pitch Envelope", pitchEnvColour, {
                { P::pitchAttack, 100.0, "A" }, { P::pitchHold, 200.0, "H" }, { P::pitchDecay, 300.0, "D" },
                { P::pitchSustain, 0.0, "S" }, { P::pitchRelease, 600.0, "R" }, { P::pitchDepth, 0.0, "Depth" } });
            pitch = addGroup ("Pitch", pitchColour, {
                { P::bendRange, 0.0, "Bend" }, { P::pitchKeytrack, 0.0, "KTrk" }, { P::pitchVeltrack, 0.0, "VTrk" } });
            tremolo = addGroup ("Tremolo LFO", lfoColour, {
                { P::ampLfoRate, 3.0, "Rate" }, { P::ampLfoDepth, 0.0, "Depth" }, { P::ampLfoDelay, 500.0, "Delay" } });
            filterLfo = addGroup ("Filter LFO", lfoColour, {
                { P::filterLfoRate, 3.0, "Rate" }, { P::filterLfoDepth, 0.0, "Depth" }, { P::filterLfoDelay, 500.0, "Delay" } });
            vibrato = addGroup ("Vibrato LFO", lfoColour, {
                { P::pitchLfoRate, 3.0, "Rate" }, { P::pitchLfoDepth, 2.0, "Depth" }, { P::pitchLfoDelay, 500.0, "Delay" } });

            const auto combo = [this] (juce::ComboBox& box, const juce::StringArray& items, SonifySampler::Param param, const juce::String& tip)
            {
                box.addItemList (items, 1);
                box.setTooltip (tip);
                box.setSelectedId (juce::roundToInt (sampler.get (param)) + 1, juce::dontSendNotification);
                box.onChange = [this, &box, param] { sampler.set (param, box.getSelectedId() - 1); updateMono(); };
                addAndMakeVisible (box);
            };

            combo (filterType, { "Low Pass", "High Pass", "Band Pass", "Notch" }, P::filterType, "The filter's kind");
            combo (playMode, { "Poly", "Mono" }, P::playMode, "Poly: every note sounds. Mono: one at a time, gliding between them");
            combo (monoMode, { "Legato", "Retrigger New", "Retrigger Stolen" }, P::monoMode,
                   "Legato: the sound goes on, gliding to the new note. Retrigger New: it starts again, gliding from the last. "
                   "Retrigger Stolen: its envelopes start again, the sound going on");
            updateMono();
            refreshPatch();
            setSize (1120, 560);    // last: resized() places what's made above
            startTimerHz (10);
        }

        ~SamplerPage() override
        {
            stopTimer();
            setLookAndFeel (nullptr);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (Look::page);
            g.setColour (Look::panel);
            g.fillRect (getLocalBounds().removeFromTop (topHeight));
            g.setColour (Look::line);
            g.drawHorizontalLine (topHeight, 0.0f, static_cast<float> (getWidth()));

            g.setFont (Look::font (11.0f));

            for (const auto& group : groups)
            {
                for (const auto* slider : group->sliders)
                {
                    g.setColour (group->colour.withAlpha (0.85f));
                    g.drawText (slider->label, juce::Rectangle<int> (slider->getX() - 6, slider->getBottom() + 2, slider->getWidth() + 12, Look::labelHeight - 2),
                                juce::Justification::centredTop, false);
                }
            }
        }

        void resized() override
        {
            if (vibrato == nullptr)
                return;    // still being made

            auto area = getLocalBounds();
            auto top = area.removeFromTop (topHeight).reduced (8, 6);

            loadButton.setBounds (top.removeFromLeft (92));       top.removeFromLeft (4);
            latestButton.setBounds (top.removeFromLeft (64));     top.removeFromLeft (4);
            clearButton.setBounds (top.removeFromLeft (54));      top.removeFromLeft (4);
            saveButton.setBounds (top.removeFromLeft (76));       top.removeFromLeft (4);
            saveAsButton.setBounds (top.removeFromLeft (84));     top.removeFromLeft (12);
            savePresetButton.setBounds (top.removeFromRight (108)); top.removeFromRight (4);
            loadPresetButton.setBounds (top.removeFromRight (108)); top.removeFromRight (12);
            patch.setBounds (top);

            area.reduce (8, 8);
            const auto floorHeight = (area.getHeight() - Look::groupGap) / 2;
            auto first = area.removeFromTop (floorHeight);
            area.removeFromTop (Look::groupGap);
            auto second = area;

            // First floor: amp, filter, filter envelope, play (what's left), volume.
            layout (*amp, first.removeFromLeft (widthFor (*amp)));
            first.removeFromLeft (Look::groupGap);

            auto filterArea = first.removeFromLeft (widthFor (*filter) + 12);
            filter->box.setBounds (filterArea);
            {
                auto inside = filterArea.reduced (8, 0).withTrimmedTop (Look::groupTitle);
                filterType.setBounds (inside.removeFromTop (24));
                inside.removeFromTop (6);
                placeSliders (*filter, inside);
            }

            first.removeFromLeft (Look::groupGap);
            layout (*filterEnv, first.removeFromLeft (widthFor (*filterEnv)));
            first.removeFromLeft (Look::groupGap);
            layout (*vol, first.removeFromRight (widthFor (*vol) + 24));   // room for its title
            first.removeFromRight (Look::groupGap);

            play->box.setBounds (first);
            {
                auto inside = first.reduced (8, 0).withTrimmedTop (Look::groupTitle);
                playMode.setBounds (inside.removeFromTop (24));
                inside.removeFromTop (4);
                monoMode.setBounds (inside.removeFromTop (24));
                inside.removeFromTop (6);
                placeSliders (*play, inside);
            }

            // Second floor: the pitch envelope, pitch, the three LFOs sharing what's left.
            layout (*pitchEnv, second.removeFromLeft (widthFor (*pitchEnv)));
            second.removeFromLeft (Look::groupGap);
            layout (*pitch, second.removeFromLeft (widthFor (*pitch)));
            second.removeFromLeft (Look::groupGap);

            const auto lfoWidth = (second.getWidth() - 2 * Look::groupGap) / 3;
            layout (*tremolo, second.removeFromLeft (lfoWidth));
            second.removeFromLeft (Look::groupGap);
            layout (*filterLfo, second.removeFromLeft (lfoWidth));
            second.removeFromLeft (Look::groupGap);
            layout (*vibrato, second);
        }

    private:
        struct Group
        {
            juce::GroupComponent box;
            juce::Colour colour;
            std::vector<ParamSlider*> sliders;
        };

        struct SliderSpec
        {
            SonifySampler::Param param;
            double middle;
            const char* label;
        };

        Group* addGroup (const juce::String& title, juce::Colour colour, std::initializer_list<SliderSpec> list)
        {
            auto group = std::make_unique<Group>();
            group->colour = colour;
            group->box.setText (title);
            group->box.setColour (juce::GroupComponent::outlineColourId, colour.withAlpha (0.45f));
            group->box.setColour (juce::GroupComponent::textColourId, colour);
            addAndMakeVisible (group->box);

            for (const auto& spec : list)
            {
                auto slider = std::make_unique<ParamSlider> (sampler, spec.param, colour, spec.middle, spec.label);
                addAndMakeVisible (*slider);
                group->sliders.push_back (slider.get());
                sliders.push_back (std::move (slider));
            }

            groups.push_back (std::move (group));
            return groups.back().get();
        }

        static int widthFor (const Group& group)
        {
            return static_cast<int> (group.sliders.size()) * Look::sliderWidth + 16;
        }

        void layout (Group& group, juce::Rectangle<int> area)
        {
            group.box.setBounds (area);
            placeSliders (group, area.reduced (8, 0).withTrimmedTop (Look::groupTitle));
        }

        /** The group's sliders side by side, in the middle of the area, their names under them. */
        static void placeSliders (Group& group, juce::Rectangle<int> area)
        {
            area.removeFromBottom (Look::labelHeight + 4);
            const auto count = static_cast<int> (group.sliders.size());
            const auto total = count * Look::sliderWidth;
            auto x = area.getX() + juce::jmax (0, (area.getWidth() - total) / 2);

            for (auto* slider : group.sliders)
            {
                slider->setBounds (x + 2, area.getY(), Look::sliderWidth - 4, juce::jmax (40, area.getHeight()));
                x += Look::sliderWidth;
            }
        }

        void updateMono()
        {
            monoMode.setVisible (juce::roundToInt (sampler.get (SonifySampler::playMode)) == 1);
            play->sliders[1]->setEnabled (juce::roundToInt (sampler.get (SonifySampler::playMode)) == 1);
        }

        void refreshPatch()
        {
            const auto file = sampler.getInstrumentFile();
            juce::String text;
            auto colour = juce::Colour (0xffd4a843);

            switch (sampler.getLoading())
            {
                case SonifySampler::Loading::loading:
                    text = "Loading " + file.getFileNameWithoutExtension() + "...";
                    colour = Look::muted;
                    break;

                case SonifySampler::Loading::failed:
                    text = sampler.getProblem();
                    colour = Look::problem;
                    break;

                case SonifySampler::Loading::ready:
                    text = sampler.describeInstrument();
                    break;

                case SonifySampler::Loading::none:
                    text = "No instrument: Load SFZ, or Latest for the one sampled last";
                    colour = Look::muted;
                    break;
            }

            if (text != patch.getText())
                patch.setText (text, juce::dontSendNotification);

            patch.setColour (juce::Label::textColourId, colour);
            patch.setTooltip (file.getFullPathName());

            const auto hasFile = file.existsAsFile();
            saveButton.setEnabled (hasFile && sampler.getLoading() == SonifySampler::Loading::ready);
            saveAsButton.setEnabled (sampler.getLoading() == SonifySampler::Loading::ready);
            clearButton.setEnabled (file != juce::File());

            if (++latestTicks % 20 == 1)
                latestButton.setEnabled (Samplers::latest().existsAsFile());
        }

        void timerCallback() override
        {
            for (auto& slider : sliders)
                slider->refresh();

            const auto sync = [this] (juce::ComboBox& box, SonifySampler::Param param)
            {
                const auto id = juce::roundToInt (sampler.get (param)) + 1;

                if (box.getSelectedId() != id && ! box.isPopupActive())
                    box.setSelectedId (id, juce::dontSendNotification);
            };

            sync (filterType, SonifySampler::filterType);
            sync (playMode, SonifySampler::playMode);
            sync (monoMode, SonifySampler::monoMode);
            updateMono();
            refreshPatch();
        }

        void chooseInstrument()
        {
            chooser = std::make_unique<juce::FileChooser> ("Load SFZ", startFolder(), "*.sfz");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safe = juce::Component::SafePointer<SamplerPage> (this)] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();

                if (safe == nullptr || ! file.existsAsFile())
                    return;

                lastFolder() = file.getParentDirectory();
                safe->sampler.loadInstrument (file, true);
            });
        }

        void chooseSaveInstrument()
        {
            const auto current = sampler.getInstrumentFile();
            const auto start = current != juce::File() ? current : startFolder().getChildFile ("Instrument.sfz");
            chooser = std::make_unique<juce::FileChooser> ("Save SFZ As", start, "*.sfz");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safe = juce::Component::SafePointer<SamplerPage> (this)] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();

                if (safe == nullptr || file == juce::File())
                    return;

                lastFolder() = file.getParentDirectory();
                safe->sampler.saveInstrument (file);
            });
        }

        void choosePreset (bool saving)
        {
            auto folder = Samplers::folder().getChildFile ("Sampler Presets");
            folder.createDirectory();
            const juce::String wildcard = juce::String ("*") + Samplers::presetExtension;

            if (saving)
            {
                const auto name = sampler.getInstrumentFile().getFileNameWithoutExtension();
                chooser = std::make_unique<juce::FileChooser> ("Save Preset", folder.getChildFile ((name.isNotEmpty() ? name : juce::String ("Preset")) + Samplers::presetExtension), wildcard);
                chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                          | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [safe = juce::Component::SafePointer<SamplerPage> (this)] (const juce::FileChooser& fc)
                {
                    if (safe != nullptr && fc.getResult() != juce::File())
                        safe->sampler.savePreset (fc.getResult());
                });
                return;
            }

            chooser = std::make_unique<juce::FileChooser> ("Load Preset", folder, wildcard);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safe = juce::Component::SafePointer<SamplerPage> (this)] (const juce::FileChooser& fc)
            {
                if (safe != nullptr && fc.getResult().existsAsFile())
                    safe->sampler.loadPreset (fc.getResult());
            });
        }

        static constexpr int topHeight = 42;

        SonifySampler& sampler;
        SamplerLook look;          // first: it outlasts the controls that use it
        juce::TooltipWindow tooltips { this, 600 };

        juce::TextButton loadButton, latestButton, clearButton, saveButton, saveAsButton, loadPresetButton, savePresetButton;
        juce::Label patch;
        juce::ComboBox filterType, playMode, monoMode;

        std::vector<std::unique_ptr<Group>> groups;
        std::vector<std::unique_ptr<ParamSlider>> sliders;
        Group* amp = nullptr;
        Group* filter = nullptr;
        Group* filterEnv = nullptr;
        Group* play = nullptr;
        Group* vol = nullptr;
        Group* pitchEnv = nullptr;
        Group* pitch = nullptr;
        Group* tremolo = nullptr;
        Group* filterLfo = nullptr;
        Group* vibrato = nullptr;

        std::unique_ptr<juce::FileChooser> chooser;
        int latestTicks = 0;
    };

    //==============================================================================
    class SamplerWindow final : public juce::DocumentWindow
    {
    public:
        explicit SamplerWindow (SonifySampler& sampler)
            : juce::DocumentWindow ("Sonify Sampler", Look::page, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            PluginHeader::attach (*this, sampler);
            setContentOwned (SamplerScreen::createPage (sampler).release(), true);
            setResizable (true, true);
            getConstrainer()->setSizeLimits (1020, 500, 2400, 1200);
            centreWithSize (getWidth(), getHeight());
        }

        void closeButtonPressed() override
        {
            setVisible (false);   // kept for next time, where it was
        }
    };

    class WindowScreen final : public Samplers::Screen
    {
    public:
        explicit WindowScreen (SonifySampler& s) : sampler (s) {}

        void show (const juce::String& title) override
        {
            if (window == nullptr)
                window = std::make_unique<SamplerWindow> (sampler);

            window->setName (title.isNotEmpty() ? title : juce::String ("Sonify Sampler"));
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
        SonifySampler& sampler;
        std::unique_ptr<SamplerWindow> window;
    };
}

//==============================================================================
std::unique_ptr<juce::Component> SamplerScreen::createPage (SonifySampler& sampler)
{
    return std::make_unique<SamplerPage> (sampler);
}

void SamplerScreen::install()
{
    Samplers::setScreenFactory ([] (SonifySampler& sampler) -> std::unique_ptr<Samplers::Screen>
    {
        return std::make_unique<WindowScreen> (sampler);
    });
}
