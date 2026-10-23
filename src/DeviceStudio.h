// D:\workspace\Fanan_web_hub\src\DeviceStudio.h

#pragma once

#include "AiAgent.h"
#include "AudioEngine.h"
#include "CodeReference.h"
#include "DeviceFiles.h"
#include "DeviceHost.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>

//==============================================================================
/**
    The Device Studio: where Sonify devices are made, Max for Live's way but by asking, in the
    Plugins Creator Studio's Build with AI (a MIDI FX). One device is open at a time. Sonify AI
    builds it with the tools here and its guide (sonify_device_guide.md): it writes the code
    (against sonify_device.h), builds it with the compiler that ships with Sonify, tests it, and
    the device plays at once in the song: in
    the MIDI FX of the track it's tried on, and every other place it already is (rebuilt, each
    keeps its settings).

    Every change is kept in the device's file in My Devices (DeviceFiles.h) as it's made, the
    conversation included, so closing Sonify loses nothing and the AI carries on next time.

    Building and testing take seconds: they run on threads of their own and answer on the
    message thread. Everything else is the message thread's.
*/
class DeviceStudio final : public AiAgent::Toolbox
{
public:
    struct Hooks
    {
        /** A new build of the open device: put it in the song, every place it is; when it's
            nowhere and addIfNowhere, on the track it's tried on. Returns where it plays now, in
            words ("Track 2 (Piano)"), or "" when it plays nowhere. */
        std::function<juce::String (const std::shared_ptr<Devices::Build>&, bool addIfNowhere)> placeBuild;

        /** Something the page shows changed (getState). */
        std::function<void()> stateChanged;
    };

    DeviceStudio (AudioEngine& engine, Hooks hooks);
    ~DeviceStudio() override;

    //==========================================================================
    // The open device.

    /** A new device, open (its file is made at its first change). */
    void newDevice();

    /** Opens a device file (from My Devices or anywhere: one from elsewhere is copied into My
        Devices first, replacing an older copy of the same device). */
    juce::Result open (const juce::File& file);

    /** Opens the device a build is of: its file in My Devices, or (a device from someone's song)
        a new file made from the build, without code. */
    juce::Result openBuild (const std::shared_ptr<const Devices::Build>& build);

    void close();

    bool isOpen() const noexcept                         { return opened; }
    const Devices::DeviceFile& getDevice() const noexcept   { return device; }
    const juce::File& getFile() const noexcept           { return file; }
    std::shared_ptr<Devices::Build> getBuild() const     { return build; }
    bool isBuilding() const noexcept                     { return building; }

    /** Renames the open device (its file follows). */
    juce::Result setInfo (const juce::String& name, const juce::String& description);

    /** Who made the open device (the developer). A new device's file isn't made for it alone: it's
        kept with the device's first change. */
    void setVendor (const juce::String& vendor);

    /** Builds the open device; done hears the result (as build_device gives it). */
    void buildDevice (std::function<void (const juce::var&)> done);

    /** The conversation to keep with the device (after each request). */
    void keepConversation (const juce::var& conversation);

    /** The user's own code for the open device (CodeReference.h), read only: kept in My Devices'
        "Reference code" folder by the device's id, where the AI reads it (read_reference,
        search_reference) and get_device lists it. */
    CodeReference getReference() const;
    CodeReference::Added addReference (const juce::Array<juce::File>& sources);

    //==========================================================================
    // My Devices.

    struct Entry
    {
        juce::File file;
        juce::String id, name, description;
        int version = 0;
        bool built = false;
    };

    /** What's in My Devices (read again when asked to refresh). */
    const std::vector<Entry>& getLibrary (bool refresh = false);

    /** A device in My Devices by its id; null when none. */
    const Entry* findInLibrary (const juce::String& deviceId);

    /** A device file's build, ready to play (made and remembered); null and why when it can't. */
    static std::shared_ptr<Devices::Build> buildOfFile (const juce::File& file, juce::String& error);

    /** Moves a device file to the trash (closing it when it's the open one). */
    juce::Result trash (juce::File target);

    /** Copies a device file into My Devices (replacing an older copy of the same device). */
    juce::Result import (const juce::File& source, juce::File& copied);

    //==========================================================================
    /** For the page: { available, why, open, file, id, name, description, vendor, version, built,
        upToDate, building, lastBuild: { ok, text }, params, outputs, files, library: [...] }. */
    juce::var getState();

    /** The open device's places in the song now: how they're doing (for the page, about ten
        times a second): { places, fault, load (share of the audio time), log (new lines) }. */
    juce::var takeLiveStatus();

    //==========================================================================
    // AiAgent::Toolbox: the AI's tools in the Device Studio.
    juce::String guide() override;
    juce::var definitions() override;
    void run (const juce::String& name, const juce::var& input, std::function<void (const juce::var&)> done) override;
    void cancel() override;
    juce::String describe (const juce::String& name, const juce::var& input, const juce::var& result) override;
    bool looksOnly (const juce::String& name) override;

    /** The guide alone (sonify_device_guide.md and sonify_device.h). */
    static juce::String deviceGuide();

    /** Runs a build over notes, faster than real time, and says what it played (test_device).
        Any thread; it stops early when cancelled turns true. */
    static juce::var runTest (const std::shared_ptr<const Devices::Build>& build, const juce::var& input, const TempoMap& songMap,
                              const std::atomic<bool>* cancelled = nullptr);

private:
    static juce::File referenceFolderOf (const juce::String& deviceId);
    juce::var getDeviceForAi();
    juce::var getSongForAi();
    juce::var writeFile (const juce::var& input);
    juce::var editFile (const juce::var& input);
    juce::var deleteFile (const juce::var& input);
    juce::var setInfoForAi (const juce::var& input);
    void test (const juce::var& input, std::function<void (const juce::var&)> done);

    juce::Result save();
    void changed();
    void resetBuildState();
    juce::var buildSummary() const;

    AudioEngine& engine;
    Hooks hooks;

    bool opened = false;
    Devices::DeviceFile device;
    juce::File file;
    std::shared_ptr<Devices::Build> build;

    bool building = false, testing = false;
    bool lastBuildOk = true;
    juce::String lastBuildText, lastBuildLog;
    juce::String playsOn;
    std::vector<Entry> library;
    bool libraryRead = false;

    // The build and the test under way, each its own: a test starting doesn't take the build's.
    std::shared_ptr<std::atomic<bool>> buildCancelled = std::make_shared<std::atomic<bool>> (false);
    std::shared_ptr<std::atomic<bool>> testCancelled = std::make_shared<std::atomic<bool>> (false);
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    // Builds and tests run here; going, the studio stops them and waits for them.
    juce::ThreadPool jobs { juce::ThreadPoolOptions{}.withThreadName ("Sonify devices").withNumberOfThreads (2) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceStudio)
};
