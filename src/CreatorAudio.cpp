// D:\workspace\Fanan_web_hub\src\CreatorAudio.cpp

#include "CreatorAudio.h"
#include "CreatorPlugin.h"   // MadePlugins::transportOf

//==============================================================================
void TestTone::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    // One-pole envelope: about 4 ms to open, 180 ms to fade.
    attackCoeff  = (float) (1.0 - std::exp (-1.0 / (0.004 * sampleRate)));
    releaseCoeff = (float) (1.0 - std::exp (-1.0 / (0.180 * sampleRate)));

    reset();
}

void TestTone::reset() noexcept
{
    for (auto& voice : voices)
        voice = Voice();
}

void TestTone::handle (const juce::MidiMessage& message) noexcept
{
    if (message.isNoteOn())
    {
        // A free voice, or the one playing longest.
        Voice* chosen = nullptr;

        for (auto& voice : voices)
            if (voice.note < 0)
                { chosen = &voice; break; }

        if (chosen == nullptr)
        {
            chosen = &voices[0];

            for (auto& voice : voices)
                if (voice.started < chosen->started)
                    chosen = &voice;
        }

        const auto frequency = juce::MidiMessage::getMidiNoteInHertz (message.getNoteNumber());

        chosen->note = message.getNoteNumber();
        chosen->step = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        chosen->target = 1.0f;
        chosen->velocity = message.getFloatVelocity();
        chosen->started = ++counter;
    }
    else if (message.isNoteOff())
    {
        for (auto& voice : voices)
            if (voice.note == message.getNoteNumber() && voice.target > 0.0f)
                voice.target = 0.0f;
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        for (auto& voice : voices)
            voice.target = 0.0f;
    }
}

void TestTone::renderVoices (float* left, float* right, int start, int end) noexcept
{
    if (end <= start)
        return;

    for (auto& voice : voices)
    {
        if (voice.note < 0)
            continue;

        const auto coeff = voice.target > voice.level ? attackCoeff : releaseCoeff;
        const auto gain = 0.18f * (0.25f + 0.75f * voice.velocity);

        for (int i = start; i < end; ++i)
        {
            voice.level += (voice.target - voice.level) * coeff;

            // A touch of the second harmonic so low notes are audible on small speakers.
            const auto sample = (float) (std::sin (voice.phase) + 0.25 * std::sin (2.0 * voice.phase)) * voice.level * gain;

            voice.phase += voice.step;

            if (voice.phase >= juce::MathConstants<double>::twoPi)
                voice.phase -= juce::MathConstants<double>::twoPi;

            left[i] += sample;

            if (right != nullptr)
                right[i] += sample;
        }

        if (voice.target == 0.0f && voice.level < 1.0e-4f)
            voice = Voice();
    }
}

void TestTone::render (float* left, float* right, int numSamples, const juce::MidiBuffer& midi) noexcept
{
    int position = 0;

    for (const auto metadata : midi)
    {
        const auto when = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderVoices (left, right, position, when);
        handle (metadata.getMessage());
        position = when;
    }

    renderVoices (left, right, position, numSamples);
}

//==============================================================================
CreatorAudio::CreatorAudio (AudioEngine& sonifyEngine)
    : sonify (sonifyEngine)
{
    midiBuffer.ensureSize ((size_t) midiBufferBytes);
    scratch.setSize (2, largestBlock);
    scratch.clear();

    prepare (sonify.getPlaybackRate(), sonify.getBlockSize());
    sonify.setCreatorPreview (this);
    startTimerHz (10);
}

CreatorAudio::~CreatorAudio()
{
    stopTimer();
    sonify.setCreatorListening (false);
    sonify.setCreatorPreview (nullptr);   // waits for a block that's playing it
    setPlugin (nullptr);
}

void CreatorAudio::prepare (double rate, int blockSize)
{
    const juce::SpinLock::ScopedLockType lock (pluginLock);

    preparedRate = rate;
    preparedBlockSize = blockSize;
    currentRate.store (rate);
    currentBlockSize.store (blockSize);
    testTone.prepare (rate);

    // Room for the device's blocks (allocated here, on the message thread, never while playing).
    if (blockSize > scratch.getNumSamples())
        scratch.setSize (2, blockSize, false, true, true);
}

void CreatorAudio::timerCallback()
{
    // Sonify's device: a new sample rate means the plugin restarts with it (LivePreview does);
    // another device (or none) shows in the top bar's pill.
    const auto rate = sonify.getPlaybackRate();
    const auto blockSize = sonify.getBlockSize();
    const auto description = describeDevice();
    const bool newRate = std::abs (rate - preparedRate) >= 0.5 || blockSize != preparedBlockSize;

    if (! newRate && description == shownDevice)
        return;

    if (newRate)
        prepare (rate, blockSize);

    shownDevice = description;

    if (onDeviceChanged != nullptr)
        onDeviceChanged();
}

void CreatorAudio::setListening (bool shouldListen)
{
    if (listening.load() == shouldListen)
        return;

    listening.store (shouldListen);
    sonify.setCreatorListening (shouldListen);

    // Leaving: whatever was held lets go (the plugin hears it as soon as it plays again).
    if (! shouldListen)
        allNotesOff();
}

//==============================================================================
juce::String CreatorAudio::describeDevice() const
{
    auto* device = sonify.getDeviceManager().getCurrentAudioDevice();

    if (device == nullptr)
        return "No audio device";

    const auto rate = device->getCurrentSampleRate();
    const auto rateText = std::abs (rate - std::round (rate / 1000.0) * 1000.0) < 1.0
                              ? juce::String (juce::roundToInt (rate / 1000.0)) + " kHz"
                              : juce::String (rate / 1000.0, 1) + " kHz";

    const auto dot = juce::String::fromUTF8 (" \xc2\xb7 ");

    return device->getTypeName() + dot + device->getName()
         + dot + rateText
         + dot + juce::String (device->getCurrentBufferSizeSamples()) + " samples";
}

juce::StringArray CreatorAudio::describeDeviceLines() const
{
    auto* device = sonify.getDeviceManager().getCurrentAudioDevice();

    if (device == nullptr)
        return { "No audio device", "Click to choose one", "" };

    const auto rate = device->getCurrentSampleRate();
    const auto rateText = std::abs (rate - std::round (rate / 1000.0) * 1000.0) < 1.0
                              ? juce::String (juce::roundToInt (rate / 1000.0)) + " kHz"
                              : juce::String (rate / 1000.0, 1) + " kHz";

    const auto buffer = device->getCurrentBufferSizeSamples();
    const auto milliseconds = rate > 0.0 ? 1000.0 * (double) buffer / rate : 0.0;

    return { device->getName(), rateText,
             juce::String (buffer) + juce::String::fromUTF8 (" samples \xc2\xb7 ") + juce::String (milliseconds, 1) + " ms" };
}

double CreatorAudio::getCpuLoad() const
{
    return sonify.getDeviceManager().getCpuUsage();
}

float CreatorAudio::takePeak (int channel) noexcept
{
    if (! juce::isPositiveAndBelow (channel, (int) peaks.size()))
        return 0.0f;

    return peaks[(size_t) channel].exchange (0.0f, std::memory_order_relaxed);
}

void CreatorAudio::raisePeak (std::atomic<float>& peak, float value) noexcept
{
    auto current = peak.load (std::memory_order_relaxed);

    while (value > current && ! peak.compare_exchange_weak (current, value, std::memory_order_relaxed))
    {
    }
}

void CreatorAudio::allNotesOff()
{
    keyboardState.allNotesOff (0);
    panicRequested.store (true);
    silenceRequested.store (true);
}

void CreatorAudio::setPlugin (std::unique_ptr<WasmPlugin> newPlugin)
{
    std::unique_ptr<WasmPlugin> old;

    {
        // Waits at most for the audio block that's using the old plugin.
        const juce::SpinLock::ScopedLockType lock (pluginLock);
        old = std::move (plugin);
        plugin = std::move (newPlugin);
    }

    // The old plugin goes here, on the message thread, never on the audio thread.
}

bool CreatorAudio::playChime()
{
    if (! sonify.isAudioRunning())
        return false;

    chimeRequested.store (true);
    return true;
}

//==============================================================================
void CreatorAudio::renderPreview (const juce::MidiBuffer& live, const float* inLeft, const float* inRight,
                                  float* left, float* right, int numSamples, const clap_event_transport_t& song) noexcept
{
    // If the plugin is being swapped (or the device's rate changed) right now, this block simply skips it.
    const juce::SpinLock::ScopedTryLockType lock (pluginLock);

    if (! lock.isLocked() || numSamples <= 0 || left == nullptr)
        return;

    numSamples = juce::jmin (numSamples, scratch.getNumSamples());
    const bool on = listening.load (std::memory_order_relaxed);

    // Sonify's live notes while the studio is in front, then the GUI's keyboard (which also lights
    // up for the notes played).
    midiBuffer.clear();

    if (on)
        midiBuffer.addEvents (live, 0, numSamples, 0);

    keyboardState.processNextMidiBuffer (midiBuffer, 0, numSamples, true);

    if (silenceRequested.exchange (false))
        testTone.reset();

    auto* outLeft = scratch.getWritePointer (0);
    auto* outRight = scratch.getWritePointer (1);
    juce::FloatVectorOperations::clear (outLeft, numSamples);
    juce::FloatVectorOperations::clear (outRight, numSamples);

    bool pluginPlayed = false;

    if (on && plugin != nullptr)
    {
        int numEvents = 0;

        // Panic: "all notes off" and "all sound off" first, so every voice lets go.
        if (panicRequested.exchange (false))
        {
            pluginEvents[(size_t) numEvents++] = { 0, 0xb0, 123, 0 };
            pluginEvents[(size_t) numEvents++] = { 0, 0xb0, 120, 0 };
        }

        for (const auto metadata : midiBuffer)
        {
            const auto* raw = metadata.data;

            if (metadata.numBytes < 1 || raw[0] >= 0xf0 || numEvents >= (int) pluginEvents.size())
                continue;

            pluginEvents[(size_t) numEvents++] = { (std::uint32_t) juce::jlimit (0, numSamples - 1, metadata.samplePosition),
                                                   raw[0],
                                                   metadata.numBytes > 1 ? raw[1] : (std::uint8_t) 0,
                                                   metadata.numBytes > 2 ? raw[2] : (std::uint8_t) 0 };
        }

        plugin->setTransport (MadePlugins::transportOf (&song));   // the song's tempo and position: tempo sync
        plugin->process (inLeft, inRight != nullptr ? inRight : inLeft, outLeft, outRight, numSamples, pluginEvents.data(), numEvents);
        pluginPlayed = true;
    }

    if (on && ! pluginPlayed && testToneOn.load (std::memory_order_relaxed))
        testTone.render (outLeft, outRight, numSamples, midiBuffer);

    // Sonify AI has finished: the chime goes over whatever is playing, wherever the user is.
    if (chimeRequested.exchange (false))
        chime.start (currentRate.load());

    if (chime.active)
        chime.render (outLeft, outRight, numSamples);

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto range = juce::FloatVectorOperations::findMinAndMax (scratch.getReadPointer (channel), numSamples);
        raisePeak (peaks[(size_t) channel], juce::jmax (std::abs (range.getStart()), std::abs (range.getEnd())));
    }

    juce::FloatVectorOperations::add (left, outLeft, numSamples);

    if (right != nullptr && right != left)
        juce::FloatVectorOperations::add (right, outRight, numSamples);
}

void CreatorAudio::Chime::render (float* left, float* right, int numSamples) noexcept
{
    // C6 then G6, 140 ms apart, each a sine with a quiet octave, a 5 ms rise and a
    // quarter-second fade: gentle, and clear over music.
    struct Note { double frequency, startSeconds; };
    static constexpr Note notes[] { { 1046.5, 0.0 }, { 1568.0, 0.14 } };
    constexpr double lengthSeconds = 1.2, attack = 0.005, decay = 0.25;
    constexpr float level = 0.16f;

    const auto total = (int) (lengthSeconds * sampleRate);

    for (int i = 0; i < numSamples && position < total; ++i, ++position)
    {
        const auto t = (double) position / sampleRate;
        double sample = 0.0;

        for (const auto& note : notes)
        {
            const auto local = t - note.startSeconds;

            if (local < 0.0)
                continue;

            const auto envelope = juce::jmin (1.0, local / attack) * std::exp (-local / decay);
            const auto phase = juce::MathConstants<double>::twoPi * note.frequency * local;
            sample += envelope * (std::sin (phase) + 0.3 * std::sin (2.0 * phase));
        }

        const auto value = (float) sample * level;
        left[i] += value;

        if (right != nullptr)
            right[i] += value;
    }

    if (position >= total)
        active = false;
}
