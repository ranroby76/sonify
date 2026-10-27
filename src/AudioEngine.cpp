// D:\workspace\Fanan_web_hub\src\AudioEngine.cpp

#include "AudioEngine.h"
#include "BuiltinPlugins.h"
#include "Mp3Encoder.h"
#include "Container.h"
#include "Preferences.h"

#include "Vst3Plugin.h"    // JUCE's VST3 host is let go at shutdown
#include "WclapPlugin.h"   // the WebAssembly engine is shut down last

namespace
{
    /** The same clips, exactly: ids, windows, mutes, seeds, notes (muted, operators), controller
        lines, audio and what it was made from: everything that changes what plays. */
    bool sameClips (const std::vector<MidiClip>& a, const std::vector<MidiClip>& b)
    {
        const auto sameLine = [] (const std::vector<MidiControlPoint>& p, const std::vector<MidiControlPoint>& q)
        {
            return std::equal (p.begin(), p.end(), q.begin(), q.end(), [] (const MidiControlPoint& x, const MidiControlPoint& y)
                               { return x.value == y.value && juce::exactlyEqual (x.time, y.time); });
        };

        const auto sameNotes = [] (const std::vector<MidiNote>& p, const std::vector<MidiNote>& q)
        {
            return std::equal (p.begin(), p.end(), q.begin(), q.end(), [] (const MidiNote& x, const MidiNote& y)
                               { return x.sameAs (y); });
        };

        return std::equal (a.begin(), a.end(), b.begin(), b.end(), [&] (const MidiClip& x, const MidiClip& y)
        {
            return x.id == y.id && juce::exactlyEqual (x.start, y.start) && juce::exactlyEqual (x.length, y.length)
                && juce::exactlyEqual (x.offset, y.offset) && x.muted == y.muted && x.seed == y.seed && sameNotes (x.notes, y.notes)
                && sameLine (x.pitchBend, y.pitchBend) && sameLine (x.aftertouch, y.aftertouch)
                && x.audioFile == y.audioFile && juce::exactlyEqual (x.audioOffset, y.audioOffset)
                && juce::exactlyEqual (x.audioLength, y.audioLength) && juce::exactlyEqual (x.audioGainDb, y.audioGainDb)
                && juce::exactlyEqual (x.fadeIn, y.fadeIn) && juce::exactlyEqual (x.fadeOut, y.fadeOut)
                && x.sourceFile == y.sourceFile && juce::exactlyEqual (x.sourceOffset, y.sourceOffset)
                && juce::exactlyEqual (x.sourceLength, y.sourceLength) && juce::exactlyEqual (x.sourceTempo, y.sourceTempo)
                && juce::exactlyEqual (x.pitch, y.pitch) && x.reversed == y.reversed && x.followTempo == y.followTempo
                && x.effectUid() == y.effectUid() && x.warp == y.warp && x.silence == y.silence && x.envelopes == y.envelopes;
        });
    }
}

//==============================================================================
// Mixdown
class AudioEngine::MixdownThread final : public juce::Thread
{
public:
    using Format = MixdownSettings::Format;

    MixdownThread (AudioEngine& e, MixdownSettings s, juce::int64 range, juce::int64 total, double rate, int block)
        : Thread ("Fanan mixdown"), engine (e), settings (std::move (s)), rangeSamples (range), totalSamples (total),
          sampleRate (rate), blockSize (block)
    {
        // Message thread: how late each file's sound comes (plugin delay compensation), to leave
        // out at its start, so every file starts with the song.
        mainSkip = engine.renderLatency (0, false, false);

        // The Mastering section, when it's in this render, makes Main's mix later by its look-ahead
        // (the unmastered file, taken before it, isn't).
        unmasteredSkip = mainSkip;

        if (settings.freezeTrackId == 0 && settings.mastered && engine.mastering.settings.isOn (MasterSection::on))
            mainSkip += engine.mastering.getChainLatency();

        for (const auto& stem : settings.stems)
            stemSkips.push_back (engine.renderLatency (stem.trackId, false, false));

        if (settings.freezeTrackId != 0)
            freezeSkip = engine.renderLatency (settings.freezeTrackId, true, settings.dry);
    }

    void run() override
    {
        // What it writes: Main's mix and each stem. A WAV at the device's rate is written as it
        // renders; anything else (another format, another rate) is rendered to a float WAV
        // beside it first, then made from that.
        const bool convert = settings.outputRate > 0.0 && ! juce::approximatelyEqual (settings.outputRate, sampleRate);
        const bool direct = settings.format == Format::wav && ! convert;

        if (settings.file != juce::File())
            targets.push_back ({ settings.file, {}, -1, nullptr });

        for (size_t i = 0; i < settings.stems.size(); ++i)
            targets.push_back ({ settings.stems[i].file, {}, static_cast<int> (i), nullptr });

        if (settings.freezeTrackId != 0)
            targets.push_back ({ settings.freezeFile, {}, freezeTarget, nullptr });

        if (settings.unmasteredFile != juce::File() && settings.freezeTrackId == 0)
            targets.push_back ({ settings.unmasteredFile, {}, unmasteredTarget, nullptr });

        for (auto& target : targets)
        {
            target.render = direct ? target.file
                                   : target.file.getSiblingFile (target.file.getFileNameWithoutExtension() + ".rendering.wav");
            target.writer = makeWavWriter (target.render, sampleRate, direct ? settings.bits : 32);

            if (target.writer == nullptr)
            {
                result = juce::Result::fail ("Couldn't write " + target.render.getFullPathName());
                removeAll();
                return;
            }
        }

        // The stems: each channel's sound as it leaves its fader, caught while the song renders.
        StemTaps taps;

        if (! settings.stems.empty())
        {
            taps.buffers.setSize (2 * static_cast<int> (settings.stems.size()), blockSize);

            for (const auto& stem : settings.stems)
            {
                taps.trackIds.push_back (stem.trackId);
                taps.lastGain.push_back (1.0f);
            }

            engine.stemTaps = &taps;
        }

        // A freeze: the track's own sound, before its fader; only it (and what it needs) plays.
        FreezeTap freeze;

        if (settings.freezeTrackId != 0)
        {
            freeze.trackId = settings.freezeTrackId;
            freeze.dry = settings.dry;
            freeze.plays = settings.freezeSources;
            freeze.plays.push_back (settings.freezeTrackId);
            freeze.buffer.setSize (2, blockSize);
            engine.freezeTap = &freeze;
        }

        // Main's mix before the Mastering section (its unmastered file), and the section left out
        // of a render that's to be without it.
        juce::AudioBuffer<float> unmastered (2, blockSize);

        if (settings.unmasteredFile != juce::File() && settings.freezeTrackId == 0)
            engine.unmasteredTap = &unmastered;

        engine.masteringLeftOut.store (! settings.mastered);

        const auto renderShare = direct ? 1.0 : 0.6;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::int64 done = 0;
        bool stopped = false;

        // Each file leaves out its own latency at the start, and the render goes on as long as
        // the latest needs, so each still has its whole length.
        const auto skipOf = [this] (const Target& target) -> juce::int64
        {
            if (target.stem == freezeTarget)      return freezeSkip;
            if (target.stem == unmasteredTarget)  return unmasteredSkip;
            if (target.stem < 0)              return mainSkip;
            return juce::isPositiveAndBelow (target.stem, static_cast<int> (stemSkips.size())) ? stemSkips[static_cast<size_t> (target.stem)] : 0;
        };

        juce::int64 longestSkip = 0;

        for (const auto& target : targets)
            longestSkip = juce::jmax (longestSkip, skipOf (target));

        const auto renderSamples = totalSamples + longestSkip;

        while (done < renderSamples)
        {
            if (threadShouldExit())
            {
                engine.stemTaps = nullptr;
                engine.freezeTap = nullptr;
                engine.unmasteredTap = nullptr;
                engine.masteringLeftOut.store (false);
                result = juce::Result::fail ("Cancelled");
                removeAll();
                return;
            }

            // The range ends exactly: the song stops there, and what still sounds rings out.
            if (! stopped && done >= rangeSamples)
            {
                engine.transport.setPlaying (false);
                stopped = true;
            }

            auto count = static_cast<int> (juce::jmin<juce::int64> (blockSize, renderSamples - done));

            if (! stopped)
                count = static_cast<int> (juce::jmin<juce::int64> (count, rangeSamples - done));

            taps.buffers.clear();
            freeze.buffer.clear();
            engine.renderBlock (nullptr, 0, buffer.getArrayOfWritePointers(), 2, count);

            for (auto& target : targets)
            {
                // The part of this block that's the file's: from its latency on, its whole length.
                const auto skip = skipOf (target);
                const auto from = juce::jmax (done, skip);
                const auto to = juce::jmin (done + count, skip + totalSamples);

                if (to <= from)
                    continue;

                const auto offset = static_cast<int> (from - done);
                const auto length = static_cast<int> (to - from);

                if (target.stem == freezeTarget)
                {
                    target.writer->writeFromAudioSampleBuffer (freeze.buffer, offset, length);
                }
                else if (target.stem == unmasteredTarget)
                {
                    target.writer->writeFromAudioSampleBuffer (unmastered, offset, length);
                }
                else if (target.stem < 0)
                {
                    target.writer->writeFromAudioSampleBuffer (buffer, offset, length);
                }
                else
                {
                    const float* channels[] = { taps.buffers.getReadPointer (2 * target.stem) + offset,
                                                taps.buffers.getReadPointer (2 * target.stem + 1) + offset };
                    target.writer->writeFromFloatArrays (channels, 2, length);
                }
            }

            done += count;
            progress.store (static_cast<double> (done) / static_cast<double> (renderSamples) * renderShare);
        }

        engine.stemTaps = nullptr;
        engine.freezeTap = nullptr;
        engine.unmasteredTap = nullptr;
        engine.masteringLeftOut.store (false);

        for (auto& target : targets)
            target.writer.reset();   // each WAV finished

        // Each file in its format and rate, from its render.
        if (! direct)
        {
            for (size_t i = 0; i < targets.size(); ++i)
            {
                auto& target = targets[i];
                const auto made = makeFinal (target.render, target.file, [this, i, renderShare] (double part)
                {
                    progress.store (renderShare + (1.0 - renderShare) * (static_cast<double> (i) + part) / static_cast<double> (targets.size()));
                });

                target.render.deleteFile();
                target.made = made.wasOk();

                if (made.failed())
                {
                    result = threadShouldExit() ? juce::Result::fail ("Cancelled") : made;
                    removeAll();
                    return;
                }
            }
        }

        progress.store (1.0);
        result = juce::Result::ok();
    }

    static std::unique_ptr<juce::AudioFormatWriter> makeWavWriter (const juce::File& file, double rate, int bits)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

        if (stream == nullptr)
            return nullptr;

        return juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                                 .withSampleRate (rate)
                                                                 .withNumChannels (2)
                                                                 .withBitsPerSample (bits)
                                                                 .withSampleFormat (bits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                                                               : juce::AudioFormatWriterOptions::SampleFormat::integral));
    }

    /** A writer for WAV, FLAC or OGG (MP3 goes through LAME instead). */
    std::unique_ptr<juce::AudioFormatWriter> makeWriter (const juce::File& file, double rate) const
    {
        if (settings.format == Format::wav)
            return makeWavWriter (file, rate, settings.bits);

        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

        if (stream == nullptr)
            return nullptr;

        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2);

        if (settings.format == Format::flac)
            return juce::FlacAudioFormat().createWriterFor (stream, options.withBitsPerSample (settings.bits == 16 ? 16 : 24)
                                                                           .withQualityOptionIndex (5));

        // OGG Vorbis: the quality whose bit rate is nearest to the one asked for.
        juce::OggVorbisAudioFormat ogg;
        const auto qualities = ogg.getQualityOptions();
        int best = 0;

        for (int i = 0; i < qualities.size(); ++i)
            if (std::abs (qualities[i].getIntValue() - settings.kbps) < std::abs (qualities[best].getIntValue() - settings.kbps))
                best = i;

        return ogg.createWriterFor (stream, options.withBitsPerSample (32).withQualityOptionIndex (best));
    }

    /** The file in its format and at its rate, made from the float WAV the song rendered to. */
    juce::Result makeFinal (const juce::File& rendered, const juce::File& file, const std::function<void (double)>& report)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (rendered));

        if (reader == nullptr)
            return juce::Result::fail ("Couldn't read the render of " + file.getFileName());

        const auto rate = settings.outputRate > 0.0 ? settings.outputRate : reader->sampleRate;
        const bool resample = ! juce::approximatelyEqual (rate, reader->sampleRate);

        std::unique_ptr<juce::AudioFormatWriter> writer;
        Mp3Encoder mp3;

        if (settings.format == Format::mp3)
        {
            if (const auto opened = mp3.open (file, rate, settings.kbps); opened.failed())
                return opened;
        }
        else if ((writer = makeWriter (file, rate)) == nullptr)
        {
            file.deleteFile();   // the format turned it down (a rate it can't write): nothing left behind
            return juce::Result::fail ("Couldn't write " + file.getFullPathName());
        }

        juce::AudioFormatReaderSource source (reader.get(), false);
        juce::ResamplingAudioSource resampler (&source, false, 2);

        if (resample)
        {
            resampler.setResamplingRatio (reader->sampleRate / rate);
            resampler.prepareToPlay (blockSize, rate);
        }

        const auto total = resample ? static_cast<juce::int64> (std::llround (static_cast<double> (reader->lengthInSamples) * rate / reader->sampleRate))
                                    : reader->lengthInSamples;
        juce::AudioBuffer<float> block (2, blockSize);

        for (juce::int64 done = 0; done < total; done += blockSize)
        {
            if (threadShouldExit())
            {
                writer.reset();
                file.deleteFile();
                return juce::Result::fail ("Cancelled");
            }

            const auto count = static_cast<int> (juce::jmin<juce::int64> (blockSize, total - done));

            if (resample)
            {
                juce::AudioSourceChannelInfo info (&block, 0, count);
                resampler.getNextAudioBlock (info);
            }
            else
            {
                reader->read (&block, 0, count, done, true, true);
            }

            const auto wrote = writer != nullptr ? writer->writeFromAudioSampleBuffer (block, 0, count)
                                                 : mp3.write (block.getReadPointer (0), block.getReadPointer (1), count);

            if (! wrote)
            {
                writer.reset();
                file.deleteFile();
                return juce::Result::fail ("Couldn't write " + file.getFullPathName());
            }

            report (static_cast<double> (done + count) / static_cast<double> (juce::jmax<juce::int64> (1, total)));
        }

        if (resample)
            resampler.releaseResources();

        if (writer != nullptr)
        {
            writer.reset();
            return juce::Result::ok();
        }

        return mp3.close();
    }

    /** A render that didn't finish leaves nothing behind: no renders, no files half made. */
    void removeAll()
    {
        for (auto& target : targets)
        {
            target.writer.reset();
            target.render.deleteFile();

            if (target.made || target.render == target.file)
                target.file.deleteFile();
        }
    }

    static constexpr int freezeTarget = -2;
    static constexpr int unmasteredTarget = -3;

    struct Target
    {
        juce::File file, render;
        int stem = -1;   // -1: Main's mix; freezeTarget: a track's freeze; unmasteredTarget: Main's mix before the Mastering section
        std::unique_ptr<juce::AudioFormatWriter> writer;
        bool made = false;
    };

    AudioEngine& engine;
    MixdownSettings settings;
    juce::int64 mainSkip = 0, freezeSkip = 0, unmasteredSkip = 0;   // samples each file leaves out at its start
    std::vector<juce::int64> stemSkips;
    const juce::int64 rangeSamples, totalSamples;
    const double sampleRate;
    const int blockSize;
    std::vector<Target> targets;
    std::atomic<double> progress { 0.0 };
    juce::Result result = juce::Result::fail ("Not finished");
};

void AudioEngine::StemTaps::take (const Track& track, int numSamples) noexcept
{
    numSamples = juce::jmin (numSamples, buffers.getNumSamples());

    for (size_t i = 0; i < trackIds.size(); ++i)
        if (trackIds[i] == track.id)
            track.sendInto (buffers.getWritePointer (2 * static_cast<int> (i)), buffers.getWritePointer (2 * static_cast<int> (i) + 1),
                            numSamples, 1.0f, lastGain[i]);
}

void AudioEngine::FreezeTap::take (const Track& track, int numSamples) noexcept
{
    numSamples = juce::jmin (numSamples, buffer.getNumSamples(), track.getBlockSamples());

    if (numSamples > 0)
    {
        buffer.copyFrom (0, 0, track.getBlock (0), numSamples);
        buffer.copyFrom (1, 0, track.getBlock (1), numSamples);
    }
}

void AudioEngine::StemTaps::takeBoard (const Track& track, int numSamples) noexcept
{
    numSamples = juce::jmin (numSamples, buffers.getNumSamples(), track.board.getOutputSamples());

    for (size_t i = 0; i < trackIds.size(); ++i)
    {
        if (trackIds[i] != track.id || numSamples <= 0)
            continue;

        const auto channel = 2 * static_cast<int> (i);
        juce::FloatVectorOperations::add (buffers.getWritePointer (channel), track.board.getOutput (0), numSamples);
        juce::FloatVectorOperations::add (buffers.getWritePointer (channel + 1), track.board.getOutput (1), numSamples);
    }
}

juce::String AudioEngine::extensionOf (MixdownSettings::Format format)
{
    switch (format)
    {
        case MixdownSettings::Format::flac: return ".flac";
        case MixdownSettings::Format::ogg:  return ".ogg";
        case MixdownSettings::Format::mp3:  return ".mp3";
        case MixdownSettings::Format::wav:  break;
    }

    return ".wav";
}


#include <cstring>
#include <iterator>

namespace
{
    constexpr int midiBufferBytes = 4096;
    constexpr int minimumMixSize = 4096;

    /** Every plugin: each track's instrument and inserts, then the Main inserts. */
    template <typename Tracks, typename Function>
    void forEachPlugin (const Tracks& tracks, const EffectChain& mainInserts, Function&& function)
    {
        for (const auto& track : tracks)
        {
            if (track->instrument != nullptr)
                function (*track->instrument);

            for (int i = 0; i < track->inserts.size(); ++i)
                if (auto* insert = track->inserts.at (i); insert->plugin != nullptr)
                    function (*insert->plugin);

            for (const auto& effect : track->clipEffects)
                if (effect != nullptr && effect->plugin != nullptr)
                    function (*effect->plugin);
        }

        for (int i = 0; i < mainInserts.size(); ++i)
            if (auto* insert = mainInserts.at (i); insert->plugin != nullptr)
                function (*insert->plugin);
    }

    // Track colours, assigned in turn as tracks are created.
    constexpr juce::uint32 trackPalette[] = { 0xffe5484d, 0xfff08c2e, 0xffe9c93b, 0xff4cbf67,
                                              0xff3cc0c8, 0xff3e82f7, 0xff9b6cf2, 0xffe85aa8 };

    void storeMax (std::atomic<float>& target, float value) noexcept
    {
        auto current = target.load (std::memory_order_relaxed);

        while (value > current
               && ! target.compare_exchange_weak (current, value, std::memory_order_relaxed))
        {
        }
    }

    /** What lets go of a sound: a note-off, a pedal up (sustain, sostenuto, hold 2), all notes
        off, all sound off. */
    bool isRelease (const juce::uint8* data, int numBytes) noexcept
    {
        if (numBytes < 3)
            return false;

        const auto type = data[0] & 0xf0;

        if (type == 0x80 || (type == 0x90 && data[2] == 0))
            return true;

        if (type == 0xb0)
        {
            const auto controller = data[1];
            return controller == 120 || controller == 123
                || ((controller == 64 || controller == 66 || controller == 69) && data[2] < 64);
        }

        return false;
    }

    /** Leaves only the releases in a buffer (scratch: room for it, so nothing is allocated). */
    void keepReleasesOnly (juce::MidiBuffer& buffer, juce::MidiBuffer& scratch) noexcept
    {
        if (buffer.isEmpty())
            return;

        scratch.clear();
        scratch.addEvents (buffer, 0, -1, 0);
        buffer.clear();

        for (const auto metadata : scratch)
            if (isRelease (metadata.data, metadata.numBytes))
                buffer.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);
    }
}

//==============================================================================
namespace
{
    /** A port's events as a route lets them through: its channels, without the kinds it keeps
        out. Channel mode messages (all notes off and the like) always pass. Audio thread. */
    void addRouted (juce::MidiBuffer& to, const juce::MidiBuffer& from, const Track::MidiRoute& route, int numSamples) noexcept
    {
        if (route.channels == 0xffff && route.blocked == 0)
        {
            to.addEvents (from, 0, numSamples, 0);
            return;
        }

        for (const auto metadata : from)
        {
            if (metadata.numBytes <= 0 || metadata.samplePosition >= numSamples)
                continue;

            const auto status = metadata.data[0];

            if (status < 0xf0)
            {
                if ((route.channels & (1u << (status & 0x0f))) == 0)
                    continue;

                const auto kind = status & 0xf0;

                if (((kind == 0xa0 || kind == 0xd0) && (route.blocked & Track::blockAftertouch) != 0)
                    || (kind == 0xc0 && (route.blocked & Track::blockProgramChange) != 0)
                    || (kind == 0xe0 && (route.blocked & Track::blockPitchBend) != 0)
                    || (kind == 0xb0 && (route.blocked & Track::blockControllers) != 0 && metadata.numBytes > 1 && metadata.data[1] < 120))
                    continue;
            }

            to.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);
        }
    }

    /** Tells an instrument an MPE controller plays it: the lower zone with all fifteen member
        channels (the MPE configuration message), and the members' pitch bend range. Audio thread. */
    void addMpeSetup (juce::MidiBuffer& to, int pitchRange) noexcept
    {
        const auto controller = [&to] (int channel, int number, int value)
        {
            const juce::uint8 bytes[] { static_cast<juce::uint8> (0xb0 | (channel - 1)), static_cast<juce::uint8> (number), static_cast<juce::uint8> (value) };
            to.addEvent (bytes, 3, 0);
        };

        controller (1, 101, 0);  controller (1, 100, 6);  controller (1, 6, 15);
        controller (1, 101, 127); controller (1, 100, 127);

        for (int channel = 2; channel <= 16; ++channel)
        {
            controller (channel, 101, 0);  controller (channel, 100, 0);
            controller (channel, 6, juce::jlimit (0, 96, pitchRange));  controller (channel, 38, 0);
            controller (channel, 101, 127); controller (channel, 100, 127);
        }
    }
}

//==============================================================================
AudioEngine::AudioEngine()
{
    // The real sample rate arrives in audioDeviceAboutToStart. Resetting now means MIDI that
    // arrives before the device starts is still accepted.
    midiCollector.reset (48000.0);
    midiCollector.ensureStorageAllocated (static_cast<size_t> (midiBufferBytes));
    midiBuffer.ensureSize (midiBufferBytes);

    // The audio thread orders the tracks in these every block, so they never grow there.
    renderOrder.reserve (1024);
    placed.reserve (1024);

    // A queue per MIDI device, so each track can choose which it hears.
    for (auto& collector : deviceCollectors)
    {
        collector = std::make_unique<juce::MidiMessageCollector>();
        collector->reset (48000.0);
        collector->ensureStorageAllocated (static_cast<size_t> (midiBufferBytes));
    }

    for (auto& buffer : deviceBuffers)
        buffer.ensureSize (midiBufferBytes);

    recordMidi.ensureSize (midiBufferBytes * 2);
    pageCollector.reset (48000.0);
    pageCollector.ensureStorageAllocated (static_cast<size_t> (midiBufferBytes));
    pageBuffer.ensureSize (midiBufferBytes);
    auditionCollector.reset (48000.0);
    auditionCollector.ensureStorageAllocated (static_cast<size_t> (midiBufferBytes));
    auditionBuffer.ensureSize (midiBufferBytes);
    rackNotes.ensureSize (midiBufferBytes);
    samplingNotes.ensureSize (midiBufferBytes);
    creatorMidi.ensureSize (midiBufferBytes * 4);   // every source's notes at once
    creatorScratch.ensureSize (midiBufferBytes * 4);
    rackLive.ensureSize (midiBufferBytes * 4);
    surfaceQueue.reserve (1024);
    linkQueue.reserve (1024);

    writerThread.startThread();

    // Start with one track, so the keyboard plays straight away.
    addTrack();

    openDevices();
    refreshMidiInputNames();

    deviceManager.addAudioCallback (this);
    deviceManager.addMidiInputDeviceCallback ({}, this);
    deviceManager.addChangeListener (this);
}

AudioEngine::~AudioEngine()
{
    if (mixdown != nullptr)
        mixdown->stopThread (10000);   // a render in progress is stopped (its file removed) first

    deviceManager.removeChangeListener (this);
    deviceManager.removeMidiInputDeviceCallback ({}, this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();

    // A recording in progress is written out (the night's too); then the writer thread can stop.
    for (auto& recording : audioRecordings)
        recording.writer.reset();

    audioRecordings.clear();
    stopLiveRecording();
    writerThread.stopThread (4000);

    // Every plugin must be gone before the WebAssembly engine stops.
    tracks.clear();
    mainInserts.clear();
    WclapPlugin::shutdownEngine();
    Vst3Plugin::shutdownFormats();
}

//==============================================================================
void AudioEngine::openDevices()
{
    const auto savedState = juce::parseXML (getStateFile());

    // Two inputs to begin with (audio tracks record from them); Audio Settings can enable more.
    deviceManager.initialise (2, 2, savedState.get(), true);

    // Settings used to choose one output pair: one that wasn't the first is where Main goes on.
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto active = device->getActiveOutputChannels();
        const auto first = active.findNextSetBit (0);

        if (first > 0 && active.countNumberOfSetBits() <= 2)
            startupOutputPair = { first, active.findNextSetBit (first + 1) };
    }

    // Every output channel open: the song's outputs say what plays where.
    openAllOutputChannels();

    if (savedState != nullptr)
        return;

    // First launch: prefer an installed ASIO driver, and listen to every MIDI input.
   #if JUCE_WINDOWS
    const auto previousType = deviceManager.getCurrentAudioDeviceType();

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() != "ASIO")
            continue;

        if (! type->getDeviceNames().isEmpty())
        {
            deviceManager.setCurrentAudioDeviceType ("ASIO", true);

            if (deviceManager.getCurrentAudioDevice() == nullptr && previousType.isNotEmpty())
                deviceManager.setCurrentAudioDeviceType (previousType, true);

            openAllOutputChannels();
        }

        break;
    }
   #endif

    for (const auto& input : juce::MidiInput::getAvailableDevices())
        deviceManager.setMidiInputDeviceEnabled (input.identifier, true);

    assignMidiDeviceSlots();
}

juce::File AudioEngine::getStateFile()
{
    // With Sonify's other settings (a new build keeps it).
    return SettingsFolder::file ("audio.xml", "FananWebHub-audio.xml");
}

void AudioEngine::saveDeviceState()
{
    // Let go for the moment (Sonify in the background): the setup stays as it was.
    if (deviceReleased)
        return;

    if (auto xml = deviceManager.createStateXml())
        xml->writeTo (getStateFile());
}

void AudioEngine::refreshMidiInputNames()
{
    // A device plugged in finds its slot, and the tracks that chose it by name hear it again.
    assignMidiDeviceSlots();

    for (const auto& track : tracks)
        resolveMidiInputs (*track);

    enabledMidiInputNames.clear();

    for (const auto& input : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled (input.identifier))
            enabledMidiInputNames.add (input.name);

    refreshMidiSources();
}

void AudioEngine::changeListenerCallback (juce::ChangeBroadcaster*)
{
    saveDeviceState();
    refreshMidiInputNames();

    // A device opened (or changed) with only some of its outputs: every one is opened.
    openAllOutputChannels();
}

//==============================================================================
void AudioEngine::queue (juce::MidiMessage message)
{
    message.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    pageCollector.addMessageToQueue (message);   // the page's notes: for the selected track

    if (monitorOn.load (std::memory_order_relaxed))
        monitor (-2, message);
}

void AudioEngine::noteOn (int note, float velocity)
{
    // Velocity 0 would turn this into a note-off.
    queue (juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, note), juce::jlimit (0.01f, 1.0f, velocity)));
}

void AudioEngine::noteOff (int note)
{
    queue (juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, note)));
}

void AudioEngine::allNotesOff()
{
    for (int channel = 1; channel <= 16; ++channel)
        queue (juce::MidiMessage::allNotesOff (channel));

    // Tracks that are not monitoring get it too.
    for (const auto& track : tracks)
        track->requestNotesOff();
}

//==============================================================================
AudioEngine::Status AudioEngine::getStatus()
{
    Status status;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        status.deviceOpen = true;
        status.running = device->isPlaying();
        status.typeName = device->getTypeName();
        status.deviceName = device->getName();
        status.sampleRate = device->getCurrentSampleRate();
        status.bufferSize = device->getCurrentBufferSizeSamples();
        status.xruns = device->getXRunCount();

        if (status.sampleRate > 0.0)
        {
            status.bufferMs = 1000.0 * status.bufferSize / status.sampleRate;
            status.outputLatencyMs = 1000.0 * device->getOutputLatencyInSamples() / status.sampleRate;
        }
    }

    status.cpu = deviceManager.getCpuUsage();
    status.peakLeft = peakLeft.exchange (0.0f, std::memory_order_relaxed);
    status.peakRight = peakRight.exchange (0.0f, std::memory_order_relaxed);
    status.hardwareNoteOns = hardwareNoteOnCount.load (std::memory_order_relaxed);
    status.notesLow = activeNotesLow.load (std::memory_order_relaxed);
    status.notesHigh = activeNotesHigh.load (std::memory_order_relaxed);
    status.midiInputs = midiSources;
    status.released = deviceReleased;
    status.transport = transport.getState();

    status.trackMeters.reserve (tracks.size());

    for (const auto& track : tracks)
    {
        const auto automated = Automation::reads (track->automationMode.load())
                               && std::any_of (track->automation.begin(), track->automation.end(), [] (const AutomationLane& lane)
                                               { return lane.target.kind != AutomationTarget::Kind::parameter; });

        status.trackMeters.push_back ({ track->id,
                                        track->peakLeft.exchange (0.0f, std::memory_order_relaxed),
                                        track->peakRight.exchange (0.0f, std::memory_order_relaxed),
                                        track->inputPeakLeft.exchange (0.0f, std::memory_order_relaxed),
                                        track->inputPeakRight.exchange (0.0f, std::memory_order_relaxed),
                                        automated, track->volumeDb.load(), track->pan.load() });
    }

    // The Sub outputs, in the song's order (their places are the plan's).
    const auto subs = juce::jmin (maxSubOutputs, static_cast<int> (outputs.size()) - 1);

    for (int slot = 0; slot < subs; ++slot)
    {
        TrackMeter meter;
        meter.id = outputs[static_cast<size_t> (slot + 1)].id;
        meter.left = subPeakLeft[static_cast<size_t> (slot)].exchange (0.0f, std::memory_order_relaxed);
        meter.right = subPeakRight[static_cast<size_t> (slot)].exchange (0.0f, std::memory_order_relaxed);
        status.outputMeters.push_back (meter);
    }

    return status;
}

//==============================================================================
Track* AudioEngine::findTrack (int id) const noexcept
{
    for (const auto& track : tracks)
        if (track->id == id)
            return track.get();

    return nullptr;
}

int AudioEngine::addTrack (Track::Kind kind, int wantedId)
{
    const auto id = wantedId > 0 && findTrack (wantedId) == nullptr ? wantedId : nextTrackId;
    nextTrackId = juce::jmax (nextTrackId, id + 1);
    // Colours follow the track count, so every song starts with the same first colour.
    const auto colour = trackPalette[tracks.size() % std::size (trackPalette)];

    auto track = std::make_unique<Track> (id, colour, kind);

    if (kind == Track::Kind::audio)
    {
        // Named like Studio One's: Audio 1, Audio 2...
        const auto audioTracks = std::count_if (tracks.begin(), tracks.end(), [] (const auto& t) { return t->kind == Track::Kind::audio; });
        track->name = "Audio " + juce::String (static_cast<int> (audioTracks) + 1);
        track->nameIsAutomatic = false;
        resolveInput (*track);
    }
    else if (kind == Track::Kind::fx || kind == Track::Kind::bus)
    {
        // FX 1, FX 2... (Studio One names them after their effect; the user can rename them), and Bus 1, Bus 2...
        const auto same = std::count_if (tracks.begin(), tracks.end(), [kind] (const auto& t) { return t->kind == kind; });
        track->name = (kind == Track::Kind::bus ? "Bus " : "FX ") + juce::String (static_cast<int> (same) + 1);
        track->nameIsAutomatic = false;
        track->monitor = false;
    }
    else
    {
        track->name = "Track " + juce::String (static_cast<int> (tracks.size()) + 1);
    }

    // Prepared before the audio thread can see it.
    if (currentSampleRate > 0.0)
        track->prepare (currentSampleRate, currentBlockSize);

    // Room for it made here, so nothing allocates under the lock (the old list goes afterwards).
    std::vector<std::unique_ptr<Track>> grown;

    if (tracks.size() == tracks.capacity())
        grown.reserve (tracks.size() * 2 + 8);

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        if (grown.capacity() > tracks.size())
        {
            for (auto& each : tracks)
                grown.push_back (std::move (each));

            tracks.swap (grown);
        }

        tracks.push_back (std::move (track));
    }

    // A new track starts with Monitor off: the Monitor button alone switches it on (the page's
    // own notes still play the selected track).
    if (auto* added = findTrack (id))
        added->monitor = false;

    // A new instrument track hears the default device (Settings > MIDI Setup), or the first
    // in its MIDI in list: the user's keyboard.
    if (kind == Track::Kind::instrument)
        if (const auto source = defaultMidiSource(); source.isNotEmpty())
            if (auto* added = findTrack (id))
                added->midiInputs = juce::StringArray (source);

    if (auto* added = findTrack (id))
        resolveMidiInputs (*added);

    if (kind != Track::Kind::fx && kind != Track::Kind::bus)
        selectTrack (id);   // FX channels and buses live in the Console; the selection stays on the tracks

    return id;
}

void AudioEngine::removeTrack (int id)
{
    int index = 0;
    auto removed = detachTrack (id, index);

    // Its render leaves memory with it.
    if (removed != nullptr && removed->isFrozen())
    {
        media.forget (removed->frozenFile);
        loadedRenders.erase (removed->frozenFile.getFullPathName());
    }

    removed.reset();   // destroyed here, outside the lock
}

namespace
{
    /** A plugin leaving the graph (removed, replaced, or set aside for undo) closes its window. */
    void hideScreen (Plugin* plugin)
    {
        if (plugin != nullptr)
            plugin->hideNativeScreen();
    }

    void hideScreens (EffectChain& chain)
    {
        for (int i = 0; i < chain.size(); ++i)
            hideScreen (chain.at (i)->plugin.get());
    }

    /** Its clips' own effects' windows too. */
    void hideClipEffectScreens (Track& track)
    {
        for (const auto& effect : track.clipEffects)
            if (effect != nullptr)
                hideScreen (effect->plugin.get());
    }
}

std::unique_ptr<Track> AudioEngine::detachTrack (int id, int& index)
{
    // A frozen track it keys or plays notes into rendered with it; a freeze rendering now
    // misses it from here.
    if (auto* leaving = findTrack (id))
    {
        ++leaving->contentRevision;
        thawDependents (id);
    }

    std::unique_ptr<Track> removed;
    size_t position = 0;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        for (size_t i = 0; i < tracks.size(); ++i)
        {
            if (tracks[i]->id == id)
            {
                removed = std::move (tracks[i]);
                tracks.erase (tracks.begin() + static_cast<std::ptrdiff_t> (i));
                position = i;
                break;
            }
        }
    }

    if (removed == nullptr)
        return nullptr;

    hideScreen (removed->instrument.get());
    hideScreens (removed->inserts);
    hideClipEffectScreens (*removed);
    index = static_cast<int> (position);

    // Choir tracks singing to its chords have none while it's away.
    refreshChoirsFollowing (id);

    // The selected track removed: the nearest track takes over, so while there are tracks one
    // is always selected (the next one down, or the one above).
    if (selectedTrackId == id)
    {
        selectedTrackId = 0;

        for (size_t i = position; i < tracks.size() && selectedTrackId == 0; ++i)
            if (! tracks[i]->isMixChannel())
                selectedTrackId = tracks[i]->id;

        for (size_t i = position; i-- > 0 && selectedTrackId == 0;)
            if (! tracks[i]->isMixChannel())
                selectedTrackId = tracks[i]->id;
    }

    // The audio thread can no longer reach it. Notes it was playing end when it comes back.
    removed->requestNotesOff();
    removed->sequencer.requestRelease();
    return removed;
}

void AudioEngine::attachTrack (std::unique_ptr<Track> track, int index)
{
    if (track == nullptr)
        return;

    // Ready for the device as it is now: it may have changed while the track was away.
    if (currentSampleRate > 0.0)
        track->prepare (currentSampleRate, currentBlockSize);

    if (track->instrument != nullptr)
        activatePlugin (*track->instrument);

    for (int i = 0; i < track->inserts.size(); ++i)
        if (auto* insert = track->inserts.at (i); insert != nullptr && insert->plugin != nullptr)
            activatePlugin (*insert->plugin);

    for (const auto& effect : track->clipEffects)
        if (effect != nullptr && effect->plugin != nullptr)
            activatePlugin (*effect->plugin);

    const auto id = track->id;
    resolveInput (*track);   // the song's inputs may have changed while it was away
    resolveOutput (*track);  // and its outputs

    // An audio track's clips at the rate the device plays now, and where the tempo puts them
    // now (updateRecording and followTempo only saw the tracks in the song). Not playing yet.
    if (track->kind == Track::Kind::audio)
    {
        for (auto& clip : track->clips)
            fitAudioLength (clip);

        rebuildSequence (*track, false);
    }

    // Frozen: its render at the rate the device plays now. The tempo, the key or the mixer
    // changed while it was away, or the render went: it plays itself again.
    if (track->isFrozen())
    {
        auto render = track->frozenMap == transport.getTempoMap() && track->frozenDry == boardWanted.load()
                          ? frozenPlaylist (track->frozenFile) : nullptr;

        if (render == nullptr)
            thaw (*track, true);
        else
            track->frozen = std::move (render);   // not playing yet: no lock needed
    }

    ++track->contentRevision;   // a freeze rendering meanwhile didn't hear it

    // Room for it made here, so nothing allocates under the lock (the old list goes afterwards).
    std::vector<std::unique_ptr<Track>> grown;

    if (tracks.size() == tracks.capacity())
        grown.reserve (tracks.size() * 2 + 8);

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        if (grown.capacity() > tracks.size())
        {
            for (auto& each : tracks)
                grown.push_back (std::move (each));

            tracks.swap (grown);
        }

        const auto where = static_cast<std::ptrdiff_t> (juce::jlimit (0, static_cast<int> (tracks.size()), index));
        tracks.insert (tracks.begin() + where, std::move (track));
    }

    thawDependents (id);   // a frozen track it keys or plays notes into rendered without it

    // A choir track back: its keys and chords as the song is now; choirs singing to its chords
    // have them again.
    if (auto* back = findTrack (id); back != nullptr && back->choir != nullptr)
        rebuildChoirTimeline (*back);

    refreshChoirsFollowing (id);
    selectTrack (id);
}

void AudioEngine::setTrackOrder (const std::vector<int>& ids)
{
    std::vector<std::unique_ptr<Track>> ordered;
    ordered.reserve (tracks.size());

    const juce::SpinLock::ScopedLockType lock (graphLock);

    for (const auto id : ids)
        for (auto& track : tracks)
            if (track != nullptr && track->id == id)
                ordered.push_back (std::move (track));

    for (auto& track : tracks)
        if (track != nullptr)
            ordered.push_back (std::move (track));

    tracks.swap (ordered);
}

std::vector<MidiClip> AudioEngine::getClips (int trackId) const
{
    if (auto* track = findTrack (trackId))
        return track->clips;

    return {};
}

void AudioEngine::setClips (int trackId, std::vector<MidiClip> clips)
{
    if (auto* track = findTrack (trackId))
    {
        // (An undo step puts back every track it noted: those it didn't change stay frozen.)
        const auto changed = ! sameClips (track->clips, clips);
        track->clips = std::move (clips);
        rebuildSequence (*track, true);

        if (changed)
            contentChanged (*track);
    }
}

void AudioEngine::selectTrack (int id)
{
    // Tracks are selected, FX channels and buses aren't (they live in the Console). Selecting
    // never changes monitoring: that is the Monitor button's alone.
    if (auto* track = findTrack (id); track != nullptr && ! track->isMixChannel())
    {
        const auto previous = selectedTrackId.exchange (id);

        // Notes the page was holding on the track left behind would hang there: they end.
        if (previous != id)
            if (auto* left = findTrack (previous))
                left->requestNotesOff();
    }
}

void AudioEngine::monitorOnce (int trackId)
{
    if (auto* track = findTrack (trackId); track != nullptr && track->kind == Track::Kind::instrument && ! track->monitorDecided)
    {
        track->monitorDecided = true;
        setTrackMonitor (trackId, true);
    }
}

void AudioEngine::setTrackMonitor (int id, bool shouldMonitor, bool byUser)
{
    if (auto* track = findTrack (id); track != nullptr && byUser)
        track->monitorDecided = true;

    // A frozen track plays its render: nothing live to hear.
    if (auto* track = findTrack (id))
        if (track->monitor.exchange (shouldMonitor && ! track->isFrozen()) && ! shouldMonitor)
            track->requestNotesOff();   // notes held when it stopped listening would hang
}

void AudioEngine::setTrackName (int id, const juce::String& name)
{
    const auto trimmed = name.trim().substring (0, 64);

    if (auto* track = findTrack (id); track != nullptr && trimmed.isNotEmpty())
    {
        track->name = trimmed;
        track->nameIsAutomatic = false;
    }
}

//==============================================================================
void AudioEngine::clearSong (bool keepLiveShow)
{
    transport.setPlaying (false);   // also ends recording
    take = {};
    abandonAudioRecording();
    stopPreview();
    inputs = songDefaultInputs;
    resolveVoiceInput();   // the same input number may be another input now
    recordBits = 24;

    while (! tracks.empty())
        removeTrack (tracks.back()->id);

    clipEffectsKnown.clear();      // the song's clip effects went with it
    clipEffectStatesLeft.clear();

    thawed.clear();   // they're gone

    // The Live Show as a new show starts: its master's and decks' settings, no tracks on its decks,
    // no pad sounds, no headphones. What MainLive.cpp keeps of it (its tab, playlist, decks and
    // pads) is left waiting, empty, so it starts afresh too; a song that has its own (Song::restore)
    // puts them in place of these. keepLiveShow (an undo's snapshot): it's left as it is, playing.
    if (! keepLiveShow)
    {
        liveShow.resetSettings();

        for (int deck = 0; deck < LiveShow::maxDecks; ++deck)
            setDeckTrack (deck, nullptr);

        for (int pad = 0; pad < LiveShow::numPads; ++pad)
            setPadSound (pad, nullptr);

        livePhonesOutput = 0;
        setLiveShowExtras ({});
        setDeckFilesToLoad ({});
        setPadFilesToLoad ({});
    }

    // The outputs new songs start with (Song Setup's Make Default), once no track plays into a Sub.
    outputs = songDefaultOutputs;
    auditionOutput = songDefaultAudition;
    nextOutputId = 1;

    for (const auto& output : outputs)
        nextOutputId = juce::jmax (nextOutputId, output.id + 1);

    planOutputs();

    // Each insert leaves through removeInsert, so it's destroyed outside the lock.
    while (mainInserts.size() > 0)
        removeInsert (mainChannel, mainInserts.at (mainInserts.size() - 1)->id);

    setMasterVolume (0.0f);
    setMasterMute (false);
    mainKept = juce::var();   // a DAWproject's placeholders on Main
    setBoardMixing (false);
    resetBoard();
    mastering.settings.reset();

    transport.setTempoMap ({});   // 120 BPM, 4/4, C major
    setMainAutomation ({});
    setSongBlocks ({});
    setSongVideo ({});
    transport.setLoop (false, 0.0, 16.0);
    transport.setMetronome (false, transport.getState().metronomeVolume);
    transport.locate (0.0);
}

void AudioEngine::setTrackLive (int trackId, bool isChannel)
{
    if (auto* track = findTrack (trackId); track != nullptr && ! track->isMixChannel())
    {
        // Under the graph lock: a block sees it one way all through (the Studio heard with the
        // show asks twice: its own pass leaves the show's channels out, renderLive plays them).
        const juce::SpinLock::ScopedLockType lock (graphLock);

        if (isChannel && ! track->liveChannel.load())
            track->live.reset();   // fresh, before the audio thread sees it as a channel

        track->liveChannel.store (isChannel);
    }
}

void AudioEngine::setDeckTrack (int deck, std::shared_ptr<const LiveShow::DeckTrack> track)
{
    if (! juce::isPositiveAndBelow (deck, LiveShow::maxDecks))
        return;

    auto& player = liveShow.decks[static_cast<size_t> (deck)];
    player.startOver();

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        player.setTrack (track.get());
    }

    // The track it had goes here (as `track` does, at the end of this call), not on the audio thread.
    std::swap (deckTracks[static_cast<size_t> (deck)], track);
}

std::shared_ptr<const LiveShow::DeckTrack> AudioEngine::getDeckTrack (int deck) const
{
    return juce::isPositiveAndBelow (deck, LiveShow::maxDecks) ? deckTracks[static_cast<size_t> (deck)] : nullptr;
}

void AudioEngine::setPadSound (int pad, std::shared_ptr<const LiveShow::DeckTrack> sound)
{
    if (! juce::isPositiveAndBelow (pad, LiveShow::numPads))
        return;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        liveShow.pads[static_cast<size_t> (pad)].setSound (sound.get());
    }

    // The sound it had goes here (as `sound` does, at the end of this call), not on the audio thread.
    std::swap (padSounds[static_cast<size_t> (pad)], sound);
}

std::shared_ptr<const LiveShow::DeckTrack> AudioEngine::getPadSound (int pad) const
{
    return juce::isPositiveAndBelow (pad, LiveShow::numPads) ? padSounds[static_cast<size_t> (pad)] : nullptr;
}

void AudioEngine::setLivePhonesOutput (int outputId)
{
    livePhonesOutput = outputId != 0 && hasOutput (outputId) ? outputId : 0;
    planOutputs();
}

LiveShow::SoundTap* AudioEngine::openLiveTap()
{
    if (liveTap == nullptr)
    {
        auto tap = std::make_unique<LiveShow::SoundTap> (getPlaybackRate(), static_cast<int> (4.0 * getPlaybackRate()));

        {
            const juce::SpinLock::ScopedLockType lock (graphLock);
            liveShow.setTap (tap.get());
        }

        liveTap = std::move (tap);
    }

    return liveTap.get();
}

void AudioEngine::closeLiveTap()
{
    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        liveShow.setTap (nullptr);
    }

    liveTap = nullptr;
}

juce::Result AudioEngine::startLiveRecording (bool stems)
{
    if (liveTake.show != nullptr)
        return juce::Result::ok();

    auto parent = recordingFolder;

    if (parent == juce::File())
        parent = UserData::recordings();   // Settings > Locations > User Data

    parent.createDirectory();
    const auto folder = parent.getNonexistentChildFile ("Live Show " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M"), "", false);

    if (! folder.createDirectory())
        return juce::Result::fail ("Sonify can't make a folder for the recording in " + parent.getFullPathName() + ".");

    juce::WavAudioFormat wav;
    const auto format = recordBits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                         : juce::AudioFormatWriterOptions::SampleFormat::integral;

    const auto open = [&] (const juce::File& file) -> std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter>
    {
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

        if (stream == nullptr)
            return nullptr;

        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (getPlaybackRate())
                                                                                     .withNumChannels (2)
                                                                                     .withBitsPerSample (recordBits)
                                                                                     .withSampleFormat (format));

        if (writer == nullptr)
            return nullptr;

        return std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 1 << 17);
    };

    LiveTake night;
    night.folder = folder;
    night.showFile = folder.getChildFile ("Show.wav");
    night.show = open (night.showFile);

    if (night.show == nullptr)
    {
        folder.deleteRecursively();
        return juce::Result::fail ("Sonify can't write the recording in " + folder.getFullPathName() + ".");
    }

    // Stems: each deck shown, and each of the show's channels.
    if (stems)
    {
        for (int d = 0; d < liveShow.getDeckCount(); ++d)
        {
            LiveTake::Stem stem;
            stem.file = folder.getChildFile ("Deck " + juce::String (d + 1) + ".wav");
            stem.writer = open (stem.file);
            stem.deck = d;

            if (stem.writer != nullptr)
                night.stems.push_back (std::move (stem));
        }

        for (const auto& track : tracks)
        {
            if (track->isMixChannel() || ! track->liveChannel.load())
                continue;

            LiveTake::Stem stem;
            stem.file = folder.getNonexistentChildFile (juce::File::createLegalFileName (track->name), ".wav", false);
            stem.writer = open (stem.file);
            stem.trackId = track->id;

            if (stem.writer != nullptr)
                night.stems.push_back (std::move (stem));
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        liveShow.setRecorder (night.show.get());

        for (auto& stem : night.stems)
        {
            if (stem.deck >= 0)
                liveShow.decks[static_cast<size_t> (stem.deck)].recorder = stem.writer.get();
            else if (auto* track = findTrack (stem.trackId))
                track->live.recorder = stem.writer.get();
        }
    }

    liveTake = std::move (night);
    return juce::Result::ok();
}

AudioEngine::LiveTakeFiles AudioEngine::stopLiveRecording()
{
    LiveTakeFiles files;

    if (liveTake.show == nullptr)
        return files;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        liveShow.setRecorder (nullptr);

        for (auto& deck : liveShow.decks)
            deck.recorder = nullptr;

        for (const auto& track : tracks)
            track->live.recorder = nullptr;
    }

    // Written out, and closed.
    liveTake.show.reset();

    for (auto& stem : liveTake.stems)
    {
        stem.writer.reset();
        files.stems.push_back (stem.file);
    }

    files.folder = liveTake.folder;
    files.show = liveTake.showFile;
    liveTake = {};
    return files;
}

double AudioEngine::getLiveRecordedSeconds() const noexcept
{
    const auto rate = getPlaybackRate();
    return rate > 0.0 ? static_cast<double> (liveShow.getRecordedSamples()) / rate : 0.0;
}

void AudioEngine::resetBoard()
{
    board.settings.reset();

    for (const auto& track : tracks)
        track->board.settings.reset();
}

bool AudioEngine::isBoardFresh() const
{
    const auto moved = [] (const juce::var& values) { return values.getDynamicObject()->getProperties().size() > 0; };

    if (moved (board.settings.toVar()))
        return false;

    for (const auto& track : tracks)
        if (! track->isMixChannel() && moved (track->board.settings.toVar()))
            return false;

    return true;
}

void AudioEngine::copyConsoleToBoard()
{
    for (const auto& track : tracks)
    {
        if (track->isMixChannel())
            continue;

        auto& settings = track->board.settings;
        settings.set (AnalogBoard::volume, track->volumeDb.load());
        settings.set (AnalogBoard::pan, track->pan.load());
        settings.set (AnalogBoard::mute, track->mute.load() ? 1.0f : 0.0f);
        settings.set (AnalogBoard::solo, track->solo.load() ? 1.0f : 0.0f);
    }

    board.settings.set (AnalogBoard::masterVolume, isMasterMuted() ? AnalogBoard::silentDb : getMasterVolume());
}

bool AudioEngine::consumePluginStateChanges() noexcept
{
    bool changed = false;

    // A clip effect turned in its window: its state goes into its clip (the song, a copy, undo keep it).
    for (const auto& track : tracks)
        for (const auto& effect : track->clipEffects)
            if (effect != nullptr && effect->plugin != nullptr && effect->plugin->consumeStateChanged())
                changed = captureClipEffect (*track, *effect) || changed;

    forEachPlugin (tracks, mainInserts, [&changed] (Plugin& plugin) { changed = plugin.consumeStateChanged() || changed; });
    return changed;
}

//==============================================================================
double AudioEngine::barStartAt (double ppq) const noexcept
{
    return transport.getTempoMap().barAt (ppq).start;
}

double AudioEngine::barEndAt (double ppq) const noexcept
{
    // The end of the bar a position is in, or the position itself on a bar line.
    const auto bar = transport.getTempoMap().barAt (ppq);
    return ppq - bar.start < 1.0e-9 ? bar.start : bar.start + bar.length();
}

double AudioEngine::audioLengthInPpq (double startPpq, double seconds) const noexcept
{
    // Audio keeps its speed: how many quarter notes it covers from where it starts.
    const auto& map = transport.getTempoMap();
    return map.ppqAt (map.secondsAt (startPpq) + seconds) - startPpq;
}

double AudioEngine::secondsBetween (double fromPpq, double toPpq) const noexcept
{
    const auto& map = transport.getTempoMap();
    return map.secondsAt (toPpq) - map.secondsAt (fromPpq);
}

void AudioEngine::setTrackRecordArm (int id, bool shouldArm)
{
    // A frozen track doesn't record: it plays its render.
    if (auto* track = findTrack (id))
        if (track->recordArm.exchange (shouldArm && ! track->isFrozen()) && ! shouldArm && ! track->monitor.load())
            track->requestNotesOff();   // it stops listening: release what was held
}

void AudioEngine::startRecording()
{
    bool anyArmed = false;

    for (const auto& track : tracks)
        anyArmed = anyArmed || track->recordArm.load();

    // Like Studio One: with nothing armed, the selected track records (unless it's frozen).
    if (! anyArmed)
        if (auto* track = findTrack (selectedTrackId); track != nullptr && ! track->isFrozen())
            track->recordArm.store (true);

    // Audio writers are ready before the transport records, so no sample is missed.
    beginAudioRecording();

    // From stopped, with the count-in and the preroll the record options ask for.
    transport.startRecording();
}

double AudioEngine::getSongEnd() const
{
    double end = 0.0;

    for (const auto& track : tracks)
    {
        for (const auto& clip : track->clips)
            end = juce::jmax (end, clip.start + clip.length);

        for (const auto& lane : track->automation)
            if (! lane.points.empty())
                end = juce::jmax (end, lane.points.back().ppq);
    }

    return end;
}

juce::Result AudioEngine::startMixdown (MixdownSettings settings)
{
    if (mixdown != nullptr)
        return juce::Result::fail ("An export is already running.");

    // The device plays silence while a mixdown renders: never in the middle of a show.
    if (liveWanted.load())
        return juce::Result::fail ("Not while the Live Show has the outputs: leave its tab, with its decks, recording and stream stopped.");

    // The Sampling Studio holds the song stopped while it samples (and may render it on a thread of its own).
    if (samplingOn.load())
        return juce::Result::fail ("Not while the Sampling Studio is sampling: let it finish, or stop it, first.");

    if (! audioRunning.load() || currentSampleRate <= 0.0 || currentBlockSize <= 0)
        return juce::Result::fail ("Open an audio device first: the mixdown renders at its sample rate.");

    if (! (settings.endPpq > settings.startPpq) || settings.startPpq < 0.0)
        return juce::Result::fail ("There's nothing to export in that range.");

    // A freeze is a float WAV of the track alone, at the device's rate.
    if (settings.freezeTrackId != 0)
    {
        if (findTrack (settings.freezeTrackId) == nullptr || settings.freezeFile == juce::File())
            return juce::Result::fail ("That track isn't in the song any more.");

        settings.file = juce::File();
        settings.stems.clear();
        settings.format = MixdownSettings::Format::wav;
        settings.bits = 32;
        settings.outputRate = 0.0;
    }
    else if (settings.file == juce::File() && settings.stems.empty())
    {
        return juce::Result::fail ("Choose where the mixdown goes.");
    }

    if (settings.format == MixdownSettings::Format::mp3)
        if (const auto problem = Mp3Encoder::problem(); problem.isNotEmpty())
            return juce::Result::fail (problem);

    for (const auto& stem : settings.stems)
        if (findTrack (stem.trackId) == nullptr || stem.file == juce::File())
            return juce::Result::fail ("A stem's channel isn't in the song any more.");

    settings.bits = settings.bits == 16 || settings.bits == 32 ? settings.bits : 24;
    settings.kbps = juce::jlimit (64, 320, settings.kbps);
    settings.outputRate = settings.outputRate >= 8000.0 && settings.outputRate <= 384000.0 ? settings.outputRate : 0.0;

    const auto state = transport.getState();
    const auto seconds = secondsBetween (settings.startPpq, settings.endPpq);   // through the tempo changes
    const auto rangeSamples = static_cast<juce::int64> (std::llround (seconds * currentSampleRate));
    const auto tailSamples = static_cast<juce::int64> (std::llround (juce::jlimit (0.0, 30.0, settings.tailSeconds) * currentSampleRate));

    {
        std::vector<juce::File> files;

        if (settings.file != juce::File())
            files.push_back (settings.file);

        for (const auto& stem : settings.stems)
            files.push_back (stem.file);

        if (settings.freezeFile != juce::File())
            files.push_back (settings.freezeFile);

        for (const auto& file : files)
            if (! file.getParentDirectory().createDirectory())
                return juce::Result::fail ("Couldn't make the folder " + file.getParentDirectory().getFullPathName());
    }

    // The song as it is, to put back afterwards.
    beforeMixdown = { state.playing, state.loopEnabled, state.metronomeEnabled, state.ppq, state.loopStart, state.loopEnd, state.metronomeVolume };

    if (state.recording)
        stopRecording();

    stopPreview();

    // The device steps aside: it plays silence until the render is done.
    offline.store (true);
    Plugin::offlineRendering().store (true);

    while (deviceInCallback.load())
        juce::Thread::sleep (1);

    // What's played live goes unheard meanwhile (its note-offs and pedal too): held notes end
    // here, so none rings into the render or hangs after it.
    for (const auto& track : tracks)
        track->requestNotesOff();

    // From the range's start, with no loop and no clicks.
    transport.setPlaying (false);
    transport.setLoop (false, state.loopStart, state.loopEnd);
    transport.setMetronome (false, state.metronomeVolume);
    transport.locate (settings.startPpq);
    transport.setPlaying (true);
    settleGains.store (true);

    updateCompensation();   // the latencies as the render starts
    mixdown = std::make_unique<MixdownThread> (*this, std::move (settings), rangeSamples, rangeSamples + tailSamples,
                                               currentSampleRate, currentBlockSize);
    mixdown->startThread (juce::Thread::Priority::high);
    return juce::Result::ok();
}

double AudioEngine::getMixdownProgress() const noexcept
{
    return mixdown != nullptr ? mixdown->progress.load() : 0.0;
}

void AudioEngine::cancelMixdown()
{
    if (mixdown != nullptr)
        mixdown->signalThreadShouldExit();
}

void AudioEngine::stopMixdownThread()
{
    // Cancelled, and waited for: nothing of the render runs once this returns (collectMixdown
    // still puts the song back, and says it was cancelled).
    if (mixdown != nullptr && mixdown->isThreadRunning())
        mixdown->stopThread (10000);
}

std::optional<juce::Result> AudioEngine::collectMixdown (juce::File& written)
{
    if (mixdown == nullptr || mixdown->isThreadRunning())
        return std::nullopt;

    const auto result = mixdown->result;

    // Clips rendered alone: the track plays all its clips again.
    if (! mixdown->settings.onlyClipIds.empty())
        if (auto* track = findTrack (mixdown->settings.freezeTrackId))
            rebuildSequence (*track, true);

    // Stems alone: the folder they're in. A freeze: its render.
    written = mixdown->settings.freezeTrackId != 0 ? mixdown->settings.freezeFile
            : mixdown->settings.file != juce::File() || mixdown->settings.stems.empty()
                  ? mixdown->settings.file
                  : mixdown->settings.stems.front().file.getParentDirectory();
    mixdown.reset();

    // The song as it was: the device takes over again from where it was.
    transport.setPlaying (false);
    transport.setLoop (beforeMixdown.loop, beforeMixdown.loopStart, beforeMixdown.loopEnd);
    transport.setMetronome (beforeMixdown.metronome, beforeMixdown.metronomeVolume);
    transport.locate (beforeMixdown.ppq);

    // Notes and pedals played during the render weren't heard: none hangs from here.
    for (const auto& track : tracks)
        track->requestNotesOff();

    // What the render left in the delays (plugin delay compensation) isn't heard: the device's
    // next block isn't the one after its last, so every delay starts again from silence.
    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        blockCount += 2;

        for (const auto& track : tracks)
            track->clearClipDelays();
    }

    Plugin::offlineRendering().store (false);
    offline.store (false);
    return result;
}

//==============================================================================
std::vector<int> AudioEngine::freezeNeeds (int trackId) const
{
    // The track first, then what it needs, and what that needs in turn.
    std::vector<int> needed { trackId };

    const auto need = [&needed] (int id)
    {
        if (id > 0 && std::find (needed.begin(), needed.end(), id) == needed.end())
            needed.push_back (id);
    };

    for (size_t i = 0; i < needed.size(); ++i)
    {
        const auto* listener = findTrack (needed[i]);

        if (listener == nullptr)
            continue;

        // The tracks whose plugins play notes into it.
        for (const auto& input : listener->midiInputs)
            if (input.startsWith ("track:"))
                need (input.substring (6).getIntValue());

        // Those keying its plugins' sidechains; a bus, the tracks playing into it; an FX channel,
        // the tracks sending to it.
        for (const auto& other : tracks)
        {
            if (listener->kind == Track::Kind::bus && other->outputBus.load() == listener->id)
                need (other->id);

            for (const auto& send : other->sends)
                if (send.target == listener->id && (send.sidechain || listener->kind == Track::Kind::fx))
                    need (other->id);
        }
    }

    return needed;
}

juce::Result AudioEngine::startFreeze (int trackId, const juce::File& file)
{
    const auto* track = findTrack (trackId);

    if (track == nullptr || track->isMixChannel())
        return juce::Result::fail ("Only instrument and audio tracks freeze.");

    if (track->isFrozen())
        return juce::Result::fail ("\"" + track->name + "\" is frozen already.");

    // Something of its own to render: clips, or notes from another track's plugin (its live
    // input or notes aren't in a render).
    if (track->clips.empty() && ! std::any_of (track->midiInputs.begin(), track->midiInputs.end(),
                                               [] (const juce::String& input) { return input.startsWith ("track:"); }))
        return juce::Result::fail ("\"" + track->name + "\" has nothing to freeze yet: no clips.");

    // Its instrument rests while it's frozen: one that plays notes into other tracks has to go on.
    const auto source = "track:" + juce::String (trackId);

    for (const auto& other : tracks)
        if (other.get() != track && other->midiInputs.contains (source))
            return juce::Result::fail ("\"" + track->name + "\" plays notes into \"" + other->name + "\": it can't be frozen.");

    MixdownSettings settings;
    settings.freezeTrackId = trackId;
    settings.freezeFile = file;
    settings.startPpq = 0.0;
    settings.endPpq = getSongEnd();
    settings.tailSeconds = 3.0;   // its effects ring out

    if (! (settings.endPpq > 0.0))
        return juce::Result::fail ("There's nothing in the song to freeze yet.");

    // Only it plays, and what it needs.
    const auto needed = freezeNeeds (trackId);
    settings.freezeSources.assign (needed.begin() + 1, needed.end());

    // What it's rendered from, to check the render against once it's done.
    FreezeStart start;
    start.trackId = trackId;
    start.map = transport.getTempoMap();
    start.dry = boardWanted.load();

    for (const auto id : needed)
        if (const auto* each = findTrack (id))
            start.revisions.emplace_back (id, each->contentRevision);

    start.keys = keyLevels (trackId, start.dry);

    const auto result = startMixdown (std::move (settings));

    if (result.wasOk())
    {
        freezeStart = std::move (start);

        // Their windows close while it renders: what changed there meanwhile wouldn't be in it.
        for (const auto id : needed)
        {
            if (auto* each = findTrack (id))
            {
                hideScreen (each->instrument.get());
                hideScreens (each->inserts);
                hideClipEffectScreens (*each);
            }
        }
    }

    return result;
}

juce::Result AudioEngine::startClipRender (int trackId, const std::vector<int>& clipIds, const juce::File& file,
                                           double tailSeconds, bool dry)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->isMixChannel())
        return juce::Result::fail ("That clip's track isn't in the song any more.");

    if (track->isFrozen())
        return juce::Result::fail ("\"" + track->name + "\" is frozen: its snowflake unfreezes it, then its clips render.");

    std::vector<MidiClip> chosen;

    for (const auto& clip : track->clips)
        if (std::find (clipIds.begin(), clipIds.end(), clip.id) != clipIds.end())
            chosen.push_back (clip);

    if (chosen.empty() || chosen.size() != clipIds.size())
        return juce::Result::fail ("That clip isn't in the song any more.");

    auto from = chosen.front().start, to = chosen.front().start + chosen.front().length;

    for (const auto& clip : chosen)
    {
        from = juce::jmin (from, clip.start);
        to = juce::jmax (to, clip.start + clip.length);
    }

    MixdownSettings settings;
    settings.freezeTrackId = trackId;
    settings.freezeFile = file;
    settings.onlyClipIds = clipIds;
    settings.dry = dry;
    settings.startPpq = from;
    settings.endPpq = to;
    settings.tailSeconds = juce::jmax (0.0, tailSeconds);   // 0: exactly the clips (an export loops as it is)

    const auto needed = freezeNeeds (trackId);
    settings.freezeSources.assign (needed.begin() + 1, needed.end());

    // Only those clips play meanwhile (the track keeps all its clips; collectMixdown plays them again).
    auto all = std::move (track->clips);
    track->clips = std::move (chosen);
    rebuildSequence (*track, true);
    track->clips = std::move (all);

    auto result = startMixdown (std::move (settings));

    if (result.failed())
        rebuildSequence (*track, true);

    return result;
}

bool AudioEngine::isFreezeRendering (int trackId) const noexcept
{
    if (mixdown == nullptr || mixdown->settings.freezeTrackId == 0 || trackId == 0)
        return false;

    const auto& sources = mixdown->settings.freezeSources;
    return trackId == mixdown->settings.freezeTrackId || std::find (sources.begin(), sources.end(), trackId) != sources.end();
}

std::vector<Track::KeyLevel> AudioEngine::keyLevels (int trackId, bool dry) const
{
    std::vector<Track::KeyLevel> levels;

    // With the Analog Board mixing, sends rest: nothing keys a sidechain.
    if (dry)
        return levels;

    const auto needed = freezeNeeds (trackId);
    const auto isNeeded = [&needed] (int id) { return std::find (needed.begin(), needed.end(), id) != needed.end(); };

    for (const auto& key : tracks)
    {
        // A send into a sidechain the render heard (it takes the track after its fader).
        if (std::none_of (key->sends.begin(), key->sends.end(), [&isNeeded] (const Track::Send& send)
                          { return send.sidechain && send.on && send.levelDb > Track::silentDb && isNeeded (send.target); }))
            continue;

        // A fader its automation drives as the song plays: the lane counts, not the knob.
        const bool reading = Automation::reads (key->automationMode.load());
        const auto automated = [&key, reading] (AutomationTarget::Kind kind)
        {
            return reading && std::any_of (key->automation.begin(), key->automation.end(), [kind] (const AutomationLane& lane)
                                           { return lane.target.kind == kind && ! lane.points.empty(); });
        };

        Track::KeyLevel level;
        level.id = key->id;
        level.mute = key->mute.load();
        level.volumeAutomated = automated (AutomationTarget::Kind::volume);
        level.panAutomated = automated (AutomationTarget::Kind::pan);
        level.volumeDb = level.volumeAutomated ? 0.0f : key->volumeDb.load();
        level.pan = level.panAutomated ? 0.0f : key->pan.load();
        levels.push_back (level);
    }

    return levels;
}

void AudioEngine::checkFrozenKeys()
{
    // A track keying a frozen track's render moved its fader, pan or mute (whatever moved it:
    // the page, a control surface, the assistant, an undo): it was rendered as they were.
    for (const auto& track : tracks)
        if (track->isFrozen() && keyLevels (track->id, track->frozenDry) != track->frozenKeys)
            thaw (*track, true);
}

void AudioEngine::faderEnvelopeChanged (Track& key)
{
    // Its volume and pan envelopes come after its own freeze, but before its sends: a frozen
    // track it keys heard them as they were, and so does a freeze rendering with it now.
    if (std::none_of (key.sends.begin(), key.sends.end(), [] (const Track::Send& send) { return send.sidechain && send.on; }))
        return;

    ++key.contentRevision;

    for (const auto& track : tracks)
        if (track->isFrozen() && std::any_of (track->frozenKeys.begin(), track->frozenKeys.end(),
                                              [&key] (const Track::KeyLevel& level) { return level.id == key.id; }))
            thaw (*track, true);
}

juce::Result AudioEngine::applyFreeze (const juce::File& written)
{
    const auto start = std::exchange (freezeStart, {});
    const auto* track = findTrack (start.trackId);

    if (track == nullptr)
        return juce::Result::fail ("That track isn't in the song any more.");

    // Rendered from what plays now: nothing it needs changed meanwhile, nor the tempos, the key,
    // the mixer or the faders of the tracks keying it (an undo, say).
    const auto same = transport.getTempoMap() == start.map && boardWanted.load() == start.dry
                   && std::all_of (start.revisions.begin(), start.revisions.end(), [this] (const std::pair<int, int>& entry)
                                   {
                                       const auto* each = findTrack (entry.first);
                                       return each != nullptr && each->contentRevision == entry.second;
                                   })
                   && keyLevels (start.trackId, start.dry) == start.keys;

    if (! same)
        return juce::Result::fail ("\"" + track->name + "\" changed while it was freezing: freeze it again.");

    // Its instrument rests once it's frozen: not while another track has come to take notes from it.
    const auto source = "track:" + juce::String (start.trackId);

    for (const auto& other : tracks)
        if (other.get() != track && other->midiInputs.contains (source))
            return juce::Result::fail ("\"" + track->name + "\" plays notes into \"" + other->name + "\" now: it can't be frozen.");

    if (! freezeTrack (start.trackId, written, start.dry, true))
        return juce::Result::fail ("Couldn't play the frozen sound of \"" + track->name + "\".");

    return juce::Result::ok();
}

std::unique_ptr<const AudioPlaylist> AudioEngine::frozenPlaylist (const juce::File& file)
{
    juce::String error;
    auto data = file.existsAsFile() ? media.get (file, getPlaybackRate(), error) : nullptr;

    if (data == nullptr)
        return nullptr;

    loadedRenders.insert (file.getFullPathName());

    // The whole render, from the song's start, as it is (no fades of its own).
    AudioPlaylist::Item item;
    item.lengthSamples = static_cast<juce::int64> (data->samples.getNumSamples());
    item.data = std::move (data);

    auto list = std::make_unique<AudioPlaylist>();
    list->items.push_back (std::move (item));
    return list;
}

bool AudioEngine::freezeTrack (int trackId, const juce::File& file, bool dry, bool justMade)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->isMixChannel())
        return false;

    media.forget (file);   // read afresh: never another file's sound kept by its name
    auto render = frozenPlaylist (file);

    if (render == nullptr)
        return false;

    if (justMade)
        sessionRenders.insert (file.getFullPathName());

    // A render it had already goes (thawed quietly: this one takes its place).
    if (track->isFrozen() && track->frozenFile != file)
        thaw (*track, false);

    // Nothing live while it's frozen, and its plugins' windows close: they rest.
    setTrackRecordArm (trackId, false);
    setTrackMonitor (trackId, false);
    hideScreen (track->instrument.get());
    hideScreens (track->inserts);
    hideClipEffectScreens (*track);

    track->frozenFile = file;
    track->frozenDry = dry;
    track->frozenMap = transport.getTempoMap();
    track->frozenKeys = keyLevels (trackId, dry);

    std::unique_ptr<const AudioPlaylist> old;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        old = std::move (track->frozen);
        track->frozen = std::move (render);
    }

    old.reset();
    track->requestNotesOff();
    return true;
}

void AudioEngine::unfreezeTrack (int trackId)
{
    if (auto* track = findTrack (trackId))
        thaw (*track, false);
}

void AudioEngine::thaw (Track& track, bool tellPage)
{
    if (! track.isFrozen())
        return;

    std::unique_ptr<const AudioPlaylist> old;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        old = std::move (track.frozen);
    }

    // Out of memory; its file stays while the song is open (an undo may freeze it again).
    old.reset();
    media.forget (track.frozenFile);
    loadedRenders.erase (track.frozenFile.getFullPathName());

    track.frozenFile = juce::File();
    track.frozenDry = false;
    track.frozenMap = {};
    track.frozenKeys.clear();

    // It plays itself again from here: its notes found afresh.
    track.requestNotesOff();
    track.sequencer.requestChase();

    if (tellPage)
        thawed.push_back (track.id);
}

void AudioEngine::thawDependents (int trackId)
{
    // A frozen track whose render it plays a part in (its notes, its sidechain key) thaws too.
    for (const auto& other : tracks)
    {
        if (other->id == trackId || ! other->isFrozen())
            continue;

        const auto needs = freezeNeeds (other->id);

        if (std::find (needs.begin() + 1, needs.end(), trackId) != needs.end())
            contentChanged (*other);
    }
}

void AudioEngine::contentChanged (Track& track)
{
    ++track.contentRevision;
    thaw (track, true);
    thawDependents (track.id);
}

void AudioEngine::contentChanged (int trackId)
{
    if (auto* track = findTrack (trackId))
        contentChanged (*track);
}

void AudioEngine::markFrozenSaved()
{
    for (const auto& track : tracks)
        if (track->isFrozen())
            savedRenders.insert (track->frozenFile.getFullPathName());
}

void AudioEngine::discardRenders (bool songGoing)
{
    // Made for the song, and named by no saved song: of no more use (unless the song that just
    // opened plays one).
    for (const auto& path : sessionRenders)
        if (savedRenders.count (path) == 0 && (songGoing || ! isFrozenRender (path)))
            juce::File (path).deleteFile();

    sessionRenders.clear();
    savedRenders.clear();

    // Out of memory, unless a track plays it.
    for (auto it = loadedRenders.begin(); it != loadedRenders.end();)
    {
        if (! songGoing && isFrozenRender (*it))
        {
            ++it;
            continue;
        }

        media.forget (juce::File (*it));
        it = loadedRenders.erase (it);
    }
}

bool AudioEngine::isFrozenRender (const juce::String& path) const
{
    return std::any_of (tracks.begin(), tracks.end(), [&path] (const auto& track)
                        { return track->isFrozen() && track->frozenFile.getFullPathName() == path; });
}

void AudioEngine::setBoardMixing (bool shouldMix)
{
    boardWanted.store (shouldMix);

    // A frozen track plays what it fed its mixer: rendered for the other one, it plays itself again.
    for (const auto& track : tracks)
        if (track->isFrozen() && track->frozenDry != shouldMix)
            thaw (*track, true);

    // The board leaves the inserts, buses, FX channels and Main's inserts out: lined up again.
    updateCompensation();
}

//==============================================================================
const std::vector<AutomationLane>* AudioEngine::getAutomation (int trackId) const
{
    const auto* track = findTrack (trackId);
    return track != nullptr ? &track->automation : nullptr;
}

bool AudioEngine::setAutomationLane (int trackId, AutomationLane lane)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return false;

    // A plugin's envelope changes what the track plays (volume and pan come after a freeze).
    const bool parameter = lane.target.kind == AutomationTarget::Kind::parameter;
    lane.points = Automation::tidy (std::move (lane.points));
    auto& lanes = track->automation;
    const auto found = std::find_if (lanes.begin(), lanes.end(), [&lane] (const AutomationLane& l) { return l.target == lane.target; });

    if (lane.points.empty())
    {
        if (found != lanes.end())
            lanes.erase (found);
    }
    else if (found != lanes.end())
    {
        found->points = std::move (lane.points);

        // Its shape regions: as sent (none sent: as they were).
        if (! lane.shapes.isVoid())
            found->shapes = std::move (lane.shapes);
    }
    else
    {
        lanes.push_back (std::move (lane));
    }

    rebuildAutomation (*track);

    if (parameter)
        contentChanged (*track);
    else
        faderEnvelopeChanged (*track);

    return true;
}

bool AudioEngine::setAutomationLanes (int trackId, std::vector<AutomationLane> lanes)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return false;

    for (auto& lane : lanes)
        lane.points = Automation::tidy (std::move (lane.points));

    lanes.erase (std::remove_if (lanes.begin(), lanes.end(), [] (const AutomationLane& l) { return l.points.empty(); }), lanes.end());

    // Its plugins' envelopes changed: what it plays changed. Its volume's and pan's: what it sends.
    const auto same = [] (const std::vector<AutomationLane>& a, const std::vector<AutomationLane>& b, bool parameters)
    {
        std::vector<const AutomationLane*> first, second;
        const auto counts = [parameters] (const AutomationLane& lane) { return (lane.target.kind == AutomationTarget::Kind::parameter) == parameters; };

        for (const auto& lane : a)
            if (counts (lane))
                first.push_back (&lane);

        for (const auto& lane : b)
            if (counts (lane))
                second.push_back (&lane);

        return std::equal (first.begin(), first.end(), second.begin(), second.end(), [] (const AutomationLane* x, const AutomationLane* y)
        {
            return x->target == y->target
                && std::equal (x->points.begin(), x->points.end(), y->points.begin(), y->points.end(), [] (const AutomationPoint& p, const AutomationPoint& q)
                               { return juce::exactlyEqual (p.ppq, q.ppq) && juce::exactlyEqual (p.value, q.value); });
        });
    };

    const auto changed = ! same (track->automation, lanes, true);
    const auto faderChanged = ! same (track->automation, lanes, false);
    track->automation = std::move (lanes);
    rebuildAutomation (*track);

    if (changed)
        contentChanged (*track);

    if (faderChanged)
        faderEnvelopeChanged (*track);

    return true;
}

void AudioEngine::setAutomationMode (int trackId, AutomationMode mode)
{
    if (auto* track = findTrack (trackId))
    {
        const auto before = track->automationMode.exchange (static_cast<int> (mode));

        // Its plugins' envelopes read or not: what it plays changed.
        if (Automation::reads (before) != Automation::reads (static_cast<int> (mode))
            && std::any_of (track->automation.begin(), track->automation.end(), [] (const AutomationLane& lane)
                            { return lane.target.kind == AutomationTarget::Kind::parameter; }))
            contentChanged (*track);
    }
}

void AudioEngine::rebuildAutomation (Track& track)
{
    auto fresh = std::make_unique<AutomationPlayback>();

    for (const auto& lane : track.automation)
        fresh->entries.push_back ({ lane.target, lane.points });

    std::unique_ptr<AutomationPlayback> stale;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        stale = std::move (track.automationPlayback);
        track.automationPlayback = std::move (fresh);
    }

    stale.reset();
}

void AudioEngine::applyAutomation (Track& track, double ppq, double heardPpq) noexcept
{
    auto* playback = track.automationPlayback.get();

    if (playback == nullptr || ! Automation::reads (track.automationMode.load (std::memory_order_relaxed)))
        return;

    for (auto& entry : playback->entries)
    {
        // Taken hold of while it's recorded: the hand moves it, not the envelope (sent again once let go).
        if (track.isHeld (entry.target))
        {
            entry.lastValue = std::numeric_limits<double>::quiet_NaN();
            continue;
        }

        // A fader or pan moves where its sound is heard; a plugin's parameter where it plays.
        const auto value = Automation::valueAt (entry.points, entry.target.kind == AutomationTarget::Kind::parameter ? ppq : heardPpq);

        // Only what changed goes out: a flat envelope costs nothing per block.
        if (juce::exactlyEqual (value, entry.lastValue))
            continue;

        entry.lastValue = value;

        switch (entry.target.kind)
        {
            case AutomationTarget::Kind::volume:
                track.volumeDb.store (static_cast<float> (juce::jlimit (static_cast<double> (Track::silentDb), static_cast<double> (Track::maxDb), value)),
                                      std::memory_order_relaxed);
                break;

            case AutomationTarget::Kind::pan:
                track.pan.store (static_cast<float> (juce::jlimit (-1.0, 1.0, value)), std::memory_order_relaxed);
                break;

            case AutomationTarget::Kind::parameter:
            {
                // Frozen, its plugins rest (their envelopes are in its render): sent again once it thaws.
                if (track.frozen != nullptr)
                {
                    entry.lastValue = std::numeric_limits<double>::quiet_NaN();
                    break;
                }

                // Looked up now: plugins may have been replaced or moved since the envelope was set.
                Plugin* plugin = nullptr;

                if (entry.target.slot == instrumentSlot)
                    plugin = track.instrument.get();
                else if (auto* insert = track.inserts.find (entry.target.slot))
                    plugin = insert->plugin.get();

                if (plugin != nullptr)
                    plugin->setParameterFromAudio (entry.target.paramId, entry.parameterIndex, value);
                else
                    entry.lastValue = std::numeric_limits<double>::quiet_NaN();   // try again when it's back

                break;
            }
        }
    }
}

//==============================================================================
std::vector<AudioEngine::SongInput> AudioEngine::defaultInputs()
{
    return { { "Input L+R", 0, 1, {} }, { "Input L", 0, -1, {} }, { "Input R", 1, -1, {} } };
}

std::vector<AudioEngine::SongInput> AudioEngine::tidyInputs (std::vector<SongInput> list)
{
    for (auto& input : list)
    {
        input.name = input.name.trim().substring (0, 40);

        if (input.name.isEmpty())
            input.name = "Input";

        input.left = juce::jlimit (0, 255, input.left);
        input.right = input.right < 0 ? -1 : juce::jlimit (0, 255, input.right);

        // A mono input's other channels: each once, none its own, in order, so many at the most.
        // A stereo input has none.
        std::vector<int> also;

        if (input.right < 0)
        {
            for (auto channel : input.also)
            {
                channel = juce::jlimit (0, 255, channel);

                if (channel != input.left && std::find (also.begin(), also.end(), channel) == also.end()
                    && static_cast<int> (also.size()) < Track::maxInputChannels - 1)
                    also.push_back (channel);
            }

            std::sort (also.begin(), also.end());
        }

        input.also = std::move (also);
    }

    return list;
}

void AudioEngine::setInputs (std::vector<SongInput> newInputs)
{
    inputs = tidyInputs (std::move (newInputs));

    for (const auto& track : tracks)
        resolveInput (*track);

    resolveVoiceInput();
}

//==============================================================================
// Outputs
std::vector<AudioEngine::SongOutput> AudioEngine::defaultOutputs()
{
    return { SongOutput {} };
}

std::vector<AudioEngine::SongOutput> AudioEngine::tidyOutputs (std::vector<SongOutput> list, int& nextId, std::map<int, int>* newIds)
{
    const auto channel = [] (int value) { return juce::jlimit (0, 1023, value); };
    std::vector<SongOutput> kept;

    // Main first, whatever came: its fader is the Main channel's, and it's Main's mix.
    SongOutput main;

    for (const auto& output : list)
        if (output.id == 0)
            main = output;

    main.name = main.name.trim().substring (0, 40);

    if (main.name.isEmpty())
        main.name = "Main";

    main.left = channel (main.left);
    main.right = main.right < 0 ? -1 : channel (main.right);
    main.mainMix = false;
    main.volumeDb = 0.0f;
    main.mute = false;
    kept.push_back (main);

    // Ids the outputs have stay theirs; the next one only ever grows, so none is used twice.
    nextId = juce::jmax (1, nextId);

    for (const auto& output : list)
        nextId = juce::jmax (nextId, output.id + 1);

    for (auto output : list)
    {
        if (output.id == 0)
            continue;

        if (static_cast<int> (kept.size()) > maxSubOutputs)
            break;

        // An id of its own: a new output (id below 0) gets one, and so does a copy of another's.
        if (const auto given = output.id; given < 0 || std::any_of (kept.begin(), kept.end(), [given] (const SongOutput& o) { return o.id == given; }))
        {
            output.id = nextId++;

            if (given < 0 && newIds != nullptr)
                (*newIds)[given] = output.id;
        }

        output.name = output.name.trim().substring (0, 40);

        if (output.name.isEmpty())
            output.name = "Sub " + juce::String (static_cast<int> (kept.size()));

        output.left = channel (output.left);
        output.right = output.right < 0 ? -1 : channel (output.right);
        output.volumeDb = juce::jlimit (Track::silentDb, Track::maxDb, output.volumeDb);
        kept.push_back (output);
    }

    return kept;
}

bool AudioEngine::hasOutput (int outputId) const noexcept
{
    return std::any_of (outputs.begin(), outputs.end(), [outputId] (const SongOutput& o) { return o.id == outputId; });
}

int AudioEngine::subSlotOf (int outputId) const noexcept
{
    const auto subs = juce::jmin (maxSubOutputs, static_cast<int> (outputs.size()) - 1);

    for (int slot = 0; outputId != 0 && slot < subs; ++slot)
        if (outputs[static_cast<size_t> (slot + 1)].id == outputId)
            return slot;

    return -1;
}

void AudioEngine::setOutputs (std::vector<SongOutput> newOutputs, std::optional<int> audition)
{
    std::map<int, int> newIds;
    outputs = tidyOutputs (std::move (newOutputs), nextOutputId, &newIds);

    if (audition.has_value())
        auditionOutput = newIds.count (*audition) > 0 ? newIds[*audition] : *audition;

    // Tracks on a Sub that's gone play into Main, and so do the preview and the click.
    for (const auto& track : tracks)
        if (! hasOutput (track->output))
            track->output = 0;

    if (! hasOutput (auditionOutput))
        auditionOutput = 0;

    if (! hasOutput (livePhonesOutput))
        livePhonesOutput = 0;

    planOutputs();
    updateCompensation();   // a new Sub's delays (plugin delay compensation)
}

void AudioEngine::planOutputs()
{
    OutputPlan plan;
    plan.mainLeft = outputs.front().left;
    plan.mainRight = outputs.front().right;
    plan.subCount = juce::jmin (maxSubOutputs, static_cast<int> (outputs.size()) - 1);

    for (int slot = 0; slot < plan.subCount; ++slot)
    {
        const auto& sub = outputs[static_cast<size_t> (slot + 1)];
        plan.subs[static_cast<size_t> (slot)] = { sub.left, sub.right, sub.mainMix };
    }

    plan.audition = subSlotOf (auditionOutput);
    plan.phones = subSlotOf (livePhonesOutput);

    std::vector<int> slots;
    slots.reserve (tracks.size());

    for (const auto& track : tracks)
        slots.push_back (subSlotOf (track->output));

    // The audio thread sees the new plan, the faders and the tracks' places all at once.
    const juce::SpinLock::ScopedLockType lock (graphLock);
    outputPlan = plan;

    for (int slot = 0; slot < plan.subCount; ++slot)
    {
        const auto& sub = outputs[static_cast<size_t> (slot + 1)];
        subVolumeDb[static_cast<size_t> (slot)].store (sub.volumeDb, std::memory_order_relaxed);
        subMute[static_cast<size_t> (slot)].store (sub.mute, std::memory_order_relaxed);
    }

    for (size_t i = 0; i < tracks.size(); ++i)
        tracks[i]->outputSlot.store (slots[i], std::memory_order_relaxed);
}

void AudioEngine::resolveOutput (Track& track)
{
    if (! hasOutput (track.output))
        track.output = 0;

    track.outputSlot.store (subSlotOf (track.output));
}

void AudioEngine::setTrackOutput (int trackId, int outputId)
{
    if (auto* track = findTrack (trackId); track != nullptr && hasOutput (outputId))
    {
        track->output = outputId;
        track->outputSlot.store (subSlotOf (outputId));
    }
}

void AudioEngine::setTrackBus (int trackId, int busId)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->isMixChannel())
        return;

    // A bus, or none: buses don't play into buses, nor FX channels into either.
    if (const auto* bus = busId != 0 ? findTrack (busId) : nullptr; busId != 0 && (bus == nullptr || bus->kind != Track::Kind::bus))
        return;

    track->outputBus.store (busId);
}

int AudioEngine::getTrackBus (int trackId) const
{
    const auto* track = findTrack (trackId);
    const auto* bus = track != nullptr && track->outputBus.load() != 0 ? findTrack (track->outputBus.load()) : nullptr;
    return bus != nullptr && bus->kind == Track::Kind::bus ? bus->id : 0;
}

Track* AudioEngine::busOf (const Track& track) const noexcept
{
    const auto busId = track.outputBus.load (std::memory_order_relaxed);

    if (busId == 0 || track.isMixChannel())
        return nullptr;

    for (const auto& other : tracks)
        if (other->id == busId)
            return other->kind == Track::Kind::bus ? other.get() : nullptr;

    return nullptr;
}

bool AudioEngine::soloedThroughBus (const Track& track) const noexcept
{
    if (track.kind == Track::Kind::bus)
    {
        for (const auto& other : tracks)
            if (other->solo.load (std::memory_order_relaxed) && busOf (*other) == &track)
                return true;

        return false;
    }

    const auto* bus = busOf (track);
    return bus != nullptr && bus->solo.load (std::memory_order_relaxed);
}

void AudioEngine::setOutputVolume (int outputId, float volumeDb)
{
    if (const auto slot = subSlotOf (outputId); slot >= 0)
    {
        const auto db = juce::jlimit (Track::silentDb, Track::maxDb, volumeDb);
        outputs[static_cast<size_t> (slot + 1)].volumeDb = db;
        subVolumeDb[static_cast<size_t> (slot)].store (db);
    }
}

void AudioEngine::setOutputMute (int outputId, bool mute)
{
    if (const auto slot = subSlotOf (outputId); slot >= 0)
    {
        outputs[static_cast<size_t> (slot + 1)].mute = mute;
        subMute[static_cast<size_t> (slot)].store (mute);
    }
}

void AudioEngine::setAuditionOutput (int outputId)
{
    auditionOutput = hasOutput (outputId) ? outputId : 0;
    planOutputs();
}

void AudioEngine::setSongDefaults (std::vector<SongInput> defaultInputsToUse, std::vector<SongOutput> defaultOutputsToUse, int audition)
{
    int next = 1;
    std::map<int, int> newIds;
    songDefaultInputs = tidyInputs (std::move (defaultInputsToUse));
    songDefaultOutputs = tidyOutputs (std::move (defaultOutputsToUse), next, &newIds);

    if (newIds.count (audition) > 0)
        audition = newIds[audition];

    songDefaultAudition = std::any_of (songDefaultOutputs.begin(), songDefaultOutputs.end(), [audition] (const SongOutput& o) { return o.id == audition; })
                              ? audition : 0;
}

juce::StringArray AudioEngine::getOutputChannelNames()
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        outputChannelNames = device->getOutputChannelNames();

    return outputChannelNames;
}

void AudioEngine::openAllOutputChannels()
{
    auto* device = deviceManager.getCurrentAudioDevice();

    // Not while it's let go, nor while a take records or a mixdown renders (reopening would pull
    // the device, and the mixdown's buffers, from under them): it's tried again afterwards.
    if (device == nullptr || deviceReleased || mixdown != nullptr || isTakeActive() || transport.getState().recording)
        return;

    outputChannelNames = device->getOutputChannelNames();

    juce::BigInteger all;
    all.setRange (0, outputChannelNames.size(), true);

    const auto active = device->getActiveOutputChannels();

    if (active == all)
    {
        outputsAskedOf.clear();   // all open: the next device (or rate) is asked afresh
        return;
    }

    // Asked once for this device at this rate (some halve their channels at high rates): a driver
    // that won't open them all keeps what it had.
    const auto key = device->getTypeName() + "|" + device->getName() + "|" + juce::String (outputChannelNames.size())
                     + "|" + juce::String (device->getCurrentSampleRate());

    if (key == outputsAskedOf)
        return;

    outputsAskedOf = key;

    const auto before = deviceManager.getAudioDeviceSetup();
    auto setup = before;
    setup.useDefaultOutputChannels = false;
    setup.outputChannels = all;

    // Refused: back to exactly the channels it had (asking for "the default" again could ask for
    // them all again, and leave no device at all).
    if (deviceManager.setAudioDeviceSetup (setup, true).isNotEmpty())
    {
        auto back = before;
        back.useDefaultOutputChannels = false;
        back.outputChannels = active;
        deviceManager.setAudioDeviceSetup (back, true);
    }
}

//==============================================================================
// Voice control
void AudioEngine::setVoiceInput (int inputIndex)
{
    voiceInput = inputIndex;
    resolveVoiceInput();
}

void AudioEngine::resolveVoiceInput() noexcept
{
    const auto valid = juce::isPositiveAndBelow (voiceInput, static_cast<int> (inputs.size()));
    voiceLeft.store (valid ? inputs[static_cast<size_t> (voiceInput)].left : -1);
    voiceRight.store (valid ? inputs[static_cast<size_t> (voiceInput)].right : -1);
}

void AudioEngine::captureVoice (const float* const* inputChannels, int numInputs, int numSamples) noexcept
{
    const auto leftChannel = voiceLeft.load (std::memory_order_relaxed);
    const auto rightChannel = voiceRight.load (std::memory_order_relaxed);

    if (! voiceListening.load (std::memory_order_relaxed) || ! juce::isPositiveAndBelow (leftChannel, numInputs)
        || inputChannels[leftChannel] == nullptr)
        return;

    const auto* left = inputChannels[leftChannel];
    const auto* right = juce::isPositiveAndBelow (rightChannel, numInputs) ? inputChannels[rightChannel] : nullptr;

    // What doesn't fit is dropped (the listener has fallen far behind; it never waits for the audio thread).
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    voiceQueue.prepareToWrite (numSamples, start1, size1, start2, size2);

    auto copy = [&] (int to, int from, int count)
    {
        for (int i = 0; i < count; ++i)
            voiceSamples[static_cast<size_t> (to + i)] = right != nullptr ? 0.5f * (left[from + i] + right[from + i]) : left[from + i];
    };

    copy (start1, 0, size1);
    copy (start2, size1, size2);
    voiceQueue.finishedWrite (size1 + size2);
}

int AudioEngine::readVoice (float* destination, int maxSamples) noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    voiceQueue.prepareToRead (maxSamples, start1, size1, start2, size2);

    if (size1 > 0) std::memcpy (destination, voiceSamples.data() + start1, sizeof (float) * static_cast<size_t> (size1));
    if (size2 > 0) std::memcpy (destination + size1, voiceSamples.data() + start2, sizeof (float) * static_cast<size_t> (size2));

    voiceQueue.finishedRead (size1 + size2);
    return size1 + size2;
}

namespace
{
    /** A cue's notes: pitch, when it starts and how long it lasts (milliseconds). */
    struct CueNote
    {
        float hz;
        int startMs, lengthMs;
    };

    struct CueShape
    {
        std::array<CueNote, 3> notes;
        int count;
    };

    // listening: "go on", rising | heard: one bright note | done: a little chime | question: a call
    // and its echo | problem: falling, low.
    constexpr std::array<CueShape, 5> cueShapes {{
        { {{ { 587.3f, 0, 90 }, { 784.0f, 120, 110 }, { 0.0f, 0, 0 } }}, 2 },
        { {{ { 987.8f, 0, 120 }, { 0.0f, 0, 0 }, { 0.0f, 0, 0 } }}, 1 },
        { {{ { 784.0f, 0, 70 }, { 987.8f, 90, 70 }, { 1318.5f, 180, 130 } }}, 3 },
        { {{ { 880.0f, 0, 110 }, { 880.0f, 200, 110 }, { 0.0f, 0, 0 } }}, 2 },
        { {{ { 440.0f, 0, 150 }, { 329.6f, 190, 220 }, { 0.0f, 0, 0 } }}, 2 },
    }};
}

void AudioEngine::mixCue (float* left, float* right, int numSamples) noexcept
{
    if (const auto requested = pendingCue.exchange (0); requested > 0)
    {
        cuePlaying = requested;   // a new cue cuts the one sounding
        cueSample = 0;
        cuePhase = 0.0;
    }

    if (cuePlaying == 0 || left == nullptr || currentSampleRate <= 0.0)
    {
        cueSounding.store (false, std::memory_order_relaxed);
        return;
    }

    cueSounding.store (true, std::memory_order_relaxed);

    const auto& shape = cueShapes[static_cast<size_t> (juce::jlimit (1, 5, cuePlaying) - 1)];
    const auto rate = currentSampleRate;
    const auto toSamples = [rate] (int ms) { return static_cast<juce::int64> (ms * rate / 1000.0); };
    const auto fade = juce::jmax<juce::int64> (1, toSamples (6));   // no clicks
    const auto& last = shape.notes[static_cast<size_t> (shape.count - 1)];
    const auto end = toSamples (last.startMs + last.lengthMs);
    constexpr float level = 0.18f;   // about -15 dBFS

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = cueSample + i;

        for (int n = 0; n < shape.count; ++n)
        {
            const auto& note = shape.notes[static_cast<size_t> (n)];
            const auto start = toSamples (note.startMs);
            const auto length = toSamples (note.lengthMs);

            if (t < start || t >= start + length)
                continue;

            const auto into = t - start;

            if (into == 0)
                cuePhase = 0.0;

            const auto edge = juce::jmin (into, length - 1 - into);
            const auto envelope = edge < fade ? static_cast<float> (edge) / static_cast<float> (fade) : 1.0f;
            const auto value = level * envelope * static_cast<float> (std::sin (cuePhase));
            cuePhase += juce::MathConstants<double>::twoPi * note.hz / rate;

            left[i] += value;

            if (right != nullptr)
                right[i] += value;
        }
    }

    cueSample += numSamples;

    if (cueSample >= end)
    {
        cuePlaying = 0;
        cueSounding.store (false, std::memory_order_relaxed);
    }
}

void AudioEngine::setTrackInput (int trackId, int inputIndex)
{
    if (auto* track = findTrack (trackId))
    {
        track->input = juce::jmax (0, inputIndex);
        resolveInput (*track);
    }
}

void AudioEngine::setTrackMono (int trackId, bool mono)
{
    // Its recording keeps the channels it started with.
    if (auto* track = findTrack (trackId); track != nullptr && track->kind == Track::Kind::audio && track->recorder == nullptr)
        track->monoMode.store (mono);
}

bool AudioEngine::setTrackChoir (int trackId, const ChoirSettings& wanted)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->kind != Track::Kind::audio)
        return false;

    auto settings = wanted;

    if (settings.chordTrack == trackId)
        settings.chordTrack = 0;

    const bool wasChoir = track->choir != nullptr;
    const auto before = wasChoir ? track->choir->getSettings() : ChoirSettings();
    std::unique_ptr<Choir> fresh;
    auto* choir = track->choir.get();

    if (choir == nullptr)
    {
        fresh = std::make_unique<Choir>();
        fresh->prepare (getPlaybackRate(), currentBlockSize > 0 ? currentBlockSize : 512);
        choir = fresh.get();
    }

    // The shifters new voices need are made here; the audio thread only sees them swapped in.
    auto change = choir->prepareChange (settings);

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        choir->apply (change);

        if (fresh != nullptr)
            track->choir = std::move (fresh);
    }

    if (! wasChoir || before.chordTrack != settings.chordTrack)
        rebuildChoirTimeline (*track);

    if (! wasChoir || before != settings)
        contentChanged (*track);

    return true;
}

const ChoirSettings* AudioEngine::getTrackChoir (int trackId) const
{
    const auto* track = findTrack (trackId);
    return track != nullptr && track->choir != nullptr ? &track->choir->getSettings() : nullptr;
}

bool AudioEngine::rebuildChoirTimeline (Track& track)
{
    if (track.choir == nullptr)
        return false;

    auto timeline = std::make_unique<Choir::Timeline>();
    const auto& map = transport.getTempoMap();
    const auto rate = getPlaybackRate();
    const auto toSample = [&map, rate] (double ppq) { return static_cast<juce::int64> (std::llround (map.secondsAt (juce::jmax (0.0, ppq)) * rate)); };

    for (const auto& key : map.getKeys())
        timeline->keys.push_back ({ toSample (map.ppqOfBar (key.bar)), key.root, TempoMap::scaleNotes (key.scale) });

    // The chords: the pitch classes sounding together on the chord track, from each change on
    // (two or more; a lone note, a bass line between chords, keeps the chord before).
    if (const auto* source = findTrack (track.choir->getSettings().chordTrack); source != nullptr && source->kind == Track::Kind::instrument)
    {
        std::vector<std::pair<double, int>> edges;   // song position, +/-(pitch class + 1)

        for (const auto& clip : source->clips)
        {
            if (clip.muted || clip.isAudio())
                continue;

            const auto clipEnd = clip.start + clip.length;

            for (const auto& note : clip.notes)
            {
                if (note.muted || note.start < clip.offset || note.start >= clip.offset + clip.length)
                    continue;

                const auto from = clip.songPosition (note.start);
                const auto to = juce::jmin (clipEnd, clip.songPosition (note.start + note.length));

                if (to <= from)
                    continue;

                const auto pitchClass = ((note.pitch % 12) + 12) % 12;
                edges.emplace_back (from, pitchClass + 1);
                edges.emplace_back (to, -(pitchClass + 1));
            }
        }

        // Ends before starts at the same moment, so a chord change isn't heard as both chords.
        std::sort (edges.begin(), edges.end(), [] (const auto& a, const auto& b) { return a.first < b.first || (juce::exactlyEqual (a.first, b.first) && a.second < b.second); });

        std::array<int, 12> sounding {};
        juce::uint16 last = 0;

        for (size_t i = 0; i < edges.size();)
        {
            const auto at = edges[i].first;

            for (; i < edges.size() && juce::exactlyEqual (edges[i].first, at); ++i)
                sounding[static_cast<size_t> (std::abs (edges[i].second) - 1)] += edges[i].second > 0 ? 1 : -1;

            juce::uint16 notes = 0;
            int count = 0;

            for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
            {
                if (sounding[static_cast<size_t> (pitchClass)] > 0)
                {
                    notes = static_cast<juce::uint16> (notes | (1 << pitchClass));
                    ++count;
                }
            }

            if (count >= 2 && notes != last)
            {
                timeline->chords.push_back ({ toSample (at), notes });
                last = notes;
            }
        }
    }

    if (const auto* now = track.choir->getTimeline(); now != nullptr && *now == *timeline)
        return false;

    std::unique_ptr<const Choir::Timeline> old;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        old = track.choir->swapTimeline (std::move (timeline));
    }

    old.reset();
    return true;
}

void AudioEngine::refreshChoirsFollowing (int trackId)
{
    for (const auto& other : tracks)
        if (other->choir != nullptr && other->id != trackId && other->choir->getSettings().chordTrack == trackId)
            if (rebuildChoirTimeline (*other))
                contentChanged (*other);
}

void AudioEngine::resolveInput (Track& track) noexcept
{
    if (track.kind != Track::Kind::audio)
        return;

    const auto valid = juce::isPositiveAndBelow (track.input, static_cast<int> (inputs.size()));
    track.inputLeft.store (valid ? inputs[static_cast<size_t> (track.input)].left : -1);
    track.inputRight.store (valid ? inputs[static_cast<size_t> (track.input)].right : -1);

    // A mono input's other channels (each stored one up: 0 is none).
    for (size_t i = 0; i < track.inputAlso.size(); ++i)
        track.inputAlso[i].store (valid && i < inputs[static_cast<size_t> (track.input)].also.size()
                                      ? inputs[static_cast<size_t> (track.input)].also[i] + 1 : 0);
}

juce::StringArray AudioEngine::getInputChannelNames() const
{
    juce::StringArray names;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto all = device->getInputChannelNames();
        const auto active = device->getActiveInputChannels();

        for (int i = 0; i < all.size(); ++i)
            if (active[i])
                names.add (all[i]);
    }

    return names;
}

void AudioEngine::setRecordBits (int bits)
{
    recordBits = bits == 16 || bits == 32 ? bits : 24;
}

void AudioEngine::beginAudioRecording()
{
    if (audioTakeActive)
        return;

    recordCompensation = getOutputLatency();   // how late the song is heard while it's played to

    auto folder = recordingFolder;

    if (folder == juce::File())
        folder = UserData::recordings();   // Settings > Locations > User Data

    folder.createDirectory();
    juce::WavAudioFormat wav;
    const auto format = recordBits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                         : juce::AudioFormatWriterOptions::SampleFormat::integral;

    for (const auto& track : tracks)
    {
        if (track->kind != Track::Kind::audio || ! track->recordArm.load() || track->inputLeft.load() < 0 || track->isFrozen())
            continue;

        // Stereo on a stereo channel hearing two channels or more; one sound on a mono channel.
        const auto channels = track->recordingChannels();
        const auto file = folder.getNonexistentChildFile (juce::File::createLegalFileName (track->name), ".wav", false);
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

        if (stream == nullptr)
            continue;

        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (getPlaybackRate())
                                                                                     .withNumChannels (channels)
                                                                                     .withBitsPerSample (recordBits)
                                                                                     .withSampleFormat (format));

        if (writer == nullptr)
        {
            file.deleteFile();
            continue;
        }

        AudioRecording recording;
        recording.trackId = track->id;
        recording.file = file;
        recording.writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 1 << 17);

        // Room for its waveform, three hours of it, made here so the audio thread never allocates.
        recording.peaks = std::make_unique<Track::LivePeaks>();
        recording.peaks->data.assign (2 * static_cast<size_t> (getPlaybackRate() * 3.0 * 3600.0 / Track::LivePeaks::peakBucket + 2.0), 0);
        audioRecordings.push_back (std::move (recording));
    }

    if (audioRecordings.empty())
        return;

    // Pass marks from before this take mean nothing to it.
    if (const auto ready = passQueue.getNumReady(); ready > 0)
        passQueue.read (ready);

    takePasses.clear();

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        for (auto& recording : audioRecordings)
        {
            if (auto* track = findTrack (recording.trackId))
            {
                track->recorder = recording.writer.get();
                track->livePeaks = recording.peaks.get();
            }
        }
    }

    audioTakeActive = true;
}

std::vector<AudioEngine::LiveWave> AudioEngine::getLiveWaves() const
{
    std::vector<LiveWave> waves;

    if (! audioTakeActive || takePasses.empty())
        return waves;

    // The take shows from its latest pass, as late into the file as the plugins' delay
    // compensation makes it (as its clip will).
    const auto& pass = takePasses.back();
    const auto firstBucket = (pass.fileSample + recordCompensation) / Track::LivePeaks::peakBucket;

    for (const auto& recording : audioRecordings)
    {
        if (recording.peaks == nullptr)
            continue;

        LiveWave wave;
        wave.trackId = recording.trackId;
        wave.ppq = pass.ppq;
        wave.firstBucket = firstBucket;
        wave.buckets = recording.peaks->buckets.load (std::memory_order_acquire);
        wave.data = recording.peaks->data.data();
        wave.rate = getPlaybackRate();
        waves.push_back (wave);
    }

    return waves;
}

void AudioEngine::finishAudioRecording()
{
    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        for (auto& recording : audioRecordings)
        {
            if (auto* track = findTrack (recording.trackId))
            {
                track->recorder = nullptr;
                track->livePeaks = nullptr;
            }
        }
    }

    // Like replacing takes: with the loop on, the last time round is the take.
    const auto pass = takePasses.empty() ? Pass { 0, 0.0 } : takePasses.back();

    for (auto& recording : audioRecordings)
    {
        recording.writer.reset();   // writes out what's queued and closes the file
        media.forget (recording.file);

        juce::String error;
        const auto data = media.get (recording.file, getPlaybackRate(), error);
        const auto total = data != nullptr ? static_cast<juce::int64> (std::llround (data->lengthSeconds * data->fileRate)) : 0;
        const auto start = juce::jlimit<juce::int64> (0, total, pass.fileSample);

        // Nothing (or next to nothing) recorded: no clip, no file.
        if (data == nullptr || takePasses.empty() || static_cast<double> (total - start) < 0.01 * data->fileRate)
        {
            media.forget (recording.file);
            recording.file.deleteFile();
            continue;
        }

        // Heard late by the plugins' delay compensation, it was played late by that much: the
        // take starts that much further into the file, so it lands where it was played.
        const auto played = juce::jlimit<juce::int64> (0, total, start + recordCompensation);

        if (static_cast<double> (total - played) < 0.01 * data->fileRate)
        {
            media.forget (recording.file);
            recording.file.deleteFile();
            continue;
        }

        MidiClip clip;
        clip.start = pass.ppq;
        clip.audioFile = recording.file.getFullPathName();
        clip.audioOffset = static_cast<double> (played) / data->fileRate;
        clip.audioLength = static_cast<double> (total - played) / data->fileRate;
        addClip (recording.trackId, clip);
    }

    audioRecordings.clear();
    takePasses.clear();
    audioTakeActive = false;
}

void AudioEngine::abandonAudioRecording()
{
    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        for (auto& recording : audioRecordings)
        {
            if (auto* track = findTrack (recording.trackId))
            {
                track->recorder = nullptr;
                track->livePeaks = nullptr;
            }
        }
    }

    for (auto& recording : audioRecordings)
        recording.writer.reset();

    audioRecordings.clear();
    takePasses.clear();
    audioTakeActive = false;
}

void AudioEngine::fitAudioLength (MidiClip& clip) const noexcept
{
    // Where an audio clip starts decides how many quarter notes its seconds are (unless it
    // follows the tempo: then its beats are its length).
    if (clip.isAudio() && ! clip.followsTempo())
        clip.length = audioLengthInPpq (clip.start, clip.audioLength);
}

bool AudioEngine::followTempo()
{
    // Audio keeps its speed: at a new tempo (or a new map) its clips cover a different number
    // of quarter notes, and start at a different time.
    const auto generation = transport.getMapGeneration();

    if (generation == followedGeneration)
        return false;

    followedGeneration = generation;
    bool any = false;

    for (const auto& track : tracks)
    {
        bool audio = false;

        for (auto& clip : track->clips)
        {
            if (clip.isAudio())
            {
                // One that follows the tempo keeps its beats; its new render is on the way.
                if (! clip.followsTempo())
                    clip.length = audioLengthInPpq (clip.start, clip.audioLength);

                audio = any = true;
            }
        }

        if (audio)
            rebuildSequence (*track, false);

        // A frozen track's render keeps the tempos, signatures and key it was made at (its MIDI FX
        // follow the key): at others, it plays itself again.
        if (track->isFrozen() && ! (track->frozenMap == transport.getTempoMap()))
            thaw (*track, true);
    }

    return any;
}

void AudioEngine::stopRecording()
{
    transport.setRecording (false);
}

bool AudioEngine::isRecordingInto (int trackId, int clipId) const noexcept
{
    if (! take.active)
        return false;

    const auto found = take.clipIds.find (trackId);
    return found != take.clipIds.end() && found->second == clipId;
}

void AudioEngine::rebuildSequence (Track& track, bool releaseSounding)
{
    // An audio track's clips become a playlist of files in memory.
    if (track.kind == Track::Kind::audio)
    {
        auto list = std::make_unique<AudioPlaylist>();

        // Clips with an effect of their own play through it (the plugins of those still there keep playing).
        auto effects = buildClipEffects (track);

        const auto slotOf = [&effects] (int clipId)
        {
            for (const auto& effect : effects)
                if (effect->clipId == clipId)
                    return effect->slotId;

            return 0;
        };

        for (const auto& clip : track.clips)
        {
            if (clip.muted)
                continue;   // the Mute tool's: kept, but silent

            juce::String error;
            auto data = clip.isAudio() ? media.get (juce::File (clip.audioFile), getPlaybackRate(), error) : nullptr;

            if (data == nullptr)
                continue;   // a missing file is silent; the song keeps the clip

            AudioPlaylist::Item item;
            item.startSample = static_cast<juce::int64> (std::llround (transport.getTempoMap().secondsAt (clip.start) * data->playbackRate));
            item.offsetSamples = static_cast<juce::int64> (std::llround (clip.audioOffset * data->playbackRate));
            item.lengthSamples = static_cast<juce::int64> (std::llround (clip.audioLength * data->playbackRate));
            item.gain = juce::Decibels::decibelsToGain (static_cast<float> (clip.audioGainDb), -100.0f);
            item.fadeInSamples = juce::jmin (item.lengthSamples, static_cast<juce::int64> (std::llround (clip.fadeIn * data->playbackRate)));
            item.fadeOutSamples = juce::jmin (item.lengthSamples, static_cast<juce::int64> (std::llround (clip.fadeOut * data->playbackRate)));
            if (clip.silence.on)
            {
                item.silence = silenceRegions (data, clip.silence);
                item.silenceFadeSamples = static_cast<juce::int64> (std::llround (juce::jlimit (0.001, 1.0, clip.silence.fade) * data->playbackRate));
            }

            // The clip's own automation, in the file's samples.
            const auto rate = data->playbackRate;
            const auto curveOf = [rate] (const std::vector<ClipEnvelopePoint>& points)
            {
                auto curve = std::make_shared<AudioPlaylist::Curve>();

                for (const auto& point : points)
                {
                    curve->at.push_back (static_cast<juce::int64> (std::llround (point.time * rate)));
                    curve->values.push_back (static_cast<float> (point.value));
                }

                return curve;
            };

            if (const auto* line = clip.envelope ("gain"))
                item.gainCurve = curveOf (line->points);

            if (const auto* line = clip.envelope ("pan"))
                item.panCurve = curveOf (line->points);

            if (clip.effect != nullptr)
            {
                auto params = std::make_shared<AudioPlaylist::ParamCurves>();

                for (const auto& line : clip.envelopes)
                {
                    if (! line.key.startsWith ("fx:"))
                        continue;

                    params->paramIds.push_back (static_cast<juce::uint32> (line.key.substring (3).getLargeIntValue()));
                    params->curves.push_back (*curveOf (line.points));
                }

                if (! params->curves.empty())
                {
                    params->lastSent.assign (params->curves.size(), std::numeric_limits<double>::quiet_NaN());
                    params->indexHints.assign (params->curves.size(), -1);
                    item.paramCurves = std::move (params);
                }
            }

            item.data = std::move (data);
            item.effectSlot = slotOf (clip.id);
            list->items.push_back (std::move (item));
        }

        std::unique_ptr<const AudioPlaylist> fresh (std::move (list)), stale;

        {
            const juce::SpinLock::ScopedLockType lock (graphLock);
            stale = std::move (track.playlist);
            track.playlist = std::move (fresh);
            track.clipEffects.swap (effects);   // effects: the list as it was
        }

        stale.reset();

        // Effects gone (their clip deleted or moved, their effect replaced or taken away): their
        // windows close, and their state is kept for a clip that brings them back (undo, paste).
        for (const auto& old : effects)
        {
            if (old == nullptr || track.findClipEffect (old->slotId) != nullptr)
                continue;

            if (old->plugin != nullptr)
            {
                juce::MemoryBlock state;

                if (old->plugin->canSaveState() && old->plugin->saveState (state))
                    clipEffectStatesLeft[old->uid] = std::move (state);

                hideScreen (old->plugin.get());
            }
        }

        effects.clear();   // destroyed here, outside the lock

        // A choir track: its keys and chords at the song's tempo now.
        if (track.choir != nullptr)
            rebuildChoirTimeline (track);

        return;
    }

    // The clip a take records into: its bend and aftertouch come from the player's hands now,
    // so they aren't played back over them (they play once the take is done).
    const auto recording = take.active ? take.clipIds.find (track.id) : take.clipIds.end();
    auto fresh = Sequence::build (track.clips, recording != take.clipIds.end() ? recording->second : 0, track.sequence.get());
    std::unique_ptr<const Sequence> old;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        // The notes' operators go on counting their passes where they were (an edit mid-loop).
        if (track.sequence != nullptr)
            fresh->carryFrom (*track.sequence);

        old = std::move (track.sequence);
        track.sequence = std::move (fresh);
    }

    // Removing notes could leave one sounding with nothing left to end it. Otherwise what's
    // sounding goes on, and the bend and aftertouch are found again where they now stand.
    if (releaseSounding)
        track.sequencer.requestRelease();
    else
        track.sequencer.requestChase();

    old.reset();

    // Choir tracks singing to this track's chords follow its notes.
    refreshChoirsFollowing (track.id);
}

int AudioEngine::addClip (int trackId, MidiClip clip, bool keepId)
{
    auto* track = findTrack (trackId);

    // Audio clips need an audio track and a window; MIDI clips an instrument track.
    if (track == nullptr || clip.isAudio() != (track->kind == Track::Kind::audio))
        return 0;

    if (clip.isAudio())
    {
        if (! (clip.audioLength > 0.0) || ! std::isfinite (clip.audioOffset))
            return 0;

        // Its seconds in quarter notes where it starts (one that follows the tempo keeps its beats).
        clip.audioOffset = juce::jmax (0.0, clip.audioOffset);
        clip.start = juce::jmax (0.0, clip.start);
        fitAudioLength (clip);
    }

    if (! (clip.length > 0.0))
        return 0;

    // Its own id kept when asked and no clip has it; new ids stay above it.
    const auto wanted = clip.id;
    const auto keptId = keepId && wanted > 0
                     && std::none_of (tracks.begin(), tracks.end(), [wanted] (const auto& each)
                                      { return std::any_of (each->clips.begin(), each->clips.end(), [wanted] (const MidiClip& c) { return c.id == wanted; }); });

    clip.id = keptId ? wanted : nextClipId;
    nextClipId = juce::jmax (nextClipId, clip.id + 1);
    clip.start = juce::jmax (0.0, clip.start);
    std::stable_sort (clip.notes.begin(), clip.notes.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start; });
    MidiClip::tidy (clip.pitchBend, MidiClip::pitchBendMin, MidiClip::pitchBendMax);
    MidiClip::tidy (clip.aftertouch, 0, MidiClip::aftertouchMax);
    ClipEnvelope::tidy (clip.envelopes, clip.isAudio());
    track->clips.push_back (std::move (clip));
    rebuildSequence (*track, false);
    contentChanged (*track);
    return track->clips.back().id;
}

bool AudioEngine::deleteClip (int trackId, int clipId)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return false;

    auto& clips = track->clips;
    const auto found = std::find_if (clips.begin(), clips.end(), [clipId] (const MidiClip& clip) { return clip.id == clipId; });

    if (found == clips.end())
        return false;

    clips.erase (found);

    // Notes still arriving in this take get a fresh clip.
    if (isRecordingInto (trackId, clipId))
        take.clipIds.erase (trackId);

    rebuildSequence (*track, true);
    contentChanged (*track);
    return true;
}

MidiClip* AudioEngine::findClip (Track& track, int clipId) noexcept
{
    for (auto& clip : track.clips)
        if (clip.id == clipId)
            return &clip;

    return nullptr;
}

bool AudioEngine::setClipNotes (int trackId, int clipId, std::vector<MidiNote> notes, const ClipWindow* window,
                                const std::vector<MidiControlPoint>* pitchBend, const std::vector<MidiControlPoint>* aftertouch,
                                const juce::uint32* seed)
{
    auto* track = findTrack (trackId);
    auto* clip = track != nullptr ? findClip (*track, clipId) : nullptr;

    if (clip == nullptr || clip->isAudio())
        return false;

    // A window that makes no sense is ignored rather than half applied.
    bool windowChanged = false;

    if (window != nullptr && std::isfinite (window->start) && std::isfinite (window->offset)
        && window->start >= 0.0 && window->length >= minimumClipLength)
    {
        windowChanged = ! juce::exactlyEqual (clip->start, window->start) || ! juce::exactlyEqual (clip->length, window->length)
                     || ! juce::exactlyEqual (clip->offset, window->offset);
        clip->start = window->start;
        clip->length = window->length;
        clip->offset = window->offset;
    }

    notes.erase (std::remove_if (notes.begin(), notes.end(), [] (const MidiNote& note)
    {
        return note.pitch < 0 || note.pitch > 127 || ! std::isfinite (note.start) || ! std::isfinite (note.length);
    }), notes.end());

    for (auto& note : notes)
    {
        note.length = juce::jmax (note.length, Sequence::minimumNoteLength);
        note.velocity = juce::jlimit (1.0f / 127.0f, 1.0f, note.velocity);
    }

    std::stable_sort (notes.begin(), notes.end(), [] (const MidiNote& a, const MidiNote& b)
    {
        return a.start < b.start || (! (b.start < a.start) && a.pitch < b.pitch);
    });

    // The same notes, muted or not (a note muted while it sounds has no note-off left to end it:
    // released). A change of operators alone plays at once, nothing released.
    const auto sameNotes = notes.size() == clip->notes.size()
                        && std::equal (notes.begin(), notes.end(), clip->notes.begin(), [] (const MidiNote& a, const MidiNote& b)
                           {
                               return a.pitch == b.pitch && juce::exactlyEqual (a.start, b.start) && juce::exactlyEqual (a.length, b.length)
                                   && juce::exactlyEqual (a.velocity, b.velocity) && a.muted == b.muted;
                           });
    clip->notes = std::move (notes);

    if (pitchBend != nullptr)
    {
        clip->pitchBend = *pitchBend;
        MidiClip::tidy (clip->pitchBend, MidiClip::pitchBendMin, MidiClip::pitchBendMax);
    }

    if (aftertouch != nullptr)
    {
        clip->aftertouch = *aftertouch;
        MidiClip::tidy (clip->aftertouch, 0, MidiClip::aftertouchMax);
    }

    if (seed != nullptr)
        clip->seed = *seed;

    // Notes changed under whatever is playing: release them rather than risk one hanging. A
    // change to the bend or aftertouch alone lets what's sounding go on.
    rebuildSequence (*track, windowChanged || ! sameNotes);
    contentChanged (*track);
    return true;
}

void AudioEngine::prepareOfflineFollowing (const std::vector<std::unique_ptr<MidiFx::Device>>& devices) const
{
    auto& stage = MidiFx::board();
    stage.clearOffline();

    for (const auto& device : devices)
    {
        const auto followed = device != nullptr ? device->followsTrack() : 0;
        const auto* track = followed > 0 ? findTrack (followed) : nullptr;

        if (track == nullptr)
            continue;

        // The notes its clips play (not a muted clip's or note's), each cut at its clip's end.
        std::vector<MidiFx::Board::OfflineNote> notes;

        for (const auto& clip : track->clips)
        {
            if (clip.isAudio() || clip.muted)
                continue;

            for (const auto& note : clip.notes)
            {
                if (note.muted || ! clip.shows (note))
                    continue;

                const auto start = clip.songPosition (note.start);
                notes.push_back ({ note.pitch, start, juce::jmin (start + note.length, clip.start + clip.length) });
            }
        }

        stage.setOffline (followed, std::move (notes));
    }
}

bool AudioEngine::setClipMuted (int trackId, int clipId, bool muted)
{
    auto* track = findTrack (trackId);
    auto* clip = track != nullptr ? findClip (*track, clipId) : nullptr;

    if (clip == nullptr || clip->muted == muted)
        return false;

    clip->muted = muted;
    rebuildSequence (*track, true);   // a note of it sounding ends now
    contentChanged (*track);
    return true;
}

bool AudioEngine::setClipAudio (int trackId, int clipId, double gainDb, double fadeIn, double fadeOut)
{
    auto* track = findTrack (trackId);
    auto* clip = track != nullptr ? findClip (*track, clipId) : nullptr;

    if (clip == nullptr || ! clip->isAudio() || ! std::isfinite (gainDb) || ! std::isfinite (fadeIn) || ! std::isfinite (fadeOut))
        return false;

    clip->audioGainDb = juce::jlimit (-60.0, 12.0, gainDb);
    fadeIn = juce::jlimit (0.0, clip->audioLength, fadeIn);
    fadeOut = juce::jlimit (0.0, clip->audioLength, fadeOut);

    // Together they fit the clip: when they'd overlap, both give way in proportion.
    if (fadeIn + fadeOut > clip->audioLength && fadeIn + fadeOut > 0.0)
    {
        const auto scale = clip->audioLength / (fadeIn + fadeOut);
        fadeIn *= scale;
        fadeOut *= scale;
    }

    clip->fadeIn = fadeIn;
    clip->fadeOut = fadeOut;
    rebuildSequence (*track, false);
    contentChanged (*track);
    return true;
}

int AudioEngine::importAudio (int trackId, const juce::File& file, double start)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->kind != Track::Kind::audio || ! std::isfinite (start))
        return 0;

    juce::String error;
    const auto data = media.get (file, getPlaybackRate(), error);

    if (data == nullptr || data->lengthSeconds <= 0.0)
        return 0;

    MidiClip clip;
    clip.start = juce::jmax (0.0, start);
    clip.audioFile = file.getFullPathName();
    clip.audioLength = data->lengthSeconds;
    return addClip (trackId, clip);
}

void AudioEngine::setCreatorPreview (CreatorPreview* preview)
{
    const juce::SpinLock::ScopedLockType lock (graphLock);
    creatorPreview = preview;
}

void AudioEngine::setSamplingTap (SamplingTap* tap, int trackId, bool renderOffline)
{
    Track* released = nullptr;
    Track* held = nullptr;
    const bool aside = tap != nullptr && renderOffline;

    // Rendered on the studio's thread (Turbo, Unleashed): the device steps aside first, as for a
    // mixdown, and plays silence until it's done. (Paired with the callback: one of the two
    // always sees the other's flag.)
    if (aside)
    {
        samplingRenders.store (true);

        while (deviceInCallback.load())
            juce::Thread::sleep (1);
    }

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        if (samplingTap != nullptr && (tap == nullptr || trackId != samplingTrackId))
            released = findTrack (samplingTrackId);

        if (tap != nullptr && (samplingTap == nullptr || trackId != samplingTrackId))
            held = findTrack (trackId);

        samplingTap = tap;
        samplingTrackId = tap != nullptr ? trackId : 0;
        samplingOffline = tap != nullptr && renderOffline;
        samplingOn.store (tap != nullptr);
    }

    // The device takes over again (the studio's thread has ended by now): what was played live
    // meanwhile went unheard, note-offs and pedals too, so no note held then hangs.
    if (! aside && samplingRenders.exchange (false))
        for (const auto& track : tracks)
            track->requestNotesOff();

    // A note left sounding goes: the studio's, on the track let go; one held on the keyboard, on
    // the track taken (it would never hear its note-off).
    if (released != nullptr)
        released->requestNotesOff();

    if (held != nullptr)
        held->requestNotesOff();
}

void AudioEngine::setSamplingRack (RackPlayer* rack)
{
    const juce::SpinLock::ScopedLockType lock (graphLock);
    samplingRack = rack;
}

void AudioEngine::renderRackBlock (const juce::MidiBuffer& notes, const juce::MidiBuffer& live, int numSamples,
                                   const clap_event_transport_t& position) noexcept
{
    numSamples = juce::jmin (numSamples, rackSound.getNumSamples());

    if (samplingRack == nullptr || numSamples <= 0 || rackSound.getNumChannels() < 2)
        return;

    rackSound.clear (0, numSamples);
    samplingRack->renderRack (notes, live, rackSound.getWritePointer (0), rackSound.getWritePointer (1), numSamples, position);
}

bool AudioEngine::renderSampling (int numSamples)
{
    // A mixdown has the song meanwhile: the studio waits for it to end.
    if (offline.load())
        return false;

    const juce::SpinLock::ScopedLockType lock (graphLock);

    // Nor while the device is stopped or starting again (its plugins and buffers changing).
    if (samplingTap == nullptr || ! samplingOffline || numSamples <= 0 || ! audioRunning.load())
        return false;

    auto* track = findTrack (samplingTrackId);
    const auto rack = samplingTrackId == rackTrackId;

    if (rack ? samplingRack == nullptr : track == nullptr)
        return false;

    numSamples = juce::jmin (numSamples, getBlockSize());

    // The device stands aside meanwhile (setSamplingTap): this thread plays the song's part of the
    // block, as the device would. The song stays where it is (stopped).
    MidiFx::board().beginBlock();
    transport.setPlaying (false);

    clap_event_transport_t songPosition {};
    transport.beginBlock (songPosition, numSamples);

    // At the BPM layer's tempo when there's one.
    clap_event_transport_t position {};
    position.header.size = sizeof (clap_event_transport_t);
    position.header.type = CLAP_EVENT_TRANSPORT;
    position.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
    position.tempo = samplingTap->tapTempo() > 0.0 ? samplingTap->tapTempo() : songPosition.tempo;
    position.tsig_num = 4;
    position.tsig_denom = 4;

    samplingNotes.clear();
    samplingTap->tapNotes (samplingNotes, numSamples);

    // The Sampling Studio's rack: its Output node's sound.
    if (rack)
    {
        numSamples = juce::jmin (numSamples, rackSound.getNumSamples());
        renderRackBlock (samplingNotes, noLive, numSamples, position);
        samplingTap->tapSound (rackSound.getReadPointer (0), rackSound.getReadPointer (1), numSamples);
    }
    else
    {
        track->render (samplingNotes, nullptr, 0, false, transport, position, numSamples, false, true);
        samplingTap->tapSound (track->getBlock (0), track->getBlock (1), juce::jmin (numSamples, track->getBlockSamples()));
    }

    transport.endBlock (nullptr, nullptr, numSamples);
    return true;
}

void AudioEngine::auditionNote (int trackId, int note, int velocity, bool on)
{
    auditionTrackId.store (trackId);
    auto message = on ? juce::MidiMessage::noteOn (1, juce::jlimit (0, 127, note), static_cast<juce::uint8> (juce::jlimit (1, 127, velocity)))
                      : juce::MidiMessage::noteOff (1, juce::jlimit (0, 127, note));
    message.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    auditionCollector.addMessageToQueue (message);
}

bool AudioEngine::previewLoop (const juce::File& file, juce::int64 loopStart, juce::int64 loopEnd, int crossfade)
{
    if (! previewAudio (file))
        return false;

    setPreviewLoop (loopStart, loopEnd, crossfade);
    return true;
}

void AudioEngine::setPreviewLoop (juce::int64 loopStart, juce::int64 loopEnd, int crossfade)
{
    const juce::SpinLock::ScopedLockType lock (graphLock);

    if (previewData == nullptr)
        return;

    previewRatio = previewData->fileRate > 0.0 ? previewData->playbackRate / previewData->fileRate : 1.0;
    previewLoopStart = static_cast<juce::int64> (std::llround (static_cast<double> (juce::jmax<juce::int64> (0, loopStart)) * previewRatio));
    previewLoopEnd = static_cast<juce::int64> (std::llround (static_cast<double> (juce::jmax<juce::int64> (0, loopEnd)) * previewRatio));
    previewCrossfade = juce::jmax (0, static_cast<int> (std::lround (crossfade * previewRatio)));
}

juce::int64 AudioEngine::getPreviewFilePosition() const noexcept
{
    return previewShown.load (std::memory_order_relaxed);
}

bool AudioEngine::previewAudio (const juce::File& file)
{
    ++previewRequest;   // one read on its own thread meanwhile doesn't start after this
    juce::String error;
    return startPreview (file, media.get (file, getPlaybackRate(), error));
}

void AudioEngine::previewAudioWhenRead (const juce::File& file, std::function<void (bool)> done)
{
    const auto request = ++previewRequest;

    media.load (file, getPlaybackRate(), [this, file, request, done = std::move (done)] (std::shared_ptr<const AudioData> data, const juce::String&)
    {
        // Another preview asked meanwhile, or the preview stopped: this one doesn't start.
        const auto playing = request == previewRequest && startPreview (file, std::move (data));

        if (done != nullptr)
            done (playing);
    });
}

bool AudioEngine::startPreview (const juce::File& file, std::shared_ptr<const AudioData> data)
{
    if (data == nullptr)
        return false;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        previewPlaying = data.get();
        previewPosition = 0;
        previewLoopStart = previewLoopEnd = 0;
        previewCrossfade = 0;
        previewLooped = false;
        previewRatio = data->fileRate > 0.0 ? data->playbackRate / data->fileRate : 1.0;
    }

    previewShown.store (0);
    previewData = std::move (data);   // the one it replaces is let go here, not on the audio thread
    previewPath = file.getFullPathName();
    previewFinished.store (false);
    return true;
}

void AudioEngine::stopPreview()
{
    ++previewRequest;   // a preview still being read doesn't start

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        previewPlaying = nullptr;
        previewLoopStart = previewLoopEnd = 0;
    }

    previewShown.store (-1);

    previewData.reset();
    previewPath.clear();
}

bool AudioEngine::moveClip (int trackId, int clipId, int toTrackId, double newStart)
{
    auto* from = findTrack (trackId);
    auto* to = findTrack (toTrackId);

    // Audio clips live on audio tracks and MIDI clips on instrument tracks.
    if (from == nullptr || to == nullptr || from->kind != to->kind || ! std::isfinite (newStart) || newStart < 0.0)
        return false;

    const auto found = std::find_if (from->clips.begin(), from->clips.end(), [clipId] (const MidiClip& clip) { return clip.id == clipId; });

    if (found == from->clips.end())
        return false;

    auto clip = std::move (*found);
    from->clips.erase (found);
    clip.start = newStart;
    fitAudioLength (clip);
    to->clips.push_back (std::move (clip));   // last in the list: drawn on top

    // A take only keeps recording into a clip that stays on its track.
    if (from != to && isRecordingInto (trackId, clipId))
        take.clipIds.erase (trackId);

    rebuildSequence (*from, true);
    contentChanged (*from);

    if (to != from)
    {
        rebuildSequence (*to, false);
        contentChanged (*to);
    }

    return true;
}

int AudioEngine::copyClip (int trackId, int clipId, int toTrackId, double newStart)
{
    auto* from = findTrack (trackId);
    auto* to = findTrack (toTrackId);
    auto* source = from != nullptr ? findClip (*from, clipId) : nullptr;

    if (source == nullptr || to == nullptr || from->kind != to->kind || ! std::isfinite (newStart) || newStart < 0.0)
        return 0;

    auto copy = *source;
    copy.id = nextClipId++;
    copy.start = newStart;
    fitAudioLength (copy);
    to->clips.push_back (std::move (copy));
    rebuildSequence (*to, false);
    contentChanged (*to);
    return to->clips.back().id;
}

bool AudioEngine::setClipRange (int trackId, int clipId, double newStart, double newLength)
{
    auto* track = findTrack (trackId);
    auto* clip = track != nullptr ? findClip (*track, clipId) : nullptr;

    if (clip == nullptr || ! std::isfinite (newStart) || ! std::isfinite (newLength)
        || newStart < 0.0 || newLength < minimumClipLength)
        return false;

    if (clip->isAudio())
    {
        // In seconds (through the tempo map), and within the file: a window can't show audio that isn't there.
        const auto& map = transport.getTempoMap();
        const auto fileLength = media.lengthOf (juce::File (clip->audioFile), getPlaybackRate());
        auto offset = clip->audioOffset + secondsBetween (clip->start, newStart);
        auto length = secondsBetween (newStart, newStart + newLength);

        if (offset < 0.0)
        {
            newStart = map.ppqAt (map.secondsAt (newStart) - offset);
            length += offset;
            offset = 0.0;
        }

        if (fileLength > 0.0)
            length = juce::jmin (length, fileLength - offset);

        if (length < secondsBetween (newStart, newStart + minimumClipLength))
            return false;

        clip->audioOffset = offset;
        clip->audioLength = length;

        // A clip following the song's tempo counts its window in the original's beats too.
        if (clip->followsTempo())
        {
            clip->offset = juce::jmax (0.0, clip->offset + newStart - clip->start);
            clip->length = juce::jmin (newLength, clip->sourceLength * clip->sourceTempo / 60.0 - clip->offset);
        }
        else
        {
            clip->length = audioLengthInPpq (newStart, length);
        }

        clip->start = newStart;
        clip->fadeIn = juce::jmin (clip->fadeIn, length);
        clip->fadeOut = juce::jmin (clip->fadeOut, length - clip->fadeIn);
        rebuildSequence (*track, true);
        contentChanged (*track);
        return true;
    }

    // Moving the left edge moves the window over the content: the notes stay put in the song.
    clip->offset += newStart - clip->start;
    clip->start = newStart;
    clip->length = newLength;

    rebuildSequence (*track, true);
    contentChanged (*track);
    return true;
}

int AudioEngine::splitClip (int trackId, int clipId, double at)
{
    auto* track = findTrack (trackId);
    auto* clip = track != nullptr ? findClip (*track, clipId) : nullptr;

    if (clip == nullptr || ! std::isfinite (at)
        || at < clip->start + minimumClipLength || at > clip->start + clip->length - minimumClipLength)
        return 0;

    if (clip->isAudio())
    {
        // Two windows on the same file, meeting where it was cut (what it was made from, both).
        const auto leftSeconds = secondsBetween (clip->start, at);

        if (leftSeconds <= 0.0 || leftSeconds >= clip->audioLength)
            return 0;

        MidiClip right = *clip;
        right.id = nextClipId++;
        right.start = at;
        right.audioOffset = clip->audioOffset + leftSeconds;
        right.audioLength = clip->audioLength - leftSeconds;
        right.length = audioLengthInPpq (at, right.audioLength);
        right.fadeIn = 0.0;
        right.fadeOut = juce::jmin (clip->fadeOut, right.audioLength);

        if (clip->followsTempo())
            right.offset = clip->offset + (at - clip->start);

        clip->audioLength = leftSeconds;
        clip->length = at - clip->start;
        clip->fadeIn = juce::jmin (clip->fadeIn, leftSeconds);
        clip->fadeOut = 0.0;

        track->clips.push_back (std::move (right));
        rebuildSequence (*track, true);
        contentChanged (*track);
        return track->clips.back().id;
    }

    const auto split = clip->contentPosition (at);

    MidiClip right;
    right.id = nextClipId++;
    right.start = at;
    right.length = clip->start + clip->length - at;
    right.offset = split;
    right.muted = clip->muted;
    right.envelopes = clip->envelopes;   // in content time: each part shows its own stretch of them

    // Notes from the split on go right; one that crosses it stays left and is cut there.
    auto& notes = clip->notes;
    const auto firstRight = std::stable_partition (notes.begin(), notes.end(), [split] (const MidiNote& note) { return note.start < split; });
    right.notes.assign (firstRight, notes.end());
    notes.erase (firstRight, notes.end());
    clip->length = at - clip->start;

    // The bend and aftertouch too; the right part opens with the values in effect at the split.
    const auto splitLine = [split] (std::vector<MidiControlPoint>& left, std::vector<MidiControlPoint>& rightLine)
    {
        const auto first = std::lower_bound (left.begin(), left.end(), split, [] (const MidiControlPoint& p, double t) { return p.time < t; });
        const auto carried = first != left.begin() ? std::prev (first)->value : 0;
        rightLine.assign (first, left.end());
        left.erase (first, left.end());

        if (carried != 0 && (rightLine.empty() || rightLine.front().time > split))
            rightLine.insert (rightLine.begin(), { split, carried });
    };

    splitLine (clip->pitchBend, right.pitchBend);
    splitLine (clip->aftertouch, right.aftertouch);

    track->clips.push_back (std::move (right));   // after this, clip may point at moved memory
    rebuildSequence (*track, true);
    contentChanged (*track);
    return track->clips.back().id;
}

MidiClip* AudioEngine::takeClip (Track& track, double from, double to)
{
    MidiClip* clip = nullptr;
    const auto known = take.clipIds.find (track.id);

    if (known != take.clipIds.end())
        for (auto& candidate : track.clips)
            if (candidate.id == known->second)
                clip = &candidate;

    if (clip == nullptr)
    {
        MidiClip fresh;
        fresh.id = nextClipId++;
        fresh.start = barStartAt (from);
        fresh.length = transport.getTempoMap().barAt (from).length();
        track.clips.push_back (fresh);
        clip = &track.clips.back();
        take.clipIds[track.id] = clip->id;
    }

    // Something before the clip (the loop went round to an earlier bar) moves its left
    // edge back; the window moves with it, so what's already in stays put.
    if (from < clip->start)
    {
        const auto newStart = barStartAt (from);
        const auto shift = clip->start - newStart;

        clip->offset -= shift;
        clip->start = newStart;
        clip->length += shift;
    }

    const auto clipEnd = barEndAt (to);

    if (clipEnd > clip->start + clip->length)
        clip->length = clipEnd - clip->start;

    return clip;
}

void AudioEngine::recordControl (juce::uint8 status, double ppq, int value)
{
    for (const auto trackId : take.trackIds)
    {
        auto* track = findTrack (trackId);

        if (track == nullptr)
            continue;

        auto* clip = takeClip (*track, ppq, ppq + Sequence::minimumNoteLength);
        auto& line = (status & 0xf0) == 0xe0 ? clip->pitchBend : clip->aftertouch;
        const MidiControlPoint point { clip->contentPosition (ppq), value };
        const auto at = std::upper_bound (line.begin(), line.end(), point.time,
                                          [] (double time, const MidiControlPoint& p) { return time < p.time; });

        // The wheel held still sends the same value again: once is enough (and rest needs none first).
        if ((at != line.begin() ? std::prev (at)->value : 0) == value)
            continue;

        line.insert (at, point);   // not played back during the take (rebuildSequence), so nothing to rebuild yet
    }
}

void AudioEngine::recordNote (int pitch, double start, double end, float velocity)
{
    for (const auto trackId : take.trackIds)
    {
        auto* track = findTrack (trackId);

        if (track == nullptr)
            continue;

        auto* clip = takeClip (*track, start, end);

        MidiNote note;
        note.pitch = pitch;
        note.start = clip->contentPosition (start);
        note.length = juce::jmax (end - start, Sequence::minimumNoteLength);
        note.velocity = velocity;

        const auto at = std::upper_bound (clip->notes.begin(), clip->notes.end(), note.start,
                                          [] (double time, const MidiNote& n) { return time < n.start; });
        clip->notes.insert (at, note);

        // Only notes were added, so nothing playing needs to stop.
        rebuildSequence (*track, false);
    }
}

void AudioEngine::finishTake()
{
    const auto state = transport.getState();

    // Notes still held when recording stopped end where it stopped.
    for (int pitch = 0; pitch < 128; ++pitch)
    {
        if (! take.held.test (static_cast<size_t> (pitch)))
            continue;

        auto end = take.lastPosition;
        const auto start = take.heldStart[static_cast<size_t> (pitch)];

        if (end <= start)
            end = state.loopEnabled && start < state.loopEnd ? state.loopEnd : start + Sequence::minimumNoteLength;

        recordNote (pitch, start, end, take.heldVelocity[static_cast<size_t> (pitch)]);
    }

    // Record Quantize: the take's notes onto the grid, in the same undo step as the take.
    if (recordQuantize.on)
        for (const auto& [trackId, clipId] : take.clipIds)
            if (auto* track = findTrack (trackId))
                if (auto* clip = findClip (*track, clipId))
                    quantizeTake (*clip);

    // A take that caught no notes (nor any bend or aftertouch away from rest) leaves nothing behind.
    const auto atRest = [] (const std::vector<MidiControlPoint>& line)
    {
        return std::all_of (line.begin(), line.end(), [] (const MidiControlPoint& point) { return point.value == 0; });
    };

    for (const auto& [trackId, clipId] : take.clipIds)
    {
        if (auto* track = findTrack (trackId))
        {
            auto& clips = track->clips;
            const auto found = std::find_if (clips.begin(), clips.end(), [clipId = clipId] (const MidiClip& clip) { return clip.id == clipId; });

            if (found != clips.end() && found->notes.empty() && atRest (found->pitchBend) && atRest (found->aftertouch))
                clips.erase (found);
        }
    }

    // Done: the take's bend and aftertouch play from now on.
    const auto trackIds = take.trackIds;
    take = {};

    for (const auto trackId : trackIds)
        if (auto* track = findTrack (trackId))
            rebuildSequence (*track, false);
}

double AudioEngine::quantized (const TempoMap& map, double ppq, const NoteQuantize& settings) noexcept
{
    return map.quantized (ppq, settings.grid, settings.swing, settings.strength);
}

void AudioEngine::quantizeTake (MidiClip& clip) const
{
    const auto& map = transport.getTempoMap();
    const auto end = clip.start + clip.length;
    bool longer = false;

    for (auto& note : clip.notes)
    {
        const auto start = clip.songPosition (note.start);
        const auto finish = start + note.length;
        const auto newStart = recordQuantize.starts ? juce::jmax (clip.start, quantized (map, start, recordQuantize)) : start;
        auto newFinish = recordQuantize.ends ? quantized (map, finish, recordQuantize) : newStart + note.length;

        // An end that lands on or before its start keeps at least a grid step (a little, at a
        // low strength).
        if (newFinish <= newStart + Sequence::minimumNoteLength)
            newFinish = newStart + juce::jmax (Sequence::minimumNoteLength, recordQuantize.ends ? recordQuantize.grid * recordQuantize.strength : note.length);

        note.start = clip.contentPosition (newStart);
        note.length = newFinish - newStart;
        longer = longer || newStart >= end - 1.0e-9;
    }

    std::stable_sort (clip.notes.begin(), clip.notes.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start; });

    // A note moved onto the bar line where the clip ends: the clip takes in that bar too.
    if (longer)
        clip.length = barEndAt (clip.start + clip.length + 1.0e-6) - clip.start;
}

bool AudioEngine::updateRecording()
{
    bool changed = false;
    const auto state = transport.getState();

    // A new sample rate: the audio in memory is loaded again at it.
    if (currentSampleRate > 0.0 && ! juce::approximatelyEqual (mediaRate, currentSampleRate))
    {
        mediaRate = currentSampleRate;
        media.clear();

        for (const auto& track : tracks)
        {
            if (track->kind == Track::Kind::audio)
                rebuildSequence (*track, false);

            // A frozen track's render too (gone from the disk: it plays itself again).
            if (track->isFrozen())
            {
                if (auto render = frozenPlaylist (track->frozenFile))
                {
                    {
                        const juce::SpinLock::ScopedLockType lock (graphLock);
                        std::swap (track->frozen, render);
                    }

                    render.reset();   // the old one, outside the lock
                }
                else
                {
                    thaw (*track, true);
                }
            }
        }
    }

    changed = followTempo() || changed;

    if (previewFinished.exchange (false))
        stopPreview();

    // Where the audio take's passes begin (kept only while one is being recorded).
    if (const auto ready = passQueue.getNumReady(); ready > 0)
    {
        const auto read = passQueue.read (ready);

        const auto keep = [this] (int first, int count)
        {
            for (int i = first; i < first + count; ++i)
                if (audioTakeActive)
                    takePasses.push_back (queuedPasses[static_cast<size_t> (i)]);
        };

        keep (read.startIndex1, read.blockSize1);
        keep (read.startIndex2, read.blockSize2);
    }

    // Recording stopped (or punched out, with something recorded): the audio take becomes clips.
    if (audioTakeActive && (! state.recording || (! state.capturing && ! takePasses.empty())))
    {
        finishAudioRecording();
        changed = true;
    }

    // Record still on, waiting to punch in again (Autopunch, the song located back): the writers
    // are ready before it does.
    if (state.recording && ! state.capturing && ! audioTakeActive)
        beginAudioRecording();

    // A take begins when recording starts (past the preroll and the count-in, inside Autopunch's
    // range): a clip appears on each armed track.
    if (state.capturing && ! take.active)
    {
        take = {};
        take.active = true;
        take.lastPosition = state.ppq;

        for (const auto& track : tracks)
        {
            if (! track->recordArm.load() || track->kind != Track::Kind::instrument)
                continue;   // an armed audio track records audio, not notes

            MidiClip clip;
            clip.id = nextClipId++;
            const auto bar = transport.getTempoMap().barAt (state.ppq);
            clip.start = bar.start;
            clip.length = bar.length();
            track->clips.push_back (clip);
            take.trackIds.push_back (track->id);
            take.clipIds[track->id] = clip.id;
            changed = true;
        }
    }

    // What the audio thread recorded since the last call.
    const auto ready = recordQueue.getNumReady();

    if (ready > 0)
    {
        const auto read = recordQueue.read (ready);

        const auto file = [this, &state, &changed] (int first, int count)
        {
            for (int i = first; i < first + count; ++i)
            {
                const auto& event = recordedEvents[static_cast<size_t> (i)];

                if (! take.active)
                    continue;

                if (event.status == punchOutMarker)
                {
                    take.lastPosition = event.ppq;
                    take.stopped = true;
                    continue;
                }

                // The pitch bend wheel and aftertouch go into the clip's lines (the page hears of
                // them a couple of times a second: a wheel sends many).
                if ((event.status & 0xf0) == 0xe0)
                {
                    recordControl (event.status, event.ppq, ((event.data2 & 0x7f) << 7 | (event.data1 & 0x7f)) - 8192);
                    take.controlsFiled = true;
                    continue;
                }

                if ((event.status & 0xf0) == 0xd0)
                {
                    recordControl (event.status, event.ppq, event.data1 & 0x7f);
                    take.controlsFiled = true;
                    continue;
                }

                const auto pitch = static_cast<size_t> (event.data1 & 0x7f);
                const bool on = (event.status & 0xf0) == 0x90 && event.data2 > 0;
                changed = true;

                if (take.held.test (pitch))
                {
                    // Released (or struck again): the held note is complete.
                    auto end = event.ppq;
                    const auto start = take.heldStart[pitch];

                    // Held across the loop going round: it ends at the loop's end.
                    if (end < start)
                        end = state.loopEnabled && start < state.loopEnd ? state.loopEnd : start + Sequence::minimumNoteLength;

                    recordNote (static_cast<int> (pitch), start, end, take.heldVelocity[pitch]);
                    take.held.reset (pitch);
                }

                if (on)
                {
                    take.held.set (pitch);
                    take.heldStart[pitch] = event.ppq;
                    take.heldVelocity[pitch] = static_cast<float> (event.data2) / 127.0f;
                }
            }
        };

        file (read.startIndex1, read.blockSize1);
        file (read.startIndex2, read.blockSize2);
    }

    if (take.controlsFiled && juce::Time::getMillisecondCounter() - take.controlsToldAt >= 500)
    {
        take.controlsFiled = false;
        take.controlsToldAt = juce::Time::getMillisecondCounter();
        changed = true;
    }

    // While recording, the clips grow with the playhead, a bar at a time.
    if (take.active && state.capturing && ! take.stopped)
    {
        take.lastPosition = state.ppq;

        for (const auto& [trackId, clipId] : take.clipIds)
        {
            if (auto* track = findTrack (trackId))
            {
                for (auto& clip : track->clips)
                {
                    if (clip.id != clipId || state.ppq <= clip.start)
                        continue;

                    const auto end = barEndAt (state.ppq);

                    if (end > clip.start + clip.length)
                    {
                        clip.length = end - clip.start;
                        changed = true;
                    }
                }
            }
        }
    }

    // The take ends when recording stops (or punches out).
    if (take.active && ! state.capturing)
    {
        finishTake();
        changed = true;
    }

    return changed;
}

//==============================================================================
void AudioEngine::setMainAutomation (std::vector<AutomationPoint> points)
{
    mainAutomation = Automation::tidy (std::move (points));
    auto fresh = mainAutomation.empty() ? nullptr : std::make_unique<const std::vector<AutomationPoint>> (mainAutomation);
    std::unique_ptr<const std::vector<AutomationPoint>> stale;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        stale = std::move (mainAutomationPlayback);
        mainAutomationPlayback = std::move (fresh);
        lastMainAutomationValue = std::numeric_limits<double>::quiet_NaN();   // set again from the next block
    }
}

void AudioEngine::setTempoMap (const TempoMap& map)
{
    transport.setTempoMap (map);
    followTempo();   // audio clips land where the new map puts them, before the next block
}

void AudioEngine::setMasterVolume (float db) noexcept
{
    masterVolumeDb.store (juce::jlimit (Track::silentDb, Track::maxDb, db));
}

//==============================================================================
bool AudioEngine::activatePlugin (Plugin& plugin)
{
    // Before any device has started, the plugin is activated when one does.
    return currentSampleRate <= 0.0 || plugin.activate (currentSampleRate, currentBlockSize);
}

EffectChain* AudioEngine::chainFor (int channel) noexcept
{
    if (channel == mainChannel)
        return &mainInserts;

    auto* track = findTrack (channel);
    return track != nullptr ? &track->inserts : nullptr;
}

//==============================================================================
// Sends and FX channels
std::vector<Track::Send> AudioEngine::getSends (int trackId) const
{
    auto* track = findTrack (trackId);
    return track != nullptr ? track->sends : std::vector<Track::Send>();
}

void AudioEngine::setSends (int trackId, std::vector<Track::Send> newSends)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return;

    if (track->kind == Track::Kind::fx)
        newSends.clear();   // FX channels don't send: no circles

    for (const auto& send : newSends)
        track->nextSendId = juce::jmax (track->nextSendId, send.id + 1);

    // A sidechain key that comes, goes or changes changes what its channel plays.
    std::vector<int> keyed;
    const auto sameSend = [] (const Track::Send& a, const Track::Send& b)
    {
        return a.id == b.id && a.sidechain == b.sidechain && a.target == b.target && a.slot == b.slot && a.on == b.on
            && juce::exactlyEqual (a.levelDb, b.levelDb);
    };

    for (const auto& send : newSends)
        if (send.sidechain && std::none_of (track->sends.begin(), track->sends.end(), [&] (const Track::Send& old) { return sameSend (old, send); }))
            keyed.push_back (send.target);

    for (const auto& old : track->sends)
        if (old.sidechain && std::none_of (newSends.begin(), newSends.end(), [&] (const Track::Send& send) { return sameSend (old, send); }))
            keyed.push_back (old.target);

    track->sends = newSends;
    auto forAudio = std::move (newSends);   // made here, so nothing allocates under the lock

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        // Each send's level ramps on from where it was, so a change never clicks.
        for (auto& send : forAudio)
            for (const auto& old : track->audioSends)
                if (old.id == send.id)
                {
                    send.lastGain = old.lastGain;

                    if (old.sidechain && send.sidechain && old.target == send.target)
                        send.keyDelay = old.keyDelay;   // (a count up: nothing allocates)
                }

        std::swap (track->audioSends, forAudio);
    }

    for (const auto target : keyed)
        contentChanged (target);
}   // the old list goes here, outside the lock

int AudioEngine::addSend (int trackId, Track::Send send)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->kind == Track::Kind::fx)
        return 0;

    if (send.sidechain)
    {
        // A plugin with a sidechain input, on another channel (a track keying itself goes in circles).
        auto* plugin = findPlugin (send.target, send.slot);

        if (plugin == nullptr || ! plugin->hasSidechain() || send.target == trackId)
            return 0;

        // A bus plays after the tracks: it keys only what plays after it (Main, an FX channel).
        if (track->kind == Track::Kind::bus && send.target != mainChannel)
            if (const auto* keyed = findTrack (send.target); keyed == nullptr || keyed->kind != Track::Kind::fx)
                return 0;
    }
    else
    {
        auto* fx = findTrack (send.target);

        if (fx == nullptr || fx->kind != Track::Kind::fx)
            return 0;
    }

    for (const auto& existing : track->sends)
        if (existing.sidechain == send.sidechain && existing.target == send.target && existing.slot == send.slot)
            return existing.id;   // one send per destination

    auto list = track->sends;
    send.id = track->nextSendId++;
    send.lastGain = 0.0f;
    list.push_back (send);
    setSends (trackId, std::move (list));
    return send.id;
}

void AudioEngine::setSendLevel (int trackId, int sendId, float levelDb)
{
    auto list = getSends (trackId);

    for (auto& send : list)
        if (send.id == sendId)
            send.levelDb = juce::jlimit (Track::silentDb, 10.0f, levelDb);

    setSends (trackId, std::move (list));
}

void AudioEngine::setSendOn (int trackId, int sendId, bool on)
{
    auto list = getSends (trackId);

    for (auto& send : list)
        if (send.id == sendId)
            send.on = on;

    setSends (trackId, std::move (list));
}

void AudioEngine::removeSend (int trackId, int sendId)
{
    auto list = getSends (trackId);
    list.erase (std::remove_if (list.begin(), list.end(), [sendId] (const Track::Send& send) { return send.id == sendId; }), list.end());
    setSends (trackId, std::move (list));
}

std::vector<AudioEngine::SidechainTarget> AudioEngine::getSidechainTargets()
{
    std::vector<SidechainTarget> found;

    const auto addChain = [&found] (EffectChain& chain, int channel, const juce::String& channelName)
    {
        for (int i = 0; i < chain.size(); ++i)
            if (auto* plugin = chain.at (i)->plugin.get(); plugin != nullptr && plugin->hasSidechain())
                found.push_back ({ channel, chain.at (i)->id, plugin->getDescriptor().name, channelName });
    };

    for (const auto& track : tracks)
    {
        if (auto* instrument = track->instrument.get(); instrument != nullptr && instrument->hasSidechain())
            found.push_back ({ track->id, instrumentSlot, instrument->getDescriptor().name, track->name });

        addChain (track->inserts, track->id, track->name);
    }

    addChain (mainInserts, mainChannel, "Main");
    return found;
}

// Audio thread, under the graph lock.
void AudioEngine::clearSidechains (int numSamples) noexcept
{
    const auto clearChain = [numSamples] (EffectChain& chain)
    {
        for (int i = 0; i < chain.size(); ++i)
            if (auto* plugin = chain.at (i)->plugin.get(); plugin != nullptr && plugin->hasSidechain())
                plugin->clearSidechain (numSamples);
    };

    for (const auto& track : tracks)
    {
        if (track->isMixChannel())
            track->busInput.clear (0, juce::jmin (numSamples, track->busInput.getNumSamples()));

        if (track->kind == Track::Kind::fx)
            track->trackSends.clear (0, juce::jmin (numSamples, track->trackSends.getNumSamples()));

        if (auto* instrument = track->instrument.get(); instrument != nullptr && instrument->hasSidechain())
            instrument->clearSidechain (numSamples);

        clearChain (track->inserts);
    }

    clearChain (mainInserts);
}

// Audio thread, under the graph lock: tracks keying another track's sidechain (or whose notes
// another track's MIDI FX follow, or that send another track MIDI) come first, then
// the buses (the tracks have played into them), FX channels last. False (natural order) only past
// the reserved size.
bool AudioEngine::orderTracks() noexcept
{
    const auto count = tracks.size();

    if (count > renderOrder.capacity() || count > placed.capacity())
        return false;

    renderOrder.clear();
    placed.resize (count);
    std::fill (placed.begin(), placed.end(), static_cast<char> (0));

    bool anySidechain = false;

    for (const auto& track : tracks)
    {
        anySidechain = anySidechain || ! track->midiSourceTracks.empty() || track->midiFx.followsAny();

        for (const auto& send : track->audioSends)
            anySidechain = anySidechain || (send.sidechain && send.on);
    }

    // Does track i wait for another track still to play (one sending to its plugins' sidechains)?
    const auto waits = [this, count] (size_t i)
    {
        for (size_t j = 0; j < count; ++j)
        {
            if (j == i || placed[j] != 0 || tracks[j]->isMixChannel())
                continue;

            // Track i takes MIDI from track j: j plays first.
            for (const auto sourceId : tracks[i]->midiSourceTracks)
                if (sourceId == tracks[j]->id)
                    return true;

            // A MIDI FX of track i follows track j's notes (Harmonize): j plays first.
            if (tracks[i]->midiFx.follows (tracks[j]->id))
                return true;

            for (const auto& send : tracks[j]->audioSends)
                if (send.sidechain && send.on && send.target == tracks[i]->id)
                    return true;
        }

        return false;
    };

    size_t remaining = 0;

    for (const auto& track : tracks)
        if (! track->isMixChannel())
            ++remaining;

    while (remaining > 0)
    {
        bool progress = false;

        for (size_t i = 0; i < count; ++i)
        {
            if (placed[i] != 0 || tracks[i]->isMixChannel() || (anySidechain && waits (i)))
                continue;

            renderOrder.push_back (tracks[i].get());
            placed[i] = 1;
            --remaining;
            progress = true;
        }

        // Sidechains going round in a circle: the first one waiting plays now.
        if (! progress)
        {
            for (size_t i = 0; i < count; ++i)
            {
                if (placed[i] == 0 && ! tracks[i]->isMixChannel())
                {
                    renderOrder.push_back (tracks[i].get());
                    placed[i] = 1;
                    --remaining;
                    break;
                }
            }
        }
    }

    for (const auto kind : { Track::Kind::bus, Track::Kind::fx })
        for (const auto& track : tracks)
            if (track->kind == kind)
                renderOrder.push_back (track.get());

    return true;
}

// Audio thread, under the graph lock: a track's sends, after it played and mixed.
void AudioEngine::sendFrom (Track& track, int numSamples, bool settle) noexcept
{
    if (track.kind == Track::Kind::fx)
        return;

    for (auto& send : track.audioSends)
    {
        const auto gain = send.on && send.levelDb > Track::silentDb ? juce::Decibels::decibelsToGain (send.levelDb) : 0.0f;
        float* left = nullptr;

        if (settle)
            send.lastGain = gain;
        float* right = nullptr;

        bool beforeCompensation = false;
        int keyWait = 0;

        if (send.sidechain)
        {
            if (auto* plugin = findPlugin (send.target, send.slot);
                plugin != nullptr && plugin->hasSidechain() && plugin->getSidechainCapacity() >= numSamples)
            {
                left = plugin->getSidechainChannel (0);
                right = plugin->getSidechainChannel (1);

                // Keying a track's plugin: that track's sound is before its delay, so the key is too.
                // Keying Main's or an FX channel's: as late as the sound that plugin hears.
                const auto* keyed = send.target == mainChannel ? nullptr : findTrack (send.target);
                beforeCompensation = keyed != nullptr && ! keyed->isMixChannel();

                if (send.target == mainChannel)
                    keyWait = ! track.isMixChannel() ? tracksWaitBlock : busesWaitBlock;
                else if (keyed != nullptr && keyed->kind == Track::Kind::fx && ! track.isMixChannel())
                    keyWait = sendsWaitBlock;
            }
        }
        else if (auto* fx = findTrack (send.target); fx != nullptr && fx->kind == Track::Kind::fx && fx->busInput.getNumSamples() >= numSamples)
        {
            // A track's send waits for the buses' sends (plugin delay compensation); a bus's goes in.
            const bool waits = sendsWaitBlock > 0 && ! track.isMixChannel() && fx->sendsDelay != nullptr
                                 && fx->trackSends.getNumSamples() >= numSamples;
            auto& into = waits ? fx->trackSends : fx->busInput;
            left = into.getWritePointer (0);
            right = into.getWritePointer (1);
        }

        if (left != nullptr && right != nullptr && keyWait > 0 && send.keyDelay != nullptr && keyScratch.getNumSamples() >= numSamples)
        {
            auto* keyLeft = keyScratch.getWritePointer (0);
            auto* keyRight = keyScratch.getWritePointer (1);
            keyScratch.clear (0, numSamples);
            track.sendInto (keyLeft, keyRight, numSamples, gain, send.lastGain);
            send.keyDelay->process (keyLeft, keyRight, numSamples, keyWait, blockCount);
            juce::FloatVectorOperations::add (left, keyLeft, numSamples);
            juce::FloatVectorOperations::add (right, keyRight, numSamples);
        }
        else if (left != nullptr && right != nullptr)
            track.sendInto (left, right, numSamples, gain, send.lastGain, beforeCompensation);
        else
            send.lastGain = 0.0f;   // nowhere to go (its destination was removed): it waits, silent
    }
}

Plugin* AudioEngine::findPlugin (int channel, int insertId) noexcept
{
    if (insertId == instrumentSlot)
    {
        auto* track = channel == mainChannel ? nullptr : findTrack (channel);
        return track != nullptr ? track->instrument.get() : nullptr;
    }

    auto* chain = chainFor (channel);
    auto* insert = chain != nullptr ? chain->find (insertId) : nullptr;
    return insert != nullptr ? insert->plugin.get() : nullptr;
}

juce::Result AudioEngine::loadPlugin (int channel, const juce::File& file, const juce::String& pluginId,
                                      int position, int replaceInsertId, int& insertIdUsed, Displaced* displaced,
                                      int wantedInsertId)
{
    insertIdUsed = instrumentSlot;
    auto* track = channel == mainChannel ? nullptr : findTrack (channel);

    if (channel != mainChannel && track == nullptr)
        return juce::Result::fail ("That track no longer exists.");

    juce::String error;
    auto newPlugin = Plugin::load (file, pluginId, error);

    if (newPlugin == nullptr)
        return juce::Result::fail (error);

    const auto pluginName = newPlugin->getDescriptor().name;
    const bool isEffect = newPlugin->getRole() == Plugin::Role::effect;

    if (! isEffect && track == nullptr)
        return juce::Result::fail (pluginName + " is an instrument. The Main channel takes effects only.");

    auto* chain = isEffect ? chainFor (channel) : nullptr;
    const bool replacing = chain != nullptr && replaceInsertId != instrumentSlot && chain->find (replaceInsertId) != nullptr;

    if (chain != nullptr && ! replacing && chain->isFull())
        return juce::Result::fail ("This channel already has " + juce::String (EffectChain::maxInserts) + " inserts.");

    if (! activatePlugin (*newPlugin))
        return juce::Result::fail (pluginName + " couldn't start at "
                                   + juce::String (currentSampleRate / 1000.0, 1) + " kHz.");

    if (chain == nullptr)
    {
        std::unique_ptr<Plugin> oldPlugin;

        {
            const juce::SpinLock::ScopedLockType lock (graphLock);
            oldPlugin = std::move (track->instrument);
            track->instrument = std::move (newPlugin);
        }

        track->instrumentOff.store (false);   // a new instrument comes in on

        track->requestNotesOff();

        // Like Studio One: a track takes its instrument's name until you name it yourself.
        if (track->nameIsAutomatic && pluginName.isNotEmpty())
            track->name = pluginName;

        if (displaced != nullptr)
        {
            hideScreen (oldPlugin.get());
            displaced->instrument = std::move (oldPlugin);
        }

        hideScreen (oldPlugin.get());
        oldPlugin.reset();
        contentChanged (*track);
        return juce::Result::ok();
    }

    // The id asked for when no insert in the song has it (Main's or any track's); new ids stay above it.
    const auto taken = [this] (int id)
    {
        if (mainInserts.find (id) != nullptr)
            return true;

        return std::any_of (tracks.begin(), tracks.end(), [id] (const auto& each)
                            { return each->inserts.find (id) != nullptr || each->findClipEffect (id) != nullptr; });
    };

    const auto newId = wantedInsertId > 0 && ! taken (wantedInsertId) ? wantedInsertId : nextInsertId;
    nextInsertId = juce::jmax (nextInsertId, newId + 1);

    auto insert = std::make_unique<EffectChain::Insert> (newId, std::move (newPlugin));
    insert->mix = 0.0f;                    // it fades in over its first block, so adding never clicks
    insertIdUsed = insert->id;

    std::unique_ptr<EffectChain::Insert> oldInsert;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        if (replacing)
            oldInsert = chain->replace (replaceInsertId, std::move (insert));
        else
            chain->add (std::move (insert), position);
    }

    if (displaced != nullptr && oldInsert != nullptr)
    {
        displaced->insertPosition = chain->indexOf (insertIdUsed);
        hideScreen (oldInsert->plugin.get());
        displaced->insert = std::move (oldInsert);
    }

    if (oldInsert != nullptr)
        hideScreen (oldInsert->plugin.get());

    oldInsert.reset();
    contentChanged (channel);
    return juce::Result::ok();
}

void AudioEngine::clearInstrument (int trackId)
{
    auto removed = swapInstrument (trackId, nullptr);
    removed.reset();   // destroyed here, outside the lock
}

std::unique_ptr<Plugin> AudioEngine::swapInstrument (int trackId, std::unique_ptr<Plugin> instrument)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return instrument;   // nowhere to put it: handed straight back

    // It may have been kept while the device changed.
    if (instrument != nullptr)
        activatePlugin (*instrument);

    std::unique_ptr<Plugin> old;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        old = std::move (track->instrument);
        track->instrument = std::move (instrument);
    }

    track->requestNotesOff();
    hideScreen (old.get());
    contentChanged (*track);
    return old;
}

std::unique_ptr<EffectChain::Insert> AudioEngine::detachInsert (int channel, int insertId, int& position)
{
    auto* chain = chainFor (channel);
    position = chain != nullptr ? chain->indexOf (insertId) : -1;

    if (position < 0)
        return nullptr;

    std::unique_ptr<EffectChain::Insert> removed;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        removed = chain->remove (insertId);
    }

    if (removed != nullptr)
    {
        hideScreen (removed->plugin.get());
        contentChanged (channel);
    }

    return removed;
}

bool AudioEngine::attachInsert (int channel, std::unique_ptr<EffectChain::Insert> insert, int position)
{
    auto* chain = chainFor (channel);

    if (chain == nullptr || insert == nullptr || chain->isFull())
        return false;

    if (insert->plugin != nullptr)
        activatePlugin (*insert->plugin);

    insert->mix = 0.0f;   // fades in over its first block, as a new insert does
    bool added = false;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        added = chain->add (std::move (insert), position);
    }

    if (added)
        contentChanged (channel);

    return added;
}

int AudioEngine::getInsertPosition (int channel, int insertId) noexcept
{
    auto* chain = chainFor (channel);
    return chain != nullptr ? chain->indexOf (insertId) : -1;
}

void AudioEngine::removeInsert (int channel, int insertId)
{
    auto* chain = chainFor (channel);

    if (chain == nullptr)
        return;

    std::unique_ptr<EffectChain::Insert> removed;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        removed = chain->remove (insertId);
    }

    if (removed != nullptr)
        contentChanged (channel);

    // Destroyed here, outside the lock.
    removed.reset();
}

void AudioEngine::moveInsert (int channel, int insertId, int position)
{
    if (auto* chain = chainFor (channel))
    {
        {
            const juce::SpinLock::ScopedLockType lock (graphLock);
            chain->move (insertId, position);
        }

        contentChanged (channel);
    }
}

void AudioEngine::setInsertBypass (int channel, int insertId, bool bypassed)
{
    if (auto* chain = chainFor (channel))
    {
        if (auto* insert = chain->find (insertId); insert != nullptr && insert->bypassed.exchange (bypassed) != bypassed)
            contentChanged (channel);
    }
}

//==============================================================================
// MIDI FX
MidiFxChain::Slot* AudioEngine::findMidiFx (int trackId, int fxId) const noexcept
{
    auto* track = findTrack (trackId);
    return track != nullptr ? track->midiFx.find (fxId) : nullptr;
}

bool AudioEngine::midiFxIdInUse (int fxId) const noexcept
{
    for (const auto& track : tracks)
        if (track->midiFx.find (fxId) != nullptr)
            return true;

    return false;
}

int AudioEngine::addMidiFx (int trackId, const juce::String& type, int position, const juce::var& settings)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->kind != Track::Kind::instrument || track->midiFx.isFull())
        return 0;

    auto device = MidiFx::create (type, settings);

    if (device == nullptr)
        return 0;

    const auto wanted = static_cast<int> (settings.getProperty ("id", 0));
    const auto id = wanted > 0 && ! midiFxIdInUse (wanted) ? wanted : nextMidiFxId;
    nextMidiFxId = juce::jmax (nextMidiFxId, id + 1);

    auto slot = std::make_unique<MidiFxChain::Slot> (id, std::move (device));
    slot->on.store (static_cast<bool> (settings.getProperty ("on", true)));
    slot->wasOn = slot->on.load();

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        if (! track->midiFx.add (std::move (slot), position))
            return 0;
    }

    contentChanged (*track);
    return id;
}

bool AudioEngine::removeMidiFx (int trackId, int fxId)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return false;

    std::unique_ptr<MidiFxChain::Slot> removed;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        removed = track->midiFx.remove (fxId);
    }

    if (removed != nullptr)
        contentChanged (*track);

    return removed != nullptr;   // destroyed here, outside the lock
}

bool AudioEngine::moveMidiFx (int trackId, int fxId, int position)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->midiFx.find (fxId) == nullptr)
        return false;

    bool moved = false;

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        moved = track->midiFx.move (fxId, position);
    }

    if (moved)
        contentChanged (*track);

    return moved;
}

bool AudioEngine::setMidiFxOn (int trackId, int fxId, bool on)
{
    auto* slot = findMidiFx (trackId, fxId);

    if (slot == nullptr)
        return false;

    if (slot->on.exchange (on) != on)
        contentChanged (trackId);

    return true;
}

float AudioEngine::setMidiFxParam (int trackId, int fxId, int index, float value)
{
    auto* slot = findMidiFx (trackId, fxId);

    if (slot == nullptr)
        return 0.0f;

    const auto before = slot->device->get (index);
    const auto used = slot->device->set (index, value);

    if (! juce::approximatelyEqual (before, used))
        contentChanged (trackId);

    return used;
}

juce::var AudioEngine::getMidiFx (int trackId) const
{
    juce::Array<juce::var> list;
    const auto* track = findTrack (trackId);

    if (track == nullptr)
        return list;

    for (int i = 0; i < track->midiFx.size(); ++i)
    {
        const auto* slot = track->midiFx.at (i);
        const auto& device = *slot->device;

        juce::DynamicObject::Ptr params (new juce::DynamicObject());

        for (size_t p = 0; p < device.spec.params.size(); ++p)
            params->setProperty (device.spec.params[p].id, static_cast<double> (device.get (static_cast<int> (p))));

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", slot->id);
        item->setProperty ("type", device.spec.id);
        item->setProperty ("name", device.spec.name);
        item->setProperty ("on", slot->on.load());
        item->setProperty ("params", juce::var (params.get()));

        if (const auto* magenta = dynamic_cast<const MidiFx::MagentaDevice*> (&device))
        {
            item->setProperty ("tool", magenta->tool);
            item->setProperty ("toolName", magenta->toolName);
            item->setProperty ("generator", magenta->generator.load());

            if (magenta->toolName.isNotEmpty())
                item->setProperty ("name", magenta->toolName);
        }

        // A device made elsewhere (a Sonify device) says what it is: its build, its maps.
        device.describeInto (*item);

        list.add (juce::var (item.get()));
    }

    return list;
}

void AudioEngine::setMidiFx (int trackId, const juce::var& devices)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->kind != Track::Kind::instrument)
        return;

    // Devices still on the track stay as they are (their settings, and what they're playing:
    // undo takes back adding, removing or moving one, not what was set since); the rest are
    // made from their descriptions, here, before the lock.
    std::vector<int> order;
    std::vector<std::unique_ptr<MidiFxChain::Slot>> fresh, gone;
    fresh.reserve (MidiFxChain::maxDevices);
    gone.reserve (MidiFxChain::maxDevices * 2);

    if (const auto* list = devices.getArray())
    {
        for (const auto& item : *list)
        {
            if (static_cast<int> (order.size()) >= MidiFxChain::maxDevices)
                break;

            const auto wanted = static_cast<int> (item.getProperty ("id", 0));

            if (wanted > 0 && track->midiFx.find (wanted) != nullptr)
            {
                if (std::find (order.begin(), order.end(), wanted) == order.end())
                    order.push_back (wanted);

                continue;
            }

            auto device = MidiFx::create (item.getProperty ("type", {}).toString(), item);

            if (device == nullptr)
                continue;

            const auto taken = [&] (int id)
            {
                return midiFxIdInUse (id) || std::any_of (fresh.begin(), fresh.end(), [id] (const auto& slot) { return slot->id == id; });
            };

            const auto id = wanted > 0 && ! taken (wanted) ? wanted : nextMidiFxId;
            nextMidiFxId = juce::jmax (nextMidiFxId, id + 1);

            auto slot = std::make_unique<MidiFxChain::Slot> (id, std::move (device));
            slot->on.store (static_cast<bool> (item.getProperty ("on", true)));
            slot->wasOn = slot->on.load();
            fresh.push_back (std::move (slot));
            order.push_back (id);
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        track->midiFx.rebuild (order, fresh, gone);
    }

    // What went is destroyed here, outside the lock.
    gone.clear();
    fresh.clear();
    contentChanged (*track);
}

int AudioEngine::replaceMidiFx (const std::function<std::unique_ptr<MidiFx::Device> (const MidiFx::Device&)>& remake)
{
    int replaced = 0;
    std::vector<std::unique_ptr<MidiFx::Device>> gone;

    // Room for every device first, so nothing allocates under the lock.
    size_t devices = 0;

    for (const auto& track : tracks)
        devices += static_cast<size_t> (track->midiFx.size());

    gone.reserve (devices);

    for (const auto& track : tracks)
    {
        bool changed = false;

        for (int i = 0; i < track->midiFx.size(); ++i)
        {
            auto* slot = track->midiFx.at (i);
            auto fresh = remake (*slot->device);

            if (fresh == nullptr)
                continue;

            const juce::SpinLock::ScopedLockType lock (graphLock);
            gone.push_back (track->midiFx.replace (slot->id, std::move (fresh)));
            ++replaced;
            changed = true;
        }

        if (changed)
            contentChanged (*track);
    }

    gone.clear();   // destroyed here, outside the lock
    return replaced;
}

void AudioEngine::takeMagentaRequests (std::vector<MagentaRequest>& into)
{
    for (const auto& track : tracks)
    {
        for (int i = 0; i < track->midiFx.size(); ++i)
        {
            const auto* slot = track->midiFx.at (i);
            auto* magenta = dynamic_cast<MidiFx::MagentaDevice*> (slot->device.get());

            if (magenta == nullptr)
                continue;

            MagentaRequest request;

            while (magenta->takeRequest (request.window))
            {
                request.trackId = track->id;
                request.fxId = slot->id;
                request.tool = magenta->tool;
                request.mode = magenta->getInt (MidiFx::MagentaDevice::mode);
                request.temperature = magenta->get (MidiFx::MagentaDevice::temperature);
                request.inKey = magenta->get (MidiFx::MagentaDevice::inKey) > 0.5f;
                into.push_back (request);
            }
        }
    }
}

bool AudioEngine::answerMagenta (int trackId, int fxId, MidiFx::MagentaDevice::Window answer)
{
    auto* slot = findMidiFx (trackId, fxId);
    auto* magenta = slot != nullptr ? dynamic_cast<MidiFx::MagentaDevice*> (slot->device.get()) : nullptr;

    if (magenta == nullptr)
        return false;

    // In the order they start, as the device plays them.
    answer.count = juce::jlimit (0, MidiFx::MagentaDevice::maxWindowNotes, answer.count);
    std::sort (answer.notes.begin(), answer.notes.begin() + answer.count,
               [] (const auto& a, const auto& b) { return a.start < b.start; });
    return magenta->giveAnswer (answer);
}

Plugin* AudioEngine::getPlugin (int channel, int insertId) noexcept
{
    if (auto* plugin = findPlugin (channel, insertId))
        return plugin;

    if (insertId == instrumentSlot)
        return nullptr;

    // A clip's own effect.
    if (channel != mainChannel)
        if (auto* track = findTrack (channel))
            if (auto* effect = track->findClipEffect (insertId))
                return effect->plugin.get();

    // Inside a container on the channel (nodes have slot ids of their own).
    Plugin* found = nullptr;
    const auto look = [&] (Plugin* plugin)
    {
        if (auto* container = dynamic_cast<ContainerPlugin*> (plugin); container != nullptr && found == nullptr)
            found = container->findPlugin (insertId);
    };

    if (channel != mainChannel)
        if (auto* track = findTrack (channel))
            look (track->instrument.get());

    if (auto* chain = chainFor (channel))
        for (int i = 0; i < chain->size() && found == nullptr; ++i)
            look (chain->at (i)->plugin.get());

    return found;
}

bool AudioEngine::locatePlugin (const Plugin& plugin, int& channel, int& slot) const
{
    const auto inside = [&plugin, &slot] (const Plugin* holder)
    {
        if (auto* container = dynamic_cast<const ContainerPlugin*> (holder))
            if (auto* node = container->findNode (plugin))
            {
                slot = node->slotId;
                return true;
            }

        return false;
    };

    const auto inChain = [&] (const EffectChain& chain, int id)
    {
        for (int i = 0; i < chain.size(); ++i)
        {
            const auto* insert = chain.at (i);

            if (insert->plugin.get() == &plugin)
            {
                channel = id;
                slot = insert->id;
                return true;
            }

            if (inside (insert->plugin.get()))
            {
                channel = id;
                return true;
            }
        }

        return false;
    };

    for (const auto& track : tracks)
    {
        if (track->instrument.get() == &plugin)
        {
            channel = track->id;
            slot = instrumentSlot;
            return true;
        }

        if (inside (track->instrument.get()))
        {
            channel = track->id;
            return true;
        }

        if (inChain (track->inserts, track->id))
            return true;

        for (const auto& effect : track->clipEffects)
        {
            if (effect != nullptr && effect->plugin.get() == &plugin)
            {
                channel = track->id;
                slot = effect->slotId;
                return true;
            }
        }
    }

    return inChain (mainInserts, mainChannel);
}

int AudioEngine::getPluginOn (const Plugin& plugin) const
{
    // In a rack (on any channel, however deep): its node's mute.
    const auto inRack = [&plugin] (const Plugin* holder)
    {
        if (auto* container = dynamic_cast<const ContainerPlugin*> (holder))
            return container->getPluginMuted (plugin);

        return -1;
    };

    const auto inChain = [&] (const EffectChain& chain)
    {
        for (int i = 0; i < chain.size(); ++i)
        {
            const auto* insert = chain.at (i);

            if (insert->plugin.get() == &plugin)
                return insert->bypassed.load() ? 0 : 1;

            if (const auto muted = inRack (insert->plugin.get()); muted >= 0)
                return muted > 0 ? 0 : 1;
        }

        return -1;
    };

    for (const auto& track : tracks)
    {
        if (track->instrument.get() == &plugin)
            return track->instrumentOff.load() ? 0 : 1;

        if (const auto muted = inRack (track->instrument.get()); muted >= 0)
            return muted > 0 ? 0 : 1;

        if (const auto on = inChain (track->inserts); on >= 0)
            return on;
    }

    return inChain (mainInserts);
}

bool AudioEngine::setPluginOn (const Plugin& plugin, bool on)
{
    const auto inRack = [&plugin, on] (Plugin* holder)
    {
        if (auto* container = dynamic_cast<ContainerPlugin*> (holder))
            return container->setPluginMuted (plugin, ! on);

        return false;
    };

    const auto inChain = [&] (EffectChain& chain, int channel)
    {
        for (int i = 0; i < chain.size(); ++i)
        {
            auto* insert = chain.at (i);

            if (insert->plugin.get() == &plugin)
            {
                setInsertBypass (channel, insert->id, ! on);
                return true;
            }

            if (inRack (insert->plugin.get()))
            {
                contentChanged (channel);
                return true;
            }
        }

        return false;
    };

    for (const auto& track : tracks)
    {
        if (track->instrument.get() == &plugin)
        {
            if (track->instrumentOff.exchange (! on) != ! on)
                contentChanged (*track);

            return true;
        }

        if (inRack (track->instrument.get()))
        {
            contentChanged (*track);
            return true;
        }

        if (inChain (track->inserts, track->id))
            return true;
    }

    return inChain (mainInserts, mainChannel);
}

double AudioEngine::setPluginParameter (int channel, int insertId, int index, double value)
{
    auto* plugin = getPlugin (channel, insertId);

    if (plugin == nullptr)
        return 0.0;

    const auto before = plugin->getParameterValue (index);
    const auto used = plugin->queueParameter (index, value);

    // A frozen track's plugin turned: it plays itself again, so it's heard.
    if (! juce::approximatelyEqual (before, used))
        contentChanged (channel);

    // With audio running the change reaches the plugin in the next block; otherwise hand it over now.
    if (! audioRunning.load())
    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        plugin->flushQueuedParameters();
    }

    return used;
}

juce::String AudioEngine::getPluginParameterText (int channel, int insertId, int index)
{
    auto* plugin = getPlugin (channel, insertId);
    return plugin != nullptr ? plugin->getParameterText (index, plugin->getParameterValue (index)) : juce::String();
}

void AudioEngine::rescanWithoutWaiting (Plugin& plugin)
{
    // The plugin is read with no lock held (it may be playing); only the swap of its lists waits
    // for the device, and the old lists are let go of after it.
    if (plugin.readParameters())
    {
        std::shared_ptr<void> old;

        {
            const juce::SpinLock::ScopedLockType lock (graphLock);
            old = plugin.takeReadParameters();
        }

        return;
    }

    const juce::SpinLock::ScopedLockType lock (graphLock);
    plugin.rescanParameters();
}

AudioEngine::PluginChange AudioEngine::updatePlugins (juce::String& message)
{
    struct Failure
    {
        int channel, insertId;
        juce::String reason;
    };

    auto change = PluginChange::none;
    std::vector<Failure> failures;

    const auto look = [&] (Plugin& plugin, int channel, int insertId)
    {
        const auto requests = plugin.takeRequests();
        const auto name = plugin.getDescriptor().name;
        juce::String bridgeError;

        // A crash or timeout inside the WebAssembly code leaves the plugin unusable.
        if (requests.processError && plugin.getError (bridgeError))
        {
            failures.push_back ({ channel, insertId, name + " stopped: " + bridgeError });
            return;
        }

        if (audioRunning.load() && ! plugin.isActive())
        {
            failures.push_back ({ channel, insertId, name + " couldn't start at "
                                                     + juce::String (currentSampleRate / 1000.0, 1) + " kHz." });
            return;
        }

        if (requests.callback)
            plugin.runOnMainThread();

        if (requests.rescan || requests.restart)
        {
            bool restarted = false;

            // A restart: the plugin is taken out of the graph (a quick swap under the lock), started
            // again outside it, so the device never waits for that, then put back where it was. A
            // failed restart is caught on the next pass, as an inactive plugin.
            if (requests.restart && plugin.isActive())
            {
                auto* track = insertId == instrumentSlot ? findTrack (channel) : nullptr;
                auto* chain = insertId == instrumentSlot ? nullptr : chainFor (channel);
                const auto position = chain != nullptr ? chain->indexOf (insertId) : -1;
                std::unique_ptr<Plugin> heldInstrument;
                std::unique_ptr<EffectChain::Insert> heldInsert;

                {
                    const juce::SpinLock::ScopedLockType lock (graphLock);

                    if (track != nullptr && track->instrument.get() == &plugin)
                    {
                        heldInstrument = std::move (track->instrument);
                        track->instrumentAway = true;   // silent meanwhile, not the test synth
                    }
                    else if (position >= 0)
                    {
                        heldInsert = chain->remove (insertId);
                    }
                }

                if (heldInstrument != nullptr || heldInsert != nullptr)
                {
                    plugin.deactivate();
                    plugin.activate (currentSampleRate, currentBlockSize);

                    if (requests.rescan)
                        plugin.rescanParameters();

                    if (heldInsert != nullptr)
                        heldInsert->mix = 0.0f;   // it fades in again over its first block

                    const juce::SpinLock::ScopedLockType lock (graphLock);

                    if (heldInstrument != nullptr)
                    {
                        track->instrument = std::move (heldInstrument);
                        track->instrumentAway = false;
                    }
                    else
                    {
                        chain->add (std::move (heldInsert), position);
                    }

                    restarted = true;
                }
            }

            if (requests.rescan && ! restarted)
                rescanWithoutWaiting (plugin);

            if (change == PluginChange::none)
                change = PluginChange::changed;
        }

        if (requests.flush && ! audioRunning.load())
        {
            const juce::SpinLock::ScopedLockType lock (graphLock);
            plugin.flushQueuedParameters();
        }
    };

    for (const auto& track : tracks)
    {
        if (track->instrument != nullptr)
            look (*track->instrument, track->id, instrumentSlot);

        for (int i = 0; i < track->inserts.size(); ++i)
            if (auto* insert = track->inserts.at (i); insert->plugin != nullptr)
                look (*insert->plugin, track->id, insert->id);
    }

    for (int i = 0; i < mainInserts.size(); ++i)
        if (auto* insert = mainInserts.at (i); insert->plugin != nullptr)
            look (*insert->plugin, mainChannel, insert->id);

    // Clips' own effects: one that fails plays its clip as it is, and says why on the clip.
    updateClipEffects (message, change);

    // Removed after the walk, so nothing moves under it.
    for (const auto& failure : failures)
    {
        message = failure.reason;

        if (failure.insertId == instrumentSlot)
            clearInstrument (failure.channel);
        else
            removeInsert (failure.channel, failure.insertId);

        change = PluginChange::failed;
    }

    // The plugins' latencies as they are now: everything lined up again (plugin delay compensation).
    updateCompensation();
    return change;
}

void AudioEngine::makeDelay (std::unique_ptr<LatencyDelay>& delay)
{
    if (delay != nullptr)
        return;

    auto made = std::make_unique<LatencyDelay>();
    made->prepare();
    const juce::SpinLock::ScopedLockType lock (graphLock);
    delay = std::move (made);
}

void AudioEngine::updateCompensation()
{
    // A mixdown renders with the latencies it started with.
    if (mixdown != nullptr && mixdown->isThreadRunning())
        return;

    // The Analog Board mixing: the tracks' inserts, the FX channels, buses and Main's inserts rest.
    const bool boardMix = boardWanted.load (std::memory_order_relaxed);
    constexpr int most = LatencyDelay::capacity - 1;
    int slowestTrack = 0, slowestBus = 0, slowestFx = 0;

    for (const auto& track : tracks)
    {
        if (track->kind == Track::Kind::bus)
            slowestBus = juce::jmax (slowestBus, track->ownLatency (false));
        else if (track->kind == Track::Kind::fx)
            slowestFx = juce::jmax (slowestFx, track->ownLatency (false));
        else
            slowestTrack = juce::jmax (slowestTrack, track->ownLatency (boardMix));
    }

    // Together they must fit in one delay line (a track straight to Main waits for all of them).
    slowestTrack = juce::jmin (slowestTrack, most);
    slowestBus = boardMix ? 0 : juce::jmin (slowestBus, most - slowestTrack);
    slowestFx = boardMix ? 0 : juce::jmin (slowestFx, most - slowestTrack - slowestBus);
    const auto mainLatency = boardMix ? 0 : juce::jmin (mainInserts.getLatencySamples(), most);

    // Each track as late as the slowest track, each bus as the slowest bus, each FX channel as the
    // slowest FX channel; what tracks send an FX channel waits for what buses send it.
    for (const auto& track : tracks)
    {
        int delay = 0;

        if (track->kind == Track::Kind::bus)
            delay = boardMix ? 0 : juce::jmax (0, slowestBus - track->ownLatency (false));
        else if (track->kind == Track::Kind::fx)
            delay = boardMix ? 0 : juce::jmax (0, slowestFx - track->ownLatency (false));
        else
            delay = juce::jmax (0, slowestTrack - track->ownLatency (boardMix));

        if (delay > 0)
            makeDelay (track->latencyDelay);

        if (track->kind == Track::Kind::fx && slowestBus > 0)
            makeDelay (track->sendsDelay);

        // Its clips as late as its slowest clip effect.
        if (const auto clipLate = track->clipEffectsLatency(); clipLate > 0)
        {
            makeDelay (track->clipsDelay);

            for (const auto& effect : track->clipEffects)
                if (effect != nullptr)
                    makeDelay (effect->delay);

            track->clipLatency.store (clipLate, std::memory_order_relaxed);
        }
        else
        {
            track->clipLatency.store (0, std::memory_order_relaxed);
        }

        // Its sidechain keys into Main's or an FX channel's plugins: as late as what they hear.
        if (slowestBus + slowestFx > 0)
        {
            for (auto& send : track->audioSends)
            {
                const auto* keyed = send.target == mainChannel ? nullptr : findTrack (send.target);

                if (send.sidechain && send.keyDelay == nullptr && (send.target == mainChannel || (keyed != nullptr && keyed->kind == Track::Kind::fx)))
                {
                    auto made = std::make_shared<LatencyDelay>();
                    made->prepare();
                    const juce::SpinLock::ScopedLockType lock (graphLock);
                    send.keyDelay = std::move (made);
                }
            }
        }

        track->compensation.store (delay, std::memory_order_relaxed);
    }

    // What goes straight to Main or a Sub: a track's waits for the buses and FX channels, a bus's
    // for the FX channels; the Subs wait for Main's inserts (Main's mix goes into some of them).
    const auto places = juce::jlimit (1, directPlaces, static_cast<int> (outputs.size()));

    for (int place = 0; place < places; ++place)
    {
        if (slowestBus + slowestFx > 0)
            makeDelay (directDelays[0][static_cast<size_t> (place)]);

        if (slowestFx > 0)
            makeDelay (directDelays[1][static_cast<size_t> (place)]);

        if (place > 0 && mainLatency > 0)
            makeDelay (subDelays[static_cast<size_t> (place - 1)]);
    }

    latencyTracks = slowestTrack;
    latencyBuses = slowestBus;
    latencyFx = slowestFx;
    latencyMain = mainLatency;

    tracksWaitSamples.store (slowestBus + slowestFx, std::memory_order_relaxed);
    busesWaitSamples.store (slowestFx, std::memory_order_relaxed);
    sendsWaitSamples.store (slowestBus, std::memory_order_relaxed);
    subsWaitSamples.store (mainLatency, std::memory_order_relaxed);
    lateTracks.store (slowestTrack, std::memory_order_relaxed);
    lateBuses.store (slowestTrack + slowestBus, std::memory_order_relaxed);
    lateFx.store (slowestTrack + slowestBus + slowestFx, std::memory_order_relaxed);

    const auto heardLate = slowestTrack + slowestBus + slowestFx + mainLatency;
    outputLatencySamples.store (heardLate, std::memory_order_relaxed);
    transport.setClickDelay (heardLate);   // the metronome as late as the song is heard
}

int AudioEngine::renderLatency (int trackId, bool freeze, bool dry) const
{
    if (trackId == 0)
        return latencyTracks + latencyBuses + latencyFx + latencyMain;   // Main's mix, after its inserts

    const auto* track = findTrack (trackId);

    if (track == nullptr)
        return 0;

    // A freeze takes the track's own sound, before it's lined up.
    if (freeze)
        return juce::jmin (track->ownLatency (dry || boardWanted.load (std::memory_order_relaxed)), LatencyDelay::capacity - 1);

    // A stem leaves its fader lined up: a track with the tracks, a bus with the buses, an FX
    // channel with the FX channels.
    return track->kind == Track::Kind::bus ? latencyTracks + latencyBuses
         : track->kind == Track::Kind::fx  ? latencyTracks + latencyBuses + latencyFx
                                           : latencyTracks;
}

//==============================================================================
void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    // An export rendering meanwhile (the device started again by itself: a driver's reset) ends
    // first, cancelled: its rate, buffers and plugins are about to change under it.
    stopMixdownThread();

    const auto sampleRate = device->getCurrentSampleRate();

    midiCollector.reset (sampleRate);
    pageCollector.reset (sampleRate);
    auditionCollector.reset (sampleRate);

    for (auto& collector : deviceCollectors)
        collector->reset (sampleRate);
    midiBuffer.ensureSize (midiBufferBytes);

    currentSampleRate = sampleRate;
    currentBlockSize = device->getCurrentBufferSizeSamples();

    // MIDI clock and timecode go out when the block they belong to is heard.
    syncDelayMs.store (sampleRate > 0.0 ? 1000.0 * device->getOutputLatencyInSamples() / sampleRate : 0.0);

    // The callback gets the device's active output channels, in order: where each one is there.
    const auto activeOutputs = device->getActiveOutputChannels();
    std::vector<int> places (static_cast<size_t> (juce::jmax (activeOutputs.getHighestBit() + 1, device->getOutputChannelNames().size())), -1);

    for (int channel = 0, place = 0; channel < static_cast<int> (places.size()); ++channel)
        if (activeOutputs[channel])
            places[static_cast<size_t> (channel)] = place++;

    {
        // No callbacks run while the device starts, so this never makes the audio thread wait.
        const juce::SpinLock::ScopedLockType lock (graphLock);
        transport.prepare (sampleRate);
        outputIndexOfChannel = std::move (places);

        mix.setSize (2, juce::jmax (currentBlockSize * 2, minimumMixSize), false, true, true);
        subMix.setSize (2 * maxSubOutputs, mix.getNumSamples(), false, true, true);
        directMix.setSize (2 * 2 * directPlaces, mix.getNumSamples(), false, true, true);
        keyScratch.setSize (2, mix.getNumSamples(), false, true, true);
        mainInserts.prepare (currentBlockSize);
        board.prepare (currentSampleRate, currentBlockSize);
        mastering.prepare (currentSampleRate, mix.getNumSamples());
        liveShow.prepare (currentSampleRate, mix.getNumSamples());
        consoleQuiet.setSize (2, mix.getNumSamples(), false, true, true);
        showMix.setSize (4, mix.getNumSamples(), false, true, true);
        studioHeardWithShow = false;
        liveSwitchKeepsStudio = false;

        for (const auto& track : tracks)
            track->prepare (currentSampleRate, currentBlockSize);

        // Failures are reported by updatePlugins(), which sees the plugin inactive.
        forEachPlugin (tracks, mainInserts, [this] (Plugin& plugin)
        {
            plugin.activate (currentSampleRate, currentBlockSize);
        });

        // The Sampling Studio's rack: its plugins at this rate, its block as long as the mix's.
        rackSound.setSize (2, mix.getNumSamples(), false, true, true);

        if (samplingRack != nullptr)
            samplingRack->prepareRack (currentSampleRate, currentBlockSize);
    }

    audioRunning.store (true);
}

void AudioEngine::audioDeviceStopped()
{
    // An export rendering meanwhile ends first, cancelled: its plugins are about to stop.
    stopMixdownThread();

    audioRunning.store (false);
    midiSync.halt (juce::Time::getMillisecondCounterHiRes());   // external devices playing along stop too

    {
        const juce::SpinLock::ScopedLockType lock (graphLock);

        forEachPlugin (tracks, mainInserts, [] (Plugin& plugin) { plugin.stopProcessing(); });

        if (samplingRack != nullptr)
            samplingRack->releaseRack();

        for (const auto& track : tracks)
            track->requestNotesOff();
    }

    activeNotesLow.store (0, std::memory_order_relaxed);
    activeNotesHigh.store (0, std::memory_order_relaxed);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    // Alive: the watch on the message thread sees it go on.
    deviceCallbacks.fetch_add (1, std::memory_order_relaxed);

    // While a mixdown renders the song, or the Sampling Studio's thread renders (Turbo, Unleashed),
    // the device plays silence and keeps out of its way. (Paired with startMixdown and
    // setSamplingTap: one of the two always sees the other's flag.)
    deviceInCallback.store (true);

    if (offline.load() || samplingRenders.load())
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

        // A beep asked for meanwhile would sound late, after the mixdown: it's let go.
        pendingCue.store (0);
        cuePlaying = 0;
        cueSounding.store (false);

        // External devices playing along stop while the mixdown has the song.
        midiSync.halt (juce::Time::getMillisecondCounterHiRes());

        deviceInCallback.store (false);
        return;
    }

    captureVoice (inputChannelData, numInputChannels, numSamples);

    // Only this callback plays the song to the device, so only it sends MIDI clock and timecode.
    blockStartTime = juce::Time::getMillisecondCounterHiRes();
    syncThisBlock = true;
    renderBlock (inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples);
    syncThisBlock = false;

    // Cues are heard where the metronome is (the Audition output); Settings' Test on Main.
    const auto channel = [&] (int index) { return juce::isPositiveAndBelow (index, numOutputChannels) ? outputChannelData[index] : nullptr; };
    mixCue (channel (cueLeftChannel), cueRightChannel != cueLeftChannel ? channel (cueRightChannel) : nullptr, numSamples);
    mixTestTone (channel (mainLeftIndex), mainRightIndex != mainLeftIndex ? channel (mainRightIndex) : nullptr, numSamples);
    deviceInCallback.store (false);
}

void AudioEngine::renderBlock (const float* const* inputChannelData, int numInputChannels,
                               float* const* outputChannelData, int numOutputChannels, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;
    ++blockCount;          // the delay lines know a block they weren't played in
    showAloneBlock = false;

    // A new block for the devices that follow another track's notes (Harmonize).
    MidiFx::board().beginBlock();

    // Silence first, channel by channel (an AudioBuffer around 32 or more would allocate).
    for (int channel = 0; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

    // The device gets every output; a mixdown (the device stands aside meanwhile) only Main's.
    const bool toDevice = ! offline.load (std::memory_order_relaxed);

    // A device channel's place among the callback's outputs (the device's open channels, in
    // order), or -1. A mixdown's buffer is Main's two channels as they are.
    const auto placeOf = [&] (int channel) -> int
    {
        if (! toDevice)
            return juce::isPositiveAndBelow (channel, numOutputChannels) ? channel : -1;

        const auto place = juce::isPositiveAndBelow (channel, static_cast<int> (outputIndexOfChannel.size()))
                               ? outputIndexOfChannel[static_cast<size_t> (channel)] : -1;
        return juce::isPositiveAndBelow (place, numOutputChannels) ? place : -1;
    };

    const auto channelAt = [&] (int place) -> float* { return juce::isPositiveAndBelow (place, numOutputChannels) ? outputChannelData[place] : nullptr; };

    // The outputs this block plays on: taken with the tracks (under the graph lock), so a track
    // and the plan always agree on which Sub is which.
    OutputPlan plan;
    bool planned = false;

    const auto usePlan = [&]
    {
        plan = outputPlan;
        planned = true;

        if (! toDevice || subMix.getNumChannels() < 2 * plan.subCount || subMix.getNumSamples() < juce::jmin (numSamples, mix.getNumSamples()))
        {
            plan.subCount = 0;
            plan.audition = -1;
        }

        // Main: both its channels, or (on a device without them, a mixdown) the first two the callback has.
        const auto stereo = plan.mainRight >= 0 && plan.mainRight != plan.mainLeft;
        mainLeftIndex = placeOf (plan.mainLeft);
        mainRightIndex = stereo ? placeOf (plan.mainRight) : -1;

        if (! toDevice || mainLeftIndex < 0 || (stereo && mainRightIndex < 0))
        {
            mainLeftIndex = numOutputChannels > 0 ? 0 : -1;
            mainRightIndex = numOutputChannels > 1 ? 1 : -1;
        }

        // The Audition output (the preview, the metronome and cues): Main, or a Sub this device can play.
        if (juce::isPositiveAndBelow (plan.audition, plan.subCount) && placeOf (plan.subs[static_cast<size_t> (plan.audition)].left) < 0)
            plan.audition = -1;

        const auto audition = juce::isPositiveAndBelow (plan.audition, plan.subCount) ? plan.audition : -1;
        cueLeftChannel = audition >= 0 ? placeOf (plan.subs[static_cast<size_t> (audition)].left) : mainLeftIndex;
        cueRightChannel = audition >= 0 ? placeOf (plan.subs[static_cast<size_t> (audition)].right) : mainRightIndex;

        if (cueRightChannel < 0)
            cueRightChannel = cueLeftChannel;
    };

    midiBuffer.clear();
    midiCollector.removeNextBlockOfMessages (midiBuffer, numSamples);
    trackNotes (midiBuffer);

    pageBuffer.clear();
    pageCollector.removeNextBlockOfMessages (pageBuffer, numSamples);
    trackNotes (pageBuffer);

    // The Sampling Studio's audition: for its track alone.
    auditionBuffer.clear();
    auditionCollector.removeNextBlockOfMessages (auditionBuffer, numSamples);

    // Each MIDI device's notes, from its own queue.
    const auto deviceSlots = juce::jmin (deviceSlotCount.load(), maxMidiDevices);

    for (int slot = 0; slot < deviceSlots; ++slot)
    {
        auto& deviceBuffer = deviceBuffers[static_cast<size_t> (slot)];
        deviceBuffer.clear();
        deviceCollectors[static_cast<size_t> (slot)]->removeNextBlockOfMessages (deviceBuffer, numSamples);
        trackNotes (deviceBuffer);
    }

    // MIDI In (setMidiFocus): the studio in front takes the live notes. The Plugins Creator
    // Studio listening: its MIDI In's ports go to the plugin being made. The Sampling Studio: its
    // rack's MIDI In node's (while the nodes show and nothing is being sampled). Either way, and
    // on the pages with no MIDI, none go to the tracks (nor into a recording); the tracks still
    // hear what lets go (note-offs, pedals up), so a note held as the studio came to the front
    // doesn't hang there.
    const auto focus = static_cast<MidiFocus> (midiFocus.load (std::memory_order_relaxed));
    const bool creatorTakesLive = toDevice && focus == MidiFocus::creator && creatorListening.load (std::memory_order_relaxed);
    const bool rackTakesLive = toDevice && focus == MidiFocus::sampling && rackMidiInOpen.load (std::memory_order_relaxed)
                               && ! samplingOn.load (std::memory_order_relaxed);
    const bool songHearsLive = ! toDevice || focus == MidiFocus::song || (focus == MidiFocus::creator && ! creatorTakesLive);

    creatorMidi.clear();
    rackLive.clear();

    // The rack's held notes as they were: kept again when the rack doesn't play this block (it
    // went to sleep, or Turbo renders it), so what it holds lets go once it plays again.
    auto& rackIn = midiIns[static_cast<size_t> (MidiIn::rack)];
    bool rackLiveDelivered = ! toDevice;

    if (toDevice)
    {
        rackIn.heldBefore = rackIn.held;
        rackIn.pedalsBefore = rackIn.pedals;
        rackIn.holdingBefore = rackIn.holding;

        gatherMidiIn (midiIns[static_cast<size_t> (MidiIn::creator)], creatorMidi, creatorTakesLive, deviceSlots, numSamples);
        gatherMidiIn (rackIn, rackLive, rackTakesLive, deviceSlots, numSamples);
    }

    if (! songHearsLive)
    {
        keepReleasesOnly (midiBuffer, creatorScratch);
        keepReleasesOnly (pageBuffer, creatorScratch);

        for (int slot = 0; slot < deviceSlots; ++slot)
            keepReleasesOnly (deviceBuffers[static_cast<size_t> (slot)], creatorScratch);
    }

    // The Live Show has the outputs, or the Sampling Studio is sampling a track: the song stays
    // stopped, whoever asks it to play.
    if (toDevice && (liveWanted.load (std::memory_order_relaxed) || samplingOn.load (std::memory_order_relaxed)))
        transport.setPlaying (false);

    clap_event_transport_t songPosition {};
    transport.beginBlock (songPosition, numSamples);

    if (syncThisBlock)
    {
        // Clock and timecode leave when the song is heard: after the device, and after plugin delay compensation.
        const auto lateMs = currentSampleRate > 0.0 ? 1000.0 * outputLatencySamples.load (std::memory_order_relaxed) / currentSampleRate : 0.0;
        midiSync.process (transport, currentSampleRate, blockStartTime, syncDelayMs.load (std::memory_order_relaxed) + lateMs);
    }

    // What the armed instrument track hears is what it records: the Hub's own notes and its devices.
    recordMidi.clear();
    const auto selectedNow = selectedTrackId.load (std::memory_order_relaxed);
    bool recordingMpe = false;

    if (songHearsLive)
    {
        recordMidi.addEvents (midiBuffer, 0, numSamples, 0);

        const juce::SpinLock::ScopedLockType lock (graphLock);

        for (const auto& track : tracks)
        {
            if (track->kind == Track::Kind::instrument && track->recordArm.load (std::memory_order_relaxed))
            {
                recordingMpe = track->mpeRange.load (std::memory_order_relaxed) > 0;

                if (track->id == selectedNow)
                    recordMidi.addEvents (pageBuffer, 0, numSamples, 0);

                for (const auto& route : track->midiRoutes)
                    if (route.slot >= 0 && route.slot < deviceSlots)
                        addRouted (recordMidi, deviceBuffers[static_cast<size_t> (route.slot)], route, numSamples);

                break;
            }
        }
    }

    captureRecording (recordMidi, recordingMpe);

    // Where a take's recorded samples sit in the song: a pass starts with the take and
    // wherever the song jumps (the loop going round), inside a block or between two.
    const bool recordingNow = transport.isRecordingNow();

    if (recordingNow)
    {
        for (int i = 0; i < transport.getNumSegments(); ++i)
        {
            const auto& segment = transport.getSegment (i);

            if (! recordingAudio || std::abs (segment.from - recordedUpTo) > 1.0e-9)
                pushPass (recordedSamples + segment.offset, segment.from);

            recordedUpTo = segment.to;
        }

        recordedSamples += numSamples;
    }
    else
    {
        recordedSamples = 0;
    }

    recordingAudio = recordingNow;

    // The Main channel's own stereo buffer: tracks mix into it, then its inserts and
    // fader run, and only then does it reach the device. Tracks playing into a Sub output
    // mix into that Sub's own buffer instead.
    const auto mixSamples = juce::jmin (numSamples, mix.getNumSamples());

    if (mixSamples > 0)
    {
        mix.clear (0, mixSamples);

        // getWritePointer marks the buffer as written, so gain and meters see the mix.
        auto* mixLeft = mix.getWritePointer (0);
        auto* mixRight = mix.getWritePointer (1);

        // A mixdown's scratch for tracks on a Sub (subMix's first two channels, unused then).
        const bool scratch = ! toDevice && subMix.getNumChannels() >= 2 && subMix.getNumSamples() >= mixSamples;

        // A mixdown's first block: every fader starts where it is.
        const bool settle = ! toDevice && settleGains.exchange (false);

        // The Console or the Analog Board: a switch fades the old mix out over this block and the
        // new one in over the next (a mixdown switches at once).
        bool fadeOut = false, fadeIn = false;

        // The show switching in or out with the Studio heard on both sides (its tab out of sight):
        // Main's Studio part plays on through it, only the show fading out or in.
        bool switchKeepsStudio = false;

        {
            const juce::SpinLock::ScopedLockType lock (graphLock);
            usePlan();

            // The Studio heard with the show, as asked for this block (its tab out of sight).
            const bool studioInFront = studioWithShow.load (std::memory_order_relaxed);

            // The Live Show or the song: switching fades the old out over this block and the new in
            // over the next. A mixdown is the song's, never the show.
            bool liveSwitching = false;

            if (toDevice)
            {
                const auto wantLive = liveWanted.load (std::memory_order_relaxed);

                if (liveFadingOut)
                {
                    liveFadingOut = false;
                    liveSwitching = true;
                    fadeIn = true;
                    switchKeepsStudio = liveSwitchKeepsStudio;

                    if (wantLive != liveActive)
                    {
                        liveActive = wantLive;

                        if (liveActive)
                        {
                            liveShow.reset();

                            for (const auto& track : tracks)
                                track->live.reset();
                        }

                        // The Studio playing on: its level as it was, handed over (the show's way
                        // has Main's fader on the Studio's part alone, the song's on all of Main).
                        if (switchKeepsStudio)
                        {
                            if (liveActive)
                            {
                                studioHeardWithShow = true;
                                studioWithShowGain = lastMasterGain;
                                lastMasterGain = 1.0f;
                            }
                            else
                            {
                                lastMasterGain = studioWithShowGain;
                                studioHeardWithShow = false;
                            }
                        }
                    }
                }
                else if (wantLive != liveActive)
                {
                    liveFadingOut = true;
                    liveSwitching = true;
                    fadeOut = true;
                    liveSwitchKeepsStudio = studioInFront;
                    switchKeepsStudio = studioInFront;
                }
            }

            const bool playLive = toDevice && liveActive;

            const auto wanted = boardWanted.load (std::memory_order_relaxed);
            const auto startBoard = [this]
            {
                board.startFresh();

                for (const auto& track : tracks)
                    track->board.reset();

                consoleQuietSamples = static_cast<juce::int64> (consoleQuietSeconds * juce::jmax (1.0, currentSampleRate));
            };

            if (! toDevice)
            {
                if (wanted != boardActive)
                {
                    boardActive = wanted;

                    if (boardActive)
                        startBoard();
                }

                boardFadingOut = false;
            }
            else if (liveSwitching || playLive)
            {
                // The board waits while the Live Show switches or plays.
            }
            else if (boardFadingOut)
            {
                boardFadingOut = false;
                fadeIn = true;

                if (wanted != boardActive)
                {
                    boardActive = wanted;

                    if (boardActive)
                        startBoard();
                }
            }
            else if (wanted != boardActive)
            {
                boardFadingOut = true;
                fadeOut = true;
            }

            // The Studio heard with the show while the show's tab is out of sight (the Studio in
            // front, say): one block more as it goes, out to silence.
            const bool studioWanted = playLive && studioInFront;
            const bool studioToo = playLive && (studioWanted || studioHeardWithShow);

            if (! studioToo)
                studioHeardWithShow = false;

            // Its headphones, on their Sub.
            const auto livePhones = juce::isPositiveAndBelow (plan.phones, plan.subCount) ? plan.phones : -1;

            if (playLive && ! studioToo)
            {
                showAloneBlock = true;   // nothing is delayed for the show: its clicks aren't either

                // The Live Show: its channels' tracks, its decks, its ducker and master, on Main.
                for (int channel = 0; channel < juce::jmax (2 * plan.subCount, scratch ? 2 : 0); ++channel)
                    juce::FloatVectorOperations::clear (subMix.getWritePointer (channel), mixSamples);

                renderLive (inputChannelData, numInputChannels, songPosition, numSamples, mixSamples, deviceSlots, mixLeft, mixRight,
                            livePhones >= 0 ? subMix.getWritePointer (2 * livePhones) : nullptr,
                            livePhones >= 0 ? subMix.getWritePointer (2 * livePhones + 1) : nullptr);

                // (A switch keeping the Studio always has it heard here; were it not, the show fades as Main would.)
                if (switchKeepsStudio)
                    mix.applyGainRamp (0, mixSamples, fadeOut ? 1.0f : 0.0f, fadeOut ? 0.0f : 1.0f);
            }
            else
            {
                // While the board mixes, the Console's effects play silence for a while (heard nowhere),
                // so their tails have died away by the time the Console mixes again; then they rest.
                const bool quietingConsole = boardActive && consoleQuietSamples > 0;
                bool anyBoardSolo = false;

                if (boardActive)
                {
                    board.begin (mixSamples);

                    // Stems are each strip as the song has it, whatever is soloed for listening (a freeze too).
                    if (stemTaps == nullptr && freezeTap == nullptr)
                        for (const auto& track : tracks)
                            anyBoardSolo = anyBoardSolo || (! track->isMixChannel() && ! (playLive && track->liveChannel.load (std::memory_order_relaxed))
                                                            && track->board.settings.isOn (AnalogBoard::solo));
                }

                for (int channel = 0; channel < juce::jmax (2 * plan.subCount, scratch ? 2 : 0); ++channel)
                    juce::FloatVectorOperations::clear (subMix.getWritePointer (channel), mixSamples);

                // (With the show, its channels' tracks are its own: their solo is no matter here.)
                bool anySolo = false;

                for (const auto& track : tracks)
                    anySolo = anySolo || (track->solo.load (std::memory_order_relaxed)
                                          && ! (playLive && ! track->isMixChannel() && track->liveChannel.load (std::memory_order_relaxed)));

                // Stems are each channel as the song has it, whatever is soloed for listening; so is
                // what a freeze hears (its sidechain keys).
                if (stemTaps != nullptr || freezeTap != nullptr)
                    anySolo = false;

                // Automation follows the song while it plays. Faders and pans move where their sound
                // is heard: as late as plugin delay compensation makes it (plugins' own parameters
                // move with the song, before their sound is delayed).
                const bool playingNow = transport.getNumSegments() > 0;
                const auto blockPpq = playingNow ? transport.getSegment (0).from : 0.0;
                const auto ppqPerSample = playingNow ? transport.getSegment (0).perSample : 0.0;
                const auto heardAt = [&] (int lateBy) { return blockPpq - static_cast<double> (lateBy) * ppqPerSample; };

                const auto tracksLate = lateTracks.load (std::memory_order_relaxed);
                const auto busesLate = lateBuses.load (std::memory_order_relaxed);
                const auto fxLate = lateFx.load (std::memory_order_relaxed);

                // Main's fader, from the song channel's Main Volume lane.
                if (playingNow && mainAutomationPlayback != nullptr)
                {
                    const auto value = Automation::valueAt (*mainAutomationPlayback, heardAt (outputLatencySamples.load (std::memory_order_relaxed)));

                    if (! juce::exactlyEqual (value, lastMainAutomationValue))
                    {
                        lastMainAutomationValue = value;
                        masterVolumeDb.store (static_cast<float> (juce::jlimit (static_cast<double> (Track::silentDb), static_cast<double> (Track::maxDb), value)),
                                              std::memory_order_relaxed);
                    }
                }

                // FX channels' inputs and plugins' sidechains start silent; the sends fill them.
                clearSidechains (mixSamples);

                // Plugin delay compensation: what goes straight to Main or a Sub gathers here, to wait
                // for the buses and FX channels: a track's for both, a bus's for the FX channels.
                // A render starts with every delay empty.
                const bool gather = directMix.getNumSamples() >= mixSamples;
                const int waits[2] = { gather ? tracksWaitSamples.load (std::memory_order_relaxed) : 0,
                                       gather ? busesWaitSamples.load (std::memory_order_relaxed) : 0 };
                const auto placesNow = 1 + plan.subCount;
                sendsWaitBlock = gather && ! boardActive ? sendsWaitSamples.load (std::memory_order_relaxed) : 0;
                tracksWaitBlock = boardActive ? 0 : waits[0];
                busesWaitBlock = boardActive ? 0 : waits[1];

                const auto directChannel = [] (int set, int place) { return 2 * (set * directPlaces + place); };

                // Which gathering a channel straight to Main or a Sub goes to (-1: none, as it is).
                const auto directSetOf = [&] (const Track& t) -> int
                {
                    const auto set = t.kind == Track::Kind::bus ? 1 : t.kind == Track::Kind::fx ? -1 : 0;
                    return set >= 0 && waits[set] > 0 ? set : -1;
                };

                if (settle)
                {
                    for (const auto& track : tracks)
                    {
                        if (track->latencyDelay != nullptr)  track->latencyDelay->clear();
                        if (track->sendsDelay != nullptr)    track->sendsDelay->clear();
                        track->trackSends.clear();
                        track->clearClipDelays();

                        for (const auto& send : track->audioSends)
                            if (send.keyDelay != nullptr)
                                send.keyDelay->clear();
                    }

                    for (auto& set : directDelays)
                        for (auto& delay : set)
                            if (delay != nullptr)
                                delay->clear();

                    for (auto& delay : subDelays)
                        if (delay != nullptr)
                            delay->clear();
                }

                for (int set = 0; set < 2; ++set)
                    if (waits[set] > 0)
                        for (int place = 0; place < placesNow; ++place)
                            for (int channel = 0; channel < 2; ++channel)
                                juce::FloatVectorOperations::clear (directMix.getWritePointer (directChannel (set, place) + channel), mixSamples);

                // A track keying a sidechain plays before the track whose plugin listens; FX channels last.
                const bool ordered = orderTracks();
                const auto count = ordered ? renderOrder.size() : tracks.size();

                for (size_t index = 0; index < count; ++index)
                {
                    auto* track = ordered ? renderOrder[index] : tracks[index].get();

                    // The Studio with the show: the show's channels' tracks are the show's (renderLive).
                    if (playLive && ! track->isMixChannel() && track->liveChannel.load (std::memory_order_relaxed))
                        continue;

                    // A freeze: the track and the ones it needs, no others.
                    if (freezeTap != nullptr && ! freezeTap->takes (track->id))
                        continue;

                    // The board mixes the tracks alone: the Console's FX channels and buses rest.
                    if (boardActive && track->isMixChannel())
                    {
                        if (quietingConsole)
                            track->render (noMidi, nullptr, 0, false, transport, songPosition, numSamples);

                        continue;
                    }

                    // Rendered by the Sampling Studio's own thread (Turbo, Unleashed): not here.
                    if (samplingOffline && samplingTap != nullptr && track->id == samplingTrackId)
                        continue;

                    // The Sampling Studio holding it: the studio's notes alone, played live. An audio
                    // track's input goes to the studio either way, and is heard while it monitors.
                    const bool sampled = toDevice && samplingTap != nullptr && track->id == samplingTrackId;
                    const bool inputUnheard = sampled && track->kind == Track::Kind::audio && ! track->monitor.load (std::memory_order_relaxed);

                    // FX channels stay audible while tracks are soloed: they play the soloed tracks' sends.
                    // A soloed bus plays its tracks; a soloed track plays through its bus.
                    const bool audible = ! track->mute.load (std::memory_order_relaxed) && ! inputUnheard
                                         && (! anySolo || track->solo.load (std::memory_order_relaxed) || track->kind == Track::Kind::fx
                                             || soloedThroughBus (*track));

                    // Live notes reach the tracks that monitor or are armed to record.
                    const bool listening = track->monitor.load (std::memory_order_relaxed)
                                           || track->recordArm.load (std::memory_order_relaxed);

                    if (playingNow)
                        applyAutomation (*track, blockPpq, heardAt (track->kind == Track::Kind::bus ? busesLate
                                                                    : track->kind == Track::Kind::fx ? fxLate : tracksLate));

                    // An FX channel: what the tracks sent it waits for what the buses sent (later).
                    if (track->kind == Track::Kind::fx)
                    {
                        if (auto* delay = track->sendsDelay.get(); delay != nullptr && track->trackSends.getNumSamples() >= mixSamples
                                                                   && track->busInput.getNumSamples() >= mixSamples)
                        {
                            const auto wait = sendsWaitBlock;
                            auto* sentLeft = track->trackSends.getWritePointer (0);
                            auto* sentRight = track->trackSends.getWritePointer (1);
                            delay->process (sentLeft, sentRight, mixSamples, wait, blockCount);

                            if (wait > 0)
                            {
                                juce::FloatVectorOperations::add (track->busInput.getWritePointer (0), sentLeft, mixSamples);
                                juce::FloatVectorOperations::add (track->busInput.getWritePointer (1), sentRight, mixSamples);
                            }
                        }
                    }

                    // Its notes in: the Hub's own and its MIDI devices while it listens, and the notes
                    // of the tracks' plugins it takes MIDI from (always, like a cable).
                    auto& live = track->liveMidi;
                    live.clear();

                    // Played by an MPE controller: the instrument hears so first (again for a new instrument).
                    if (const auto range = track->mpeRange.load (std::memory_order_relaxed); range > 0)
                    {
                        if (track->mpeResend.exchange (false) || track->instrument.get() != track->mpeToldInstrument)
                        {
                            addMpeSetup (live, range);
                            track->mpeToldInstrument = track->instrument.get();
                        }
                    }

                    if (sampled)
                    {
                        samplingTap->tapNotes (live, numSamples);
                    }
                    else
                    {
                        // The page's own notes (the piano roll's) go to the selected track, monitored or not.
                        // A render (a mixdown, a freeze) plays the song alone: nothing played meanwhile.
                        if (toDevice && track->id == selectedTrackId.load (std::memory_order_relaxed))
                            live.addEvents (pageBuffer, 0, numSamples, 0);

                        // MIDI devices reach the tracks that monitor (or record): those they chose, and
                        // any device the Hub couldn't tell apart.
                        if (toDevice && listening)
                        {
                            live.addEvents (midiBuffer, 0, numSamples, 0);

                            for (const auto& route : track->midiRoutes)
                                if (route.slot >= 0 && route.slot < deviceSlots)
                                    addRouted (live, deviceBuffers[static_cast<size_t> (route.slot)], route, numSamples);
                        }

                        for (const auto sourceId : track->midiSourceTracks)
                            if (auto* source = findTrack (sourceId); source != nullptr && source != track
                                                                     && source->instrument != nullptr && source->instrument->producesMidi())
                                live.addEvents (source->instrument->getMidiOutput(), 0, numSamples, 0);
                    }

                    // The Sampling Studio's audition, for its track.
                    if (toDevice && ! sampled && track->id == auditionTrackId.load (std::memory_order_relaxed))
                        live.addEvents (auditionBuffer, 0, numSamples, 0);

                    // Sampled, it plays live through its inserts, even while the board mixes (at a BPM
                    // layer's tempo when the studio asks for one).
                    auto heard = songPosition;

                    if (sampled)
                        if (const auto tempo = samplingTap->tapTempo(); tempo > 0.0)
                            heard.tempo = tempo;

                    const bool renderDry = freezeTap != nullptr && freezeTap->dry && track->id == freezeTap->trackId;

                    track->render (live, inputChannelData, numInputChannels, recordingNow,
                                   transport, heard, numSamples, (boardActive && ! sampled) || renderDry, sampled);

                    if (sampled)
                        samplingTap->tapSound (track->getBlock (0), track->getBlock (1), juce::jmin (numSamples, track->getBlockSamples()));

                    if (freezeTap != nullptr && track->id == freezeTap->trackId)
                        freezeTap->take (*track, mixSamples);

                    // Plugin delay compensation: as late as the slowest of its kind, so everything
                    // meets in time (a freeze takes its sound before this: its render is in time).
                    // Its sound before the delay stays for sends keying another track's sidechain.
                    if (auto* delay = track->latencyDelay.get())
                    {
                        const auto lateBy = track->compensation.load (std::memory_order_relaxed);

                        if (lateBy > 0 && ! track->isMixChannel())
                            track->keepKeyBlock (mixSamples);
                        else
                            track->dropKeyBlock();

                        delay->process (track->getWritableBlock (0), track->getWritableBlock (1),
                                        juce::jmin (mixSamples, track->getBlockSamples()), lateBy, blockCount);
                    }
                    else
                    {
                        track->dropKeyBlock();
                    }

                    // On the board: its dry sound through its strip's chain (the preamp, filters, EQ and
                    // compressor). Its dynamic EQ, fader and sends come once every strip's chain has run
                    // (below), so it can listen to any other strip, whatever order they play in.
                    if (boardActive)
                    {
                        track->board.processChain (track->getBlock (0), track->getBlock (1), juce::jmin (mixSamples, track->getBlockSamples()),
                                                   blockCount, settle);

                        // (Heard on the board, for its finish: not muted, not left out by a solo, its input heard.)
                        track->boardInputUnheard = inputUnheard;

                        if (quietingConsole)
                            track->quietInserts (songPosition);

                        continue;
                    }

                    // Into its bus, Main, or its Sub output. A mixdown is Main's alone: a track on a Sub
                    // plays into a scratch buffer then (its sends to FX channels still count).
                    const auto slot = track->outputSlot.load (std::memory_order_relaxed);
                    auto* intoLeft = mixLeft;
                    auto* intoRight = mixRight;

                    if (auto* bus = busOf (*track); bus != nullptr && bus->busInput.getNumSamples() >= mixSamples)
                    {
                        intoLeft = bus->busInput.getWritePointer (0);
                        intoRight = bus->busInput.getWritePointer (1);
                    }
                    else if (juce::isPositiveAndBelow (slot, plan.subCount) || (slot >= 0 && scratch))
                    {
                        const auto channel = juce::isPositiveAndBelow (slot, plan.subCount) ? 2 * slot : 0;
                        intoLeft = subMix.getWritePointer (channel);
                        intoRight = subMix.getWritePointer (channel + 1);

                        // Straight to a Sub: it waits for the buses and FX channels (below).
                        if (const auto set = directSetOf (*track); set >= 0 && juce::isPositiveAndBelow (slot, plan.subCount))
                        {
                            intoLeft = directMix.getWritePointer (directChannel (set, slot + 1));
                            intoRight = directMix.getWritePointer (directChannel (set, slot + 1) + 1);
                        }
                    }
                    else if (const auto set = directSetOf (*track); set >= 0)
                    {
                        // Straight to Main: it waits for the buses and FX channels (below).
                        intoLeft = directMix.getWritePointer (directChannel (set, 0));
                        intoRight = directMix.getWritePointer (directChannel (set, 0) + 1);
                    }

                    if (settle)
                        track->settleGain (audible);

                    track->mixInto (intoLeft, intoRight, mixSamples, audible);
                    sendFrom (*track, mixSamples, settle);

                    if (stemTaps != nullptr)
                        stemTaps->take (*track, mixSamples);
                }

                // On the board: each strip's dynamic EQ (hearing its key: its own sound, or another
                // strip's after that strip's compressor), then its pan and fader, into the board's bus
                // and sends.
                if (boardActive)
                {
                    for (const auto& track : tracks)
                    {
                        auto& strip = track->board;

                        if (track->isMixChannel() || ! strip.isWaitingToFinish())
                            continue;

                        const auto keyId = strip.getKeyTrack();
                        const Track* keyTrack = keyId == 0 || keyId == track->id ? track.get() : findTrack (keyId);
                        const float* keyLeft = nullptr;
                        const float* keyRight = nullptr;

                        if (keyTrack != nullptr && ! keyTrack->isMixChannel() && keyTrack->board.getOutputSamples() >= strip.getOutputSamples())
                        {
                            keyLeft = keyTrack->board.getKey (0, blockCount);
                            keyRight = keyTrack->board.getKey (1, blockCount);
                        }

                        const bool onBoard = ! strip.settings.isOn (AnalogBoard::mute) && ! track->boardInputUnheard
                                             && (! anyBoardSolo || strip.settings.isOn (AnalogBoard::solo));

                        strip.finish (keyLeft, keyRight, board, onBoard, settle);

                        if (const auto made = strip.getOutputSamples(); made > 0)
                        {
                            const auto peakOf = [made] (const float* data)
                            {
                                const auto range = juce::FloatVectorOperations::findMinAndMax (data, made);
                                return juce::jmax (-range.getStart(), range.getEnd());
                            };

                            track->showLevels (peakOf (strip.getOutput (0)), peakOf (strip.getOutput (1)));
                        }

                        if (stemTaps != nullptr)
                            stemTaps->takeBoard (*track, mixSamples);
                    }
                }

                // What went straight to Main and the Subs, as late as the buses and FX channels.
                for (int set = 0; set < 2; ++set)
                {
                    if (waits[set] <= 0)
                        continue;

                    for (int place = 0; place < placesNow; ++place)
                    {
                        auto* fromLeft = directMix.getWritePointer (directChannel (set, place));
                        auto* fromRight = directMix.getWritePointer (directChannel (set, place) + 1);

                        if (auto* delay = directDelays[static_cast<size_t> (set)][static_cast<size_t> (place)].get())
                            delay->process (fromLeft, fromRight, mixSamples, waits[set], blockCount);

                        auto* toLeft = place == 0 ? mixLeft : subMix.getWritePointer (2 * (place - 1));
                        auto* toRight = place == 0 ? mixRight : subMix.getWritePointer (2 * (place - 1) + 1);
                        juce::FloatVectorOperations::add (toLeft, fromLeft, mixSamples);
                        juce::FloatVectorOperations::add (toRight, fromRight, mixSamples);
                    }
                }

                // The board's master section plays on Main in place of Main's inserts and fader.
                if (boardActive)
                {
                    board.finish (mixLeft, mixRight, mixSamples, songPosition.tempo, settle);

                    if (quietingConsole && consoleQuiet.getNumSamples() >= mixSamples)
                    {
                        consoleQuiet.clear (0, mixSamples);
                        mainInserts.process (consoleQuiet.getWritePointer (0), consoleQuiet.getWritePointer (1), mixSamples, noMidi, songPosition);
                    }

                    consoleQuietSamples = juce::jmax<juce::int64> (0, consoleQuietSamples - mixSamples);
                }
                else
                {
                    mainInserts.process (mixLeft, mixRight, mixSamples, noMidi, songPosition);

                    // The Subs wait as long as Main's inserts make Main, before Main's mix goes into
                    // any of them (plugin delay compensation; the show's headphones come after).
                    if (const auto subsWait = subsWaitSamples.load (std::memory_order_relaxed); subsWait > 0)
                        for (int slot = 0; slot < plan.subCount; ++slot)
                            if (auto* delay = subDelays[static_cast<size_t> (slot)].get())
                                delay->process (subMix.getWritePointer (2 * slot), subMix.getWritePointer (2 * slot + 1),
                                                mixSamples, subsWait, blockCount);
                }

                // The show on top of the Studio: the Studio's part through Main's fader first (the
                // board's master has its own), in from silence as it comes and out to it as it
                // goes, its Sub outputs too; then the show at its own master, its headphones on
                // their Sub.
                if (studioToo)
                {
                    const auto mainDb = masterVolumeDb.load (std::memory_order_relaxed);
                    const auto studioGain = ! studioWanted ? 0.0f
                                          : boardActive ? 1.0f
                                          : masterMute.load (std::memory_order_relaxed) || mainDb <= Track::silentDb ? 0.0f
                                          : juce::Decibels::decibelsToGain (mainDb);
                    const auto subsFrom = studioHeardWithShow ? 1.0f : 0.0f, subsTo = studioWanted ? 1.0f : 0.0f;

                    if (! studioHeardWithShow)
                        studioWithShowGain = 0.0f;

                    mix.applyGainRamp (0, mixSamples, studioWithShowGain, studioGain);
                    studioWithShowGain = studioGain;

                    if (! juce::exactlyEqual (subsFrom, subsTo))
                        for (int channel = 0; channel < 2 * plan.subCount; ++channel)
                            subMix.applyGainRamp (channel, 0, mixSamples, subsFrom, subsTo);

                    studioHeardWithShow = studioWanted;

                    showMix.clear (0, mixSamples);
                    renderLive (inputChannelData, numInputChannels, songPosition, numSamples, mixSamples, deviceSlots,
                                showMix.getWritePointer (0), showMix.getWritePointer (1),
                                livePhones >= 0 ? showMix.getWritePointer (2) : nullptr,
                                livePhones >= 0 ? showMix.getWritePointer (3) : nullptr);

                    // The show coming in or going while the Studio plays on: the show alone fades
                    // (its headphones with the Sub outputs, below).
                    if (switchKeepsStudio && (fadeOut || fadeIn))
                    {
                        showMix.applyGainRamp (0, 0, mixSamples, fadeOut ? 1.0f : 0.0f, fadeOut ? 0.0f : 1.0f);
                        showMix.applyGainRamp (1, 0, mixSamples, fadeOut ? 1.0f : 0.0f, fadeOut ? 0.0f : 1.0f);
                    }

                    juce::FloatVectorOperations::add (mixLeft, showMix.getReadPointer (0), mixSamples);
                    juce::FloatVectorOperations::add (mixRight, showMix.getReadPointer (1), mixSamples);

                    if (livePhones >= 0)
                    {
                        juce::FloatVectorOperations::add (subMix.getWritePointer (2 * livePhones), showMix.getReadPointer (2), mixSamples);
                        juce::FloatVectorOperations::add (subMix.getWritePointer (2 * livePhones + 1), showMix.getReadPointer (3), mixSamples);
                    }
                }
            }
        }

        // Main fader, ramped across the block like the track faders so moves never click. (The
        // show is the device's alone: a mixdown always has Main's fader, whatever the device last played.)
        const auto masterDb = masterVolumeDb.load (std::memory_order_relaxed);
        const auto masterGain = boardActive || (toDevice && liveActive) ? 1.0f
                              : masterMute.load (std::memory_order_relaxed) || masterDb <= Track::silentDb
                                    ? 0.0f
                                    : juce::Decibels::decibelsToGain (masterDb);

        if (settle)
            lastMasterGain = masterGain;

        mix.applyGainRamp (0, mixSamples, lastMasterGain, masterGain);
        lastMasterGain = masterGain;

        // The Mastering section: the last thing on Main, after its fader (the Console's, or the
        // board's Master), before the Subs take Main's mix. Not on the Live Show while it plays
        // (it has its own effects), nor a render without it; a mixdown's unmastered file takes Main first.
        {
            if (unmasteredTap != nullptr && unmasteredTap->getNumSamples() >= mixSamples)
            {
                unmasteredTap->copyFrom (0, 0, mix, 0, 0, mixSamples);
                unmasteredTap->copyFrom (1, 0, mix, 1, 0, mixSamples);
            }

            const bool playingSong = transport.getNumSegments() > 0;

            if (playingSong && ! masteringWasPlaying)
                mastering.meters.resetRequested.store (true);   // its integrated loudness: from this play on

            masteringWasPlaying = playingSong;

            const bool leftOut = (toDevice && liveActive) || freezeTap != nullptr || masteringLeftOut.load (std::memory_order_relaxed);
            mastering.process (mix.getWritePointer (0), mix.getWritePointer (1), mixSamples, leftOut, playingSong, settle);

            // AI Mix's A/B: the original heard at the suggestion's loudness (the device only).
            if (toDevice)
            {
                const auto trim = juce::Decibels::decibelsToGain (listenTrimDb.load (std::memory_order_relaxed));

                if (! juce::approximatelyEqual (trim, 1.0f) || ! juce::approximatelyEqual (lastListenTrim, 1.0f))
                    mix.applyGainRamp (0, mixSamples, lastListenTrim, trim);

                lastListenTrim = trim;
            }
        }

        // Sub outputs: the Main mix where asked (as it is after the Main fader), then each one's fader.
        for (int slot = 0; slot < plan.subCount; ++slot)
        {
            const auto place = static_cast<size_t> (slot);

            // (Not on the Live Show's headphones while it plays: they have the show already.)
            if (plan.subs[place].mainMix && ! (toDevice && liveActive && slot == plan.phones))
            {
                juce::FloatVectorOperations::add (subMix.getWritePointer (2 * slot), mixLeft, mixSamples);
                juce::FloatVectorOperations::add (subMix.getWritePointer (2 * slot + 1), mixRight, mixSamples);
            }

            const auto db = subVolumeDb[place].load (std::memory_order_relaxed);
            const auto gain = subMute[place].load (std::memory_order_relaxed) || db <= Track::silentDb ? 0.0f : juce::Decibels::decibelsToGain (db);
            subMix.applyGainRamp (2 * slot, 0, mixSamples, subLastGain[place], gain);
            subMix.applyGainRamp (2 * slot + 1, 0, mixSamples, subLastGain[place], gain);
            subLastGain[place] = gain;
        }

        // Switching between the Console and the board, or the song and the show: out, then in.
        // (The show switching while the Studio plays on: Main's own fades were the show's alone.)
        if (fadeOut || fadeIn)
        {
            const auto from = fadeOut ? 1.0f : 0.0f, to = fadeOut ? 0.0f : 1.0f;

            if (! switchKeepsStudio)
                mix.applyGainRamp (0, mixSamples, from, to);

            for (int channel = 0; channel < 2 * plan.subCount; ++channel)
                subMix.applyGainRamp (channel, 0, mixSamples, from, to);
        }

        // The Browser's preview: on the Audition output after its fader, so it's heard however the mix is set.
        {
            const auto auditionSub = juce::isPositiveAndBelow (plan.audition, plan.subCount) ? plan.audition : -1;
            auto* previewLeft = auditionSub >= 0 ? subMix.getWritePointer (2 * auditionSub) : mixLeft;
            auto* previewRight = auditionSub >= 0 ? subMix.getWritePointer (2 * auditionSub + 1) : mixRight;

            const juce::SpinLock::ScopedLockType lock (graphLock);
            mixPreview (previewLeft, previewRight, mixSamples);

            // The Plugins Creator Studio's plugin, on the device only.
            if (toDevice && creatorPreview != nullptr)
                creatorPreview->renderPreview (creatorMidi,
                                               numInputChannels > 0 ? inputChannelData[0] : nullptr,
                                               numInputChannels > 1 ? inputChannelData[1] : nullptr,
                                               previewLeft, previewRight, mixSamples, songPosition);

            // The Sampling Studio's rack, while it's awake: the studio's notes while it samples
            // (or its audition's), its MIDI In node's live notes, its sound to the studio and
            // heard here. Rendered by the studio's
            // own thread (Turbo, Unleashed): not here.
            const auto rackTapped = samplingTap != nullptr && samplingTrackId == rackTrackId;

            if (toDevice && samplingRack != nullptr && samplingRack->isRackAwake() && ! (rackTapped && samplingOffline))
            {
                rackNotes.clear();
                auto heard = songPosition;

                if (rackTapped)
                {
                    samplingTap->tapNotes (rackNotes, mixSamples);

                    if (const auto tempo = samplingTap->tapTempo(); tempo > 0.0)
                        heard.tempo = tempo;
                }
                else if (auditionTrackId.load (std::memory_order_relaxed) == rackTrackId)
                {
                    rackNotes.addEvents (auditionBuffer, 0, mixSamples, 0);
                }

                const auto rackSamples = juce::jmin (mixSamples, rackSound.getNumSamples());
                renderRackBlock (rackNotes, rackLive, rackSamples, heard);
                rackLiveDelivered = true;

                if (rackTapped)
                    samplingTap->tapSound (rackSound.getReadPointer (0), rackSound.getReadPointer (1), rackSamples);

                juce::FloatVectorOperations::add (previewLeft, rackSound.getReadPointer (0), rackSamples);
                juce::FloatVectorOperations::add (previewRight, rackSound.getReadPointer (1), rackSamples);
            }
        }

        // The meters read the mixes, not the clicks.
        storeMax (peakLeft, mix.getMagnitude (0, 0, mixSamples));
        storeMax (peakRight, mix.getMagnitude (1, 0, mixSamples));

        for (int slot = 0; slot < plan.subCount; ++slot)
        {
            storeMax (subPeakLeft[static_cast<size_t> (slot)], subMix.getMagnitude (2 * slot, 0, mixSamples));
            storeMax (subPeakRight[static_cast<size_t> (slot)], subMix.getMagnitude (2 * slot + 1, 0, mixSamples));
        }

        // To the device: stereo as it is; on one channel (a mono output, a mono device), the
        // average of both sides. Added, so outputs that share a channel are both heard.
        const auto playOn = [&] (int leftPlace, int rightPlace, const float* sourceLeft, const float* sourceRight)
        {
            auto* outLeft = channelAt (leftPlace);
            auto* outRight = rightPlace != leftPlace ? channelAt (rightPlace) : nullptr;

            if (outLeft != nullptr && outRight != nullptr)
            {
                juce::FloatVectorOperations::add (outLeft, sourceLeft, mixSamples);
                juce::FloatVectorOperations::add (outRight, sourceRight, mixSamples);
            }
            else if (outLeft != nullptr)
            {
                juce::FloatVectorOperations::addWithMultiply (outLeft, sourceLeft, 0.5f, mixSamples);
                juce::FloatVectorOperations::addWithMultiply (outLeft, sourceRight, 0.5f, mixSamples);
            }
        };

        playOn (mainLeftIndex, mainRightIndex, mixLeft, mixRight);

        for (int slot = 0; slot < plan.subCount; ++slot)
        {
            const auto& sub = plan.subs[static_cast<size_t> (slot)];
            playOn (placeOf (sub.left), sub.right >= 0 ? placeOf (sub.right) : -1,
                    subMix.getReadPointer (2 * slot), subMix.getReadPointer (2 * slot + 1));
        }
    }

    if (! planned)
    {
        const juce::SpinLock::ScopedLockType lock (graphLock);
        usePlan();
    }

    if (! rackLiveDelivered)
    {
        rackIn.held = rackIn.heldBefore;
        rackIn.pedals = rackIn.pedalsBefore;
        rackIn.holding = rackIn.holdingBefore;
        rackIn.letGoPending = rackIn.holding;
    }

    // The metronome goes in last, so no effect or fader ever changes the clicks: on the Audition output.
    transport.setClicksOnTime (showAloneBlock);   // the show alone isn't late: its clicks aren't either
    transport.endBlock (channelAt (cueLeftChannel), cueRightChannel != cueLeftChannel ? channelAt (cueRightChannel) : nullptr, numSamples);
}

void AudioEngine::mixTestTone (float* left, float* right, int numSamples) noexcept
{
    if (testToneRequested.exchange (false))
    {
        testTonePlaying = true;
        testToneSample = 0;
        testTonePhase = 0.0;
    }

    if (! testTonePlaying || left == nullptr || currentSampleRate <= 0.0)
        return;

    // Half a second of 1 kHz at about -14 dBFS, faded in and out so it never clicks.
    const auto length = static_cast<juce::int64> (0.5 * currentSampleRate);
    const auto fade = juce::jmax<juce::int64> (1, static_cast<juce::int64> (0.01 * currentSampleRate));
    const auto step = juce::MathConstants<double>::twoPi * 1000.0 / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = testToneSample + i;

        if (t >= length)
            break;

        const auto edge = juce::jmin (t, length - 1 - t);
        const auto envelope = edge < fade ? static_cast<float> (edge) / static_cast<float> (fade) : 1.0f;
        const auto value = 0.2f * envelope * static_cast<float> (std::sin (testTonePhase));
        testTonePhase += step;

        left[i] += value;

        if (right != nullptr)
            right[i] += value;
    }

    testToneSample += numSamples;

    if (testToneSample >= length)
        testTonePlaying = false;
}

void AudioEngine::renderLive (const float* const* deviceInputs, int numInputs, const clap_event_transport_t& position,
                              int numSamples, int mixSamples, int deviceSlots, float* left, float* right,
                              float* phonesLeft, float* phonesRight) noexcept
{
    liveShow.begin (mixSamples);

    // The show's channels send nothing: plugins' sidechains and FX channels hear nothing of them
    // (with the Studio heard alongside, its own have played already).
    clearSidechains (mixSamples);

    for (const auto& owned : tracks)
    {
        auto* track = owned.get();

        if (track->isMixChannel())
            continue;

        // Switched off as one of the show's channels while the night records: its stem goes on in
        // silence, so it keeps time with Show.wav (as when the mics are muted). consoleQuiet is
        // free here: the Console's silence is made before the show plays, or not at all.
        if (! track->liveChannel.load (std::memory_order_relaxed))
        {
            if (track->live.recorder != nullptr && mixSamples > 0 && consoleQuiet.getNumChannels() >= 2
                && consoleQuiet.getNumSamples() >= mixSamples)
            {
                consoleQuiet.clear (0, mixSamples);
                track->live.recorder->write (consoleQuiet.getArrayOfReadPointers(), mixSamples);
            }

            continue;
        }

        // An instrument plays what's played live: the page's notes when it's the selected track,
        // and its MIDI devices (as when it monitors).
        auto& notes = track->liveMidi;
        notes.clear();

        if (track->kind == Track::Kind::instrument)
        {
            if (track->id == selectedTrackId.load (std::memory_order_relaxed))
                notes.addEvents (pageBuffer, 0, numSamples, 0);

            notes.addEvents (midiBuffer, 0, numSamples, 0);

            for (const auto& route : track->midiRoutes)
                if (route.slot >= 0 && route.slot < deviceSlots)
                    addRouted (notes, deviceBuffers[static_cast<size_t> (route.slot)], route, numSamples);
        }

        // An audio track hears its input whether or not it monitors: through its inserts.
        track->render (notes, deviceInputs, numInputs, false, transport, position, numSamples, false, true);

        const auto made = juce::jmin (mixSamples, track->getBlockSamples());
        liveShow.addChannel (track->live, track->getBlock (0), track->getBlock (1), made);

        // The Studio's meters show it too.
        if (made > 0)
        {
            const auto peakOf = [made] (const float* data)
            {
                const auto range = juce::FloatVectorOperations::findMinAndMax (data, made);
                return juce::jmax (-range.getStart(), range.getEnd());
            };

            track->showLevels (peakOf (track->getBlock (0)), peakOf (track->getBlock (1)));
        }
    }

    liveShow.finish (left, right, mixSamples, phonesLeft, phonesRight);
}

void AudioEngine::mixPreview (float* left, float* right, int numSamples) noexcept
{
    if (previewPlaying == nullptr)
        return;

    const auto& samples = previewPlaying->samples;
    const auto available = static_cast<juce::int64> (samples.getNumSamples());

    // The Loop Editor's: from the start, then round the loop for ever, as an SFZ player plays it
    // (the Sonify Sampler, sfizz): the loop's end fades (equal power) into what comes just before
    // its start, so the turn back is seamless. (Sampler Arena's preview faded into the start
    // itself, then jumped back: a click no player makes.)
    if (previewLoopEnd > previewLoopStart && previewLoopEnd < available)
    {
        const auto* sourceLeft = samples.getReadPointer (0);
        const auto* sourceRight = samples.getReadPointer (samples.getNumChannels() > 1 ? 1 : 0);
        const auto loopLength = previewLoopEnd - previewLoopStart;
        const auto fade = juce::jlimit<juce::int64> (0, juce::jmin (loopLength, previewLoopStart), previewCrossfade);

        for (int i = 0; i < numSamples; ++i)
        {
            if (previewPosition >= previewLoopEnd)
            {
                previewPosition = previewLoopStart;
                previewLooped = true;
            }

            auto l = sourceLeft[previewPosition];
            auto r = sourceRight[previewPosition];

            if (fade > 0 && previewPosition >= previewLoopEnd - fade)
            {
                const auto into = previewPosition - (previewLoopEnd - fade);
                const auto t = static_cast<float> (into) / static_cast<float> (fade);
                const auto out = std::sqrt (1.0f - t), in = std::sqrt (t);
                const auto back = previewPosition - loopLength;
                l = l * out + sourceLeft[back] * in;
                r = r * out + sourceRight[back] * in;
            }

            left[i] += l;
            right[i] += r;
            ++previewPosition;
        }

        previewShown.store (static_cast<juce::int64> (static_cast<double> (previewPosition) / juce::jmax (1.0e-9, previewRatio)), std::memory_order_relaxed);
        return;
    }

    const auto count = static_cast<int> (juce::jmin<juce::int64> (numSamples, available - previewPosition));

    if (count > 0)
    {
        const auto start = static_cast<int> (previewPosition);
        juce::FloatVectorOperations::add (left, samples.getReadPointer (0, start), count);
        juce::FloatVectorOperations::add (right, samples.getReadPointer (samples.getNumChannels() > 1 ? 1 : 0, start), count);
        previewPosition += count;
    }

    previewShown.store (static_cast<juce::int64> (static_cast<double> (previewPosition) / juce::jmax (1.0e-9, previewRatio)), std::memory_order_relaxed);

    if (previewPosition >= available)
    {
        previewPlaying = nullptr;
        previewFinished.store (true);
    }
}

void AudioEngine::pushPass (juce::int64 fileSample, double ppq) noexcept
{
    const auto write = passQueue.write (1);

    if (write.blockSize1 > 0)
        queuedPasses[static_cast<size_t> (write.startIndex1)] = { fileSample, ppq };
}

void AudioEngine::captureRecording (const juce::MidiBuffer& live, bool mpe) noexcept
{
    const bool recordingNow = transport.isRecordingNow();

    // Recording just stopped (punch out or stop): say exactly where, so notes still held
    // end there rather than wherever the next check happens to find the playhead.
    if (capturing && ! recordingNow)
    {
        const auto write = recordQueue.write (1);

        if (write.blockSize1 > 0)
            recordedEvents[static_cast<size_t> (write.startIndex1)] = { transport.getBlockStart(), punchOutMarker, 0, 0 };
    }

    // A take starting: the wheel's and pressure's first values are news.
    if (recordingNow && ! capturing)
        lastBendQueued = lastPressureQueued = -1;

    capturing = recordingNow;

    if (! recordingNow || live.isEmpty())
        return;

    const auto segments = transport.getNumSegments();

    for (const auto metadata : live)
    {
        if (metadata.numBytes < 2)
            continue;

        // Notes, the pitch bend wheel and aftertouch (channel pressure, two bytes).
        const auto kind = metadata.data[0] & 0xf0;

        if (kind != 0x80 && kind != 0x90 && kind != 0xe0 && kind != 0xd0)
            continue;

        if (kind != 0xd0 && metadata.numBytes < 3)
            continue;

        // An MPE controller bends each note on a channel of its own: only its master channel's
        // bend and pressure are the whole part's. A value sent again is no news.
        if (kind == 0xd0 || kind == 0xe0)
        {
            if (mpe && (metadata.data[0] & 0x0f) != 0)
                continue;

            const auto value = kind == 0xe0 ? ((metadata.data[2] & 0x7f) << 7 | (metadata.data[1] & 0x7f)) : (metadata.data[1] & 0x7f);
            auto& last = kind == 0xe0 ? lastBendQueued : lastPressureQueued;

            if (value == last)
                continue;

            last = value;
        }

        // The song position of this sample: find its stretch in the block's plan.
        auto ppq = transport.getSegment (0).from;

        for (int i = 0; i < segments; ++i)
        {
            const auto& segment = transport.getSegment (i);

            if (metadata.samplePosition >= segment.offset && metadata.samplePosition < segment.offset + segment.length)
            {
                ppq = segment.from + (metadata.samplePosition - segment.offset) * segment.perSample;   // each stretch its own rate (a ramp)

                // Played to the song heard late by the plugins' delay compensation: where it was meant.
                // Not before the loop's start once it's round in the loop (a note just after the
                // jump back is beat one, played a touch early, not the loop's end).
                const auto played = ppq;
                ppq = juce::jmax (0.0, ppq - outputLatencySamples.load (std::memory_order_relaxed) * segment.perSample);

                if (double loopFrom = 0.0, loopTo = 0.0; transport.getLoopNow (loopFrom, loopTo) && played >= loopFrom && ppq < loopFrom)
                    ppq = loopFrom;

                break;
            }
        }

        // A full queue drops the note rather than wait: the audio thread never blocks.
        const auto write = recordQueue.write (1);

        if (write.blockSize1 > 0)
            recordedEvents[static_cast<size_t> (write.startIndex1)] = { ppq, metadata.data[0], metadata.data[1],
                                                                         kind == 0xd0 ? juce::uint8 (0) : metadata.data[2] };
    }
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message)
{
    // Into its device's own queue, so only the tracks that chose that device hear it.
    int slot = -1;
    bool surface = false, linked = false;
    juce::uint32 linkKey = 0;

    if (source != nullptr)
    {
        const auto identifier = source->getIdentifier();
        const juce::SpinLock::ScopedLockType lock (deviceLock);
        slot = deviceSlotIds.indexOf (identifier);
        surface = slot >= 0 && surfaceSlots[static_cast<size_t> (slot)];
        linkKey = slot >= 0 && ! surface ? linkKeyOf (slot, message) : 0u;
        linked = linkKey != 0 && std::binary_search (linkedKeys.begin(), linkedKeys.end(), linkKey);
    }

    // A control surface's buttons and faders are for the surfaces, never notes for a track.
    if (surface)
    {
        const juce::ScopedLock lock (surfaceQueueLock);

        if (surfaceQueue.size() < maxSurfaceMessages)
            surfaceQueue.emplace_back (slot, message);

        return;
    }

    // MIDI Link sees the knobs, sliders, pads and wheels; a linked one plays Sonify, not a track.
    if (linkKey != 0)
    {
        {
            const juce::ScopedLock lock (linkQueueLock);

            if (linkQueue.size() < maxLinkMessages)
                linkQueue.emplace_back (slot, message);
        }

        if (linked)
            return;
    }

    if (message.isNoteOn())
        hardwareNoteOnCount.fetch_add (1, std::memory_order_relaxed);

    if (monitorOn.load (std::memory_order_relaxed))
        monitor (slot, message);

    if (slot >= 0)
        deviceCollectors[static_cast<size_t> (slot)]->addMessageToQueue (message);
    else
        midiCollector.addMessageToQueue (message);
}

void AudioEngine::assignMidiDeviceSlots()
{
    const auto devices = juce::MidiInput::getAvailableDevices();
    const juce::SpinLock::ScopedLockType lock (deviceLock);

    // Slots are kept: a device keeps its queue for as long as the Hub runs.
    for (const auto& device : devices)
    {
        if (deviceSlotIds.contains (device.identifier) || deviceSlotIds.size() >= maxMidiDevices)
            continue;

        deviceSlotIds.add (device.identifier);
        deviceSlotNames.add (device.name);
    }

    deviceSlotCount.store (deviceSlotIds.size());
    markSurfaceSlots();   // a surface's port may have just come
    markLinkedKeys();     // and a linked device's
    markMidiInSlots();    // and a port a MIDI In picked
}

//==============================================================================
void AudioEngine::setMidiInInputs (MidiIn which, const juce::StringArray& ports)
{
    auto list = ports;
    list.trim();
    list.removeEmptyStrings();
    list.removeDuplicates (false);

    midiIns[static_cast<size_t> (which)].inputs = list;

    const juce::SpinLock::ScopedLockType lock (deviceLock);
    markMidiInSlots();
}

juce::StringArray AudioEngine::getMidiInInputs (MidiIn which) const
{
    return midiIns[static_cast<size_t> (which)].inputs;
}

void AudioEngine::markMidiInSlots()
{
    for (auto& in : midiIns)
    {
        const auto every = in.inputs.contains (everyMidiInput);
        juce::uint32 mask = every ? 0xffffffffu : 0u;

        if (! every)
            for (const auto& port : in.inputs)
                if (const auto slot = deviceSlotNames.indexOf (port); juce::isPositiveAndBelow (slot, maxMidiDevices))
                    mask |= 1u << slot;

        in.mask.store (mask);
        in.every.store (every);
    }
}

std::vector<AudioEngine::MidiInPort> AudioEngine::getMidiInPorts() const
{
    std::vector<MidiInPort> ports;

    for (const auto& name : enabledMidiInputNames)
    {
        if (surfacePorts.contains (name))
            continue;

        MidiInPort port;
        port.name = name;

        for (const auto& device : midiDevices)
        {
            if (device.input == name)
            {
                port.device = device.name;
                break;
            }
        }

        ports.push_back (port);
    }

    return ports;
}

bool AudioEngine::frontHears (int slot) const noexcept
{
    const auto hears = [slot] (const MidiInState& in) noexcept
    {
        if (slot == -2)
            return true;   // the page's keyboards

        if (! juce::isPositiveAndBelow (slot, maxMidiDevices))
            return in.every.load (std::memory_order_relaxed);

        return ((in.mask.load (std::memory_order_relaxed) >> slot) & 1u) != 0;
    };

    switch (static_cast<MidiFocus> (midiFocus.load (std::memory_order_relaxed)))
    {
        case MidiFocus::song:
            return true;

        case MidiFocus::creator:
            return ! creatorListening.load (std::memory_order_relaxed) || hears (midiIns[static_cast<size_t> (MidiIn::creator)]);

        case MidiFocus::sampling:
            return rackMidiInOpen.load (std::memory_order_relaxed) && ! samplingOn.load (std::memory_order_relaxed)
                && hears (midiIns[static_cast<size_t> (MidiIn::rack)]);

        case MidiFocus::none:
            break;
    }

    return false;
}

void AudioEngine::monitor (int slot, const juce::MidiMessage& message) noexcept
{
    const auto* data = message.getRawData();
    const auto size = message.getRawDataSize();

    // The clock and active sensing would drown everything else.
    if (size <= 0 || data[0] == 0xf8 || data[0] == 0xfe)
        return;

    RawMonitored item;
    item.time = juce::Time::getMillisecondCounterHiRes() * 0.001;
    item.slot = slot;
    item.size = juce::jmin (3, size);
    item.sysexLength = data[0] == 0xf0 ? size : 0;
    std::memcpy (item.bytes, data, static_cast<size_t> (item.size));
    item.heard = frontHears (slot);

    const juce::SpinLock::ScopedLockType lock (monitorLock);
    monitorRing[static_cast<size_t> ((monitorStart + monitorCount) % monitorCapacity)] = item;

    // Full: the oldest makes room.
    if (monitorCount < monitorCapacity)
        ++monitorCount;
    else
        monitorStart = (monitorStart + 1) % monitorCapacity;
}

void AudioEngine::takeMonitored (std::vector<MonitoredMessage>& into)
{
    std::vector<RawMonitored> taken;
    taken.reserve (static_cast<size_t> (monitorCapacity));

    {
        const juce::SpinLock::ScopedLockType lock (monitorLock);

        for (int i = 0; i < monitorCount; ++i)
            taken.push_back (monitorRing[static_cast<size_t> ((monitorStart + i) % monitorCapacity)]);

        monitorStart = monitorCount = 0;
    }

    if (taken.empty())
        return;

    juce::StringArray names;

    {
        const juce::SpinLock::ScopedLockType lock (deviceLock);
        names = deviceSlotNames;
    }

    for (const auto& raw : taken)
    {
        MonitoredMessage message;
        message.time = raw.time;
        message.source = raw.slot == -2 ? juce::String ("Keyboard")
                       : juce::isPositiveAndBelow (raw.slot, names.size()) ? names[raw.slot] : juce::String ("MIDI");
        std::memcpy (message.bytes, raw.bytes, sizeof (message.bytes));
        message.size = raw.size;
        message.sysexLength = raw.sysexLength;
        message.heard = raw.heard;
        into.push_back (message);
    }
}

namespace
{
    /** The notes and sustain pedals a MIDI In holds down, as a block's events leave them. */
    template <typename State>
    void watchHeld (State& in, const juce::MidiBuffer& buffer) noexcept
    {
        for (const auto metadata : buffer)
        {
            if (metadata.numBytes < 3)
                continue;

            const auto type = metadata.data[0] & 0xf0;
            const auto channel = static_cast<size_t> (metadata.data[0] & 0x0f);
            const auto number = metadata.data[1] & 0x7f;
            auto& word = in.held[channel][static_cast<size_t> (number >> 6)];
            const auto bit = juce::uint64 (1) << (number & 63);

            if (type == 0x90 && metadata.data[2] > 0)
                word |= bit;
            else if (type == 0x80 || type == 0x90)
                word &= ~bit;
            else if (type == 0xb0 && number == 64)
                in.pedals = static_cast<juce::uint16> (metadata.data[2] >= 64 ? (in.pedals | (1u << channel)) : (in.pedals & ~(1u << channel)));
            else if (type == 0xb0 && (number == 120 || number == 123))
                in.held[channel] = {};
        }

        in.holding = in.pedals != 0;

        for (const auto& words : in.held)
            in.holding = in.holding || words[0] != 0 || words[1] != 0;
    }

    /** Each note held lets go, each pedal comes up: at the block's start. */
    template <typename State>
    void letGoHeld (State& in, juce::MidiBuffer& into) noexcept
    {
        for (int channel = 0; channel < 16; ++channel)
        {
            auto& words = in.held[static_cast<size_t> (channel)];

            for (int half = 0; half < 2; ++half)
            {
                for (auto word = words[static_cast<size_t> (half)]; word != 0; word &= word - 1)
                {
                    int bit = 0;

                    while (((word >> bit) & 1u) == 0)
                        ++bit;

                    const juce::uint8 off[3] { static_cast<juce::uint8> (0x80 | channel), static_cast<juce::uint8> (half * 64 + bit), 0 };
                    into.addEvent (off, 3, 0);
                }
            }

            words = {};

            if ((in.pedals & (1u << channel)) != 0)
            {
                const juce::uint8 up[3] { static_cast<juce::uint8> (0xb0 | channel), 64, 0 };
                into.addEvent (up, 3, 0);
            }
        }

        in.pedals = 0;
        in.holding = false;
        in.letGoPending = false;
    }
}

void AudioEngine::gatherMidiIn (MidiInState& in, juce::MidiBuffer& into, bool takes, int deviceSlots, int numSamples) noexcept
{
    const auto mask = in.mask.load (std::memory_order_relaxed);
    const auto every = in.every.load (std::memory_order_relaxed);

    // Not listening any more, a port taken away, or back after blocks it didn't play: what it
    // held lets go, first.
    if (in.holding && (! takes || in.letGoPending || (in.maskHeard & ~mask) != 0 || (in.everyHeard && ! every)))
        letGoHeld (in, into);

    in.maskHeard = mask;
    in.everyHeard = every;

    if (! takes)
        return;

    // The page's keyboards always; then the ports picked.
    bool heard = ! pageBuffer.isEmpty();
    into.addEvents (pageBuffer, 0, numSamples, 0);

    if (every && ! midiBuffer.isEmpty())
    {
        into.addEvents (midiBuffer, 0, numSamples, 0);
        heard = true;
    }

    for (int slot = 0; slot < deviceSlots; ++slot)
    {
        const auto& buffer = deviceBuffers[static_cast<size_t> (slot)];

        if (((mask >> slot) & 1u) != 0 && ! buffer.isEmpty())
        {
            into.addEvents (buffer, 0, numSamples, 0);
            heard = true;
        }
    }

    if (heard)
    {
        watchHeld (in, into);
        in.activity.fetch_add (1, std::memory_order_relaxed);
    }
}

void AudioEngine::markSurfaceSlots()
{
    for (int slot = 0; slot < maxMidiDevices; ++slot)
        surfaceSlots[static_cast<size_t> (slot)] = slot < deviceSlotNames.size() && surfacePorts.contains (deviceSlotNames[slot]);
}

void AudioEngine::setSurfacePorts (const juce::StringArray& portNames)
{
    if (portNames == surfacePorts)
        return;

    surfacePorts = portNames;

    {
        const juce::SpinLock::ScopedLockType lock (deviceLock);
        markSurfaceSlots();
    }

    // Tracks no longer list them (setMidiDevices, which follows, refreshes the rest).
    refreshMidiSources();
}

juce::uint32 AudioEngine::linkKeyOf (int slot, int kind, int channel, int number) noexcept
{
    if (! juce::isPositiveAndBelow (slot, maxMidiDevices) || ! juce::isPositiveAndBelow (kind, 3)
        || channel < 1 || channel > 16 || ! juce::isPositiveAndBelow (number, 128))
        return 0;

    return 0x80000000u | (static_cast<juce::uint32> (slot) << 20) | (static_cast<juce::uint32> (kind) << 16)
         | (static_cast<juce::uint32> (channel - 1) << 8) | static_cast<juce::uint32> (number);
}

juce::uint32 AudioEngine::linkKeyOf (int slot, const juce::MidiMessage& message) noexcept
{
    if (message.isController())
        return linkKeyOf (slot, 0, message.getChannel(), message.getControllerNumber());

    if (message.isNoteOnOrOff())
        return linkKeyOf (slot, 1, message.getChannel(), message.getNoteNumber());

    if (message.isPitchWheel())
        return linkKeyOf (slot, 2, message.getChannel(), 0);

    return 0;
}

void AudioEngine::markLinkedKeys()
{
    std::vector<juce::uint32> keys;

    for (const auto& source : linkedSources)
        for (int slot = 0; slot < deviceSlotNames.size(); ++slot)
            if (deviceSlotNames[slot] == source.port)
                if (const auto key = linkKeyOf (slot, source.kind, source.channel, source.number); key != 0)
                    keys.push_back (key);

    std::sort (keys.begin(), keys.end());
    linkedKeys.swap (keys);
}

void AudioEngine::setLinkedSources (std::vector<LinkSource> sources)
{
    linkedSources = std::move (sources);
    const juce::SpinLock::ScopedLockType lock (deviceLock);
    markLinkedKeys();
}

void AudioEngine::takeLinkMessages (std::vector<std::pair<juce::String, juce::MidiMessage>>& into)
{
    std::vector<std::pair<int, juce::MidiMessage>> taken;

    {
        const juce::ScopedLock lock (linkQueueLock);

        if (linkQueue.empty())
            return;

        taken = linkQueue;
        linkQueue.clear();   // keeps its room, so the MIDI thread rarely allocates
    }

    const juce::SpinLock::ScopedLockType lock (deviceLock);

    for (auto& [slot, message] : taken)
        if (juce::isPositiveAndBelow (slot, deviceSlotNames.size()))
            into.emplace_back (deviceSlotNames[slot], std::move (message));
}

void AudioEngine::takeSurfaceMessages (std::vector<std::pair<juce::String, juce::MidiMessage>>& into)
{
    std::vector<std::pair<int, juce::MidiMessage>> taken;

    {
        const juce::ScopedLock lock (surfaceQueueLock);

        if (surfaceQueue.empty())
            return;

        taken = surfaceQueue;
        surfaceQueue.clear();   // keeps its room, so the MIDI thread rarely allocates
    }

    const juce::SpinLock::ScopedLockType lock (deviceLock);

    for (auto& [slot, message] : taken)
        if (juce::isPositiveAndBelow (slot, deviceSlotNames.size()))
            into.emplace_back (deviceSlotNames[slot], std::move (message));
}

void AudioEngine::resolveMidiInputs (Track& track)
{
    std::vector<Track::MidiRoute> routes;
    std::vector<int> sources;
    int mpeRange = 0;

    {
        const juce::SpinLock::ScopedLockType lock (deviceLock);

        for (const auto& input : track.midiInputs)
        {
            if (input.startsWith ("track:"))
                continue;

            // A device ("Keys"), one channel of a split device ("Keys|3"), or a port no device uses.
            const auto bar = input.lastIndexOfChar ('|');
            const auto name = bar >= 0 ? input.substring (0, bar) : input;
            const auto channel = bar >= 0 ? input.substring (bar + 1).getIntValue() : 0;
            Track::MidiRoute route;
            int routeMpe = 0;

            if (const auto* device = findMidiDevice (name))
            {
                route.slot = device->input.isNotEmpty() ? deviceSlotNames.indexOf (device->input) : -1;
                route.channels = device->channels;
                route.blocked = device->blocked;
                routeMpe = device->mpe ? device->pitchRange : 0;

                if (channel >= 1 && channel <= 16)
                    route.channels = static_cast<juce::uint16> (route.channels & (1u << (channel - 1)));
            }
            else
            {
                route.slot = deviceSlotNames.indexOf (name);
            }

            if (route.slot < 0 || route.channels == 0)
                continue;

            mpeRange = juce::jmax (mpeRange, routeMpe);

            // Two choices on one port (a device and one of its channels) are heard once.
            const auto same = std::find_if (routes.begin(), routes.end(), [&route] (const Track::MidiRoute& r) { return r.slot == route.slot; });

            if (same != routes.end())
            {
                same->channels = static_cast<juce::uint16> (same->channels | route.channels);
                same->blocked = static_cast<juce::uint8> (same->blocked & route.blocked);
            }
            else
            {
                routes.push_back (route);
            }
        }
    }

    for (const auto& input : track.midiInputs)
        if (input.startsWith ("track:"))
            if (const auto id = input.substring (6).getIntValue(); id > 0 && id != track.id)
                sources.push_back (id);

    // A newly chosen MPE device, or a new pitch range: the instrument hears of it in the next block.
    if (track.mpeRange.exchange (mpeRange) != mpeRange && mpeRange > 0)
        track.mpeResend.store (true);

    const juce::SpinLock::ScopedLockType lock (graphLock);
    std::swap (track.midiRoutes, routes);
    std::swap (track.midiSourceTracks, sources);
}

//==============================================================================
const AudioEngine::MidiDevice* AudioEngine::findMidiDevice (const juce::String& name) const noexcept
{
    for (const auto& device : midiDevices)
        if (device.name == name)
            return &device;

    return nullptr;
}

void AudioEngine::setMidiDevices (std::vector<MidiDevice> devices)
{
    midiDevices = std::move (devices);
    refreshMidiSources();

    for (const auto& track : tracks)
        resolveMidiInputs (*track);
}

void AudioEngine::renameMidiSource (const juce::String& from, const juce::String& to)
{
    if (from.isEmpty() || to.isEmpty() || from == to)
        return;

    for (const auto& track : tracks)
    {
        auto changed = false;

        for (auto& input : track->midiInputs)
        {
            if (input == from)
            {
                input = to;
                changed = true;
            }
            else if (input.startsWith (from + "|"))
            {
                input = to + input.substring (from.length());
                changed = true;
            }
        }

        if (changed)
            resolveMidiInputs (*track);
    }
}

void AudioEngine::refreshMidiSources()
{
    juce::StringArray sources, portsUsed;

    // Devices whose port is here and enabled, each channel too when split.
    for (const auto& device : midiDevices)
    {
        if (device.input.isEmpty())
            continue;

        portsUsed.add (device.input);

        if (! enabledMidiInputNames.contains (device.input))
            continue;

        sources.add (device.name);

        if (device.split)
            for (int channel = 1; channel <= 16; ++channel)
                if ((device.channels & (1u << (channel - 1))) != 0)
                    sources.add (device.name + "|" + juce::String (channel));
    }

    // Then the ports no device uses, by their own names, as before devices existed (a control
    // surface's port plays no track).
    for (const auto& port : enabledMidiInputNames)
        if (! portsUsed.contains (port) && ! surfacePorts.contains (port))
            sources.addIfNotAlreadyThere (port);

    midiSources = sources;
}

juce::String AudioEngine::defaultMidiSource() const
{
    for (const auto& device : midiDevices)
        if (device.defaultInput)
            return device.name;

    // The first in the track's MIDI in list: a keyboard before a controller or a virtual cable.
    if (preferredMidiSource.isNotEmpty() && midiSources.contains (preferredMidiSource))
        return preferredMidiSource;

    if (! midiSources.isEmpty())
        return midiSources[0];

    const juce::SpinLock::ScopedLockType lock (deviceLock);

    for (const auto& name : deviceSlotNames)
        if (! surfacePorts.contains (name))
            return name;

    return {};
}

void AudioEngine::releaseDevice (bool shouldRelease)
{
    if (shouldRelease == deviceReleased)
        return;

    if (shouldRelease)
    {
        // Saved as it is first: closing it mustn't change what's remembered.
        saveDeviceState();
        deviceReleased = true;
        deviceManager.closeAudioDevice();
    }
    else
    {
        deviceReleased = false;
        deviceManager.restartLastAudioDevice();
    }
}

bool AudioEngine::reopenDevice (const juce::AudioDeviceManager::AudioDeviceSetup& setup)
{
    if (deviceReleased || (setup.outputDeviceName.isEmpty() && setup.inputDeviceName.isEmpty()))
        return false;

    // Closed, then opened with that setup (not JUCE's own last one: it forgets the device's name
    // once an open fails). Not the user's choice anew: the setup kept for next time stays as it is.
    deviceManager.closeAudioDevice();

    if (deviceManager.setAudioDeviceSetup (setup, false).isNotEmpty())
        return false;

    auto* device = deviceManager.getCurrentAudioDevice();
    return device != nullptr && device->isPlaying();
}

void AudioEngine::setTrackMidiInputs (int trackId, const juce::StringArray& sources)
{
    auto* track = findTrack (trackId);

    if (track == nullptr || track->kind != Track::Kind::instrument)
        return;

    auto list = sources;
    list.removeDuplicates (false);
    list.removeEmptyStrings();

    // The tracks whose plugins play notes into it (its devices are live: never in a render).
    const auto players = [] (const juce::StringArray& names)
    {
        juce::StringArray found;

        for (const auto& name : names)
            if (name.startsWith ("track:"))
                found.add (name);

        found.sort (false);
        return found;
    };

    const bool othersChanged = players (track->midiInputs) != players (list);
    track->midiInputs = list;
    resolveMidiInputs (*track);

    // Other notes change what it plays: frozen (or freezing), it plays itself again.
    if (othersChanged)
        contentChanged (*track);

    // A frozen track it now takes notes from plays itself again: its instrument makes them.
    for (const auto& source : list)
        if (source.startsWith ("track:"))
            if (auto* player = findTrack (source.substring (6).getIntValue()); player != nullptr && player != track)
                thaw (*player, true);
}

//==============================================================================
void AudioEngine::trackNotes (const juce::MidiBuffer& buffer) noexcept
{
    // Reads raw bytes so nothing is allocated on the audio thread.
    for (const auto metadata : buffer)
    {
        if (metadata.numBytes < 3)
            continue;

        const auto statusByte = metadata.data[0] & 0xf0;
        const auto dataOne = metadata.data[1] & 0x7f;
        const auto dataTwo = metadata.data[2] & 0x7f;

        if (statusByte == 0x90 && dataTwo > 0)
        {
            setNoteBit (dataOne, true);
        }
        else if (statusByte == 0x80 || statusByte == 0x90)
        {
            setNoteBit (dataOne, false);
        }
        else if (statusByte == 0xb0 && (dataOne == 120 || dataOne == 123))
        {
            activeNotesLow.store (0, std::memory_order_relaxed);
            activeNotesHigh.store (0, std::memory_order_relaxed);
        }
    }
}

void AudioEngine::setNoteBit (int note, bool isOn) noexcept
{
    auto& word = note < 64 ? activeNotesLow : activeNotesHigh;
    const auto mask = juce::uint64 { 1 } << (note & 63);

    if (isOn)
        word.fetch_or (mask, std::memory_order_relaxed);
    else
        word.fetch_and (~mask, std::memory_order_relaxed);
}

//==============================================================================
// Clip effects
std::vector<std::shared_ptr<Track::ClipEffectSlot>> AudioEngine::buildClipEffects (Track& track)
{
    std::vector<std::shared_ptr<Track::ClipEffectSlot>> list;

    for (const auto& clip : track.clips)
    {
        if (! clip.isAudio() || clip.effect == nullptr)
            continue;

        const auto& effect = *clip.effect;
        clipEffectsKnown[effect.uid] = clip.effect;   // the latest of it, for a paste

        // Still there: it plays on as it is.
        std::shared_ptr<Track::ClipEffectSlot> kept;

        for (const auto& slot : track.clipEffects)
            if (slot != nullptr && slot->clipId == clip.id && slot->uid == effect.uid)
                kept = slot;

        if (kept != nullptr)
        {
            list.push_back (std::move (kept));
            continue;
        }

        // New here (added, pasted, split, moved, an undo): its plugin, at the state the effect is
        // in now (another clip's plugin of the same effect, or the last one that went, or the clip's).
        auto slot = std::make_shared<Track::ClipEffectSlot>();
        slot->clipId = clip.id;
        slot->uid = effect.uid;
        slot->slotId = nextInsertId++;

        juce::MemoryBlock state (effect.state);

        if (auto now = clipEffectNow (effect.uid); now != nullptr)
            state = now->state;

        const juce::File file (Builtins::isBuiltinId (effect.pluginId) ? Builtins::file().getFullPathName() : effect.file);
        const auto name = effect.name.isNotEmpty() ? effect.name : juce::String ("The clip's effect");
        juce::String error;
        std::unique_ptr<Plugin> plugin;

        if (auto loaded = clipEffectsLoaded.find (effect.uid); loaded != clipEffectsLoaded.end())
        {
            plugin = std::move (loaded->second);   // just chosen (setClipEffect): read and started already
            clipEffectsLoaded.erase (loaded);
        }
        else if (! juce::File::isAbsolutePath (file.getFullPathName()) || ! file.exists())
            error = name + " isn't on this computer: " + file.getFullPathName();
        else if ((plugin = Plugin::load (file, effect.pluginId, error)) == nullptr)
            error = error.isNotEmpty() ? error : name + " couldn't be loaded.";
        else if (plugin->getRole() != Plugin::Role::effect)
            error = name + " is an instrument: a clip takes an effect.";
        else if (! activatePlugin (*plugin))
            error = name + " couldn't start at " + juce::String (currentSampleRate / 1000.0, 1) + " kHz.";

        if (error.isNotEmpty())
            plugin.reset();
        else if (state.getSize() > 0 && plugin->canSaveState())
            plugin->loadState (state);

        slot->plugin = std::move (plugin);
        slot->error = error;
        slot->sound.setSize (2, juce::jmax (track.getBlockCapacity(), currentBlockSize * 2), false, true, false);
        list.push_back (std::move (slot));
    }

    return list;
}

bool AudioEngine::captureClipEffect (Track& track, Track::ClipEffectSlot& slot)
{
    if (slot.plugin == nullptr || ! slot.plugin->canSaveState())
        return false;

    for (auto& clip : track.clips)
    {
        if (clip.id != slot.clipId || clip.effect == nullptr || clip.effect->uid != slot.uid)
            continue;

        juce::MemoryBlock state;

        if (! slot.plugin->saveState (state) || state == clip.effect->state)
            return false;

        auto now = std::make_shared<ClipEffect> (*clip.effect);
        now->state = std::move (state);
        clip.effect = now;
        clipEffectsKnown[now->uid] = clip.effect;
        return true;
    }

    return false;
}

void AudioEngine::captureClipEffects()
{
    for (const auto& track : tracks)
        for (const auto& effect : track->clipEffects)
            if (effect != nullptr)
                captureClipEffect (*track, *effect);
}

std::shared_ptr<const ClipEffect> AudioEngine::clipEffectNow (int uid)
{
    const auto known = clipEffectsKnown.find (uid);
    auto latest = known != clipEffectsKnown.end() ? known->second.lock() : nullptr;

    // A plugin of it playing now: its state as it is.
    for (const auto& track : tracks)
    {
        for (const auto& effect : track->clipEffects)
        {
            if (effect == nullptr || effect->uid != uid || effect->plugin == nullptr || ! effect->plugin->canSaveState())
                continue;

            juce::MemoryBlock state;

            if (latest != nullptr && effect->plugin->saveState (state))
            {
                auto now = std::make_shared<ClipEffect> (*latest);
                now->state = std::move (state);
                return now;
            }
        }
    }

    // One that went: the state it had.
    if (latest != nullptr)
    {
        if (const auto left = clipEffectStatesLeft.find (uid); left != clipEffectStatesLeft.end())
        {
            auto now = std::make_shared<ClipEffect> (*latest);
            now->state = left->second;
            return now;
        }
    }

    return latest;
}

juce::Result AudioEngine::setClipEffect (int trackId, int clipId, const juce::File& file, const juce::String& pluginId)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return juce::Result::fail ("That track no longer exists.");

    if (track->isFrozen())
        return juce::Result::fail ("\"" + track->name + "\" is frozen: its snowflake unfreezes it first.");

    for (auto& clip : track->clips)
    {
        if (clip.id != clipId)
            continue;

        if (! clip.isAudio())
            return juce::Result::fail ("Only an audio clip takes an effect of its own.");

        // Read once here, to say at once when it won't do (the clip's own plugin is made with the playlist).
        juce::String error;
        auto plugin = Plugin::load (file, pluginId, error);

        if (plugin == nullptr)
            return juce::Result::fail (error.isNotEmpty() ? error : "That plugin couldn't be loaded.");

        if (plugin->getRole() != Plugin::Role::effect)
            return juce::Result::fail (plugin->getDescriptor().name + " is an instrument: a clip takes an effect.");

        auto effect = std::make_shared<ClipEffect>();
        effect->file = plugin->getSourceFile().getFullPathName();
        effect->pluginId = plugin->getDescriptor().id;
        effect->name = plugin->getDescriptor().name;
        effect->format = plugin->getFormatName();
        effect->uid = ClipEffect::newUid();

        if (! activatePlugin (*plugin))
            return juce::Result::fail (effect->name + " couldn't start at " + juce::String (currentSampleRate / 1000.0, 1) + " kHz.");

        clipEffectsLoaded[effect->uid] = std::move (plugin);   // its slot takes it (rebuildSequence, now)

        clip.effect = std::move (effect);
        rebuildSequence (*track, false);
        clipEffectsLoaded.clear();   // (taken)
        contentChanged (*track);
        return juce::Result::ok();
    }

    return juce::Result::fail ("That clip no longer exists.");
}

bool AudioEngine::removeClipEffect (int trackId, int clipId)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return false;

    for (auto& clip : track->clips)
    {
        if (clip.id != clipId || clip.effect == nullptr)
            continue;

        clip.effect.reset();
        rebuildSequence (*track, false);
        contentChanged (*track);
        return true;
    }

    return false;
}

int AudioEngine::clipEffectSlot (int trackId, int clipId, juce::String& error) const
{
    error.clear();

    if (const auto* track = findTrack (trackId))
    {
        for (const auto& effect : track->clipEffects)
        {
            if (effect != nullptr && effect->clipId == clipId)
            {
                error = effect->error;
                return effect->slotId;
            }
        }
    }

    return 0;
}

void AudioEngine::updateClipEffects (juce::String& message, PluginChange& change)
{
    for (const auto& track : tracks)
    {
        for (const auto& effect : track->clipEffects)
        {
            auto* plugin = effect != nullptr ? effect->plugin.get() : nullptr;

            if (plugin == nullptr)
                continue;

            const auto requests = plugin->takeRequests();
            const auto name = plugin->getDescriptor().name;
            juce::String bridgeError;

            // A crash inside it, or it couldn't start: the clip plays as it is, and says why.
            if ((requests.processError && plugin->getError (bridgeError)) || (audioRunning.load() && ! plugin->isActive()))
            {
                const auto reason = bridgeError.isNotEmpty() ? name + " stopped: " + bridgeError
                                                             : name + " couldn't start at " + juce::String (currentSampleRate / 1000.0, 1) + " kHz.";

                if (effect->error != reason)
                {
                    effect->error = reason;
                    message = reason + " (a clip's effect: its clip plays as it is)";
                    change = PluginChange::failed;
                }

                continue;
            }

            if (requests.callback)
                plugin->runOnMainThread();

            if (requests.restart && plugin->isActive())
            {
                // Out of the sound for a moment (the clip plays as it is), started again, back.
                {
                    const juce::SpinLock::ScopedLockType lock (graphLock);
                    effect->away.store (true);
                }

                plugin->deactivate();
                plugin->activate (currentSampleRate, currentBlockSize);

                if (requests.rescan)
                    rescanWithoutWaiting (*plugin);

                const juce::SpinLock::ScopedLockType lock (graphLock);
                effect->away.store (false);
            }
            else if (requests.rescan)
            {
                rescanWithoutWaiting (*plugin);
            }

            if ((requests.rescan || requests.restart) && change == PluginChange::none)
                change = PluginChange::changed;

            if (requests.flush && ! audioRunning.load())
            {
                const juce::SpinLock::ScopedLockType lock (graphLock);
                plugin->flushQueuedParameters();
            }
        }
    }
}

//==============================================================================
// Silence Areas
std::shared_ptr<const AudioEngine::SilenceRegions> AudioEngine::silenceRegions (const std::shared_ptr<const AudioData>& data,
                                                                                const MidiClip::Silence& silence)
{
    auto regions = std::make_shared<SilenceRegions>();

    if (data == nullptr || data->playbackRate <= 0.0 || data->samples.getNumSamples() == 0)
        return regions;

    // Files gone from memory take their levels with them.
    for (auto it = silenceLevels.begin(); it != silenceLevels.end();)
        it = it->second.data.expired() ? silenceLevels.erase (it) : std::next (it);

    auto& levels = silenceLevels[data.get()];

    if (levels.data.expired() || levels.peaks.empty())
    {
        levels.data = data;
        levels.window = juce::jmax (1, static_cast<int> (std::lround (data->playbackRate * 0.01)));
        levels.peaks.clear();

        const auto total = data->samples.getNumSamples();

        for (int from = 0; from < total; from += levels.window)
        {
            const auto count = juce::jmin (levels.window, total - from);
            float peak = 0.0f;

            for (int c = 0; c < data->samples.getNumChannels(); ++c)
                peak = juce::jmax (peak, data->samples.getMagnitude (c, from, count));

            levels.peaks.push_back (peak);
        }
    }

    // Runs of windows quieter than the threshold, at least the shortest silence long.
    const auto threshold = juce::Decibels::decibelsToGain (static_cast<float> (juce::jlimit (-120.0, 0.0, silence.thresholdDb)), -200.0f);
    const auto shortest = juce::jmax<size_t> (1, static_cast<size_t> (std::ceil (juce::jmax (0.01, silence.shortest) / 0.01)));
    const auto total = static_cast<juce::int64> (data->samples.getNumSamples());
    const auto& peaks = levels.peaks;

    for (size_t i = 0; i < peaks.size();)
    {
        if (peaks[i] >= threshold)
        {
            ++i;
            continue;
        }

        auto end = i;

        while (end < peaks.size() && peaks[end] < threshold)
            ++end;

        if (end - i >= shortest)
            regions->push_back ({ static_cast<juce::int64> (i) * levels.window,
                                  juce::jmin (total, static_cast<juce::int64> (end) * levels.window) });

        i = end;
    }

    return regions;
}

std::vector<std::pair<double, double>> AudioEngine::silenceSeconds (const juce::String& file, const MidiClip::Silence& silence,
                                                                    double from, double to, double& silencedSeconds)
{
    std::vector<std::pair<double, double>> seconds;
    silencedSeconds = 0.0;
    juce::String error;
    const auto data = media.get (juce::File (file), getPlaybackRate(), error);

    if (data == nullptr)
        return seconds;

    const auto regions = silenceRegions (data, silence);
    const auto rate = data->playbackRate;

    for (const auto& [start, end] : *regions)
    {
        const auto a = juce::jmax (from, static_cast<double> (start) / rate);
        const auto b = juce::jmin (to, static_cast<double> (end) / rate);

        if (b > a)
        {
            seconds.push_back ({ a, b });
            silencedSeconds += b - a;
        }
    }

    return seconds;
}

bool AudioEngine::setClipEnvelope (int trackId, int clipId, const juce::String& key, std::vector<ClipEnvelopePoint> points)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return false;

    for (auto& clip : track->clips)
    {
        if (clip.id != clipId || ! ClipEnvelope::isKey (key, clip.isAudio()))
            continue;

        auto lines = clip.envelopes;
        lines.erase (std::remove_if (lines.begin(), lines.end(), [&key] (const ClipEnvelope& line) { return line.key == key; }), lines.end());

        if (! points.empty())
            lines.push_back ({ key, std::move (points) });

        ClipEnvelope::tidy (lines, clip.isAudio());

        if (lines == clip.envelopes)
            return false;

        clip.envelopes = std::move (lines);
        rebuildSequence (*track, false);
        contentChanged (*track);
        return true;
    }

    return false;
}

bool AudioEngine::setClipSilence (int trackId, int clipId, const MidiClip::Silence& silence)
{
    auto* track = findTrack (trackId);

    if (track == nullptr)
        return false;

    for (auto& clip : track->clips)
    {
        if (clip.id != clipId || ! clip.isAudio() || clip.silence == silence)
            continue;

        clip.silence = silence;
        rebuildSequence (*track, false);
        contentChanged (*track);
        return true;
    }

    return false;
}
