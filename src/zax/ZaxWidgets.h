// C:\workspace\SONIFY PLUGIN zax\src\ZaxWidgets.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "ZaxSliders.h"

//==============================================================================
//  ZaxWidgets.h  —  the editor's small parts, from Grex's EDM kit editor:
//      ZaxTabBar     equal tabs across the slider mixer, each with its "i"
//      ZaxInfoPanel  what a tab's sliders shape, in a callout
//      ZaxLed        the selected key's MIDI LED (note-on to note-off)
//      ZaxPageBar    a row of page buttons (the FX tab's chain stages)
//==============================================================================

class ZaxTabBar : public juce::Component
{
public:
    std::function<void (int index)> onSelect;
    std::function<void (int index)> onInfo;

    void setTabs (const juce::StringArray& names, int sel)
    {
        tabs     = names;
        selected = juce::jlimit (0, juce::jmax (0, tabs.size() - 1), sel);
        repaint();
    }

    juce::String         getTabName (int i) const   { return tabs[i]; }
    juce::Rectangle<int> getTabArea (int i) const   { return tabRect (i).toNearestInt(); }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < tabs.size(); ++i)
        {
            const auto r  = tabRect (i);
            const bool on = (i == selected);
            g.setColour (on ? juce::Colour (Pal::kAccent) : juce::Colour (0xFF232323));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? juce::Colour (Pal::kAccentLight) : juce::Colour (0xFF333333));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);

            const auto ib = infoRect (i);
            g.setColour (on ? juce::Colour (0xFF141414) : juce::Colour (0xFFBDBDBD));
            g.setFont (juce::Font (juce::FontOptions (11.5f).withStyle ("Bold")));
            g.drawFittedText (tabs[i], r.withRight (ib.getX() - 2.0f).withTrimmedLeft (4.0f).toNearestInt(),
                              juce::Justification::centred, 1, 0.7f);

            g.setColour (juce::Colours::white);
            g.fillEllipse (ib);
            g.setColour (juce::Colours::black);
            g.setFont (juce::Font (juce::FontOptions (ib.getHeight() * 0.78f).withStyle ("Bold")));
            g.drawText ("i", ib.translated (0.0f, 0.5f), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = tabAt (e.position);
        if (i < 0) return;
        if (infoRect (i).expanded (2.0f).contains (e.position))
        {
            if (onInfo) onInfo (i);
            return;
        }
        if (i == selected) return;
        selected = i;
        repaint();
        if (onSelect) onSelect (i);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int i = tabAt (e.position);
        const bool overInfo = i >= 0 && infoRect (i).expanded (2.0f).contains (e.position);
        setMouseCursor (overInfo ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

private:
    juce::Rectangle<float> tabRect (int i) const
    {
        const float w = (float) getWidth() / (float) juce::jmax (1, tabs.size());
        return juce::Rectangle<float> ((float) i * w, 0.0f, w, (float) getHeight()).reduced (1.5f, 1.0f);
    }

    juce::Rectangle<float> infoRect (int i) const
    {
        const auto  r = tabRect (i);
        const float d = juce::jmin (14.0f, r.getHeight() - 8.0f);
        return { r.getRight() - d - 5.0f, r.getCentreY() - d * 0.5f, d, d };
    }

    int tabAt (juce::Point<float> p) const
    {
        if (tabs.isEmpty() || getWidth() <= 0) return -1;
        return juce::jlimit (0, tabs.size() - 1, (int) (p.x / ((float) getWidth() / (float) tabs.size())));
    }

    juce::StringArray tabs;
    int               selected = 0;
};

//==============================================================================
class ZaxInfoPanel : public juce::Component
{
public:
    ZaxInfoPanel (const juce::String& title, const juce::String& text)
    {
        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        as.setJustification (juce::Justification::topLeft);
        as.setLineSpacing (2.0f);

        const juce::Font titleFont (juce::FontOptions (15.0f).withStyle ("Bold"));
        const juce::Font labelFont (juce::FontOptions (12.5f).withStyle ("Bold"));
        const juce::Font bodyFont  (juce::FontOptions (12.5f));

        as.append (title + "\n", titleFont, juce::Colour (Pal::kAccentLight));
        juce::StringArray lines;
        lines.addLines (text);
        for (const auto& line : lines)
        {
            const int bar = line.indexOfChar ('|');
            if (bar < 0)
                as.append (line + "\n\n", bodyFont, juce::Colour (0xFFCFCFCF));
            else
            {
                as.append (line.substring (0, bar), labelFont, juce::Colours::white);
                as.append ("   " + line.substring (bar + 1) + "\n", bodyFont, juce::Colour (0xFFB4B4B4));
            }
        }
        layout.createLayout (as, (float) (kWidth - 2 * kPad));
        setSize (kWidth, (int) std::ceil (layout.getHeight()) + 2 * kPad);
    }

    void paint (juce::Graphics& g) override   { layout.draw (g, getLocalBounds().toFloat().reduced ((float) kPad)); }

private:
    static constexpr int kWidth = 440, kPad = 14;
    juce::TextLayout layout;
};

//==============================================================================
class ZaxLed : public juce::Component
{
public:
    void setOn (bool on)   { if (on != lit) { lit = on; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const float d = juce::jmin (r.getHeight(), 11.0f);
        const auto dot = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 2.0f, r.getCentreY() });
        if (lit)
        {
            g.setColour (juce::Colour (0x553CFF5A));
            g.fillEllipse (dot.expanded (2.5f));
        }
        g.setColour (lit ? juce::Colour (0xFF3CFF5A) : juce::Colour (0xFF1C3A22));
        g.fillEllipse (dot);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (dot, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.95f : 0.55f));
        g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
        g.drawText ("MIDI", r.withTrimmedLeft (d + 7.0f), juce::Justification::centredLeft);
    }

private:
    bool lit = false;
};

//==============================================================================
//  The FX tab's stage row: the chain's stages left to right, in signal order,
//  each lit green when it is on.  Arrows between them show the order.
//==============================================================================
class ZaxPageBar : public juce::Component
{
public:
    std::function<void (int page)> onSelect;

    void setPages (const juce::StringArray& names)          { pages = names; repaint(); }
    void setSelected (int p)                                { selected = p; repaint(); }
    void setStageOn (int p, bool on)                        { if (p >= 0 && p < 16) stageOn[p] = on; repaint(); }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < pages.size(); ++i)
        {
            const auto r  = pageRect (i);
            const bool sel = i == selected;
            g.setColour (sel ? juce::Colour (0xFFD4A937) : juce::Colour (0xFF262626));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (stageOn[i] ? juce::Colour (0xFF3CB85A) : juce::Colour (0xFF3A3A3A));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, stageOn[i] ? 2.0f : 1.0f);
            g.setColour (sel ? juce::Colours::black : juce::Colour (0xFFD0D0D0));
            g.setFont (juce::Font (juce::FontOptions (11.5f).withStyle ("Bold")));
            g.drawFittedText (pages[i], r.toNearestInt(), juce::Justification::centred, 1, 0.7f);

            if (i + 1 < pages.size())                       // the signal goes on to the next stage
            {
                const float y = r.getCentreY(), x0 = r.getRight() + 2.0f, x1 = pageRect (i + 1).getX() - 2.0f;
                g.setColour (juce::Colour (0xFF8A8A8A));
                g.drawArrow ({ x0, y, x1, y }, 1.2f, 5.0f, 5.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < pages.size(); ++i)
            if (pageRect (i).contains (e.position))
            {
                selected = i;
                repaint();
                if (onSelect) onSelect (i);
                return;
            }
    }

private:
    juce::Rectangle<float> pageRect (int i) const
    {
        const float gap = 14.0f;
        const float w = ((float) getWidth() - gap * (float) juce::jmax (0, pages.size() - 1)) / (float) juce::jmax (1, pages.size());
        return { (float) i * (w + gap), 0.0f, w, (float) getHeight() };
    }

    juce::StringArray pages;
    int  selected = 0;
    bool stageOn[16] {};
};



