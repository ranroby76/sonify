// C:\workspace\SONIFY PLUGIN natura\src\NaturaLibrary.h
#pragma once
//==============================================================================
//  NaturaLibrary.h  -  the LIBRARY tab, Natura's first: sounds picked the
//  Tomahawk way (SOURCE, CATEGORY and INSTRUMENT columns, eight to a page,
//  the family's picture), and the MACROS along the bottom - the settings a
//  player reaches for, on the captain's steer knob.
//
//  The steer knob is a Sonify look (.fklayers) compiled in from
//  src/natura/knobs (NaturaKnobs.h); without it the knobs wear Sonify's
//  example knob, so the panel never goes without.
//==============================================================================
#include "NaturaPanels.h"
#include "../LayerRender.h"

#if __has_include ("NaturaKnobs.h")
 #include "NaturaKnobs.h"
 #define NATURA_HAS_KNOBS 1
#else
 #define NATURA_HAS_KNOBS 0
#endif

#if __has_include ("TomahawkImages.h")
 #include "TomahawkImages.h"
 #define NATURA_HAS_FAMILY_ICONS 1
#else
 #define NATURA_HAS_FAMILY_ICONS 0
#endif

#include <map>
#include <random>

namespace natura
{
    //==========================================================================
    //  Families: how the library sorts its sounds
    //==========================================================================
    enum class Family : int { Piano, EPiano, Mallets, Organ, Accordion, Guitar, Bass, Plucked, Strings, Brass, Reeds, Flutes, World, Synth, kCount };
    constexpr int kNumFamilies = (int) Family::kCount;

    inline const char* familyName (int f)
    {
        static const char* names[kNumFamilies] = { "PIANO", "ELECTRIC PIANO", "MALLETS & BELLS", "ORGAN", "ACCORDION & HARMONICA",
                                                   "GUITAR", "BASS", "HARP & HARPSICHORD", "STRINGS", "BRASS", "REEDS", "FLUTES & PIPES",
                                                   "WORLD WINDS", "SYNTH" };
        return names[juce::jlimit (0, kNumFamilies - 1, f)];
    }

    /** A sound's family from what it is made of (a user's preset). */
    inline Family familyOf (const std::vector<float>& v)
    {
        using A = Archetype;
        auto at = [&v] (P p) { return v[(size_t) p]; };
        switch ((A) juce::jlimit (0, kNumArchetypes - 1, (int) std::lround (at (P::archetype))))
        {
            case A::StruckString:  return Family::Piano;
            case A::TineEP:        return Family::EPiano;
            case A::StruckBar:     return Family::Mallets;
            case A::Tonewheel:
            case A::PipeOrgan:     return Family::Organ;
            case A::FreeReed:      return (int) std::lround (at (P::fr_register)) == 7 ? Family::Organ : Family::Accordion;
            case A::PluckedString:
                if (at (P::pl_plectrum) >= 90.0f && at (P::pl_amp) < 0.5f) return Family::Plucked;
                if (at (P::pl_ring) >= 80.0f && at (P::pl_plectrum) <= 30.0f) return Family::Plucked;
                return at (P::octave) <= -0.5f ? Family::Bass : Family::Guitar;
            case A::BowedString:   return Family::Strings;
            case A::LipReed:       return Family::Brass;
            case A::SingleReed:
            case A::DoubleReed:    return Family::Reeds;
            case A::AirJet:        return Family::Flutes;
            case A::Synth:
            default:               return Family::Synth;
        }
    }

    /** A factory sound's family: by name where the name says more than its makeup. */
    inline Family familyOf (const Factory& f)
    {
        static const std::map<juce::String, Family> byName {
            { "Dulcimer", Family::Piano },          { "Clavinet", Family::EPiano },
            { "Reed Organ", Family::Organ },        { "Harmonica", Family::Accordion },
            { "Harp", Family::Plucked },            { "Harpsichord", Family::Plucked },
            { "Acoustic Bass", Family::Bass },      { "Finger Bass", Family::Bass },     { "Pick Bass", Family::Bass },
            { "Synth Bass 1", Family::Synth },      { "Synth Bass 2", Family::Synth },
            { "Duduk", Family::World },             { "Klezmer Clarinet", Family::World }, { "Zurna", Family::World },
            { "Ney", Family::World },               { "Bagpipe Chanter", Family::World }
        };
        if (const auto it = byName.find (f.name); it != byName.end()) return it->second;
        return familyOf (factoryValues (f));
    }

    /** The family's picture: Tomahawk's family icons (assets/instruments icons). */
    inline juce::Image familyPicture (int f)
    {
       #if NATURA_HAS_FAMILY_ICONS
        static const char* names[kNumFamilies] = { "piano_png", "piano_png", "piano_png", "organ_png", "accordion_png",
                                                   "guitar_png", "guitar_png", "guitar_png", "strings_png", "brass_png",
                                                   "wind_png", "wind_png", "wind_png", "synth_png" };
        int size = 0;
        if (const char* data = TomahawkImages::getNamedResource (names[juce::jlimit (0, kNumFamilies - 1, f)], size); data != nullptr && size > 0)
            return juce::ImageCache::getFromMemory (data, size);
       #else
        juce::ignoreUnused (f);
       #endif
        return {};
    }

    //==========================================================================
    //  One column of the selector (Tomahawk's SelectorColumn): rows of text,
    //  the picked one in gold, a faint wash under the mouse, one rule between
    //  columns and no boxes.
    //==========================================================================
    class SelectorColumn final : public juce::Component
    {
    public:
        std::function<void (int)> onRowClicked;

        void setTitle (const juce::String& t) { title = t; repaint(); }
        void setMaxRowH (int h)               { maxRowH = juce::jmax (12, h); }
        void setDrawRule (bool r)             { drawRule = r; repaint(); }
        void setTextScale (float s)           { textScale = juce::jlimit (0.5f, 3.0f, s); repaint(); }

        void setItems (const juce::StringArray& names)
        {
            if (names == items) return;
            items = names;
            if (selected >= items.size()) selected = -1;
            hovered = -1;
            repaint();
        }

        void setSelected (int idx) { if (selected != idx) { selected = idx; repaint(); } }
        int  getNumItems() const noexcept { return items.size(); }

        void paint (juce::Graphics& g) override
        {
            const int w = getWidth(), h = getHeight();
            int y = 0;
            if (title.isNotEmpty())
            {
                g.setColour (juce::Colours::white.withAlpha (0.38f));
                g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
                g.drawFittedText (title, juce::Rectangle<int> (2, 0, w - 8, kTitleH), juce::Justification::centredLeft, 1);
                y = kTitleH;
            }
            const int rowH = rowHeight();
            const float fh = juce::jlimit (8.5f, 12.0f, (float) rowH * 0.58f) * textScale;
            for (int i = 0; i < items.size(); ++i)
            {
                const int ry = y + i * rowH;
                if (ry + rowH > h) break;
                const juce::Rectangle<int> row (0, ry, w - 4, rowH);
                if (i == hovered)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.07f));
                    g.fillRect (row);
                }
                const bool sel = i == selected;
                g.setColour (sel ? juce::Colour (Theme::Gold::kAccent).brighter (0.35f) : juce::Colours::white.withAlpha (0.72f));
                g.setFont (juce::Font (juce::FontOptions (fh, sel ? juce::Font::bold : juce::Font::plain)));
                g.drawFittedText (items[i], row.withTrimmedLeft (6), juce::Justification::centredLeft, 1);
            }
            if (drawRule)
            {
                g.setColour (juce::Colours::white.withAlpha (0.22f));
                g.drawVerticalLine (w - 1, 0.0f, (float) h);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override { setHover (rowAt (e.y)); }
        void mouseExit (const juce::MouseEvent&) override   { setHover (-1); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            const int r = rowAt (e.y);
            if (r >= 0 && r < items.size() && onRowClicked) onRowClicked (r);
        }

    private:
        static constexpr int kTitleH = 16;

        int rowHeight() const
        {
            const int top = title.isNotEmpty() ? kTitleH : 0;
            return juce::jlimit (13, maxRowH, (getHeight() - top) / juce::jmax (1, items.size()));
        }

        int rowAt (int py) const
        {
            const int top = title.isNotEmpty() ? kTitleH : 0;
            if (py < top) return -1;
            const int r = (py - top) / juce::jmax (1, rowHeight());
            return r < items.size() ? r : -1;
        }

        void setHover (int r) { if (hovered != r) { hovered = r; repaint(); } }

        juce::StringArray items;
        juce::String title;
        int selected = -1, hovered = -1, maxRowH = 26;
        float textScale = 1.0f;
        bool drawRule = true;
    };

    //==========================================================================
    //  The family's picture with Tomahawk's glow and rising smoke.
    //==========================================================================
    class FamilyPicture final : public juce::Component, private juce::Timer
    {
    public:
        FamilyPicture() { setInterceptsMouseClicks (false, false); }
        ~FamilyPicture() override { stopTimer(); }

        void set (juce::Image img)
        {
            image = std::move (img);
            smoke.clear();
            if (image.isValid()) startTimerHz (30); else stopTimer();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (! image.isValid()) return;
            const float W = (float) getWidth(), H = (float) getHeight();
            const float cx = W * 0.5f, cy = H * 0.5f, iw = W * 0.75f, ih = H * 0.75f;
            for (int layer = 5; layer >= 1; --layer)
            {
                const float expand = (float) layer * 5.0f;
                g.setColour (juce::Colours::white.withAlpha (0.04f * (float) (6 - layer) / 5.0f));
                g.fillEllipse (cx - iw * 0.5f - expand, cy - ih * 0.5f - expand, iw + expand * 2.0f, ih + expand * 2.0f);
            }
            for (auto& p : smoke)
            {
                juce::ColourGradient gr (juce::Colours::white.withAlpha (p.alpha), p.x, p.y,
                                         juce::Colours::white.withAlpha (0.0f), p.x + p.r, p.y, true);
                g.setGradientFill (gr);
                g.fillEllipse (p.x - p.r, p.y - p.r, p.r * 2.0f, p.r * 2.0f);
            }
            g.drawImageWithin (image, 0, 0, (int) W, (int) H, juce::RectanglePlacement::centred);
        }

    private:
        struct Puff { float x, y, vy, r, alpha; };

        void timerCallback() override
        {
            const float W = (float) getWidth(), H = (float) getHeight();
            if (W <= 0 || ! image.isValid() || ! isShowing()) return;
            spawn += 1.0f;
            if (spawn > 2.5f && smoke.size() < 16)
            {
                smoke.push_back ({ W * 0.15f + rnd (rng) * W * 0.7f, H * 0.8f + rnd (rng) * H * 0.1f,
                                   -(0.25f + rnd (rng) * 0.4f), 3.0f + rnd (rng) * 5.0f, 0.12f + rnd (rng) * 0.08f });
                spawn = 0.0f;
            }
            for (auto& p : smoke) { p.y += p.vy; p.r += 0.12f; p.alpha -= 0.0025f; }
            smoke.erase (std::remove_if (smoke.begin(), smoke.end(), [] (const Puff& p) { return p.alpha <= 0.0f || p.y < -20.0f; }), smoke.end());
            repaint();
        }

        std::vector<Puff> smoke;
        juce::Image image;
        float spawn = 0.0f;
        std::mt19937 rng { 0x5EEDu };
        std::uniform_real_distribution<float> rnd { 0.0f, 1.0f };
    };

    //==========================================================================
    //  The captain's steer knob: its frames drawn from the look as they are
    //  needed, at the screen's own resolution, and kept.
    //==========================================================================
    class SteerLook
    {
    public:
        static SteerLook& get() { static SteerLook look; return look; }

        /** The frame for value (0..1), size pixels square at scale screen pixels per pixel;
            pad: the room (screen pixels) the look's shadows take round it. */
        juce::Image frame (int size, float value, float scale, float& pad)
        {
            const int count = juce::jmax (1, doc.frames);
            const int index = count > 1 ? juce::roundToInt (juce::jlimit (0.0f, 1.0f, value) * (float) (count - 1)) : 0;
            const float fit = (float) size * scale / (float) juce::jmax (8, juce::jmax (doc.canvasWidth, doc.canvasHeight));
            pad = std::ceil (LayerRender::overhangOf (doc) * fit);
            const auto key = juce::String (size) + "@" + juce::String (juce::roundToInt (scale * 4.0f)) + "#" + juce::String (index);
            if (const auto it = cache.find (key); it != cache.end()) return it->second;
            if (cache.size() > 1200) cache.clear();
            const float t = count > 1 ? (float) index / (float) (count - 1) : 0.0f;
            auto img = LayerRender::renderFrame (doc, t, fit, pad);
            cache[key] = img;
            return img;
        }

    private:
        SteerLook()
        {
           #if NATURA_HAS_KNOBS
            const juce::String xml (juce::CharPointer_UTF8 (NaturaKnobs::captains_steer_knob_fklayers),
                                    (size_t) NaturaKnobs::captains_steer_knob_fklayersSize);
            if (const auto parsed = juce::parseXML (xml))
            {
                const auto tree = juce::ValueTree::fromXml (*parsed);
                if (tree.isValid() && tree.hasType ("LayerDoc")) { doc = LayerDoc::fromValueTree (tree); return; }
            }
           #endif
            doc = LayerDoc::knobExample();
        }

        LayerDoc doc;
        std::map<juce::String, juce::Image> cache;
    };

    /** A macro: one parameter on a steer knob, its name over it and its value under it.
        Drag up or down (Shift: finely), the wheel steps, a double-click goes back to default. */
    class SteerKnob final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        SteerKnob (NaturaProcessor& proc, P p, const juce::String& caption)
            : id (p), name (caption),
              attach (*proc.param (p), [this] (float v) { value = v; repaint(); }, nullptr)
        {
            setTooltip (info (p).name);
            attach.sendInitialUpdate();
        }

        const P id;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds();
            const float alpha = isEnabled() ? 1.0f : 0.3f;
            g.setColour (juce::Colour (Theme::Gold::kAccent).withMultipliedAlpha (alpha));
            g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            g.drawFittedText (name, r.removeFromTop (20), juce::Justification::centred, 1);
            g.setColour (juce::Colours::white.withAlpha (0.85f * alpha));
            g.setFont (juce::Font (juce::FontOptions (13.0f)));
            g.drawFittedText (displayText (id, value), r.removeFromBottom (20), juce::Justification::centred, 1);

            const int size = juce::jmax (8, juce::jmin (r.getWidth(), r.getHeight()) - 4);
            const auto box = r.withSizeKeepingCentre (size, size);
            const float scale = juce::jlimit (1.0f, 3.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
            float pad = 0.0f;
            const auto img = SteerLook::get().frame (size, norm(), scale, pad);
            const float pv = pad / scale;
            g.setOpacity (alpha);
            g.drawImage (img, box.toFloat().expanded (pv), juce::RectanglePlacement::stretchToFit);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! isEnabled()) return;
            attach.beginGesture();
            dragFrom = norm();
            dragY = e.position.y;
            dragging = true;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging) return;
            const float travel = e.mods.isShiftDown() ? 900.0f : 220.0f;
            setNorm (dragFrom + (dragY - e.position.y) / travel, true);
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragging) attach.endGesture();
            dragging = false;
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (isEnabled()) attach.setValueAsCompleteGesture (info (id).defV);
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            if (! isEnabled()) return;
            const auto& in = info (id);
            const float steps = (in.kind == Kind::Float) ? 100.0f : (in.maxV - in.minV);
            const float dir = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) > 0.0f ? 1.0f : -1.0f;
            attach.setValueAsCompleteGesture (snap (juce::jlimit (in.minV, in.maxV, value + dir * (in.maxV - in.minV) / juce::jmax (1.0f, steps))));
        }

    private:
        float norm() const
        {
            const auto& in = info (id);
            return (value - in.minV) / juce::jmax (1.0e-6f, in.maxV - in.minV);
        }

        float snap (float v) const
        {
            const auto& in = info (id);
            if (in.kind != Kind::Float || in.unit == Unit::Pct || in.unit == Unit::Bip || in.unit == Unit::VibDepth) return std::round (v);
            return v;
        }

        void setNorm (float n, bool inGesture)
        {
            const auto& in = info (id);
            const float v = snap (in.minV + juce::jlimit (0.0f, 1.0f, n) * (in.maxV - in.minV));
            if (v == value) return;
            if (inGesture) attach.setValueAsPartOfGesture (v);
            else attach.setValueAsCompleteGesture (v);
        }

        juce::String name;
        juce::ParameterAttachment attach;
        float value = 0.0f, dragFrom = 0.0f, dragY = 0.0f;
        bool dragging = false;
    };

    //==========================================================================
    //  LIBRARY
    //==========================================================================
    class LibraryPage final : public juce::Component
    {
    public:
        /** A sound was loaded from here (the header's step buttons start from it). */
        std::function<void (int factoryIndex, const juce::File& userFile)> onLoaded;

        explicit LibraryPage (NaturaProcessor& p) : proc (p)
        {
            colSource.setTitle ("SOURCE");
            colSource.setItems ({ "FACTORY", "MY PRESETS" });
            colSource.setMaxRowH (34);
            colSource.onRowClicked = [this] (int r) { source = r; category = 0; page = 0; followSound = false; refreshColumns(); };

            colCategory.setTitle ("CATEGORY");
            colCategory.setMaxRowH (30);
            colCategory.onRowClicked = [this] (int r) { category = r; page = 0; followSound = false; refreshColumns(); };

            colSound.setTitle ("INSTRUMENT");
            colSound.setMaxRowH (34);
            colSound.setDrawRule (false);
            colSound.onRowClicked = [this] (int r) { pick (r); };

            for (auto* c : { &colSource, &colCategory, &colSound })
            {
                c->setTextScale (1.35f);
                addAndMakeVisible (c);
            }

            pageCaption.setText ("PAGE", juce::dontSendNotification);
            pageCaption.setColour (juce::Label::textColourId, juce::Colour (Theme::Gold::kAccentLight));
            pageCaption.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            addChildComponent (pageCaption);

            addAndMakeVisible (picture);
            soundName.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
            soundName.setColour (juce::Label::textColourId, juce::Colours::white);
            soundName.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (soundName);
            soundInfo.setFont (juce::Font (juce::FontOptions (14.0f)));
            soundInfo.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
            soundInfo.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (soundInfo);

            const std::pair<P, const char*> macros[] = {
                { P::exc_transient, "ATTACK" }, { P::exc_sample, "PUNCH" }, { P::body_mix, "BODY" }, { P::chr_tone, "TONE" },
                { P::tl_mix, "LAYER" },         { P::tl_sub, "DEPTH" },
                { P::vib_depth, "VIBRATO" },    { P::amp_r, "RELEASE" },     { P::rev_mix, "REVERB" }, { P::pb_range, "BEND" },
                { P::out_volume, "VOLUME" } };
            for (const auto& [id, cap] : macros)
                addAndMakeVisible (knobs.add (new SteerKnob (proc, id, cap)));

            rescan();
        }

        /** The user's presets read again (after a save, or when the tab is shown). */
        void rescan()
        {
            const auto files = PresetManager::userPresets();
            std::vector<UserSound> fresh;
            for (const auto& f : files)
            {
                const auto key = f.getFullPathName();
                const auto when = f.getLastModificationTime();
                UserSound us;
                us.file = f;
                if (const auto it = userCache.find (key); it != userCache.end() && it->second.when == when)
                    us = it->second;
                else if (const auto loaded = PresetManager::load (f))
                {
                    us.name = loaded->name.isNotEmpty() ? loaded->name : f.getFileNameWithoutExtension();
                    us.family = (int) familyOf (loaded->values);
                    us.when = when;
                    us.ok = true;
                    userCache[key] = us;
                }
                if (us.ok) fresh.push_back (us);
            }
            std::sort (fresh.begin(), fresh.end(), [] (const UserSound& a, const UserSound& b)
                       { return a.family != b.family ? a.family < b.family : a.name.compareNatural (b.name) < 0; });
            user = std::move (fresh);
            followSound = true;
            refreshColumns();
        }

        /** The sound playing and the archetype now (the editor's timer). */
        void refresh()
        {
            const auto now = proc.presetName + "|" + proc.presetFile.getFullPathName();
            if (now != shown) { shown = now; followSound = true; refreshColumns(); showSound(); }
            const int arch = (int) proc.value (P::archetype);
            if (arch != shownArch)
            {
                shownArch = arch;
                for (auto* k : knobs) k->setEnabled (usesShared ((Archetype) arch, k->id));
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            InstrEditStyle::paintComponentFrame (g, soundFrame, "SOUND");
            InstrEditStyle::paintComponentFrame (g, macroFrame, "MACROS");
            if (colSound.getNumItems() == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.setFont (juce::Font (juce::FontOptions (15.0f)));
                g.drawFittedText (source == 1 ? "No presets of your own yet.\nSAVE in the header keeps the sound playing as one."
                                              : juce::String(),
                                  colSound.getBounds().reduced (10), juce::Justification::centredTop, 3);
            }
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (10, 8);
            macroFrame = area.removeFromBottom (juce::jmin (210, area.getHeight() * 38 / 100));
            area.removeFromBottom (10);

            auto left = area.removeFromLeft (area.getWidth() * 62 / 100);
            area.removeFromLeft (16);
            soundFrame = left;

            auto columns = InstrEditStyle::componentFrameContent (soundFrame).reduced (6, 0);
            colSource.setBounds (columns.removeFromLeft (170));
            columns.removeFromLeft (10);
            colCategory.setBounds (columns.removeFromLeft (290));
            columns.removeFromLeft (10);
            pagerArea = columns.removeFromBottom (32);
            colSound.setBounds (columns);
            pageCaption.setBounds (pagerArea.removeFromLeft (52));
            layoutPageButtons();

            soundInfo.setBounds (area.removeFromBottom (22));
            soundName.setBounds (area.removeFromBottom (32));
            area.removeFromBottom (4);
            picture.setBounds (area);

            auto row = InstrEditStyle::componentFrameContent (macroFrame);
            const int w = row.getWidth() / juce::jmax (1, knobs.size());
            for (auto* k : knobs) k->setBounds (row.removeFromLeft (w).reduced (6, 0));
        }

    private:
        static constexpr int perPage = 8;

        struct UserSound { juce::File file; juce::String name; int family = 0; juce::Time when; bool ok = false; };
        struct Row { juce::String name; int factory = -1; juce::File file; };

        juce::Array<int> categories() const
        {
            juce::Array<int> list;
            if (source == 0)
                for (int f = 0; f < kNumFamilies; ++f) list.add (f);
            else
                for (const auto& u : user) list.addIfNotAlreadyThere (u.family);
            return list;
        }

        std::vector<Row> rows() const
        {
            std::vector<Row> list;
            const auto cats = categories();
            if (! juce::isPositiveAndBelow (category, cats.size())) return list;
            const int fam = cats[category];
            if (source == 0)
            {
                const auto& fac = factoryPresets();
                for (int i = 0; i < (int) fac.size(); ++i)
                    if ((int) familyOf (fac[(size_t) i]) == fam) list.push_back ({ fac[(size_t) i].name, i, {} });
            }
            else
                for (const auto& u : user)
                    if (u.family == fam) list.push_back ({ u.name, -1, u.file });
            return list;
        }

        int pageCount() const { return juce::jmax (1, ((int) rows().size() + perPage - 1) / perPage); }

        /** Where the sound playing lives: its source, its category, its page. */
        void locate()
        {
            const bool mine = proc.presetFile != juce::File();
            int fam = -1;
            if (mine)
            {
                for (const auto& u : user) if (u.file == proc.presetFile) fam = u.family;
                if (fam < 0) return;
                source = 1;
            }
            else
            {
                const auto& fac = factoryPresets();
                for (const auto& f : fac) if (proc.presetName == f.name) fam = (int) familyOf (f);
                if (fam < 0) return;     // a root model, or a sound from a song: the columns stay where they are
                source = 0;
            }
            category = juce::jmax (0, categories().indexOf (fam));
            const auto list = rows();
            for (size_t i = 0; i < list.size(); ++i)
                if (isPlaying (list[i])) page = (int) i / perPage;
        }

        bool isPlaying (const Row& r) const
        {
            return r.factory >= 0 ? (proc.presetFile == juce::File() && proc.presetName == factoryPresets()[(size_t) r.factory].name)
                                  : (r.file == proc.presetFile);
        }

        void refreshColumns()
        {
            if (followSound) { locate(); followSound = false; }
            colSource.setSelected (source);

            const auto cats = categories();
            juce::StringArray catNames;
            for (int f : cats) catNames.add (familyName (f));
            colCategory.setItems (catNames);
            category = cats.isEmpty() ? 0 : juce::jlimit (0, cats.size() - 1, category);
            colCategory.setSelected (cats.isEmpty() ? -1 : category);

            const auto list = rows();
            page = juce::jlimit (0, pageCount() - 1, page);
            juce::StringArray names;
            int sel = -1;
            for (int i = 0; i < perPage; ++i)
            {
                const auto idx = (size_t) (page * perPage + i);
                if (idx >= list.size()) break;
                names.add (list[idx].name);
                if (isPlaying (list[idx])) sel = i;
            }
            colSound.setItems (names);
            colSound.setSelected (sel);
            pageCaption.setVisible (pageCount() > 1);
            layoutPageButtons();
            repaint();
        }

        void layoutPageButtons()
        {
            const int count = pageCount() > 1 ? pageCount() : 0;
            while (pageButtons.size() > count) pageButtons.removeLast();
            while (pageButtons.size() < count)
            {
                const int index = pageButtons.size();
                auto* b = pageButtons.add (new juce::TextButton (juce::String (index + 1)));
                b->onClick = [this, index] { page = index; refreshColumns(); };
                addAndMakeVisible (b);
            }
            auto row = pagerArea;
            for (int i = 0; i < pageButtons.size(); ++i)
            {
                InstrEditStyle::styleSquareButton (*pageButtons[i], i == page);
                pageButtons[i]->setBounds (row.removeFromLeft (36).reduced (2));
                row.removeFromLeft (4);
            }
        }

        void pick (int r)
        {
            const auto list = rows();
            const auto idx = (size_t) (page * perPage + r);
            if (idx >= list.size()) return;
            const auto& row = list[idx];
            if (row.factory >= 0) proc.loadFactory (row.factory);
            else if (! proc.loadPresetFile (row.file)) return;
            if (onLoaded) onLoaded (row.factory, row.file);
            refresh();
        }

        void showSound()
        {
            int fam = -1;
            juce::String where;
            if (proc.presetFile != juce::File())
            {
                for (const auto& u : user) if (u.file == proc.presetFile) fam = u.family;
                where = "MY PRESETS";
            }
            else
            {
                for (const auto& f : factoryPresets()) if (proc.presetName == f.name) fam = (int) familyOf (f);
                where = fam >= 0 ? "FACTORY" : "";
            }
            if (fam < 0)
            {
                std::vector<float> v ((size_t) kNumParams);
                for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = proc.value ((P) i);
                fam = (int) familyOf (v);
            }
            soundName.setText (proc.presetName, juce::dontSendNotification);
            const juce::String arch (archetypeName ((int) proc.value (P::archetype)));
            juce::StringArray parts;
            if (where.isNotEmpty()) parts.add (where);
            parts.add (familyName (fam));
            if (! parts.contains (arch)) parts.add (arch);
            soundInfo.setText (parts.joinIntoString ("   "), juce::dontSendNotification);
            if (fam != shownFamily) { shownFamily = fam; picture.set (familyPicture (fam)); }
        }

        NaturaProcessor& proc;
        SelectorColumn colSource, colCategory, colSound;
        juce::Label pageCaption, soundName, soundInfo;
        juce::OwnedArray<juce::TextButton> pageButtons;
        FamilyPicture picture;
        juce::OwnedArray<SteerKnob> knobs;
        juce::Rectangle<int> soundFrame, macroFrame, pagerArea;

        std::vector<UserSound> user;
        std::map<juce::String, UserSound> userCache;
        int source = 0, category = 0, page = 0, shownArch = -1, shownFamily = -2;
        bool followSound = true;
        juce::String shown;
    };
}
