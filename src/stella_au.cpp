// D:\workspace\Fanan_web_hub\src\stella_au.cpp
//
// An exported plugin's Audio Unit side (macOS, version 2), when it's exported on a Mac: no JUCE
// and no SDK classes, just Apple's Audio Unit API (AudioToolbox) around the plugin's core
// (stella_plugin.h). The bundle's Info.plist (written by Export) names the factory below.
//
// An instrument is a music device ('aumu': notes in, audio out), an effect an effect ('aufx':
// audio in and out), a widget a music effect ('aumf': audio and notes in). Parameter ids are
// the same stable hashes as the other formats (kept below 2^31); the state is the same
// "id=value" text, kept in the preset dictionary's data. Its window is the plugin's own view
// (stella_gui_mac.mm), found by the host through the bundle and a view factory class whose name
// is the plugin's own (STELLA_OBJC_SUFFIX), so two exported plugins never mix.

#include "stella_plugin.h"

#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/AudioUnitUtilities.h>
#include <CoreFoundation/CoreFoundation.h>

#include <dlfcn.h>

#include <atomic>
#include <mutex>

#ifndef STELLA_OBJC_SUFFIX
 #define STELLA_OBJC_SUFFIX Default
#endif

#define STELLA_AU_TEXT2(x) #x
#define STELLA_AU_TEXT(x) STELLA_AU_TEXT2(x)

namespace
{
    using stella::plugin::Core;

    /** Private: the plugin's link for its view (stella_gui_mac.mm), within the same process. */
    constexpr AudioUnitPropertyID windowLinkProperty = 64917;

    constexpr OSStatus noError = 0;

    AudioUnitParameterID idFor (const std::string& text)
    {
        return stella::plugin::hashOf (text) & 0x7fffffffu;
    }

    CFStringRef makeString (const std::string& text)
    {
        auto* result = CFStringCreateWithCString (nullptr, text.c_str(), kCFStringEncodingUTF8);
        return result != nullptr ? result : CFStringCreateWithCString (nullptr, "", kCFStringEncodingUTF8);
    }

    std::string fromString (CFStringRef text)
    {
        if (text == nullptr)
            return {};

        char buffer[512] {};
        CFStringGetCString (text, buffer, sizeof (buffer), kCFStringEncodingUTF8);
        return buffer;
    }

    void addNumber (CFMutableDictionaryRef dictionary, const char* key, SInt32 value)
    {
        auto* k = CFStringCreateWithCString (nullptr, key, kCFStringEncodingUTF8);
        auto* n = CFNumberCreate (nullptr, kCFNumberSInt32Type, &value);
        CFDictionarySetValue (dictionary, k, n);
        CFRelease (n);
        CFRelease (k);
    }

    bool numberIn (CFDictionaryRef dictionary, const char* key, SInt32& value)
    {
        auto* k = CFStringCreateWithCString (nullptr, key, kCFStringEncodingUTF8);
        const auto* n = static_cast<CFNumberRef> (CFDictionaryGetValue (dictionary, k));
        CFRelease (k);
        return n != nullptr && CFGetTypeID (n) == CFNumberGetTypeID() && CFNumberGetValue (n, kCFNumberSInt32Type, &value);
    }

    /** A parameter's unit as Audio Units name them; anything else in words of its own. */
    AudioUnitParameterUnit unitFor (const std::string& unit)
    {
        if (unit == "Hz")                   return kAudioUnitParameterUnit_Hertz;
        if (unit == "dB")                   return kAudioUnitParameterUnit_Decibels;
        if (unit == "%")                    return kAudioUnitParameterUnit_Percent;
        if (unit == "ms")                   return kAudioUnitParameterUnit_Milliseconds;
        if (unit == "s" || unit == "sec")   return kAudioUnitParameterUnit_Seconds;
        if (unit == "st" || unit == "semitones") return kAudioUnitParameterUnit_RelativeSemiTones;
        if (unit == "cents" || unit == "ct")     return kAudioUnitParameterUnit_Cents;
        if (unit == "BPM" || unit == "bpm")      return kAudioUnitParameterUnit_BPM;
        return unit.empty() ? kAudioUnitParameterUnit_Generic : kAudioUnitParameterUnit_CustomUnit;
    }

    struct Listener
    {
        AudioUnitPropertyID id;
        AudioUnitPropertyListenerProc proc;
        void* data;
    };

    struct RenderNotify
    {
        AURenderCallback proc;
        void* data;
    };

    /** A render notification's place: changed on the host's threads while render reads it
        without a lock (its version is odd while it changes; render skips it then). */
    struct NotifySlot
    {
        std::atomic<AURenderCallback> proc { nullptr };
        std::atomic<void*> data { nullptr };
        std::atomic<std::uint32_t> version { 0 };

        void set (AURenderCallback newProc, void* newData) noexcept
        {
            const auto v = version.load (std::memory_order_relaxed);
            version.store (v + 1, std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_release);
            proc.store (newProc, std::memory_order_relaxed);
            data.store (newData, std::memory_order_relaxed);
            version.store (v + 2, std::memory_order_release);
        }

        bool read (RenderNotify& n) const noexcept
        {
            const auto v = version.load (std::memory_order_acquire);
            n.proc = proc.load (std::memory_order_relaxed);
            n.data = data.load (std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_acquire);
            return (v & 1u) == 0 && n.proc != nullptr && version.load (std::memory_order_relaxed) == v;
        }
    };

    /** Two channels' worth of AudioBufferList. */
    struct StereoBufferList
    {
        UInt32 mNumberBuffers;
        AudioBuffer mBuffers[2];
    };

    //==========================================================================
    struct Instance final : public Core
    {
        Instance (AudioComponentInstance c) : component (c)
        {
            if (! build())
                return;

            for (int i = 0; i < numParams(); ++i)
            {
                ids.push_back (idFor (paramInfo (i).id));
                unitNames.push_back (makeString (paramInfo (i).unit));
            }

            for (int i = 0; i < stella::gui::numPresets; ++i)
                presetNames.push_back (makeString (stella::gui::presets[i].name != nullptr ? stella::gui::presets[i].name : "Preset"));

            for (int i = 0; i < (int) presetNames.size(); ++i)
                factoryPresets.push_back ({ (SInt32) i, presetNames[(size_t) i] });

            currentPresetName = makeString (stella::info::name);
        }

        ~Instance() override
        {
            // Open windows let go of it first.
           #if defined (__APPLE__)
            const auto open = views;

            for (auto* view : open)
                stella::gui::releaseMacViewHost (view);
           #endif

            for (auto* s : unitNames)    CFRelease (s);
            for (auto* s : presetNames)  CFRelease (s);

            if (currentPresetName != nullptr)
                CFRelease (currentPresetName);
        }

        int indexOf (AudioUnitParameterID id) const
        {
            for (size_t i = 0; i < ids.size(); ++i)
                if (ids[i] == id)
                    return (int) i;

            return -1;
        }

        UInt32 elementCount (AudioUnitScope scope) const
        {
            if (scope == kAudioUnitScope_Global)  return 1;
            if (scope == kAudioUnitScope_Input)   return stella::info::isInstrument ? 0 : 1;
            if (scope == kAudioUnitScope_Output)  return 1;
            return 0;
        }

        AudioStreamBasicDescription formatOf (UInt32 channels) const
        {
            AudioStreamBasicDescription f {};
            f.mSampleRate = sampleRate;
            f.mFormatID = kAudioFormatLinearPCM;
            f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
            f.mBytesPerPacket = 4;
            f.mFramesPerPacket = 1;
            f.mBytesPerFrame = 4;
            f.mChannelsPerFrame = channels;
            f.mBitsPerChannel = 32;
            return f;
        }

        void propertyChanged (AudioUnitPropertyID id, AudioUnitScope scope = kAudioUnitScope_Global, AudioUnitElement element = 0)
        {
            const auto now = listeners;

            for (const auto& l : now)
                if (l.id == id)
                    l.proc (l.data, component, id, scope, element);
        }

        /** The host hears every value changed at once (a preset, a state). */
        void allValuesChanged()
        {
            AudioUnitParameter any { component, kAUParameterListener_AnyParameter, kAudioUnitScope_Global, 0 };
            AUParameterListenerNotify (nullptr, nullptr, &any);
        }

        void tellHost (int index, AudioUnitEventType type)
        {
            if (index < 0 || index >= (int) ids.size())
                return;

            AudioUnitEvent event {};
            event.mEventType = type;
            event.mArgument.mParameter = { component, ids[(size_t) index], kAudioUnitScope_Global, 0 };
            AUEventListenerNotify (nullptr, nullptr, &event);
        }

        // The window's edits, on the main thread: the host hears each, to record automation.
        void beginEdit (int param) override    { tellHost (param, kAudioUnitEvent_BeginParameterChangeGesture); }
        void endEdit (int param) override      { tellHost (param, kAudioUnitEvent_EndParameterChangeGesture); }

        void setValue (int param, float value) override
        {
            if (param < 0 || param >= numParams())
                return;

            storeValue (param, value);
            tellHost (param, kAudioUnitEvent_ParameterValueChange);
        }

        void setCurrentPreset (int number, CFStringRef name)
        {
            currentPreset = number;

            if (name != nullptr)
                CFRetain (name);

            if (currentPresetName != nullptr)
                CFRelease (currentPresetName);

            currentPresetName = name != nullptr ? name : makeString (stella::info::name);
        }

        void applyFactoryPreset (int index)
        {
            const auto& preset = stella::gui::presets[index];

            for (int v = 0; v < preset.numValues; ++v)
                if (preset.values[v].param != nullptr)
                    storeValue (indexOfParam (preset.values[v].param), preset.values[v].value);

            setCurrentPreset (index, presetNames[(size_t) index]);
            allValuesChanged();
        }

        stella::plugin::Transport transport() const
        {
            stella::plugin::Transport t;

            if (hostCallbacks.beatAndTempoProc != nullptr)
            {
                Float64 beat = 0.0, tempo = 0.0;

                if (hostCallbacks.beatAndTempoProc (hostCallbacks.hostUserData, &beat, &tempo) == noError)
                {
                    t.beat = beat;
                    t.tempo = tempo;
                }
            }

            if (hostCallbacks.musicalTimeLocationProc != nullptr)
            {
                UInt32 toNextBeat = 0, denominator = 4;
                Float32 numerator = 4.0f;
                Float64 downBeat = 0.0;

                if (hostCallbacks.musicalTimeLocationProc (hostCallbacks.hostUserData, &toNextBeat, &numerator, &denominator, &downBeat) == noError)
                {
                    t.numerator = (int) numerator;
                    t.denominator = (int) denominator;
                    t.barStart = downBeat;
                }
            }

            Boolean playing = false;

            if (hostCallbacks.transportStateProc2 != nullptr)
            {
                if (hostCallbacks.transportStateProc2 (hostCallbacks.hostUserData, &playing, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == noError)
                    t.playing = playing;
            }
            else if (hostCallbacks.transportStateProc != nullptr)
            {
                if (hostCallbacks.transportStateProc (hostCallbacks.hostUserData, &playing, nullptr, nullptr, nullptr, nullptr, nullptr) == noError)
                    t.playing = playing;
            }

            return t;
        }

        AudioComponentInstance component;
        std::vector<AudioUnitParameterID> ids;
        std::vector<CFStringRef> unitNames, presetNames;
        std::vector<AUPreset> factoryPresets;
        int currentPreset = -1;
        CFStringRef currentPresetName = nullptr;

        bool initialized = false, bypassed = false;
        Float64 sampleRate = 44100.0;
        UInt32 maxFramesPerSlice = 1156;
        UInt32 inputChannels = 2, outputChannels = 2;
        UInt32 offline = 0, shouldAllocate = 1;

        AURenderCallbackStruct inputCallback {};
        AudioUnitConnection connection {};
        bool connected = false;
        HostCallbackInfo hostCallbacks {};

        std::vector<Listener> listeners;
        static constexpr int maxNotifies = 32;
        std::array<NotifySlot, maxNotifies> notifies;   // render reads them without a lock
        std::mutex notifyChanges;                       // Add / RemoveRenderNotify against each other (never render)
        bool wasBypassed = false;                       // render's own

        // MIDI for the next render (the host sends it on the render thread, just before).
        std::array<stella::Event, 1024> midi {};
        int numMidi = 0;
        std::array<stella::Event, 1024> pending {};

        std::vector<float> inputBuffers[2], outputBuffers[2];

        // Its windows, while they're open (main thread).
        std::vector<void*> views;
        stella::gui::MacViewLink link {};
    };

    /** What the host holds: the plug-in interface first, then the plugin. */
    struct PlugInHandle
    {
        AudioComponentPlugInInterface plugIn;
        Instance* instance;
    };

    Instance* self (void* plugIn)
    {
        return reinterpret_cast<PlugInHandle*> (plugIn)->instance;
    }

    //==========================================================================
    // Properties
    OSStatus propertyInfo (Instance& p, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, UInt32& size, bool& writable)
    {
        writable = false;

        // Scope and element first: an element the scope hasn't is no such element.
        const bool global = scope == kAudioUnitScope_Global;
        const bool inOut = (scope == kAudioUnitScope_Input || scope == kAudioUnitScope_Output) && element < p.elementCount (scope);

        switch (id)
        {
            case kAudioUnitProperty_ClassInfo:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (CFPropertyListRef); writable = true; return noError;

            case kAudioUnitProperty_MakeConnection:
                if (scope != kAudioUnitScope_Input || stella::info::isInstrument) return kAudioUnitErr_InvalidScope;
                if (element != 0) return kAudioUnitErr_InvalidElement;
                size = sizeof (AudioUnitConnection); writable = true; return noError;

            case kAudioUnitProperty_SampleRate:
                if (! global && ! inOut) return scope == kAudioUnitScope_Input || scope == kAudioUnitScope_Output ? kAudioUnitErr_InvalidElement : kAudioUnitErr_InvalidScope;
                size = sizeof (Float64); writable = true; return noError;

            case kAudioUnitProperty_ParameterList:
                if (! global) { size = 0; return noError; }   // nothing in the other scopes
                size = (UInt32) (sizeof (AudioUnitParameterID) * p.ids.size()); return noError;

            case kAudioUnitProperty_ParameterInfo:
                if (! global) return kAudioUnitErr_InvalidScope;
                if (p.indexOf (element) < 0) return kAudioUnitErr_InvalidParameter;
                size = sizeof (AudioUnitParameterInfo); return noError;

            case kAudioUnitProperty_StreamFormat:
                if (! inOut) return scope == kAudioUnitScope_Input || scope == kAudioUnitScope_Output ? kAudioUnitErr_InvalidElement : kAudioUnitErr_InvalidScope;
                size = sizeof (AudioStreamBasicDescription); writable = true; return noError;

            case kAudioUnitProperty_ElementCount:
                size = sizeof (UInt32); return noError;

            case kAudioUnitProperty_Latency:
            case kAudioUnitProperty_TailTime:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (Float64); return noError;

            case kAudioUnitProperty_SupportedNumChannels:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = (UInt32) (sizeof (AUChannelInfo) * 2); return noError;

            case kAudioUnitProperty_MaximumFramesPerSlice:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (UInt32); writable = true; return noError;

            case kAudioUnitProperty_BypassEffect:
                if (stella::info::isInstrument) return kAudioUnitErr_InvalidProperty;
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (UInt32); writable = true; return noError;

            case kAudioUnitProperty_LastRenderError:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (OSStatus); return noError;

            case kAudioUnitProperty_SetRenderCallback:
                if (scope != kAudioUnitScope_Input || stella::info::isInstrument) return kAudioUnitErr_InvalidScope;
                if (element != 0) return kAudioUnitErr_InvalidElement;
                size = sizeof (AURenderCallbackStruct); writable = true; return noError;

            case kAudioUnitProperty_FactoryPresets:
                if (! global) return kAudioUnitErr_InvalidScope;
                if (p.factoryPresets.empty()) return kAudioUnitErr_InvalidProperty;
                size = sizeof (CFArrayRef); return noError;

            case kAudioUnitProperty_HostCallbacks:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (HostCallbackInfo); writable = true; return noError;

            case kAudioUnitProperty_InPlaceProcessing:
            case kAudioUnitProperty_OfflineRender:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (UInt32); writable = true; return noError;

            case kAudioUnitProperty_ShouldAllocateBuffer:
                if (! inOut) return kAudioUnitErr_InvalidScope;
                size = sizeof (UInt32); writable = true; return noError;

            case kAudioUnitProperty_CocoaUI:
                if (! global) return kAudioUnitErr_InvalidScope;
                if (! stella::gui::hasWindow()) return kAudioUnitErr_InvalidProperty;
                size = sizeof (AudioUnitCocoaViewInfo); return noError;

            case kAudioUnitProperty_ParameterStringFromValue:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (AudioUnitParameterStringFromValue); return noError;

            case kAudioUnitProperty_ParameterValueFromString:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (AudioUnitParameterValueFromString); return noError;

            case kAudioUnitProperty_PresentPreset:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (AUPreset); writable = true; return noError;

            case kAudioUnitProperty_ContextName:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (CFStringRef); writable = true; return noError;

            case windowLinkProperty:
                if (! global) return kAudioUnitErr_InvalidScope;
                size = sizeof (void*); return noError;

            default:
                return kAudioUnitErr_InvalidProperty;
        }
    }

    OSStatus getProperty (Instance& p, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, void* data)
    {
        switch (id)
        {
            case kAudioUnitProperty_ClassInfo:
            {
                auto* dictionary = CFDictionaryCreateMutable (nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                addNumber (dictionary, kAUPresetVersionKey, 0);
                addNumber (dictionary, kAUPresetTypeKey, (SInt32) stella::info::auType);
                addNumber (dictionary, kAUPresetSubtypeKey, (SInt32) stella::info::auSubtype);
                addNumber (dictionary, kAUPresetManufacturerKey, (SInt32) stella::info::auManufacturer);

                auto* nameKey = CFStringCreateWithCString (nullptr, kAUPresetNameKey, kCFStringEncodingUTF8);
                CFDictionarySetValue (dictionary, nameKey, p.currentPresetName);
                CFRelease (nameKey);

                const auto text = p.saveState();
                auto* bytes = CFDataCreate (nullptr, reinterpret_cast<const UInt8*> (text.data()), (CFIndex) text.size());
                auto* dataKey = CFStringCreateWithCString (nullptr, kAUPresetDataKey, kCFStringEncodingUTF8);
                CFDictionarySetValue (dictionary, dataKey, bytes);
                CFRelease (dataKey);
                CFRelease (bytes);

                *static_cast<CFPropertyListRef*> (data) = dictionary;   // the host releases it
                return noError;
            }

            case kAudioUnitProperty_SampleRate:
                *static_cast<Float64*> (data) = p.sampleRate;
                return noError;

            case kAudioUnitProperty_ParameterList:
                if (scope == kAudioUnitScope_Global)
                    std::memcpy (data, p.ids.data(), sizeof (AudioUnitParameterID) * p.ids.size());
                return noError;

            case kAudioUnitProperty_ParameterInfo:
            {
                const auto index = p.indexOf (element);

                if (index < 0)
                    return kAudioUnitErr_InvalidParameter;

                const auto& param = p.paramInfo (index);
                AudioUnitParameterInfo info {};
                std::strncpy (info.name, param.name.c_str(), sizeof (info.name) - 1);
                info.cfNameString = makeString (param.name);   // the host releases it (CFNameRelease)
                info.unit = unitFor (param.unit);
                info.unitName = info.unit == kAudioUnitParameterUnit_CustomUnit ? p.unitNames[(size_t) index] : nullptr;
                info.minValue = param.min;
                info.maxValue = param.max;
                info.defaultValue = param.def;
                info.flags = kAudioUnitParameterFlag_IsReadable | kAudioUnitParameterFlag_IsWritable
                           | kAudioUnitParameterFlag_HasCFNameString | kAudioUnitParameterFlag_CFNameRelease;
                *static_cast<AudioUnitParameterInfo*> (data) = info;
                return noError;
            }

            case kAudioUnitProperty_StreamFormat:
                *static_cast<AudioStreamBasicDescription*> (data) = p.formatOf (scope == kAudioUnitScope_Input ? p.inputChannels : p.outputChannels);
                return noError;

            case kAudioUnitProperty_ElementCount:
                *static_cast<UInt32*> (data) = p.elementCount (scope);
                return noError;

            case kAudioUnitProperty_Latency:
                *static_cast<Float64*> (data) = 0.0;
                return noError;

            case kAudioUnitProperty_TailTime:
                *static_cast<Float64*> (data) = 10.0;   // releases and echoes ring on
                return noError;

            case kAudioUnitProperty_SupportedNumChannels:
            {
                auto* channels = static_cast<AUChannelInfo*> (data);

                if (stella::info::isInstrument)
                {
                    channels[0] = { 0, 2 };
                    channels[1] = { 0, 1 };
                }
                else
                {
                    channels[0] = { 2, 2 };
                    channels[1] = { 1, 1 };
                }

                return noError;
            }

            case kAudioUnitProperty_MaximumFramesPerSlice:
                *static_cast<UInt32*> (data) = p.maxFramesPerSlice;
                return noError;

            case kAudioUnitProperty_BypassEffect:
                *static_cast<UInt32*> (data) = p.bypassed ? 1 : 0;
                return noError;

            case kAudioUnitProperty_LastRenderError:
                *static_cast<OSStatus*> (data) = noError;
                return noError;

            case kAudioUnitProperty_FactoryPresets:
            {
                std::vector<const void*> items;

                for (auto& preset : p.factoryPresets)
                    items.push_back (&preset);

                // An array of the presets themselves (no callbacks): it's the host's to release.
                *static_cast<CFArrayRef*> (data) = CFArrayCreate (nullptr, items.data(), (CFIndex) items.size(), nullptr);
                return noError;
            }

            case kAudioUnitProperty_HostCallbacks:
                *static_cast<HostCallbackInfo*> (data) = p.hostCallbacks;
                return noError;

            case kAudioUnitProperty_InPlaceProcessing:
                *static_cast<UInt32*> (data) = 0;
                return noError;

            case kAudioUnitProperty_OfflineRender:
                *static_cast<UInt32*> (data) = p.offline;
                return noError;

            case kAudioUnitProperty_ShouldAllocateBuffer:
                *static_cast<UInt32*> (data) = p.shouldAllocate;
                return noError;

            case kAudioUnitProperty_CocoaUI:
            {
                // The bundle this binary is in (<name>.component/Contents/MacOS/<name>), and its view factory.
                Dl_info where {};

                if (dladdr (reinterpret_cast<const void*> (&propertyInfo), &where) == 0 || where.dli_fname == nullptr)
                    return kAudioUnitErr_InvalidProperty;

                std::string path (where.dli_fname);

                for (int up = 0; up < 3; ++up)
                    if (const auto slash = path.find_last_of ('/'); slash != std::string::npos)
                        path.resize (slash);

                auto* info = static_cast<AudioUnitCocoaViewInfo*> (data);
                info->mCocoaAUViewBundleLocation = CFURLCreateFromFileSystemRepresentation (nullptr, reinterpret_cast<const UInt8*> (path.c_str()),
                                                                                            (CFIndex) path.size(), true);
                info->mCocoaAUViewClass[0] = makeString ("StellaViewFactory_" STELLA_AU_TEXT (STELLA_OBJC_SUFFIX));
                return info->mCocoaAUViewBundleLocation != nullptr ? noError : kAudioUnitErr_InvalidProperty;
            }

            case kAudioUnitProperty_ParameterStringFromValue:
            {
                auto* request = static_cast<AudioUnitParameterStringFromValue*> (data);
                const auto index = p.indexOf (request->inParamID);

                if (index < 0)
                    return kAudioUnitErr_InvalidParameter;

                request->outString = makeString (p.valueText (index, request->inValue != nullptr ? *request->inValue : p.valueOf (index)));
                return noError;
            }

            case kAudioUnitProperty_ParameterValueFromString:
            {
                auto* request = static_cast<AudioUnitParameterValueFromString*> (data);
                const auto index = p.indexOf (request->inParamID);

                if (index < 0)
                    return kAudioUnitErr_InvalidParameter;

                // Whatever the host's locale ('.' or ',' as the point): strtod follows it.
                const auto text = fromString (request->inString);
                bool found = false;
                const auto parsed = stella::plugin::numberFrom (text.c_str(), &found);

                if (! found)
                    return kAudioUnitErr_InvalidPropertyValue;

                request->outValue = p.clampValue (index, (float) parsed);
                return noError;
            }

            case kAudioUnitProperty_PresentPreset:
            {
                CFRetain (p.currentPresetName);   // the host releases the name
                *static_cast<AUPreset*> (data) = { (SInt32) p.currentPreset, p.currentPresetName };
                return noError;
            }

            case windowLinkProperty:
                p.link.host = &p;
                p.link.context = &p;
                p.link.opened = [] (void* context, void* view) { static_cast<Instance*> (context)->views.push_back (view); };
                p.link.gone = [] (void* context, void* view)
                {
                    auto& open = static_cast<Instance*> (context)->views;
                    open.erase (std::remove (open.begin(), open.end(), view), open.end());
                };

                *static_cast<void**> (data) = &p.link;
                return noError;

            default:
                return kAudioUnitErr_InvalidProperty;
        }
    }

    OSStatus setProperty (Instance& p, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, const void* data, UInt32 size)
    {
        UInt32 expected = 0;
        bool writable = false;

        if (const auto status = propertyInfo (p, id, scope, element, expected, writable); status != noError)
            return status;

        if (! writable)
            return kAudioUnitErr_PropertyNotWritable;

        if (data == nullptr || size < expected)
            return kAudioUnitErr_InvalidPropertyValue;

        switch (id)
        {
            case kAudioUnitProperty_ClassInfo:
            {
                const auto* list = *static_cast<const CFPropertyListRef*> (data);

                if (list == nullptr || CFGetTypeID (list) != CFDictionaryGetTypeID())
                    return kAudioUnitErr_InvalidPropertyValue;

                const auto* dictionary = static_cast<CFDictionaryRef> (list);
                SInt32 type = 0, subtype = 0, manufacturer = 0;

                // Another plugin's preset isn't this one's.
                if ((numberIn (dictionary, kAUPresetTypeKey, type) && (OSType) type != stella::info::auType)
                    || (numberIn (dictionary, kAUPresetSubtypeKey, subtype) && (OSType) subtype != stella::info::auSubtype)
                    || (numberIn (dictionary, kAUPresetManufacturerKey, manufacturer) && (OSType) manufacturer != stella::info::auManufacturer))
                    return kAudioUnitErr_InvalidPropertyValue;

                auto* dataKey = CFStringCreateWithCString (nullptr, kAUPresetDataKey, kCFStringEncodingUTF8);
                const auto* bytes = static_cast<CFDataRef> (CFDictionaryGetValue (dictionary, dataKey));
                CFRelease (dataKey);

                if (bytes != nullptr && CFGetTypeID (bytes) == CFDataGetTypeID())
                    p.loadState (std::string (reinterpret_cast<const char*> (CFDataGetBytePtr (bytes)), (size_t) CFDataGetLength (bytes)));

                auto* nameKey = CFStringCreateWithCString (nullptr, kAUPresetNameKey, kCFStringEncodingUTF8);
                const auto* name = static_cast<CFStringRef> (CFDictionaryGetValue (dictionary, nameKey));
                CFRelease (nameKey);

                p.setCurrentPreset (-1, name != nullptr && CFGetTypeID (name) == CFStringGetTypeID() ? name : nullptr);
                p.allValuesChanged();
                p.propertyChanged (kAudioUnitProperty_PresentPreset);
                return noError;
            }

            case kAudioUnitProperty_MakeConnection:
            {
                p.connection = *static_cast<const AudioUnitConnection*> (data);
                p.connected = p.connection.sourceAudioUnit != nullptr;

                if (p.connected)
                    p.inputCallback = {};

                return noError;
            }

            case kAudioUnitProperty_SetRenderCallback:
            {
                p.inputCallback = *static_cast<const AURenderCallbackStruct*> (data);

                if (p.inputCallback.inputProc != nullptr)
                    p.connected = false;

                return noError;
            }

            case kAudioUnitProperty_SampleRate:
            {
                const auto rate = *static_cast<const Float64*> (data);

                if (rate <= 0.0)
                    return kAudioUnitErr_InvalidPropertyValue;

                if (rate == p.sampleRate)
                    return noError;

                if (p.initialized)
                    return kAudioUnitErr_Initialized;

                p.sampleRate = rate;
                p.propertyChanged (kAudioUnitProperty_SampleRate, scope, element);
                return noError;
            }

            case kAudioUnitProperty_StreamFormat:
            {
                const auto& f = *static_cast<const AudioStreamBasicDescription*> (data);
                const bool floats = f.mFormatID == kAudioFormatLinearPCM && (f.mFormatFlags & kAudioFormatFlagIsFloat) != 0
                                 && (f.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0 && f.mBitsPerChannel == 32;

                if (! floats || f.mChannelsPerFrame < 1 || f.mChannelsPerFrame > 2 || f.mSampleRate <= 0.0)
                    return kAudioUnitErr_FormatNotSupported;

                auto& channels = scope == kAudioUnitScope_Input ? p.inputChannels : p.outputChannels;

                if (f.mChannelsPerFrame == channels && f.mSampleRate == p.sampleRate)
                    return noError;

                if (p.initialized)
                    return kAudioUnitErr_Initialized;

                channels = f.mChannelsPerFrame;
                p.sampleRate = f.mSampleRate;
                p.propertyChanged (kAudioUnitProperty_StreamFormat, scope, element);
                return noError;
            }

            case kAudioUnitProperty_MaximumFramesPerSlice:
            {
                const auto frames = *static_cast<const UInt32*> (data);

                if (frames == 0)
                    return kAudioUnitErr_InvalidPropertyValue;

                if (frames == p.maxFramesPerSlice)
                    return noError;

                // Only while uninitialized, as AUBase: render may be running now, and the
                // buffers it uses would be reallocated under it.
                if (p.initialized)
                    return kAudioUnitErr_Initialized;

                p.maxFramesPerSlice = frames;
                p.propertyChanged (kAudioUnitProperty_MaximumFramesPerSlice);
                return noError;
            }

            case kAudioUnitProperty_BypassEffect:
                p.bypassed = *static_cast<const UInt32*> (data) != 0;
                return noError;

            case kAudioUnitProperty_HostCallbacks:
                p.hostCallbacks = {};
                std::memcpy (&p.hostCallbacks, data, std::min ((size_t) size, sizeof (HostCallbackInfo)));
                return noError;

            case kAudioUnitProperty_InPlaceProcessing:
                return noError;

            case kAudioUnitProperty_OfflineRender:
                p.offline = *static_cast<const UInt32*> (data);
                return noError;

            case kAudioUnitProperty_ShouldAllocateBuffer:
                p.shouldAllocate = *static_cast<const UInt32*> (data);
                return noError;

            case kAudioUnitProperty_PresentPreset:
            {
                const auto& preset = *static_cast<const AUPreset*> (data);

                // One of the plugin's own: its values. One of the host's (below 0): the host sets the state.
                if (preset.presetNumber >= 0)
                {
                    if (preset.presetNumber >= (SInt32) p.factoryPresets.size())
                        return kAudioUnitErr_InvalidPropertyValue;

                    p.applyFactoryPreset ((int) preset.presetNumber);
                }
                else
                {
                    p.setCurrentPreset ((int) preset.presetNumber, preset.presetName);
                }

                return noError;
            }

            case kAudioUnitProperty_ContextName:
                return noError;

            default:
                return kAudioUnitErr_InvalidProperty;
        }
    }

    //==========================================================================
    // Rendering
    OSStatus render (Instance& p, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* timeStamp, UInt32 bus, UInt32 frames, AudioBufferList* io)
    {
        if (! p.initialized)
            return kAudioUnitErr_Uninitialized;

        if (bus != 0)
            return kAudioUnitErr_InvalidElement;

        if (frames > p.maxFramesPerSlice)
            return kAudioUnitErr_TooManyFramesToProcess;

        if (io == nullptr || io->mNumberBuffers == 0)
            return kAudio_ParamError;

        AudioUnitRenderActionFlags notifyFlags = (flags != nullptr ? *flags : 0) | kAudioUnitRenderAction_PreRender;

        // Who's notified, as they are now: the same ones before and after.
        RenderNotify notified[Instance::maxNotifies];
        int numNotified = 0;

        for (const auto& slot : p.notifies)
            if (slot.read (notified[numNotified]))
                ++numNotified;

        for (int i = 0; i < numNotified; ++i)
            notified[i].proc (notified[i].data, &notifyFlags, timeStamp, bus, frames, io);

        // The input, pulled from what's connected.
        StereoBufferList input {};
        const float* inputs[2] {};
        int inputChannels = 0;

        if (! stella::info::isInstrument)
        {
            input.mNumberBuffers = p.inputChannels;

            for (UInt32 c = 0; c < p.inputChannels; ++c)
                input.mBuffers[c] = { 1, frames * (UInt32) sizeof (float), p.inputBuffers[c].data() };

            AudioUnitRenderActionFlags inputFlags = 0;
            OSStatus status = kAudioUnitErr_NoConnection;

            if (p.connected)
                status = AudioUnitRender (p.connection.sourceAudioUnit, &inputFlags, timeStamp, p.connection.sourceOutputNumber, frames,
                                          reinterpret_cast<AudioBufferList*> (&input));
            else if (p.inputCallback.inputProc != nullptr)
                status = p.inputCallback.inputProc (p.inputCallback.inputProcRefCon, &inputFlags, timeStamp, 0, frames,
                                                    reinterpret_cast<AudioBufferList*> (&input));

            if (status != noError)
            {
                p.numMidi = 0;   // dropped, as when bypassed
                return status;
            }

            for (UInt32 c = 0; c < p.inputChannels; ++c)
                inputs[c] = static_cast<const float*> (input.mBuffers[c].mData);

            inputChannels = (int) p.inputChannels;
        }

        // The output: the host's buffers, or ours when it gives none.
        float* outputs[2] {};
        const auto outputChannels = (int) std::min (io->mNumberBuffers, (UInt32) 2);

        for (int c = 0; c < outputChannels; ++c)
        {
            if (io->mBuffers[c].mData == nullptr)
                io->mBuffers[c].mData = p.outputBuffers[c].data();

            io->mBuffers[c].mDataByteSize = frames * (UInt32) sizeof (float);
            outputs[c] = static_cast<float*> (io->mBuffers[c].mData);
        }

        if (p.bypassed)
        {
            for (int c = 0; c < outputChannels; ++c)
            {
                const auto* from = inputChannels > 0 ? inputs[std::min (c, inputChannels - 1)] : nullptr;

                if (from != nullptr && from != outputs[c])
                    std::memcpy (outputs[c], from, sizeof (float) * frames);
                else if (from == nullptr)
                    std::memset (outputs[c], 0, sizeof (float) * frames);
            }

            // Its notes go nowhere while it's bypassed: dropped now (the host's below), not piled
            // up to fire at once when it comes back.
            p.takeWindowNotes (p.pending.data(), 0, (std::uint32_t) p.pending.size());
            p.wasBypassed = true;
        }
        else
        {
            const stella::plugin::NoDenormals noDenormals;
            int count = 0;

            // Back from being bypassed: every note ends first (their releases were dropped).
            if (p.wasBypassed)
            {
                p.pending[(size_t) count++] = { 0, 0xb0, 123, 0, 0 };
                p.notes.clear();
                p.wasBypassed = false;
            }

            // The notes: the window keyboard's first, then the host's, in time order.
            count = (int) p.takeWindowNotes (p.pending.data(), (std::uint32_t) count, (std::uint32_t) p.pending.size());

            for (int i = 0; i < p.numMidi && count < (int) p.pending.size(); ++i)
                p.pending[(size_t) count++] = p.midi[(size_t) i];

            for (int i = 1; i < count; ++i)
                for (int j = i; j > 0 && p.pending[(size_t) j - 1].frame > p.pending[(size_t) j].frame; --j)
                    std::swap (p.pending[(size_t) j - 1], p.pending[(size_t) j]);

            const auto transport = p.transport();
            const auto chunk = (UInt32) p.getMaxFrames();
            auto& s = p.shared();
            int nextEvent = 0;

            for (UInt32 start = 0; start < frames; start += chunk)
            {
                const auto length = std::min (frames - start, chunk);
                std::uint32_t events = 0;

                while (nextEvent < count && p.pending[(size_t) nextEvent].frame < start + length)
                {
                    auto event = p.pending[(size_t) nextEvent++];
                    event.frame = event.frame > start ? event.frame - start : 0;

                    if (events < (std::uint32_t) stella::maxEvents)
                        s.events[events++] = event;
                }

                p.run (inputs, inputChannels, outputs, outputChannels, (int) start, (int) length, events, transport);
            }
        }

        p.numMidi = 0;   // every render, bypassed or not
        notifyFlags = (flags != nullptr ? *flags : 0) | kAudioUnitRenderAction_PostRender;

        for (int i = 0; i < numNotified; ++i)
            notified[i].proc (notified[i].data, &notifyFlags, timeStamp, bus, frames, io);

        return noError;
    }

    //==========================================================================
    // The selectors
    OSStatus auInitialize (void* plugIn)
    {
        auto& p = *self (plugIn);

        if (! p.isBuilt())
            return kAudioUnitErr_FailedInitialization;

        if (! p.prepare (p.sampleRate, (int) p.maxFramesPerSlice))
            return kAudioUnitErr_FailedInitialization;

        for (auto& b : p.inputBuffers)  b.assign (p.maxFramesPerSlice, 0.0f);
        for (auto& b : p.outputBuffers) b.assign (p.maxFramesPerSlice, 0.0f);

        p.numMidi = 0;
        p.initialized = true;
        return noError;
    }

    OSStatus auUninitialize (void* plugIn)
    {
        self (plugIn)->initialized = false;
        return noError;
    }

    OSStatus auGetPropertyInfo (void* plugIn, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, UInt32* outSize, Boolean* outWritable)
    {
        UInt32 size = 0;
        bool writable = false;
        const auto status = propertyInfo (*self (plugIn), id, scope, element, size, writable);

        if (status == noError)
        {
            if (outSize != nullptr)     *outSize = size;
            if (outWritable != nullptr) *outWritable = writable;
        }

        return status;
    }

    OSStatus auGetProperty (void* plugIn, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, void* outData, UInt32* ioSize)
    {
        auto& p = *self (plugIn);
        UInt32 size = 0;
        bool writable = false;

        if (const auto status = propertyInfo (p, id, scope, element, size, writable); status != noError)
            return status;

        if (ioSize == nullptr)
            return kAudio_ParamError;

        // Asked for its size only.
        if (outData == nullptr)
        {
            *ioSize = size;
            return noError;
        }

        if (*ioSize < size)
            return kAudioUnitErr_InvalidPropertyValue;

        // Written into a buffer of the right size, then copied: the host's may be bigger.
        std::vector<std::uint8_t> buffer (std::max ((UInt32) 1, size), 0);

        // Some requests carry what's asked in the data itself.
        if (id == kAudioUnitProperty_ParameterStringFromValue || id == kAudioUnitProperty_ParameterValueFromString)
            std::memcpy (buffer.data(), outData, size);

        if (const auto status = getProperty (p, id, scope, element, buffer.data()); status != noError)
            return status;

        std::memcpy (outData, buffer.data(), size);
        *ioSize = size;
        return noError;
    }

    OSStatus auSetProperty (void* plugIn, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element, const void* data, UInt32 size)
    {
        return setProperty (*self (plugIn), id, scope, element, data, size);
    }

    OSStatus auAddPropertyListener (void* plugIn, AudioUnitPropertyID id, AudioUnitPropertyListenerProc proc, void* data)
    {
        self (plugIn)->listeners.push_back ({ id, proc, data });
        return noError;
    }

    OSStatus auRemovePropertyListener (void* plugIn, AudioUnitPropertyID id, AudioUnitPropertyListenerProc proc)
    {
        auto& l = self (plugIn)->listeners;
        l.erase (std::remove_if (l.begin(), l.end(), [&] (const Listener& x) { return x.id == id && x.proc == proc; }), l.end());
        return noError;
    }

    OSStatus auRemovePropertyListenerWithUserData (void* plugIn, AudioUnitPropertyID id, AudioUnitPropertyListenerProc proc, void* data)
    {
        auto& l = self (plugIn)->listeners;
        l.erase (std::remove_if (l.begin(), l.end(), [&] (const Listener& x) { return x.id == id && x.proc == proc && x.data == data; }), l.end());
        return noError;
    }

    OSStatus auAddRenderNotify (void* plugIn, AURenderCallback proc, void* data)
    {
        auto& p = *self (plugIn);

        if (proc == nullptr)
            return kAudio_ParamError;

        const std::lock_guard<std::mutex> lock (p.notifyChanges);

        for (auto& slot : p.notifies)
        {
            if (slot.proc.load (std::memory_order_relaxed) == nullptr)
            {
                slot.set (proc, data);
                return noError;
            }
        }

        return kAudio_MemFullError;
    }

    OSStatus auRemoveRenderNotify (void* plugIn, AURenderCallback proc, void* data)
    {
        auto& p = *self (plugIn);
        const std::lock_guard<std::mutex> lock (p.notifyChanges);

        for (auto& slot : p.notifies)
            if (slot.proc.load (std::memory_order_relaxed) == proc && slot.data.load (std::memory_order_relaxed) == data)
                slot.set (nullptr, nullptr);

        return noError;
    }

    OSStatus auGetParameter (void* plugIn, AudioUnitParameterID id, AudioUnitScope scope, AudioUnitElement, AudioUnitParameterValue* value)
    {
        auto& p = *self (plugIn);

        if (scope != kAudioUnitScope_Global)
            return kAudioUnitErr_InvalidScope;

        const auto index = p.indexOf (id);

        if (index < 0 || value == nullptr)
            return kAudioUnitErr_InvalidParameter;

        *value = p.valueOf (index);
        return noError;
    }

    OSStatus auSetParameter (void* plugIn, AudioUnitParameterID id, AudioUnitScope scope, AudioUnitElement, AudioUnitParameterValue value, UInt32)
    {
        auto& p = *self (plugIn);

        if (scope != kAudioUnitScope_Global)
            return kAudioUnitErr_InvalidScope;

        const auto index = p.indexOf (id);

        if (index < 0)
            return kAudioUnitErr_InvalidParameter;

        p.storeValue (index, value);
        return noError;
    }

    OSStatus auScheduleParameters (void* plugIn, const AudioUnitParameterEvent* events, UInt32 count)
    {
        for (UInt32 i = 0; i < count; ++i)
        {
            const auto& e = events[i];
            const auto value = e.eventType == kParameterEvent_Ramped ? e.eventValues.ramp.endValue : e.eventValues.immediate.value;
            auSetParameter (plugIn, e.parameter, e.scope, e.element, value, 0);
        }

        return noError;
    }

    OSStatus auRender (void* plugIn, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* timeStamp, UInt32 bus, UInt32 frames, AudioBufferList* io)
    {
        return render (*self (plugIn), flags, timeStamp, bus, frames, io);
    }

    OSStatus auReset (void* plugIn, AudioUnitScope, AudioUnitElement)
    {
        auto& p = *self (plugIn);
        p.reset();
        p.numMidi = 0;
        return noError;
    }

    OSStatus auMidiEvent (void* plugIn, UInt32 status, UInt32 data1, UInt32 data2, UInt32 offset)
    {
        auto& p = *self (plugIn);

        if (status >= 0xf0 || p.numMidi >= (int) p.midi.size())
            return noError;

        p.midi[(size_t) p.numMidi++] = { offset, (std::uint8_t) status, (std::uint8_t) (data1 & 0x7f), (std::uint8_t) (data2 & 0x7f), 0 };
        return noError;
    }

    OSStatus auSysEx (void*, const UInt8*, UInt32)
    {
        return noError;
    }

    AudioComponentMethod lookup (SInt16 selector)
    {
        switch (selector)
        {
            case kAudioUnitInitializeSelect:                         return reinterpret_cast<AudioComponentMethod> (auInitialize);
            case kAudioUnitUninitializeSelect:                       return reinterpret_cast<AudioComponentMethod> (auUninitialize);
            case kAudioUnitGetPropertyInfoSelect:                    return reinterpret_cast<AudioComponentMethod> (auGetPropertyInfo);
            case kAudioUnitGetPropertySelect:                        return reinterpret_cast<AudioComponentMethod> (auGetProperty);
            case kAudioUnitSetPropertySelect:                        return reinterpret_cast<AudioComponentMethod> (auSetProperty);
            case kAudioUnitAddPropertyListenerSelect:                return reinterpret_cast<AudioComponentMethod> (auAddPropertyListener);
            case kAudioUnitRemovePropertyListenerSelect:             return reinterpret_cast<AudioComponentMethod> (auRemovePropertyListener);
            case kAudioUnitRemovePropertyListenerWithUserDataSelect: return reinterpret_cast<AudioComponentMethod> (auRemovePropertyListenerWithUserData);
            case kAudioUnitAddRenderNotifySelect:                    return reinterpret_cast<AudioComponentMethod> (auAddRenderNotify);
            case kAudioUnitRemoveRenderNotifySelect:                 return reinterpret_cast<AudioComponentMethod> (auRemoveRenderNotify);
            case kAudioUnitGetParameterSelect:                       return reinterpret_cast<AudioComponentMethod> (auGetParameter);
            case kAudioUnitSetParameterSelect:                       return reinterpret_cast<AudioComponentMethod> (auSetParameter);
            case kAudioUnitScheduleParametersSelect:                 return reinterpret_cast<AudioComponentMethod> (auScheduleParameters);
            case kAudioUnitRenderSelect:                             return reinterpret_cast<AudioComponentMethod> (auRender);
            case kAudioUnitResetSelect:                              return reinterpret_cast<AudioComponentMethod> (auReset);
            default:                                                 break;
        }

        // Notes: instruments and widgets.
        if (stella::info::kind != 1)
        {
            if (selector == kMusicDeviceMIDIEventSelect)  return reinterpret_cast<AudioComponentMethod> (auMidiEvent);
            if (selector == kMusicDeviceSysExSelect)      return reinterpret_cast<AudioComponentMethod> (auSysEx);
        }

        return nullptr;
    }

    OSStatus open (void* plugIn, AudioComponentInstance component)
    {
        auto* handle = reinterpret_cast<PlugInHandle*> (plugIn);
        handle->instance = new Instance (component);
        return handle->instance->isBuilt() ? noError : kAudioUnitErr_FailedInitialization;
    }

    OSStatus close (void* plugIn)
    {
        auto* handle = reinterpret_cast<PlugInHandle*> (plugIn);
        delete handle->instance;
        delete handle;
        return noError;
    }
}

//==============================================================================
// Named by the bundle's Info.plist (AudioComponents: factoryFunction): one interface per instance.
extern "C" __attribute__ ((visibility ("default"))) void* StellaAudioUnitFactory (const AudioComponentDescription*)
{
    auto* handle = new PlugInHandle();
    handle->plugIn.Open = open;
    handle->plugIn.Close = close;
    handle->plugIn.Lookup = lookup;
    handle->plugIn.reserved = nullptr;
    handle->instance = nullptr;
    return &handle->plugIn;
}
