// C:\workspace\SONIFY PLUGIN zax\src\ZaxKeyboard.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include "ZaxKit.h"
#include "ZaxSliders.h"

//==============================================================================
//  ZaxKeyboard.h  —  Zax's big keyboard, GM keys 27..84.
//
//  Every key carries its GM name on an arrow:
//      WHITE keys  the arrow leaves the BOTTOM of the key and points down to
//                  the name written under the keyboard
//      BLACK keys  the arrow leaves the TOP of the key and points up to the
//                  name written above the keyboard
//  Neighbouring names sit on two staggered rows, so no arrow ever runs through
//  another key's name.  Under each name: the key number.  Inside each key, up
//  its length, the sound it plays now, in its family's colour.
//
//  Click a key: it plays (lower on the key = harder) and opens in the editor.
//  A key lights while it sounds - the MIDI truth, whether it came from the
//  host or from a click.
//==============================================================================

class ZaxKeyboard : public juce::Component
{
public:
    std::function<void (int key, int velocity)> onPress;
    std::function<void (int key)>               onRelease;
    std::function<void (int key)>               onSelect;

    static constexpr int kLabelBand = 112;      // above and below the keys: arrows + two rows of names
    static constexpr int kRowH      = 40;       // one row of names

    void setKit (const zax::Kit* k)              { kit = k; repaint(); }
    void setSelected (int key)                   { if (key != selected) { selected = key; repaint(); } }

    void setActivity (const std::array<uint32_t, 4>& m)
    {
        if (m != activity) { activity = m; repaint(); }
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        const auto keys = keysArea();

        // White keys first, black keys over them
        for (int k = zax::kFirstKey; k <= zax::kLastKey; ++k)
            if (! zax::isBlackKey (k)) paintKey (g, k);
        for (int k = zax::kFirstKey; k <= zax::kLastKey; ++k)
            if (zax::isBlackKey (k)) paintKey (g, k);

        // The names and their arrows
        for (int k = zax::kFirstKey; k <= zax::kLastKey; ++k)
            paintLabel (g, k, keys);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int k = keyAt (e.position);
        if (k < 0) return;
        const auto r = keyRect (k);
        const float depth = juce::jlimit (0.0f, 1.0f, (e.position.y - r.getY()) / juce::jmax (1.0f, r.getHeight()));
        const int vel = juce::jlimit (1, 127, (int) std::lround (40.0f + 87.0f * depth));
        pressed = k;
        if (onSelect) onSelect (k);
        if (onPress)  onPress (k, vel);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (pressed >= 0 && onRelease) onRelease (pressed);
        pressed = -1;
    }

private:
    //==========================================================================
    int numWhites() const noexcept
    {
        int n = 0;
        for (int k = zax::kFirstKey; k <= zax::kLastKey; ++k)
            if (! zax::isBlackKey (k)) ++n;
        return n;
    }

    juce::Rectangle<float> keysArea() const
    {
        return getLocalBounds().toFloat().withTrimmedTop ((float) kLabelBand).withTrimmedBottom ((float) kLabelBand);
    }

    float whiteW() const   { return keysArea().getWidth() / ((float) numWhites() + 0.7f); }
    float leftPad() const  { return whiteW() * 0.7f; }      // room for key 27, a black key, at the left end

    int whiteIndex (int key) const noexcept
    {
        int n = 0;
        for (int k = zax::kFirstKey; k < key; ++k)
            if (! zax::isBlackKey (k)) ++n;
        return n;
    }

    juce::Rectangle<float> keyRect (int key) const
    {
        const auto a = keysArea();
        const float w = whiteW();
        const int   wi = whiteIndex (key);                  // whites to the left of this key
        if (! zax::isBlackKey (key))
            return { a.getX() + leftPad() + (float) wi * w, a.getY(), w, a.getHeight() };
        const float bw = w * 0.62f;
        const float cx = a.getX() + leftPad() + (float) wi * w;   // the line between two whites
        return { cx - bw * 0.5f, a.getY(), bw, a.getHeight() * 0.6f };
    }

    int keyAt (juce::Point<float> p) const
    {
        for (int k = zax::kFirstKey; k <= zax::kLastKey; ++k)          // black keys win
            if (zax::isBlackKey (k) && keyRect (k).contains (p)) return k;
        for (int k = zax::kFirstKey; k <= zax::kLastKey; ++k)
            if (! zax::isBlackKey (k) && keyRect (k).contains (p)) return k;
        return -1;
    }

    bool isLit (int key) const noexcept
    {
        return ((activity[(size_t) (key >> 5)] >> (key & 31)) & 1u) != 0;
    }

    void paintKey (juce::Graphics& g, int key)
    {
        const auto r     = keyRect (key);
        const bool black = zax::isBlackKey (key);
        const auto fam   = juce::Colour (zax::familyArgb (zax::roleFamily (key)));
        const bool lit   = isLit (key);
        const bool sel   = key == selected;

        if (! black)
        {
            juce::ColourGradient grad (juce::Colour (0xFFF4F1EA), r.getX(), r.getY(),
                                       juce::Colour (0xFFD6D1C6), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r.reduced (1.0f, 0.0f), 4.0f);
            g.setColour (fam.withAlpha (0.85f));                          // the family band at the foot
            g.fillRect (r.reduced (3.0f, 0.0f).removeFromBottom (8.0f).translated (0.0f, -3.0f));
        }
        else
        {
            juce::ColourGradient grad (juce::Colour (0xFF3A3A3A), r.getX(), r.getY(),
                                       juce::Colour (0xFF0C0C0C), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (fam.withAlpha (0.9f));
            g.fillRect (r.reduced (3.0f, 0.0f).removeFromBottom (6.0f).translated (0.0f, -3.0f));
        }

        if (lit)
        {
            g.setColour (fam.brighter (0.3f).withAlpha (0.55f));
            g.fillRoundedRectangle (r.reduced (black ? 1.0f : 2.0f, 1.0f), 4.0f);
        }

        g.setColour (sel ? juce::Colour (Pal::kAccentLight) : juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (black ? 0.5f : 1.0f, 0.5f), 4.0f, sel ? 3.0f : 1.0f);

        // The sound, up the key's length
        if (kit != nullptr)
        {
            const char* snd = kit->cells[(size_t) key].sound.name();
            if (snd != nullptr && *snd != 0)
            {
                const auto inner = r.reduced (2.0f, 12.0f).withTrimmedBottom (black ? 6.0f : 10.0f);
                juce::Graphics::ScopedSaveState ss (g);
                g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                                 inner.getCentreX(), inner.getCentreY()));
                const auto rot = juce::Rectangle<float> (inner.getHeight(), inner.getWidth()).withCentre (inner.getCentre());
                g.setColour (black ? fam.brighter (0.6f) : fam.darker (0.55f));
                g.setFont (juce::Font (juce::FontOptions (juce::jmin (14.0f, r.getWidth() * 0.42f)).withStyle ("Bold")));
                g.drawFittedText (juce::String (snd), rot.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
            }
        }
    }

    //  The name, its row and its arrow.  Whites alternate their two rows by
    //  white index, blacks by black index, so neighbours never share a row.
    void paintLabel (juce::Graphics& g, int key, juce::Rectangle<float> keys)
    {
        const auto* gmKey = zax::gm (key);
        if (gmKey == nullptr) return;
        const bool  black = zax::isBlackKey (key);
        const auto  kr    = keyRect (key);
        const float w     = whiteW();
        const float cx    = kr.getCentreX();
        const float boxW  = 2.0f * w - 6.0f;

        int idx = 0;                                        // index among keys of the same colour
        for (int k = zax::kFirstKey; k < key; ++k)
            if (zax::isBlackKey (k) == black) ++idx;
        const int row = idx % 2;

        juce::Rectangle<float> box;
        juce::Line<float>      arrow;
        const float gap = 14.0f;
        if (! black)
        {
            const float top = keys.getBottom() + gap + (float) row * kRowH;
            box   = { cx - boxW * 0.5f, top, boxW, (float) kRowH - 2.0f };
            arrow = { cx, keys.getBottom() + 2.0f, cx, top - 1.0f };
        }
        else
        {
            const float bottom = keys.getY() - gap - (float) row * kRowH;
            box   = { cx - boxW * 0.5f, bottom - ((float) kRowH - 2.0f), boxW, (float) kRowH - 2.0f };
            arrow = { cx, keys.getY() - 2.0f, cx, bottom + 1.0f };
        }

        const bool sel = key == selected;
        const auto fam = juce::Colour (zax::familyArgb (gmKey->family));
        g.setColour (sel ? juce::Colour (Pal::kAccentLight) : fam.brighter (0.15f).withAlpha (0.9f));
        g.drawArrow (arrow, sel ? 1.8f : 1.2f, 6.0f, 6.0f);

        // GM name (up to two lines), then the key number
        auto text = box;
        const auto numRow = black ? text.removeFromTop (12.0f) : text.removeFromBottom (12.0f);
        g.setColour (sel ? juce::Colour (Pal::kAccentLight) : juce::Colour (0xFFE8E8E8));
        g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
        g.drawFittedText (gmKey->name, text.toNearestInt(), black ? juce::Justification::centredBottom
                                                                  : juce::Justification::centredTop, 2, 0.8f);
        g.setColour (juce::Colour (0xFF8C8C8C));
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText (juce::String (key) + "  " + juce::String (zax::noteName (key)), numRow, juce::Justification::centred);
    }

    const zax::Kit*           kit = nullptr;
    int                       selected = 36, pressed = -1;
    std::array<uint32_t, 4>   activity {};
};
