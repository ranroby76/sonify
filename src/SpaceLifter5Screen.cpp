// D:\workspace\Fanan_web_hub\src\SpaceLifter5Screen.cpp

#include "SpaceLifter5Screen.h"
#include "NebulaFaceplate.h"
#include "PluginHeader.h"
#include "SpaceLifter5.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>

namespace
{
    using nebula_ui::Param;

    /** The faceplate's parameters, as the built-in keeps them (by their names in its specs). */
    class BuiltinModel final : public nebula_ui::Model
    {
    public:
        explicit BuiltinModel (BuiltinPlugin& owner) : plugin (owner)
        {
            static const std::array<std::pair<Param, const char*>, nebula_ui::numParams> names { {
                { Param::type, "Type" }, { Param::mix, "Mix" }, { Param::predelay, "Pre-Delay" }, { Param::decay, "Decay" },
                { Param::modulation, "Modulation" }, { Param::earlyLate, "Early-Late" }, { Param::lowCut, "Low Cut" },
                { Param::highCut, "High Cut" }, { Param::damping, "Damping" }, { Param::shimmer, "Shimmer" },
                { Param::feedback, "Feedback" }, { Param::shift, "Shift" }, { Param::split, "Split" }, { Param::gate, "Gate" },
                { Param::threshold, "Threshold" }, { Param::attack, "Attack" }, { Param::hold, "Hold" },
                { Param::release, "Release" }, { Param::bypass, "Bypass" } } };

            indices.fill (-1);
            const auto& specs = plugin.getSpecs();

            for (const auto& [param, name] : names)
                for (size_t i = 0; i < specs.size(); ++i)
                    if (juce::String (specs[i].name) == name)
                        indices[(size_t) param] = static_cast<int> (i);

            // Every faceplate control is on one of Space Lifter 5's parameters.
            jassert (std::find (indices.begin(), indices.end(), -1) == indices.end());
        }

        float get (Param p) const override
        {
            const auto index = indices[(size_t) p];
            return index >= 0 ? static_cast<float> (plugin.getParameterValue (index)) : nebula_ui::defaultOf (p);
        }

        void set (Param p, float value) override
        {
            const auto index = indices[(size_t) p];

            if (index < 0)
                return;

            plugin.queueParameter (index, static_cast<double> (value));
            plugin.panelTouched (index);     // MIDI Link's Last Touched, as the designed panels do
        }

        float outputPeak (int channel) const override   { return plugin.readSource (channel == 0 ? "out L" : "out R"); }
        unsigned outputOvers (int channel) const override { return (unsigned) plugin.readSource (channel == 0 ? "over L" : "over R"); }

        std::vector<nebula_ui::TypeShape> types() const override
        {
            std::vector<nebula_ui::TypeShape> list;

            for (const auto& t : SpaceLifter5::typeTable())
                list.push_back ({ t.name, t.hiMult, t.erTaps, t.erStartMs, t.erSpanMs });

            return list;
        }

        juce::String title() const override       { return "SPACE LIFTER"; }
        juce::String badgeCode() const override   { return "SL-5"; }
        juce::String footer() const override      { return juce::String::fromUTF8 ("SONIFY \xc2\xb7 SPACE LIFTER 5"); }

    private:
        BuiltinPlugin& plugin;
        std::array<int, nebula_ui::numParams> indices {};
    };

    //==============================================================================
    class FaceplateWindow final : public juce::DocumentWindow
    {
    public:
        explicit FaceplateWindow (BuiltinPlugin& owner)
            : DocumentWindow (owner.getDescriptor().name, juce::Colours::black, DocumentWindow::closeButton),
              model (owner), faceplate (model)
        {
            setUsingNativeTitleBar (true);
            PluginHeader::attach (*this, owner);    // before the content and the limits (PluginHeader.h)

            constexpr int w = nebula_ui::Faceplate::designW, h = nebula_ui::Faceplate::designH;
            setContentNonOwned (&faceplate, false);
            setContentComponentSize (w * 4 / 5, h * 4 / 5);    // as the VST3 opens
            setResizable (true, false);
            getConstrainer()->setFixedAspectRatio (static_cast<double> (w) / h);
            getConstrainer()->setSizeLimits (w / 2, h / 2, w * 3 / 2, h * 3 / 2);
            centreWithSize (getWidth(), getHeight());
        }

        ~FaceplateWindow() override   { clearContentComponent(); }

        void closeButtonPressed() override   { setVisible (false); }

    private:
        BuiltinModel model;                     // before the faceplate, which reads it
        nebula_ui::ScaledFaceplate faceplate;
    };

    class Screen final : public BuiltinPlugin::Screen
    {
    public:
        explicit Screen (BuiltinPlugin& owner) : plugin (owner) {}

        void show (const juce::String& title) override
        {
            if (window == nullptr)
                window = std::make_unique<FaceplateWindow> (plugin);

            window->setName (title.isNotEmpty() ? title : plugin.getDescriptor().name);
            window->setVisible (true);
            window->toFront (true);
        }

        void hide() override
        {
            if (window != nullptr)
                window->setVisible (false);
        }

    private:
        BuiltinPlugin& plugin;
        std::unique_ptr<FaceplateWindow> window;
    };
}

std::unique_ptr<BuiltinPlugin::Screen> SpaceLifter5Screen::create (BuiltinPlugin& plugin)
{
    return std::make_unique<Screen> (plugin);
}
