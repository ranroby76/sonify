// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakPanels.cpp
#include "ZodiakPanels.h"
#include "ZodiakLinkUi.h"

namespace zodiak::ui
{
    juce::String chanText (int param, float v)
    {
        const int i = (int) std::lround (v);
        switch (param)
        {
            case cpTranspose:   return (i > 0 ? "+" : "") + juce::String (i) + " st";
            case cpOctave:      return i == 0 ? juce::String ("0") : (i > 0 ? "+" : "") + juce::String (i) + " oct";
            case cpPan:         return std::abs (v) < 0.01f ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String ((int) std::lround (std::abs (v) * 100.0f));
            default: break;
        }
        const auto& pi = chanParamInfo (param);
        if (juce::String (pi.unit).isNotEmpty()) return valueText (v, pi.unit);
        return juce::String ((int) std::lround (v * 100.0f));
    }

    void bindKnob (ZodiakKnob& k, std::atomic<float>& value, float min, float max, float def, float skew, bool stepped,
                   std::function<juce::String (float)> text)
    {
        k.getNorm = [&value, min, max, skew] { return toNorm (value.load(), min, max, skew); };
        k.setNorm = [&value, min, max, skew, stepped] (float n)
        {
            float v = fromNorm (n, min, max, skew);
            if (stepped) v = std::round (v);
            value.store (v);
        };
        k.textFor = [min, max, skew, text] (float n) { return text (fromNorm (n, min, max, skew)); };
        k.rest    = toNorm (def, min, max, skew);
        k.bipolar = false;
        k.refresh();
    }

    //==========================================================================
    //  InsertPanel: a built-in VST FX's knobs
    //==========================================================================
    InsertPanel::InsertPanel (inserts::BuiltIn& insert, const ChannelColours& colours) : fx (insert)
    {
        const int k = fx.getKind();
        for (int i = 0; i < inserts::numParams (k); ++i)
        {
            const auto& ps = inserts::param (k, i);
            auto knob = std::make_unique<ZodiakKnob>();
            knob->name = ps.name;
            knob->colours = &colours;
            knob->nameHeight = 18.0f;
            const juce::String unit (ps.unit);
            bindKnob (*knob, fx.value (i), ps.min, ps.max, ps.def, ps.skew, false, [unit] (float v) { return valueText (v, unit); });
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        }
        setSize (juce::jmax (300, 96 * (int) knobs.size() + 40), 150);
        startTimerHz (20);
    }

    InsertPanel::~InsertPanel()   { stopTimer(); }

    void InsertPanel::paint (juce::Graphics& g)   { g.fillAll (win::bg); }

    void InsertPanel::resized()
    {
        auto r = getLocalBounds().reduced (20, 16);
        for (auto& k : knobs) k->setBounds (r.removeFromLeft (96).reduced (6, 0));
    }

    void InsertPanel::timerCallback()   { for (auto& k : knobs) k->refresh(); }

    //==========================================================================
    //  DynEqPanel: Grex's DUCKER page, in Zodiak's window
    //==========================================================================
    DynEqPanel::DynEqPanel (ZBetel::StyleDucker& d, juce::String keyedBy) : ducker (d), curve (d)
    {
        onBtn.setColour (juce::ToggleButton::textColourId, win::text);
        onBtn.onClick = [this] { ducker.setEnabled (onBtn.getToggleState()); curve.refresh(); };
        addAndMakeVisible (onBtn);

        keyLbl.setText ("Keyed by: " + (keyedBy.isNotEmpty() ? keyedBy : juce::String ("no channel yet (S.C. SENDS on another strip)")),
                        juce::dontSendNotification);
        keyLbl.setColour (juce::Label::textColourId, win::label);
        keyLbl.setFont (font (13.0f));
        addAndMakeVisible (keyLbl);

        hint.setText ("Drag a point: across = frequency, down = how deep it may duck. Double-click a point to zero it.",
                      juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, win::dim);
        hint.setFont (font (12.0f));
        addAndMakeVisible (hint);

        curve.onPointMoved = [this] (int, float, float) { refreshFromDucker(); };
        addAndMakeVisible (curve);

        for (int b = 0; b < ZBetel::StyleDucker::kNumBands; ++b)
        {
            auto& c = cols[(size_t) b];
            c.on.setButtonText ("Point " + juce::String (b + 1));
            c.on.setColour (juce::ToggleButton::textColourId, win::text);
            c.on.onClick = [this, b] { ducker.setBandEnabled (b, cols[(size_t) b].on.getToggleState()); curve.refresh(); };
            addAndMakeVisible (c.on);

            setupRow (c.freq,    "Freq",    ZBetel::StyleDucker::kMinFreqHz, ZBetel::StyleDucker::kMaxFreqHz, 1.0, 600.0,
                      [this, b] (float v) { ducker.setFreq (b, v); }, " Hz");
            setupRow (c.q,       "Q",       ZBetel::StyleDucker::kMinQ, ZBetel::StyleDucker::kMaxQ, 0.05, 1.5,
                      [this, b] (float v) { ducker.setQ (b, v); }, "");
            setupRow (c.depth,   "Depth",   0.0, ZBetel::StyleDucker::kMaxDepthDb, 0.5, -1.0,
                      [this, b] (float v) { ducker.setDepthDb (b, v); }, " dB");
            setupRow (c.thresh,  "Thresh",  ZBetel::StyleDucker::kMinThreshDb, ZBetel::StyleDucker::kMaxThreshDb, 0.5, -1.0,
                      [this, b] (float v) { ducker.setThresholdDb (b, v); }, " dB");
            setupRow (c.ratio,   "Ratio",   ZBetel::StyleDucker::kMinRatio, ZBetel::StyleDucker::kMaxRatio, 0.1, 4.0,
                      [this, b] (float v) { ducker.setRatio (b, v); }, ":1");
            setupRow (c.attack,  "Attack",  ZBetel::StyleDucker::kMinAttackMs, ZBetel::StyleDucker::kMaxAttackMs, 0.5, 20.0,
                      [this, b] (float v) { ducker.setAttackMs (b, v); }, " ms");
            setupRow (c.release, "Release", ZBetel::StyleDucker::kMinRelMs, ZBetel::StyleDucker::kMaxRelMs, 5.0, 200.0,
                      [this, b] (float v) { ducker.setReleaseMs (b, v); }, " ms");
        }

        refreshFromDucker();
        setSize (760, 600);
        startTimerHz (20);
    }

    DynEqPanel::~DynEqPanel()   { stopTimer(); }

    void DynEqPanel::setupRow (Row& r, const juce::String& name, double lo, double hi, double step, double skewMid,
                               std::function<void (float)> apply, const juce::String& suffix)
    {
        r.name.setText (name, juce::dontSendNotification);
        r.name.setColour (juce::Label::textColourId, win::label);
        r.name.setFont (font (12.0f));
        addAndMakeVisible (r.name);

        r.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        r.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 18);
        r.slider.setRange (lo, hi, step);
        if (skewMid > lo && skewMid < hi) r.slider.setSkewFactorFromMidPoint (skewMid);
        r.slider.setTextValueSuffix (suffix);
        r.slider.setColour (juce::Slider::trackColourId, juce::Colour (ZBetel::Pal::kAccent));
        r.slider.setColour (juce::Slider::backgroundColourId, win::well);
        r.slider.setColour (juce::Slider::thumbColourId, juce::Colour (ZBetel::Pal::kAccentBright));
        r.slider.setColour (juce::Slider::textBoxTextColourId, win::text);
        r.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        r.slider.onValueChange = [this, &r, apply]
        {
            if (seeding) return;
            apply ((float) r.slider.getValue());
            curve.refresh();
        };
        addAndMakeVisible (r.slider);
    }

    void DynEqPanel::refreshFromDucker()
    {
        seeding = true;
        onBtn.setToggleState (ducker.isEnabled(), juce::dontSendNotification);
        for (int b = 0; b < ZBetel::StyleDucker::kNumBands; ++b)
        {
            auto& c = cols[(size_t) b];
            c.on.setToggleState (ducker.isBandEnabled (b), juce::dontSendNotification);
            c.freq.slider.setValue (ducker.getFreq (b), juce::dontSendNotification);
            c.q.slider.setValue (ducker.getQ (b), juce::dontSendNotification);
            c.depth.slider.setValue (ducker.getDepthDb (b), juce::dontSendNotification);
            c.thresh.slider.setValue (ducker.getThresholdDb (b), juce::dontSendNotification);
            c.ratio.slider.setValue (ducker.getRatio (b), juce::dontSendNotification);
            c.attack.slider.setValue (ducker.getAttackMs (b), juce::dontSendNotification);
            c.release.slider.setValue (ducker.getReleaseMs (b), juce::dontSendNotification);
        }
        seeding = false;
        curve.refresh();
    }

    void DynEqPanel::paint (juce::Graphics& g)
    {
        g.fillAll (win::bg);
        for (int b = 0; b < ZBetel::StyleDucker::kNumBands; ++b) paintKeyMeter (g, b);
    }

    // What the keying channels deliver to this point's detector, against the
    // threshold it has to beat (Grex's KEY meter)
    void DynEqPanel::paintKeyMeter (juce::Graphics& g, int b)
    {
        const auto area = cols[(size_t) b].keyArea;
        if (area.isEmpty()) return;

        g.setColour (win::label);
        g.setFont (font (12.0f));
        g.drawText ("Key", area.withWidth (34.0f), juce::Justification::centredLeft, false);

        auto bar = area.withTrimmedLeft (36.0f).withTrimmedRight (56.0f).reduced (0.0f, 5.0f);
        g.setColour (win::well);
        g.fillRect (bar);

        const float lvl = ducker.getSidechainLevelDb (b), thr = ducker.getThresholdDb (b);
        const float nLvl = juce::jlimit (0.0f, 1.0f, (lvl + 60.0f) / 60.0f), nThr = juce::jlimit (0.0f, 1.0f, (thr + 60.0f) / 60.0f);
        const bool over = lvl > thr;
        g.setColour (over ? juce::Colour (ZBetel::Pal::kAccent) : juce::Colour (0xFF3A5A3A));
        g.fillRect (bar.withWidth (bar.getWidth() * nLvl));
        const float tx = bar.getX() + bar.getWidth() * nThr;
        g.setColour (juce::Colour (ZBetel::Pal::kAccentBright));
        g.drawLine (tx, bar.getY() - 2.0f, tx, bar.getBottom() + 2.0f, 1.6f);
        g.setColour (over ? juce::Colour (ZBetel::Pal::kAccent) : win::dim);
        g.drawText (lvl <= -119.0f ? juce::String ("--") : juce::String ((int) lvl) + " dB",
                    area.withTrimmedLeft (area.getWidth() - 54.0f), juce::Justification::centredRight, false);
    }

    void DynEqPanel::resized()
    {
        auto r = getLocalBounds().reduced (16, 12);
        auto top = r.removeFromTop (28);
        onBtn.setBounds (top.removeFromLeft (130));
        top.removeFromLeft (12);
        keyLbl.setBounds (top);
        r.removeFromTop (6);
        curve.setBounds (r.removeFromTop (220));
        hint.setBounds (r.removeFromTop (22));
        r.removeFromTop (6);

        const int colW = (r.getWidth() - 20) / ZBetel::StyleDucker::kNumBands;
        for (int b = 0; b < ZBetel::StyleDucker::kNumBands; ++b)
        {
            auto c = r.removeFromLeft (colW);
            if (b == 0) r.removeFromLeft (20);
            auto& col = cols[(size_t) b];
            col.on.setBounds (c.removeFromTop (24));
            for (auto* row : { &col.freq, &col.q, &col.depth, &col.thresh, &col.ratio, &col.attack, &col.release })
            {
                auto line = c.removeFromTop (26);
                row->name.setBounds (line.removeFromLeft (58));
                row->slider.setBounds (line);
            }
            col.keyArea = c.removeFromTop (26).toFloat();
        }
    }

    void DynEqPanel::timerCallback()
    {
        onBtn.setToggleState (ducker.isEnabled(), juce::dontSendNotification);
        repaint();
    }

}
