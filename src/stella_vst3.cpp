// D:\workspace\Fanan_web_hub\src\stella_vst3.cpp
//
// An exported plugin's VST3 side: no JUCE and no SDK helper classes, just the VST3 interfaces
// (pluginterfaces, MIT licensed) around the plugin's core (stella_plugin.h). One object is both
// the processor and the edit controller ("single component"), so the window, the host and the
// audio all share one set of parameter values.
//
// Parameter ids are stable (a hash of "module.param", kept below 2^31), state is "id=value"
// lines. VST3 hosts send pitch bend and the mod wheel as hidden parameters (IMidiMapping);
// they reach the modules as MIDI again. On Linux the window runs on the host's run loop
// (Linux::IRunLoop): a timer, and its connection to the X server watched.

#include "stella_plugin.h"

#define INIT_CLASS_IID   // this file defines the interface ids it uses
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/vstspeaker.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace
{
    using stella::plugin::Core;

    //==========================================================================
    constexpr ParamID pitchBendId = 0x7ffffff0, modWheelId = 0x7ffffff1;

    ParamID idFor (const std::string& text)
    {
        // Kept below 2^31 (higher ids are the host's).
        const auto hash = stella::plugin::hashOf (text) & 0x7fffffffu;
        return hash == pitchBendId || hash == modWheelId ? hash - 2 : hash;
    }

    /** UTF-8 to UTF-16, for the host's strings. */
    void toUtf16 (const std::string& text, char16* destination, int capacity)
    {
        int out = 0;

        for (size_t i = 0; i < text.size() && out < capacity - 1;)
        {
            const auto c = (unsigned char) text[i];
            std::uint32_t code = c;
            int extra = 0;

            if (c >= 0xf0)      { code = c & 0x07; extra = 3; }
            else if (c >= 0xe0) { code = c & 0x0f; extra = 2; }
            else if (c >= 0xc0) { code = c & 0x1f; extra = 1; }

            ++i;

            for (int k = 0; k < extra && i < text.size(); ++k, ++i)
                code = (code << 6) | ((unsigned char) text[i] & 0x3f);

            if (code >= 0x10000 && out < capacity - 2)
            {
                code -= 0x10000;
                destination[out++] = (char16) (0xd800 + (code >> 10));
                destination[out++] = (char16) (0xdc00 + (code & 0x3ff));
            }
            else if (code < 0x10000)
            {
                destination[out++] = (char16) code;
            }
        }

        destination[out] = 0;
    }

    std::string fromUtf16 (const char16* text)
    {
        std::string result;

        for (int i = 0; text != nullptr && text[i] != 0 && i < 256; ++i)
            result += text[i] < 128 ? (char) text[i] : '?';

        return result;
    }

    /** A number the host's user typed: '.' or ',' as its point, whatever the locale. False if
        there's no number at its start. */
    bool numberIn (const std::string& text, double& value)
    {
        size_t i = 0;

        while (i < text.size() && (text[i] == ' ' || text[i] == '\t'))
            ++i;

        if (i < text.size() && (text[i] == '-' || text[i] == '+'))
            ++i;

        if (i < text.size() && (text[i] == '.' || text[i] == ','))
            ++i;

        if (i >= text.size() || text[i] < '0' || text[i] > '9')
            return false;

        value = stella::plugin::numberFrom (text.c_str());
        return true;
    }

    //==========================================================================
    /** A key the host hands the view (IPlugView::onKeyDown), for the window: its VirtualKeyCodes
        and KeyModifier (pluginterfaces/base/keycodes.h, not in every copy of the interfaces). */
    bool windowKey (stella::gui::Window& window, char16 key, int16 keyCode, int16 modifiers)
    {
        using stella::gui::Key;

        enum { keyBack = 1, keyTab = 2, keyReturn = 4, keyEscape = 6, keySpace = 7, keyEnd = 9, keyHome = 10, keyLeft = 11,
               keyRight = 13, keyInsert = 21, keyDelete = 22, keyEnter = 19 };
        enum { shiftModifier = 1 << 0, commandModifier = 1 << 2 };   // command: Ctrl on Windows, Cmd on a Mac

        const int mods = ((modifiers & shiftModifier) != 0 ? stella::gui::shiftKey : 0)
                       | ((modifiers & commandModifier) != 0 ? stella::gui::commandKey : 0);

        switch (keyCode)
        {
            case keyBack:                  return window.keyDown (Key::backspace, 0, mods);
            case keyTab:                   return window.keyDown (Key::tab, 0, mods);
            case keyReturn: case keyEnter: return window.keyDown (Key::enter, 0, mods);
            case keyEscape:                return window.keyDown (Key::escape, 0, mods);
            case keySpace:                 return window.keyDown (Key::character, ' ', 0);
            case keyEnd:                   return window.keyDown (Key::end, 0, mods);
            case keyHome:                  return window.keyDown (Key::home, 0, mods);
            case keyLeft:                  return window.keyDown (Key::left, 0, mods);
            case keyRight:                 return window.keyDown (Key::right, 0, mods);
            case keyDelete:                return window.keyDown (Key::forwardDelete, 0, mods);

            case keyInsert:   // Shift+Insert pastes, Ctrl+Insert copies
                if ((mods & stella::gui::shiftKey) != 0)   return window.keyDown (Key::character, 'v', stella::gui::commandKey);
                if ((mods & stella::gui::commandKey) != 0) return window.keyDown (Key::character, 'c', stella::gui::commandKey);
                return false;

            default:
                break;
        }

        auto c = (std::uint32_t) key;

        if ((mods & stella::gui::commandKey) != 0)
        {
            // A shortcut: by its letter (some hosts send Ctrl+V as the control character 22).
            if (c >= 1 && c <= 26)
                c = 'a' + c - 1;

            return c >= 0x20 && c < 0x7f && window.keyDown (Key::character, c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c, mods);
        }

        if (c < 0x20 || c == 0x7f || (c >= 0xd800 && c < 0xe000))
            return false;   // not something that types (a half of a character beyond the first 65536 included)

        return window.keyDown (Key::character, c, 0);
    }

    void copyAscii (char8* destination, size_t capacity, const char* text)
    {
        std::strncpy (destination, text, capacity - 1);
        destination[capacity - 1] = 0;
    }

    const char* subCategories()
    {
        return stella::info::kind == 0 ? "Instrument|Synth" : (stella::info::kind == 2 ? "Fx|Analyzer" : "Fx");
    }

    //==========================================================================
    class Processor final : public IComponent,
                            public IAudioProcessor,
                            public IProcessContextRequirements,
                            public IEditController,
                            public IMidiMapping,
                            public Core
    {
    public:
        Processor()
        {
            if (! build())
                return;

            for (int i = 0; i < numParams(); ++i)
                ids.push_back (idFor (paramInfo (i).id));
        }

        ~Processor()
        {
            if (handler != nullptr)
                handler->release();
        }

        //======================================================================
        // FUnknown: one object, several interfaces.
        tresult PLUGIN_API queryInterface (const TUID iid, void** obj) override
        {
            QUERY_INTERFACE (iid, obj, FUnknown::iid, IComponent)
            QUERY_INTERFACE (iid, obj, IPluginBase::iid, IComponent)
            QUERY_INTERFACE (iid, obj, IComponent::iid, IComponent)
            QUERY_INTERFACE (iid, obj, IAudioProcessor::iid, IAudioProcessor)
            QUERY_INTERFACE (iid, obj, IProcessContextRequirements::iid, IProcessContextRequirements)
            QUERY_INTERFACE (iid, obj, IEditController::iid, IEditController)
            QUERY_INTERFACE (iid, obj, IMidiMapping::iid, IMidiMapping)
            *obj = nullptr;
            return kNoInterface;
        }

        uint32 PLUGIN_API addRef() override    { return ++refs; }

        uint32 PLUGIN_API release() override
        {
            const auto left = --refs;

            if (left == 0)
                delete this;

            return left;
        }

        //======================================================================
        // IPluginBase (shared by the component and the controller)
        tresult PLUGIN_API initialize (FUnknown*) override    { return isBuilt() ? kResultOk : kResultFalse; }
        tresult PLUGIN_API terminate() override                { return kResultOk; }

        //======================================================================
        // IComponent
        tresult PLUGIN_API getControllerClassId (TUID) override    { return kNotImplemented; }   // single component
        tresult PLUGIN_API setIoMode (IoMode) override              { return kResultOk; }

        int32 PLUGIN_API getBusCount (MediaType type, BusDirection direction) override
        {
            if (type == kAudio)
                return direction == kInput ? (stella::info::isInstrument ? 0 : 1) : 1;

            return direction == kInput ? 1 : 0;   // notes in
        }

        tresult PLUGIN_API getBusInfo (MediaType type, BusDirection direction, int32 index, BusInfo& bus) override
        {
            if (index != 0 || index >= getBusCount (type, direction))
                return kInvalidArgument;

            bus.mediaType = type;
            bus.direction = direction;
            bus.channelCount = type == kAudio ? 2 : 16;
            toUtf16 (type == kAudio ? (direction == kInput ? "Input" : "Output") : "Notes", bus.name, 128);
            bus.busType = kMain;
            bus.flags = BusInfo::kDefaultActive;
            return kResultOk;
        }

        tresult PLUGIN_API getRoutingInfo (RoutingInfo&, RoutingInfo&) override    { return kNotImplemented; }

        tresult PLUGIN_API activateBus (MediaType type, BusDirection direction, int32 index, TBool) override
        {
            return index >= 0 && index < getBusCount (type, direction) ? kResultTrue : kInvalidArgument;
        }

        tresult PLUGIN_API setActive (TBool state) override
        {
            if (state && ! isPrepared())
                prepare (sampleRate, maxFrames);

            if (! state)
                reset();

            return kResultOk;
        }

        // Shared by IComponent and IEditController: the same state either way.
        tresult PLUGIN_API setState (IBStream* stream) override
        {
            if (stream == nullptr)
                return kInvalidArgument;

            std::string text;
            char buffer[4096];

            for (;;)
            {
                int32 read = 0;

                if (stream->read (buffer, (int32) sizeof (buffer), &read) != kResultOk || read <= 0)
                    break;

                text.append (buffer, (size_t) read);
            }

            return loadState (text) ? kResultOk : kResultFalse;
        }

        tresult PLUGIN_API getState (IBStream* stream) override
        {
            if (stream == nullptr)
                return kInvalidArgument;

            const auto text = saveState();
            int32 written = 0;
            return stream->write ((void*) text.data(), (int32) text.size(), &written) == kResultOk && written == (int32) text.size() ? kResultOk : kResultFalse;
        }

        //======================================================================
        // IAudioProcessor
        tresult PLUGIN_API setBusArrangements (SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) override
        {
            const auto expectedIns = stella::info::isInstrument ? 0 : 1;

            if (numIns != expectedIns || numOuts != 1)
                return kResultFalse;

            for (int32 i = 0; i < numIns; ++i)
                if (inputs[i] != SpeakerArr::kStereo)
                    return kResultFalse;

            return outputs[0] == SpeakerArr::kStereo ? kResultTrue : kResultFalse;
        }

        tresult PLUGIN_API getBusArrangement (BusDirection direction, int32 index, SpeakerArrangement& arrangement) override
        {
            if (index != 0 || index >= getBusCount (kAudio, direction))
                return kInvalidArgument;

            arrangement = SpeakerArr::kStereo;
            return kResultOk;
        }

        tresult PLUGIN_API canProcessSampleSize (int32 size) override    { return size == kSample32 ? kResultTrue : kResultFalse; }
        uint32 PLUGIN_API getLatencySamples() override                    { return 0; }
        uint32 PLUGIN_API getTailSamples() override                       { return kInfiniteTail; }   // releases and echoes ring on

        tresult PLUGIN_API setupProcessing (ProcessSetup& setup) override
        {
            sampleRate = setup.sampleRate > 0.0 ? setup.sampleRate : 48000.0;
            maxFrames = setup.maxSamplesPerBlock > 0 ? std::min (setup.maxSamplesPerBlock, (int32) 8192) : 512;
            return prepare (sampleRate, maxFrames) ? kResultOk : kResultFalse;
        }

        tresult PLUGIN_API setProcessing (TBool) override    { return kResultOk; }

        // The modules follow the song's tempo and position (tempo sync).
        uint32 PLUGIN_API getProcessContextRequirements() override
        {
            return kNeedProjectTimeMusic | kNeedBarPositionMusic | kNeedTempo | kNeedTimeSignature | kNeedTransportState;
        }

        tresult PLUGIN_API process (ProcessData& data) override
        {
            const stella::plugin::NoDenormals noDenormals;
            int numPending = 0;

            // Parameter changes: the last point of each queue; pitch bend and mod wheel become MIDI.
            if (data.inputParameterChanges != nullptr)
            {
                for (int32 q = 0; q < data.inputParameterChanges->getParameterCount(); ++q)
                {
                    auto* queue = data.inputParameterChanges->getParameterData (q);

                    if (queue == nullptr || queue->getPointCount() <= 0)
                        continue;

                    int32 offset = 0;
                    ParamValue value = 0.0;

                    if (queue->getPoint (queue->getPointCount() - 1, offset, value) != kResultOk)
                        continue;

                    const auto id = queue->getParameterId();

                    if (id == pitchBendId || id == modWheelId)
                    {
                        if (numPending >= (int) pending.size())
                            continue;

                        if (id == pitchBendId)
                        {
                            const auto bend = std::min (16383, std::max (0, (int) std::lround (value * 16383.0)));
                            pending[(size_t) numPending++] = { (std::uint32_t) std::max (0, offset), 0xe0, (std::uint8_t) (bend & 0x7f), (std::uint8_t) (bend >> 7), 0 };
                        }
                        else
                        {
                            pending[(size_t) numPending++] = { (std::uint32_t) std::max (0, offset), 0xb0, 1, (std::uint8_t) std::lround (value * 127.0), 0 };
                        }

                        continue;
                    }

                    const auto index = indexOf (id);

                    if (index >= 0)
                        storeValue (index, toPlain (index, value));
                }
            }

            // Nothing to play now (a parameter flush, or not ready): the window keyboard's notes
            // stay in their queue for the next block that plays, so none (a release) is lost.
            const bool silent = data.numSamples <= 0 || data.numOutputs < 1 || data.outputs[0].numChannels < 1 || ! isPrepared();

            // The window's keyboard: its notes come first, at the start of the block.
            if (! silent)
                numPending = (int) takeWindowNotes (pending.data(), (std::uint32_t) numPending, (std::uint32_t) pending.size());

            // The host's notes.
            if (data.inputEvents != nullptr)
            {
                for (int32 e = 0; e < data.inputEvents->getEventCount() && numPending < (int) pending.size(); ++e)
                {
                    Event event {};

                    if (data.inputEvents->getEvent (e, event) != kResultOk)
                        continue;

                    if (event.type == Event::kNoteOnEvent && event.noteOn.pitch >= 0 && event.noteOn.pitch < 128)
                    {
                        const auto velocity = std::min (127, std::max (1, (int) std::lround (event.noteOn.velocity * 127.0f)));
                        pending[(size_t) numPending++] = { (std::uint32_t) std::max (0, event.sampleOffset), (std::uint8_t) (0x90 | (event.noteOn.channel & 15)),
                                                           (std::uint8_t) event.noteOn.pitch, (std::uint8_t) velocity, 0 };
                    }
                    else if (event.type == Event::kNoteOffEvent && event.noteOff.pitch >= 0 && event.noteOff.pitch < 128)
                    {
                        pending[(size_t) numPending++] = { (std::uint32_t) std::max (0, event.sampleOffset), (std::uint8_t) (0x80 | (event.noteOff.channel & 15)),
                                                           (std::uint8_t) event.noteOff.pitch, 0, 0 };
                    }
                }
            }

            // In time order, keeping the order of events at the same moment (a note's start
            // before its stop). Few events at a time, so a plain insertion sort.
            for (int i = 1; i < numPending; ++i)
                for (int j = i; j > 0 && pending[(size_t) j - 1].frame > pending[(size_t) j].frame; --j)
                    std::swap (pending[(size_t) j - 1], pending[(size_t) j]);

            if (silent)
            {
                // Nothing to play now: what the keyboard shows down still follows the notes.
                for (int i = 0; i < numPending; ++i)
                {
                    const auto type = pending[(size_t) i].status & 0xf0;

                    if (type == 0x90 || type == 0x80)
                        notes.sounding (pending[(size_t) i].data1, type == 0x90 && pending[(size_t) i].data2 > 0);
                }

                return kResultOk;
            }

            // The host's tempo and position at the block's start.
            stella::plugin::Transport transport;

            if (const auto* context = data.processContext)
            {
                if ((context->state & ProcessContext::kTempoValid) != 0)            transport.tempo = context->tempo;
                if ((context->state & ProcessContext::kProjectTimeMusicValid) != 0) transport.beat = context->projectTimeMusic;
                if ((context->state & ProcessContext::kBarPositionValid) != 0)      transport.barStart = context->barPositionMusic;

                if ((context->state & ProcessContext::kTimeSigValid) != 0)
                {
                    transport.numerator = context->timeSigNumerator;
                    transport.denominator = context->timeSigDenominator;
                }

                transport.playing = (context->state & ProcessContext::kPlaying) != 0;
            }

            auto& s = shared();
            const auto total = (std::uint32_t) data.numSamples;
            const auto chunk = (std::uint32_t) getMaxFrames();
            float** outputs = data.outputs[0].channelBuffers32;
            const auto outputChannels = (int) data.outputs[0].numChannels;
            float** inputs = data.numInputs > 0 ? data.inputs[0].channelBuffers32 : nullptr;
            const auto inputChannels = data.numInputs > 0 ? (int) data.inputs[0].numChannels : 0;
            int nextEvent = 0;

            for (std::uint32_t start = 0; start < total; start += chunk)
            {
                const auto frames = std::min (total - start, chunk);
                std::uint32_t count = 0;

                while (nextEvent < numPending && pending[(size_t) nextEvent].frame < start + frames)
                {
                    auto event = pending[(size_t) nextEvent++];
                    event.frame = event.frame > start ? event.frame - start : 0;

                    if (count < (std::uint32_t) stella::maxEvents)
                        s.events[count++] = event;
                }

                run (inputs, inputChannels, outputs, outputChannels, (int) start, (int) frames, count, transport);
            }

            data.outputs[0].silenceFlags = 0;
            return kResultOk;
        }

        //======================================================================
        // IEditController
        tresult PLUGIN_API setComponentState (IBStream*) override    { return kResultOk; }   // the same object holds it

        int32 PLUGIN_API getParameterCount() override    { return (int32) numParams() + 2; }

        tresult PLUGIN_API getParameterInfo (int32 index, ParameterInfo& info) override
        {
            if (index < 0 || index >= getParameterCount())
                return kInvalidArgument;

            std::memset (&info, 0, sizeof (info));
            info.unitId = kRootUnitId;

            if (index >= (int32) numParams())
            {
                const bool bend = index == (int32) numParams();
                info.id = bend ? pitchBendId : modWheelId;
                toUtf16 (bend ? "Pitch Bend" : "Mod Wheel", info.title, 128);
                toUtf16 (bend ? "Bend" : "Mod", info.shortTitle, 128);
                info.defaultNormalizedValue = bend ? 0.5 : 0.0;
                info.flags = ParameterInfo::kIsHidden;
                return kResultOk;
            }

            const auto& p = paramInfo ((int) index);
            info.id = ids[(size_t) index];
            toUtf16 (p.name, info.title, 128);
            toUtf16 (p.name, info.shortTitle, 128);
            toUtf16 (p.unit, info.units, 128);
            info.stepCount = 0;
            info.defaultNormalizedValue = toNormalized ((int) index, p.def);
            info.flags = ParameterInfo::kCanAutomate;
            return kResultOk;
        }

        tresult PLUGIN_API getParamStringByValue (ParamID id, ParamValue normalized, String128 text) override
        {
            const auto index = indexOf (id);
            std::string shown;

            if (index < 0)
            {
                if (id != pitchBendId && id != modWheelId)
                    return kInvalidArgument;

                char buffer[64];
                std::snprintf (buffer, sizeof (buffer), "%.2f", id == pitchBendId ? normalized * 2.0 - 1.0 : normalized);
                shown = buffer;
            }
            else
            {
                shown = valueText (index, (double) toPlain (index, normalized));
            }

            toUtf16 (shown, text, 128);
            return kResultOk;
        }

        tresult PLUGIN_API getParamValueByString (ParamID id, TChar* text, ParamValue& normalized) override
        {
            const auto index = indexOf (id);

            if (index < 0 && id != pitchBendId && id != modWheelId)
                return kInvalidArgument;

            // Not strtod: it follows the host's locale ("0.5" stops at the '.' under a German one).
            const auto ascii = fromUtf16 (text);
            double parsed = 0.0;

            if (! numberIn (ascii, parsed))
                return kResultFalse;

            if (index >= 0)
                normalized = toNormalized (index, (float) parsed);
            else
                normalized = std::min (1.0, std::max (0.0, id == pitchBendId ? (parsed + 1.0) * 0.5 : parsed));

            return kResultOk;
        }

        ParamValue PLUGIN_API normalizedParamToPlain (ParamID id, ParamValue normalized) override
        {
            const auto index = indexOf (id);
            return index >= 0 ? (ParamValue) toPlain (index, normalized) : normalized;
        }

        ParamValue PLUGIN_API plainParamToNormalized (ParamID id, ParamValue plain) override
        {
            const auto index = indexOf (id);
            return index >= 0 ? toNormalized (index, (float) plain) : plain;
        }

        ParamValue PLUGIN_API getParamNormalized (ParamID id) override
        {
            const auto index = indexOf (id);

            if (index < 0)
                return id == pitchBendId ? 0.5 : 0.0;

            return toNormalized (index, valueOf (index));
        }

        tresult PLUGIN_API setParamNormalized (ParamID id, ParamValue value) override
        {
            const auto index = indexOf (id);

            if (index < 0)
                return id == pitchBendId || id == modWheelId ? kResultOk : kInvalidArgument;

            storeValue (index, toPlain (index, value));
            return kResultOk;
        }

        tresult PLUGIN_API setComponentHandler (IComponentHandler* newHandler) override
        {
            if (newHandler == handler)
                return kResultOk;

            if (handler != nullptr)
                handler->release();

            handler = newHandler;

            if (handler != nullptr)
                handler->addRef();

            return kResultOk;
        }

        IPlugView* PLUGIN_API createView (FIDString name) override;

        //======================================================================
        // IMidiMapping: pitch bend and the mod wheel arrive as these hidden parameters.
        tresult PLUGIN_API getMidiControllerAssignment (int32 busIndex, int16, CtrlNumber controller, ParamID& id) override
        {
            if (busIndex != 0)
                return kResultFalse;

            if (controller == kPitchBend)    { id = pitchBendId; return kResultTrue; }
            if (controller == kCtrlModWheel) { id = modWheelId; return kResultTrue; }
            return kResultFalse;
        }

        //======================================================================
        // The window's edits, on the UI thread.
        void beginEdit (int param) override
        {
            if (handler != nullptr && param >= 0 && param < (int) ids.size())
                handler->beginEdit (ids[(size_t) param]);
        }

        void setValue (int param, float value) override
        {
            if (param < 0 || param >= numParams())
                return;

            storeValue (param, value);

            if (handler != nullptr)
                handler->performEdit (ids[(size_t) param], toNormalized (param, valueOf (param)));
        }

        void endEdit (int param) override
        {
            if (handler != nullptr && param >= 0 && param < (int) ids.size())
                handler->endEdit (ids[(size_t) param]);
        }

    private:
        int indexOf (ParamID id) const
        {
            for (size_t i = 0; i < ids.size(); ++i)
                if (ids[i] == id)
                    return (int) i;

            return -1;
        }

        float toPlain (int index, ParamValue normalized) const
        {
            const auto& p = paramInfo (index);
            return clampValue (index, p.min + (p.max - p.min) * (float) std::min (1.0, std::max (0.0, normalized)));
        }

        ParamValue toNormalized (int index, float plain) const
        {
            const auto& p = paramInfo (index);
            return p.max > p.min ? std::min (1.0, std::max (0.0, (double) (plain - p.min) / (double) (p.max - p.min))) : 0.0;
        }

        std::atomic<uint32> refs { 1 };
        double sampleRate = 48000.0;
        int32 maxFrames = 512;
        std::vector<ParamID> ids;
        std::array<stella::Event, 1024> pending {};
        IComponentHandler* handler = nullptr;
    };

    //==========================================================================
   #if defined (__linux__)
    /** The host's run loop calls these: the window's timer, and its connection to the X server. */
    class RunLoopHandler final : public Linux::IEventHandler, public Linux::ITimerHandler
    {
    public:
        explicit RunLoopHandler (stella::gui::Window& w) : window (w) {}

        tresult PLUGIN_API queryInterface (const TUID iid, void** obj) override
        {
            QUERY_INTERFACE (iid, obj, FUnknown::iid, Linux::IEventHandler)
            QUERY_INTERFACE (iid, obj, Linux::IEventHandler::iid, Linux::IEventHandler)
            QUERY_INTERFACE (iid, obj, Linux::ITimerHandler::iid, Linux::ITimerHandler)
            *obj = nullptr;
            return kNoInterface;
        }

        // Owned by the view, which unregisters it before it goes.
        uint32 PLUGIN_API addRef() override     { return 1000; }
        uint32 PLUGIN_API release() override    { return 1000; }

        void PLUGIN_API onFDIsSet (Linux::FileDescriptor) override    { window.idle(); }
        void PLUGIN_API onTimer() override                             { window.idle(); }

    private:
        stella::gui::Window& window;
    };
   #endif

    /** The window, inside the host's. */
    class View final : public IPlugView
    {
    public:
        explicit View (Processor& p) : processor (p)    { processor.addRef(); }

        ~View()
        {
            removed();

            if (frame != nullptr)
                frame->release();

            processor.release();
        }

        tresult PLUGIN_API queryInterface (const TUID iid, void** obj) override
        {
            QUERY_INTERFACE (iid, obj, FUnknown::iid, IPlugView)
            QUERY_INTERFACE (iid, obj, IPlugView::iid, IPlugView)
            *obj = nullptr;
            return kNoInterface;
        }

        uint32 PLUGIN_API addRef() override    { return ++refs; }

        uint32 PLUGIN_API release() override
        {
            const auto left = --refs;

            if (left == 0)
                delete this;

            return left;
        }

        tresult PLUGIN_API isPlatformTypeSupported (FIDString type) override
        {
           #if defined (_WIN32)
            const auto* wanted = kPlatformTypeHWND;
           #elif defined (__APPLE__)
            const auto* wanted = kPlatformTypeNSView;
           #else
            const auto* wanted = kPlatformTypeX11EmbedWindowID;
           #endif

            return type != nullptr && std::strcmp (type, wanted) == 0 ? kResultTrue : kResultFalse;
        }

        tresult PLUGIN_API attached (void* parent, FIDString type) override
        {
            if (isPlatformTypeSupported (type) != kResultTrue || window != nullptr)
                return kResultFalse;

           #if defined (__linux__)
            // It runs on the host's run loop: without one it can't.
            if (frame == nullptr || frame->queryInterface (Linux::IRunLoop::iid, (void**) &runLoop) != kResultOk || runLoop == nullptr)
                return kResultFalse;
           #endif

            window = stella::gui::Window::create (processor);

            if (window == nullptr || ! window->attach (parent))
            {
                removed();
                return kResultFalse;
            }

           #if defined (__linux__)
            handler = std::make_unique<RunLoopHandler> (*window);
            runLoop->registerTimer (handler.get(), 33);

            if (const auto fd = window->getFileDescriptor(); fd >= 0)
                watching = runLoop->registerEventHandler (handler.get(), fd) == kResultOk;
           #endif

            return kResultOk;
        }

        tresult PLUGIN_API removed() override
        {
           #if defined (__linux__)
            if (runLoop != nullptr)
            {
                if (handler != nullptr)
                {
                    runLoop->unregisterTimer (handler.get());

                    if (watching)
                        runLoop->unregisterEventHandler (handler.get());
                }

                runLoop->release();
                runLoop = nullptr;
            }

            watching = false;
            handler.reset();
           #endif

            window.reset();
            return kResultOk;
        }

        tresult PLUGIN_API onWheel (float) override    { return kResultFalse; }

        // Keys the host hands the view: the window's while Registration is open (many hosts
        // keep the keyboard to themselves); otherwise the host's.
        tresult PLUGIN_API onKeyDown (char16 key, int16 keyCode, int16 modifiers) override
        {
            return window != nullptr && window->wantsKeys() && windowKey (*window, key, keyCode, modifiers) ? kResultTrue : kResultFalse;
        }

        tresult PLUGIN_API onKeyUp (char16, int16, int16) override
        {
            return window != nullptr && window->wantsKeys() ? kResultTrue : kResultFalse;
        }

        tresult PLUGIN_API onSize (ViewRect*) override                 { return kResultTrue; }
        tresult PLUGIN_API onFocus (TBool) override                    { return kResultTrue; }
        tresult PLUGIN_API canResize() override                        { return kResultFalse; }

        tresult PLUGIN_API setFrame (IPlugFrame* newFrame) override
        {
            if (newFrame != nullptr)
                newFrame->addRef();

            if (frame != nullptr)
                frame->release();

            frame = newFrame;
            return kResultTrue;
        }

        tresult PLUGIN_API getSize (ViewRect* size) override
        {
            if (size == nullptr)
                return kInvalidArgument;

            *size = ViewRect (0, 0, stella::gui::width, stella::gui::height);
            return kResultOk;
        }

        tresult PLUGIN_API checkSizeConstraint (ViewRect* rect) override
        {
            if (rect == nullptr)
                return kInvalidArgument;

            rect->right = rect->left + stella::gui::width;
            rect->bottom = rect->top + stella::gui::height;
            return kResultTrue;
        }

    private:
        std::atomic<uint32> refs { 1 };
        Processor& processor;
        IPlugFrame* frame = nullptr;
        std::unique_ptr<stella::gui::Window> window;

       #if defined (__linux__)
        Linux::IRunLoop* runLoop = nullptr;
        std::unique_ptr<RunLoopHandler> handler;
        bool watching = false;
       #endif
    };

    IPlugView* PLUGIN_API Processor::createView (FIDString name)
    {
        if (name != nullptr && std::strcmp (name, ViewType::kEditor) == 0 && stella::gui::hasWindow())
            return new View (*this);

        return nullptr;
    }

    //==========================================================================
    class Factory final : public IPluginFactory3
    {
    public:
        tresult PLUGIN_API queryInterface (const TUID iid, void** obj) override
        {
            QUERY_INTERFACE (iid, obj, FUnknown::iid, IPluginFactory)
            QUERY_INTERFACE (iid, obj, IPluginFactory::iid, IPluginFactory)
            QUERY_INTERFACE (iid, obj, IPluginFactory2::iid, IPluginFactory2)
            QUERY_INTERFACE (iid, obj, IPluginFactory3::iid, IPluginFactory3)
            *obj = nullptr;
            return kNoInterface;
        }

        uint32 PLUGIN_API addRef() override     { return 1; }   // one factory for the module's life
        uint32 PLUGIN_API release() override    { return 1; }

        tresult PLUGIN_API getFactoryInfo (PFactoryInfo* info) override
        {
            if (info == nullptr)
                return kInvalidArgument;

            *info = PFactoryInfo();
            copyAscii (info->vendor, sizeof (info->vendor), stella::info::vendor);
            info->flags = PFactoryInfo::kUnicode;
            return kResultOk;
        }

        int32 PLUGIN_API countClasses() override    { return 1; }

        tresult PLUGIN_API getClassInfo (int32 index, PClassInfo* info) override
        {
            if (index != 0 || info == nullptr)
                return kInvalidArgument;

            *info = PClassInfo();
            std::memcpy (info->cid, stella::info::vst3Id, sizeof (TUID));
            info->cardinality = PClassInfo::kManyInstances;
            copyAscii (info->category, sizeof (info->category), kVstAudioEffectClass);
            copyAscii (info->name, sizeof (info->name), stella::info::name);
            return kResultOk;
        }

        tresult PLUGIN_API getClassInfo2 (int32 index, PClassInfo2* info) override
        {
            if (index != 0 || info == nullptr)
                return kInvalidArgument;

            *info = PClassInfo2();
            std::memcpy (info->cid, stella::info::vst3Id, sizeof (TUID));
            info->cardinality = PClassInfo::kManyInstances;
            copyAscii (info->category, sizeof (info->category), kVstAudioEffectClass);
            copyAscii (info->name, sizeof (info->name), stella::info::name);
            info->classFlags = 0;   // one component, not distributable
            copyAscii (info->subCategories, sizeof (info->subCategories), subCategories());
            copyAscii (info->vendor, sizeof (info->vendor), stella::info::vendor);
            copyAscii (info->version, sizeof (info->version), stella::info::version);
            copyAscii (info->sdkVersion, sizeof (info->sdkVersion), kVstVersionString);
            return kResultOk;
        }

        tresult PLUGIN_API getClassInfoUnicode (int32 index, PClassInfoW* info) override
        {
            if (index != 0 || info == nullptr)
                return kInvalidArgument;

            *info = PClassInfoW();
            std::memcpy (info->cid, stella::info::vst3Id, sizeof (TUID));
            info->cardinality = PClassInfo::kManyInstances;
            copyAscii (info->category, sizeof (info->category), kVstAudioEffectClass);
            toUtf16 (stella::info::name, info->name, 64);
            info->classFlags = 0;
            copyAscii (info->subCategories, sizeof (info->subCategories), subCategories());
            toUtf16 (stella::info::vendor, info->vendor, 64);
            toUtf16 (stella::info::version, info->version, 64);
            toUtf16 (kVstVersionString, info->sdkVersion, 64);
            return kResultOk;
        }

        tresult PLUGIN_API createInstance (FIDString cid, FIDString iid, void** obj) override
        {
            if (obj == nullptr)
                return kInvalidArgument;

            *obj = nullptr;

            if (cid == nullptr || std::memcmp (cid, stella::info::vst3Id, sizeof (TUID)) != 0)
                return kNoInterface;

            auto* processor = new Processor();

            if (! processor->isBuilt())
            {
                processor->release();
                return kResultFalse;
            }

            TUID wanted;
            std::memcpy (wanted, iid, sizeof (TUID));
            const auto result = processor->queryInterface (wanted, obj);
            processor->release();   // the interface handed out holds the only reference now
            return result;
        }

        tresult PLUGIN_API setHostContext (FUnknown*) override    { return kResultOk; }
    };

    Factory factory;
}

//==============================================================================
extern "C"
{
    SMTG_EXPORT_SYMBOL IPluginFactory* PLUGIN_API GetPluginFactory()
    {
        return &factory;
    }

   #if defined (_WIN32)
    SMTG_EXPORT_SYMBOL bool InitDll()    { return true; }
    SMTG_EXPORT_SYMBOL bool ExitDll()    { return true; }
   #elif defined (__APPLE__)
    SMTG_EXPORT_SYMBOL bool bundleEntry (void*)    { return true; }
    SMTG_EXPORT_SYMBOL bool bundleExit()           { return true; }
   #else
    SMTG_EXPORT_SYMBOL bool ModuleEntry (void*)    { return true; }
    SMTG_EXPORT_SYMBOL bool ModuleExit()           { return true; }
   #endif
}
