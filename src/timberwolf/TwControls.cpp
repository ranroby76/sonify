// D:\workspace\Fanan_web_hub\src\timberwolf\TwControls.cpp
#include "TwControls.h"

#include "../Body3D.h"
#include "../Model3D.h"
#include "../Render3D.h"
#include "../SonifyPresets.h"

namespace tw
{
    namespace
    {
        constexpr float lightAngle = 120.0f, lightAltitude = 30.0f;
        constexpr int knobFrames = 61;
        constexpr const char* cacheVersion = "v1";

        Model3D modelOf (Frames::Kind kind)
        {
            switch (kind)
            {
                case Frames::Kind::lever:
                    return Model3D::preset (Model3D::Kind::toggle, 0);   // chrome lever

                case Frames::Kind::footswitch:
                    return Model3D::preset (Model3D::Kind::button, 2);   // round chrome

                case Frames::Kind::knob:
                case Frames::Kind::body:
                default:
                {
                    // A chrome cap on a black knurled body, its light ring amber: the value lit round it.
                    auto m = Model3D::preset (Model3D::Kind::knob, 2);
                    m.set ("ring", true);
                    m.set ("ringColour", "#FFFF8A2A");
                    m.set ("ringSegments", 0);
                    return m;
                }
            }
        }

        int frameCount (Frames::Kind kind)
        {
            return kind == Frames::Kind::knob ? knobFrames : (kind == Frames::Kind::body ? 1 : 2);
        }
    }

    //==========================================================================
    struct Frames::Job final : public juce::ThreadPoolJob
    {
        Job (Frames& o, juce::String k, Kind kd, int ww, int hh, float sc)
            : ThreadPoolJob ("Timber Wolf 3D"), owner (o), key (std::move (k)), kind (kd), w (ww), h (hh), scale (sc) {}

        JobStatus runJob() override
        {
            std::vector<juce::Image> frames;
            const auto cached = Frames::cacheFolder().getChildFile (key + ".png");

            // On disk from an earlier time: a strip of frames, one under the other.
            if (cached.existsAsFile())
            {
                const auto strip = juce::ImageFileFormat::loadFrom (cached);
                const int count = frameCount (kind);

                if (strip.isValid() && strip.getHeight() % count == 0)
                {
                    const int fh = strip.getHeight() / count;

                    for (int i = 0; i < count; ++i)
                        frames.push_back (strip.getClippedImage ({ 0, i * fh, strip.getWidth(), fh }).createCopy());
                }
            }

            if (frames.empty())
            {
                if (kind == Kind::body)
                {
                    if (auto body = Body3D::fromVar (Frames::bodyJson (w, h), w, h))
                        frames.push_back (Render3D::renderBody (*body, scale, lightAngle, lightAltitude, 0.0f));
                }
                else
                {
                    const auto model = modelOf (kind);
                    const float pad = std::ceil (Render3D::overhang (model, w, h, lightAltitude) * scale);
                    const int count = frameCount (kind);

                    for (int i = 0; i < count && ! shouldExit(); ++i)
                    {
                        const float t = count > 1 ? (float) i / (float) (count - 1) : 0.0f;
                        frames.push_back (Render3D::renderFrame (model, w, h, t, scale, pad, lightAngle, lightAltitude));
                    }
                }

                if (shouldExit() || frames.empty() || ! frames.front().isValid())
                    return jobHasFinished;

                // Kept for the next time.
                const int fw = frames.front().getWidth(), fh = frames.front().getHeight();
                juce::Image strip (juce::Image::ARGB, fw, fh * (int) frames.size(), true);

                {
                    juce::Graphics g (strip);
                    for (size_t i = 0; i < frames.size(); ++i)
                        g.drawImageAt (frames[i], 0, (int) i * fh);
                }

                Frames::cacheFolder().createDirectory();
                juce::PNGImageFormat png;
                juce::FileOutputStream out (cached);

                if (out.openedOk())
                {
                    out.setPosition (0);
                    out.truncate();
                    png.writeImageToStream (strip, out);
                }
            }

            auto* self = &owner;
            auto k = key;
            juce::MessageManager::callAsync ([self, k, frames]() mutable { self->done (k, std::move (frames)); });
            return jobHasFinished;
        }

        Frames& owner;
        juce::String key;
        Kind kind;
        int w, h;
        float scale;
    };

    //==========================================================================
    Frames& Frames::get()
    {
        static Frames f;
        return f;
    }

    Frames::Frames() = default;

    Frames::~Frames()
    {
        pool.removeAllJobs (true, 4000);
    }

    juce::File Frames::cacheFolder()
    {
        return sonify::presets::settingsFolder().getChildFile ("Timber Wolf").getChildFile ("cache");
    }

    juce::String Frames::keyOf (Kind kind, int w, int h, float scale) const
    {
        return juce::String (cacheVersion) + "_" + juce::String ((int) kind) + "_" + juce::String (w) + "x" + juce::String (h)
               + "_" + juce::String (juce::roundToInt (scale * 100.0f));
    }

    const std::vector<juce::Image>* Frames::find (Kind kind, int w, int h, float scale, juce::Component* who)
    {
        w = juce::jmax (8, w);
        h = juce::jmax (8, h);
        scale = juce::jlimit (0.5f, 4.0f, std::round (scale * 4.0f) / 4.0f);
        const auto key = keyOf (kind, w, h, scale);

        if (auto it = made.find (key); it != made.end())
            return &it->second;

        if (who != nullptr)
            waiting[key].push_back (who);

        if (making.insert (key).second)
            pool.addJob (new Job (*this, key, kind, w, h, scale), true);

        return nullptr;
    }

    void Frames::done (const juce::String& key, std::vector<juce::Image> frames)
    {
        making.erase (key);
        made[key] = std::move (frames);

        for (auto& c : waiting[key])
            if (c != nullptr)
                c->repaint();

        waiting.erase (key);
    }

    float Frames::padOf (Kind kind, int size) const
    {
        if (kind == Kind::body)
            return 0.0f;

        static std::map<int, float> knob, lever, foot;
        auto& table = kind == Kind::knob ? knob : (kind == Kind::lever ? lever : foot);

        if (auto it = table.find (size); it != table.end())
            return it->second;

        const float pad = Render3D::overhang (modelOf (kind), size, size, lightAltitude);
        table[size] = pad;
        return pad;
    }

    juce::var Frames::bodyJson (int w, int h)
    {
        // The unit: a dark anodised case, the screen's glass in its bezel up top, a brushed deck
        // for the controls, the left panel's plate, and the rubber foot strip along the front.
        const float W = (float) w, H = (float) h;
        const float screenX = 236.0f, screenW = W - screenX - 14.0f, screenTop = 16.0f, screenH = H * 0.40f;
        const float deckTop = screenTop + screenH + 12.0f, deckH = H * 0.33f;
        const float footTop = deckTop + deckH + 10.0f, footH = H - footTop - 10.0f;

        const auto box = [] (const juce::String& name, float x, float z, float y, float bw, float bh, float bd, float round,
                             const juce::String& material, const juce::String& colour)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", name);
            o->setProperty ("kind", "box");
            o->setProperty ("x", x); o->setProperty ("z", z); o->setProperty ("y", y);
            o->setProperty ("w", bw); o->setProperty ("h", bh); o->setProperty ("d", bd);
            o->setProperty ("round", round);
            o->setProperty ("material", material);
            o->setProperty ("color", colour);
            return o;
        };

        juce::Array<juce::var> parts;
        parts.add (juce::var (box ("Case", W * 0.5f, H * 0.5f, 0.0f, W - 4.0f, 34.0f, H - 4.0f, 20.0f, "Painted metal", "#FF18191D")));

        auto* chamfer = box ("Top edge", W * 0.5f, H * 0.5f, 30.0f, W - 18.0f, 4.0f, H - 18.0f, 14.0f, "Brushed aluminium", "#FF2C2F35");
        chamfer->setProperty ("surface", "brushed");
        parts.add (juce::var (chamfer));

        parts.add (juce::var (box ("Left plate", 118.0f, (screenTop + footTop) * 0.5f, 34.0f, 212.0f, 3.0f, footTop - screenTop - 8.0f, 10.0f,
                                   "Painted metal", "#FF202227")));

        parts.add (juce::var (box ("Screen bezel", screenX + screenW * 0.5f, screenTop + screenH * 0.5f, 34.0f, screenW, 4.0f, screenH, 12.0f,
                                   "Gloss plastic", "#FF08090B")));

        auto* glass = box ("Screen", screenX + screenW * 0.5f, screenTop + screenH * 0.5f, 36.0f, screenW - 16.0f, 2.0f, screenH - 16.0f, 7.0f,
                           "Gloss plastic", "#FF070B12");
        glass->setProperty ("glass", 35);
        glass->setProperty ("glow", "#FF0E1A2C");
        glass->setProperty ("glowStrength", 25);
        parts.add (juce::var (glass));

        auto* deck = box ("Deck", screenX + screenW * 0.5f, deckTop + deckH * 0.5f, 34.0f, screenW, 3.0f, deckH, 10.0f, "Brushed aluminium", "#FF33363D");
        deck->setProperty ("surface", "brushed");
        deck->setProperty ("surfaceScale", 1.5f);
        parts.add (juce::var (deck));

        auto* foot = box ("Foot strip", W * 0.5f, footTop + footH * 0.5f, 34.0f, W - 28.0f, 2.0f, footH, 10.0f, "Rubber", "#FF121316");
        foot->setProperty ("surface", "rubber");
        parts.add (juce::var (foot));

        auto* root = new juce::DynamicObject();
        root->setProperty ("tilt", 0);
        root->setProperty ("shadow", 0);
        root->setProperty ("softness", 55);
        root->setProperty ("floor", "#FF0B0C0E");
        root->setProperty ("parts", parts);
        return juce::var (root);
    }

    //==========================================================================
    //  Knob
    //==========================================================================
    KnobView::KnobView (Binding b) : binding (std::move (b))
    {
        setRepaintsOnMouseActivity (true);
    }

    juce::Rectangle<int> KnobView::knobArea() const
    {
        auto r = getLocalBounds().reduced (2);
        r.removeFromTop (16);
        r.removeFromBottom (20);
        const int s = juce::jmin (r.getWidth(), r.getHeight());
        return r.withSizeKeepingCentre (s, s);
    }

    void KnobView::paint (juce::Graphics& g)
    {
        const auto area = knobArea();
        const float v = binding.get ? binding.get() : 0.0f;

        g.setColour (look::inkDim);
        g.setFont (look::font (11.5f, true));
        g.drawFittedText (binding.name.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centred, 1);

        const float scale = juce::jmax (1.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto* frames = Frames::get().find (Frames::Kind::knob, area.getWidth(), area.getHeight(), scale, this);

        if (frames != nullptr && ! frames->empty())
        {
            const int index = juce::jlimit (0, (int) frames->size() - 1, juce::roundToInt (v * (float) (frames->size() - 1)));
            const auto& img = (*frames)[(size_t) index];
            const float pad = Frames::get().padOf (Frames::Kind::knob, area.getWidth());
            const auto dest = area.toFloat().expanded (pad);
            g.setOpacity (1.0f);
            g.drawImage (img, dest, juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            // A stand-in while its frames are made: a dark knob with its arc.
            const auto c = area.toFloat().reduced (area.getWidth() * 0.12f);
            g.setColour (juce::Colour (0xff15161a));
            g.fillEllipse (c);
            g.setColour (juce::Colour (0xff8c9099));
            g.fillEllipse (c.reduced (c.getWidth() * 0.18f));
            juce::Path arc;
            const float a0 = -2.4f, a1 = a0 + 4.8f * v;
            arc.addCentredArc (c.getCentreX(), c.getCentreY(), c.getWidth() * 0.56f, c.getWidth() * 0.56f, 0.0f, a0, a1, true);
            g.setColour (accent);
            g.strokePath (arc, juce::PathStrokeType (2.5f));
        }

        // The value, in a little display under it.
        auto value = getLocalBounds().removeFromBottom (19).reduced (getWidth() > 70 ? 6 : 2, 1).toFloat();
        g.setColour (juce::Colour (0xff0a0c10));
        g.fillRoundedRectangle (value, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (value, 4.0f, 1.0f);
        g.setColour (isMouseOverOrDragging() ? look::accentHot : look::accent);
        g.setFont (look::font (12.0f));
        g.drawFittedText (binding.text ? binding.text() : juce::String(), value.toNearestInt(), juce::Justification::centred, 1);
    }

    void KnobView::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            return;

        startValue = binding.get ? binding.get() : 0.0f;
        dragging = true;
        if (binding.begin) binding.begin();
    }

    void KnobView::mouseDrag (const juce::MouseEvent& e)
    {
        if (! dragging || ! binding.set)
            return;

        const float sensitivity = e.mods.isShiftDown() ? 0.0012f : 0.0055f;
        float v = juce::jlimit (0.0f, 1.0f, startValue - (float) e.getDistanceFromDragStartY() * sensitivity
                                                 + (float) e.getDistanceFromDragStartX() * sensitivity * 0.5f);

        if (binding.steps > 1)
            v = std::round (v * (float) (binding.steps - 1)) / (float) (binding.steps - 1);

        binding.set (v);
        repaint();
    }

    void KnobView::mouseUp (const juce::MouseEvent&)
    {
        if (dragging && binding.end)
            binding.end();

        dragging = false;
    }

    void KnobView::mouseDoubleClick (const juce::MouseEvent&)
    {
        if (binding.reset)
            binding.reset();

        repaint();
    }

    void KnobView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        if (! binding.set || ! binding.get)
            return;

        const float step = binding.steps > 1 ? 1.0f / (float) (binding.steps - 1) : (e.mods.isShiftDown() ? 0.002f : 0.01f);
        const float dir = (w.deltaY + w.deltaX) > 0.0f ? 1.0f : -1.0f;

        if (binding.begin) binding.begin();
        binding.set (juce::jlimit (0.0f, 1.0f, binding.get() + dir * step));
        if (binding.end) binding.end();
        repaint();
    }

    //==========================================================================
    //  Lever
    //==========================================================================
    LeverView::LeverView (Binding b) : binding (std::move (b)) {}

    void LeverView::paint (juce::Graphics& g)
    {
        const bool on = binding.get && binding.get() > 0.5f;

        g.setColour (look::inkDim);
        g.setFont (look::font (11.5f, true));
        g.drawFittedText (binding.name.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centred, 1);

        auto r = getLocalBounds().reduced (2);
        r.removeFromTop (16);
        r.removeFromBottom (20);
        const int s = juce::jmin (r.getWidth(), r.getHeight());
        const auto area = r.withSizeKeepingCentre (s * 3 / 4, s);

        const float scale = juce::jmax (1.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto* frames = Frames::get().find (Frames::Kind::lever, area.getWidth(), area.getHeight(), scale, this);

        if (frames != nullptr && frames->size() >= 2)
        {
            const float pad = Frames::get().padOf (Frames::Kind::lever, area.getWidth());
            g.setOpacity (1.0f);
            g.drawImage ((*frames)[on ? 1 : 0], area.toFloat().expanded (pad), juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            g.setColour (juce::Colour (0xff15161a));
            g.fillRoundedRectangle (area.toFloat().reduced (area.getWidth() * 0.25f, area.getHeight() * 0.3f), 3.0f);
            g.setColour (juce::Colour (0xffc8ccd2));
            const auto c = area.getCentre().toFloat();
            g.drawLine (c.x, c.y, c.x, c.y + (on ? -1.0f : 1.0f) * area.getHeight() * 0.4f, 4.0f);
        }

        auto value = getLocalBounds().removeFromBottom (19).reduced (getWidth() > 70 ? 6 : 2, 1).toFloat();
        g.setColour (juce::Colour (0xff0a0c10));
        g.fillRoundedRectangle (value, 4.0f);
        g.setColour (on ? look::accent : look::inkDim);
        g.setFont (look::font (12.0f));
        g.drawFittedText (binding.text ? binding.text() : juce::String(), value.toNearestInt(), juce::Justification::centred, 1);
    }

    void LeverView::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu() || ! binding.set || ! binding.get)
            return;

        if (binding.begin) binding.begin();
        binding.set (binding.get() > 0.5f ? 0.0f : 1.0f);
        if (binding.end) binding.end();
        repaint();
    }

    //==========================================================================
    //  Footswitch
    //==========================================================================
    FootswitchView::FootswitchView()   { setRepaintsOnMouseActivity (true); }

    void FootswitchView::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds();

        // Its light.
        auto ledRow = r.removeFromTop (16);
        const auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre (ledRow.getCentre().toFloat());

        if (lit)
        {
            juce::DropShadow (light.withAlpha (0.9f), 10, {}).drawForRectangle (g, led.toNearestInt());
            g.setColour (light);
        }
        else
        {
            g.setColour (light.withMultipliedBrightness (0.22f).withMultipliedSaturation (0.6f));
        }

        g.fillEllipse (led);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.7f : 0.15f));
        g.fillEllipse (led.reduced (2.5f).translated (-1.0f, -1.0f).withSizeKeepingCentre (3.0f, 3.0f));

        // The switch.
        auto strip = r.removeFromBottom (34);
        const int s = juce::jmin (r.getWidth(), r.getHeight()) - 4;
        const auto area = r.withSizeKeepingCentre (s, s);
        const float scale = juce::jmax (1.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto* frames = Frames::get().find (Frames::Kind::footswitch, area.getWidth(), area.getHeight(), scale, this);

        if (frames != nullptr && frames->size() >= 2)
        {
            const float pad = Frames::get().padOf (Frames::Kind::footswitch, area.getWidth());
            g.setOpacity (1.0f);   // drawImage takes the current colour's alpha: the LED's left it dim
            g.drawImage ((*frames)[down ? 1 : 0], area.toFloat().expanded (pad), juce::RectanglePlacement::stretchToFit);
        }
        else
        {
            const auto c = area.toFloat().reduced (s * 0.1f);
            g.setColour (juce::Colour (0xff0d0e10));
            g.fillEllipse (c.expanded (3.0f));
            juce::ColourGradient chrome (juce::Colour (0xffeef1f5), c.getX(), c.getY(), juce::Colour (0xff5f646c), c.getRight(), c.getBottom(), false);
            g.setGradientFill (chrome);
            g.fillEllipse (down ? c.reduced (2.0f) : c);
        }

        // Its label strip: the job, and the MIDI it answers.
        const auto box = strip.reduced (4, 2).toFloat();
        g.setColour (juce::Colour (0xff0a0c10));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (learning ? look::accent : juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (box, 4.0f, learning ? 1.5f : 1.0f);

        auto text = box.toNearestInt().reduced (4, 1);
        g.setColour (look::ink);
        g.setFont (look::font (12.0f, true));
        g.drawFittedText (learning ? juce::String ("Press a pedal...") : label, text.removeFromTop (16), juce::Justification::centred, 1);
        g.setColour (look::inkFaint);
        g.setFont (look::font (10.0f));
        g.drawFittedText (number + (midi.isNotEmpty() && midi != "-" ? "  " + midi : juce::String()), text, juce::Justification::centred, 1);
    }

    void FootswitchView::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            // A right double-click learns MIDI (as Grex's pads do); one right-click opens the menu.
            const auto now = juce::Time::getMillisecondCounter();

            if (now - lastRightClick < 350)
            {
                lastRightClick = 0;
                if (onLearn) onLearn();
                return;
            }

            lastRightClick = now;
            auto safe = juce::Component::SafePointer<FootswitchView> (this);

            juce::Timer::callAfterDelay (360, [safe]
            {
                if (safe != nullptr && safe->lastRightClick != 0 && juce::Time::getMillisecondCounter() - safe->lastRightClick >= 350)
                {
                    safe->lastRightClick = 0;
                    if (safe->onMenu) safe->onMenu();
                }
            });

            return;
        }

        down = true;
        repaint();
        if (onPress) onPress();
    }

    void FootswitchView::mouseUp (const juce::MouseEvent&)
    {
        down = false;
        repaint();
    }

    void FootswitchView::mouseDoubleClick (const juce::MouseEvent&) {}

    //==========================================================================
    //  Expression pedal
    //==========================================================================
    PedalView::PedalView()   { setRepaintsOnMouseActivity (true); }

    void PedalView::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().reduced (4);
        const float pos = get ? get() : 0.0f;

        g.setColour (look::inkDim);
        g.setFont (look::font (11.5f, true));
        g.drawFittedText (learning ? juce::String ("Move a pedal...") : label, r.removeFromTop (16), juce::Justification::centred, 1);

        auto bottom = r.removeFromBottom (30);
        auto body = r.reduced (juce::jmax (0, (r.getWidth() - 54) / 2), 2).toFloat();

        // The base, and the treadle rocked by the position: heel down at 0, toe down at 1.
        g.setColour (juce::Colour (0xff0c0d10));
        g.fillRoundedRectangle (body.expanded (3.0f), 8.0f);

        const float tilt = (pos - 0.5f) * 0.18f;
        auto treadle = body.reduced (3.0f);
        juce::Path p;
        p.addRoundedRectangle (treadle, 6.0f);
        p.applyTransform (juce::AffineTransform::scale (1.0f, 1.0f - std::abs (tilt), treadle.getCentreX(), treadle.getCentreY()));

        juce::ColourGradient face (juce::Colour (0xff3a3d44).brighter (pos * 0.3f), treadle.getCentreX(), treadle.getY(),
                                   juce::Colour (0xff1b1d21), treadle.getCentreX(), treadle.getBottom(), false);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 8, { 0, 3 }).drawForPath (g, p);
        g.setGradientFill (face);
        g.fillPath (p);

        // The rubber grip.
        g.setColour (juce::Colour (0xff111214));
        const auto grip = treadle.reduced (7.0f, 12.0f);
        for (float y = grip.getY(); y < grip.getBottom(); y += 6.0f)
            g.fillRoundedRectangle (grip.getX(), y, grip.getWidth(), 3.0f, 1.5f);

        // Its travel, lit on the side.
        const auto bar = juce::Rectangle<float> (4.0f, body.getHeight()).withPosition (body.getRight() + 6.0f, body.getY());
        g.setColour (juce::Colour (0xff0a0c10));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (look::accent);
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * pos), 2.0f);

        auto box = bottom.reduced (2, 2).toFloat();
        g.setColour (juce::Colour (0xff0a0c10));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (learning ? look::accent : juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (look::ink);
        g.setFont (look::font (10.5f, true));
        auto t = box.toNearestInt().reduced (3, 1);
        g.drawFittedText (target, t.removeFromTop (14), juce::Justification::centred, 1);
        g.setColour (look::inkFaint);
        g.setFont (look::font (9.5f));
        g.drawFittedText (juce::String (juce::roundToInt (pos * 100.0f)) + " %" + (midi.isNotEmpty() && midi != "-" ? "  " + midi : juce::String()),
                          t, juce::Justification::centred, 1);
    }

    void PedalView::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            const auto now = juce::Time::getMillisecondCounter();

            if (now - lastRightClick < 350)
            {
                lastRightClick = 0;
                if (onLearn) onLearn();
                return;
            }

            lastRightClick = now;
            auto safe = juce::Component::SafePointer<PedalView> (this);

            juce::Timer::callAfterDelay (360, [safe]
            {
                if (safe != nullptr && safe->lastRightClick != 0 && juce::Time::getMillisecondCounter() - safe->lastRightClick >= 350)
                {
                    safe->lastRightClick = 0;
                    if (safe->onMenu) safe->onMenu();
                }
            });

            return;
        }

        mouseDrag (e);
    }

    void PedalView::mouseDrag (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu() || ! set)
            return;

        auto r = getLocalBounds().reduced (4);
        r.removeFromTop (16);
        r.removeFromBottom (30);
        set (juce::jlimit (0.0f, 1.0f, 1.0f - (float) (e.y - r.getY()) / (float) juce::jmax (1, r.getHeight())));
        repaint();
    }

    void PedalView::mouseUp (const juce::MouseEvent&) {}
}
