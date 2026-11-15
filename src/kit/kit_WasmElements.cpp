// D:\workspace\Fanan_web_hub\src\kit\kit_WasmElements.cpp
//
// The plugin's programmed elements: the toolbox's own (the drop-down list, the range slider,
// the dual knob, the 3D displays, the panel painter...) and the plugin's own (its elements
// folder), compiled straight into the plugin and run natively through stella::ui::ElementRuntime,
// as an exported plugin runs them. Sonify runs the same elements as WebAssembly, in a sandbox,
// because there Fanan AI writes them as the plugin is made. The class keeps Sonify's name and
// interface, so the window's code (PluginCanvas*.cpp) is Sonify's file as it is.

#include "WasmElements.h"
#include "stella_elements.h"

#include <algorithm>

namespace
{
    /** Forwards what the elements ask to the host set with setHost (the canvas), and answers
        with nothing while none is set. */
    struct Forwarder final : stella::ui::ElementHost
    {
        stella::ui::ElementHost* target = nullptr;

        bool paramInfo (int element, int slot, int info, float& result) override
        {
            return target != nullptr && target->paramInfo (element, slot, info, result);
        }

        int findSlot (int element, const std::string& role) override        { return target != nullptr ? target->findSlot (element, role) : -1; }
        void setValue (int element, int slot, float newValue) override       { if (target != nullptr) target->setValue (element, slot, newValue); }
        void gesture (int element, int slot, bool starts) override           { if (target != nullptr) target->gesture (element, slot, starts); }

        bool text (int element, int which, int index, const std::string& key, std::string& result) override
        {
            return target != nullptr && target->text (element, which, index, key, result);
        }

        int numOptions (int element) override                                { return target != nullptr ? target->numOptions (element) : 0; }
        std::uint32_t colour (int element) override                          { return target != nullptr ? target->colour (element) : 0u; }
        float level (int element, bool rms) override                         { return target != nullptr ? target->level (element, rms) : 0.0f; }

        void readScope (int element, float* destination, int numSamples) override
        {
            if (target != nullptr)
                target->readScope (element, destination, numSamples);
            else
                std::fill (destination, destination + numSamples, 0.0f);
        }

        float sourceLevel (int element, const std::string& role, bool rms) override
        {
            return target != nullptr ? target->sourceLevel (element, role, rms) : 0.0f;
        }

        void readSourceScope (int element, const std::string& role, float* destination, int numSamples) override
        {
            if (target != nullptr)
                target->readSourceScope (element, role, destination, numSamples);
            else
                std::fill (destination, destination + numSamples, 0.0f);
        }

        int presetInfo (int element, int which, int index) override
        {
            return target != nullptr ? target->presetInfo (element, which, index) : (which == presetCurrentInfo ? -1 : 0);
        }

        void presetTask (int element, int task, int index) override          { if (target != nullptr) target->presetTask (element, task, index); }

        bool pictureSize (int element, const std::string& name, int& width, int& height) override
        {
            return target != nullptr && target->pictureSize (element, name, width, height);
        }

        bool readPicture (int element, const std::string& name, int width, int height, std::uint32_t* destination) override
        {
            return target != nullptr && target->readPicture (element, name, width, height, destination);
        }

        int fontData (const std::string& name, bool bold, std::uint8_t* destination, int capacity) override
        {
            return target != nullptr ? target->fontData (name, bold, destination, capacity) : 0;
        }

        void playNote (int element, int note, float velocity) override       { if (target != nullptr) target->playNote (element, note, velocity); }
        bool isNoteDown (int note) override                                  { return target != nullptr && target->isNoteDown (note); }
        double seconds() override                                            { return target != nullptr ? target->seconds() : 0.0; }
    };
}

//==============================================================================
struct WasmElements::Impl
{
    Forwarder host;
    stella::ui::ElementRuntime runtime { host };   // after the host it asks
    std::vector<Type> types;
    std::string failure;
};

std::unique_ptr<WasmElements> WasmElements::load (const std::vector<std::uint8_t>&, std::string& error)
{
    std::unique_ptr<WasmElements> elements (new WasmElements());

    if (const auto* problem = stella::ui::ElementRuntime::problem(); problem != nullptr && *problem != 0)
    {
        error = problem;
        return nullptr;
    }

    for (int i = 0; i < stella::ui::ElementRuntime::numTypes(); ++i)
        elements->impl->types.push_back ({ stella::ui::ElementRuntime::typeName (i), stella::ui::ElementRuntime::typeDescription (i) });

    return elements;
}

WasmElements::WasmElements() : impl (std::make_unique<Impl>()) {}
WasmElements::~WasmElements() = default;

const std::vector<WasmElements::Type>& WasmElements::getTypes() const noexcept   { return impl->types; }

bool WasmElements::hasType (const std::string& name) const
{
    for (const auto& type : impl->types)
        if (type.name == name)
            return true;

    return false;
}

void WasmElements::setHost (stella::ui::ElementHost* host) noexcept   { impl->host.target = host; }
void WasmElements::setPatience (double) noexcept                      {}   // native: nothing is stopped

bool WasmElements::create (int element, const std::string& type, int width, int height)
{
    return impl->runtime.create (element, type.c_str(), width, height);
}

void WasmElements::destroy (int element)                       { impl->runtime.destroy (element); }
void WasmElements::resize (int element, int width, int height) { impl->runtime.resize (element, width, height); }
void WasmElements::invalidate (int element)                    { impl->runtime.invalidate (element); }
bool WasmElements::needsPaint (int element)                    { return impl->runtime.needsPaint (element); }
bool WasmElements::render (int element)                        { return impl->runtime.render (element); }

const std::uint32_t* WasmElements::pixels (int element, bool popup, int, int)
{
    return impl->runtime.pixels (element, popup);
}

bool WasmElements::popupArea (int element, int& x, int& y, int& w, int& h)
{
    return impl->runtime.popupArea (element, x, y, w, h);
}

bool WasmElements::mouse (int element, bool popup, int event, float x, float y, int flags, float notches)
{
    stella::ui::Mouse m;
    m.x = x;
    m.y = y;
    m.shift = (flags & 1) != 0;
    m.doubleClick = (flags & 2) != 0;
    return impl->runtime.mouse (element, popup, event, m, notches);
}

void WasmElements::closePopup (int element)          { impl->runtime.closePopup (element); }
bool WasmElements::hasFailed() const noexcept        { return false; }
const std::string& WasmElements::getFailure() const noexcept   { return impl->failure; }
