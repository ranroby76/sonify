// C:\workspace\SONIFY PLUGIN natura\src\Theme.h
#pragma once

#include <JuceHeader.h>

// =============================================================================
// Theme
//
// All colors and standard dimensions used across the plugin GUI.
// Single source of truth — change one value, the whole plugin re-skins.
// =============================================================================
namespace Theme
{
    // -------------------------------------------------------------------------
    // Window
    // -------------------------------------------------------------------------
    constexpr int defaultWidth  = 1280;
    constexpr int defaultHeight = 720;     // 16:9
    constexpr int minWidth      = 960;
    constexpr int minHeight     = 540;     // 16:9
    constexpr int maxWidth      = 1920;
    constexpr int maxHeight     = 1080;    // 16:9

    constexpr double aspectRatio = 16.0 / 9.0;

    // -------------------------------------------------------------------------
    // Layout heights — scaled by Layout::scale() at runtime
    // -------------------------------------------------------------------------
    namespace BaseSize
    {
        constexpr int headerHeight        = 38;
        constexpr int topNavHeight        = 32;
        constexpr int slotsColumnWidth    = 148;
        constexpr int subTabHeight        = 32;
        constexpr int modDrawerBarHeight  = 26;
        constexpr int modDrawerOpenHeight = 200;
        constexpr int bottomTabHeight     = 26;
        constexpr int bottomContentHeight = 84;
        constexpr int paddingS            = 4;
        constexpr int paddingM            = 8;
        constexpr int paddingL            = 12;
        constexpr int cornerRadius        = 6;
    }

    // -------------------------------------------------------------------------
    // Colors — dark theme, flat, modern
    // -------------------------------------------------------------------------
    namespace Color
    {
        // Backgrounds (darkest -> lightest)
        const juce::Colour bgPage        { 0xFF14141A };  // deepest, page bg
        const juce::Colour bgWorkspace   { 0xFF1B1B22 };  // main workspace area
        const juce::Colour bgStrip       { 0xFF20202A };  // header / strips
        const juce::Colour bgPanel       { 0xFF272731 };  // panels / cards
        const juce::Colour bgPanelHi     { 0xFF2F2F3B };  // hovered panel
        const juce::Colour bgInput       { 0xFF181820 };  // input fields / dropdowns

        // Borders
        const juce::Colour border        { 0xFF34343F };
        const juce::Colour borderStrong  { 0xFF4A4A57 };
        const juce::Colour borderSubtle  { 0xFF2A2A33 };

        // Text
        const juce::Colour textPrimary   { 0xFFE8E8EE };
        const juce::Colour textSecondary { 0xFFA0A0AC };
        const juce::Colour textTertiary  { 0xFF6A6A78 };
        const juce::Colour textDisabled  { 0xFF4A4A55 };

        // Accent (used for non-slot-specific highlights)
        const juce::Colour accent        { 0xFF4DD4C5 };  // bright teal-cyan
        const juce::Colour accentDim     { 0xFF2A8378 };

        // Status
        const juce::Colour success       { 0xFF6BCB77 };
        const juce::Colour warning       { 0xFFE0A323 };
        const juce::Colour danger        { 0xFFE65A5A };
    }

    // -------------------------------------------------------------------------
    // Fonts
    // -------------------------------------------------------------------------
    namespace Font
    {
        constexpr float sizeXS = 10.0f;
        constexpr float sizeS  = 11.0f;
        constexpr float sizeM  = 13.0f;
        constexpr float sizeL  = 15.0f;
        constexpr float sizeXL = 18.0f;

        inline juce::Font regular (float sizePt)
        {
            return juce::Font (juce::FontOptions (sizePt, juce::Font::plain));
        }

        inline juce::Font bold (float sizePt)
        {
            return juce::Font (juce::FontOptions (sizePt, juce::Font::bold));
        }

        // Logo font — Biergarten (Fanan Team brand). Falls back to bold sans
        // if the typeface isn't loaded. Wired up later via the L&F.
        inline juce::Font logo (float sizePt)
        {
            return juce::Font (juce::FontOptions (sizePt, juce::Font::bold));
        }
    }

    // -------------------------------------------------------------------------
    // Layout — runtime scale factor based on current window vs default size
    // -------------------------------------------------------------------------
    struct Layout
    {
        static float scale (int currentHeight) noexcept
        {
            return juce::jlimit (0.6f, 2.0f,
                                 (float) currentHeight / (float) defaultHeight);
        }

        static int s (int basePx, int currentHeight) noexcept
        {
            return juce::roundToInt ((float) basePx * scale (currentHeight));
        }
    };
    // -------------------------------------------------------------------------
    // Natura additions: the window, and the Grex slider gold (GoldSlider,
    // VelCurveDisplay and the frame titles use these, exactly as in Grex).
    // -------------------------------------------------------------------------
    namespace Natura
    {
        constexpr int width  = 1440;
        constexpr int height = 760;
    }

    namespace Gold
    {
        constexpr juce::uint32 kAccent       = 0xFFB87A36;   // Grex gold
        constexpr juce::uint32 kAccentLight  = 0xFFE5BB8D;
        constexpr juce::uint32 kAccentBright = 0xFFD09657;
        constexpr juce::uint32 kAccentGrad   = 0xFFA97032;
        constexpr juce::uint32 kAccentGradDk = 0xFF5D370F;
        constexpr juce::uint32 kAccentDark   = 0xFF54320E;
    }
}
