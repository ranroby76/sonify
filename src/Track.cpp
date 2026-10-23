// D:\workspace\Fanan_web_hub\src\Track.cpp

#include "Track.h"

#include <algorithm>
#include <cmath>

namespace
{
    //==============================================================================
    // Test instrument. Fast attack and a bright start, so latency is easy to judge by ear.
    struct TestSound final : public juce::SynthesiserSound
    {
        bool appliesToNote (int) override      { return true; }
        bool appliesToChannel (int) override   { return true; }
    };

    class TestVoice final : public juce::SynthesiserVoice
    {
    public:
        bool canPlaySound (juce::SynthesiserSound* sound) override
        {
            return dynamic_cast<TestSound*> (sound) != nullptr;
        }

        void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
        {
            const auto sampleRate = getSampleRate();

            phase = 0.0;
            phaseDelta = juce::MathConstants<double>::twoPi
                         * juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber) / sampleRate;
            gain = 0.14f * velocity;
            brightness = 1.0;
            brightnessDecay = std::exp (-1.0 / (0.25 * sampleRate));

            envelope.setSampleRate (sampleRate);
            envelope.setParameters ({ 0.001f, 0.35f, 0.45f, 0.18f });
            envelope.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                envelope.noteOff();
            }
            else
            {
                envelope.reset();
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override
        {
            if (! isVoiceActive())
                return;

            const auto numChannels = output.getNumChannels();

            while (--numSamples >= 0)
            {
                const auto partials = std::sin (phase)
                                    + brightness * (0.5 * std::sin (2.0 * phase) + 0.25 * std::sin (3.0 * phase));

                const auto sample = static_cast<float> (partials) * gain * envelope.getNextSample();

                for (int channel = 0; channel < numChannels; ++channel)
                    output.addSample (channel, startSample, sample);

                phase += phaseDelta;

                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;

                brightness *= brightnessDecay;
                ++startSample;

                if (! envelope.isActive())
                {
                    clearCurrentNote();
                    break;
                }
            }
        }

    private:
        juce::ADSR envelope;
        double phase = 0.0;
        double phaseDelta = 0.0;
        double brightness = 1.0;
        double brightnessDecay = 1.0;
        float gain = 0.0f;
    };

    constexpr int numTestVoices = 16;
    constexpr int minimumBufferSize = 4096;

    void storeMax (std::atomic<float>& target, float value) noexcept
    {
        auto current = target.load (std::memory_order_relaxed);

        while (value > current && ! target.compare_exchange_weak (current, value, std::memory_order_relaxed))
        {
        }
    }
}

//==============================================================================
Track::Track (int trackId, juce::uint32 colourArgb, Kind trackKind)
    : id (trackId), kind (trackKind), colour (colourArgb)
{
    for (int i = 0; i < numTestVoices; ++i)
        synth.addVoice (new TestVoice());

    synth.addSound (new TestSound());

    // Built once, so releasing notes never allocates on the audio thread.
    for (int channel = 1; channel <= 16; ++channel)
    {
        notesOffMidi.addEvent (juce::MidiMessage::allSoundOff (channel), 0);
        notesOffMidi.addEvent (juce::MidiMessage::controllerEvent (channel, 64, 0), 0);   // the sustain pedal up
        notesOffMidi.addEvent (juce::MidiMessage::allNotesOff (channel), 0);
    }

    // Room for a busy block after the MIDI FX (chords, echoes), so it never allocates.
    releaseMidi.ensureSize (32768);
    blockMidi.ensureSize (32768);

    // Start at the fader position, so a new track does not fade in.
    lastGainLeft = lastGainRight = 1.0f;
}

Track::~Track()
{
    // The plugins go with the members, on the message thread: the inserts, then the instrument.
}

//==============================================================================
void Track::prepare (double sampleRate, int maxBlockSize)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    buffer.setSize (2, juce::jmax (maxBlockSize * 2, minimumBufferSize), false, true, true);
    buffer.clear();
    keyBlock.setSize (2, buffer.getNumSamples(), false, true, true);
    keyHeld = false;
    trackSends.setSize (2, buffer.getNumSamples(), false, true, true);
    trackSends.clear();
    inputSum.setSize (2, buffer.getNumSamples(), false, true, true);
    busInput.setSize (2, juce::jmax (maxBlockSize * 2, minimumBufferSize), false, true, true);
    busInput.clear();
    liveMidi.ensureSize (16384);
    inserts.prepare (maxBlockSize);
    board.prepare (sampleRate, maxBlockSize);
    live.prepare (sampleRate);

    if (choir != nullptr)
        choir->prepare (sampleRate, maxBlockSize);

    for (const auto& effect : clipEffects)
        if (effect != nullptr)
            effect->sound.setSize (2, buffer.getNumSamples(), false, true, true);
}

int Track::ownLatency (bool dry) const noexcept
{
    if (isFrozen())
        return 0;   // its render plays: made in time (the freeze leaves its own latency out)

    int total = 0;

    if (kind == Kind::instrument && instrument != nullptr && instrument->isActive())
        total += juce::jmax (0, instrument->getLatencySamples());

    if (! dry)
        total += inserts.getLatencySamples();

    return total + clipEffectsLatency();
}

int Track::clipEffectsLatency() const noexcept
{
    if (kind != Kind::audio)
        return 0;

    int slowest = 0;

    for (const auto& effect : clipEffects)
        if (effect != nullptr && effect->plugin != nullptr && effect->plugin->isActive() && ! effect->away.load (std::memory_order_relaxed))
            slowest = juce::jmax (slowest, effect->plugin->getLatencySamples());

    return juce::jlimit (0, LatencyDelay::capacity - 1, slowest);
}

void Track::clearClipDelays() noexcept
{
    if (clipsDelay != nullptr)
        clipsDelay->clear();

    for (const auto& effect : clipEffects)
        if (effect != nullptr && effect->delay != nullptr)
            effect->delay->clear();
}

Track::ClipEffectSlot* Track::findClipEffect (int slotId) const noexcept
{
    for (const auto& effect : clipEffects)
        if (effect != nullptr && effect->slotId == slotId)
            return effect.get();

    return nullptr;
}

void Track::quietInserts (const clap_event_transport_t& position) noexcept
{
    // (Frozen, they rest anyway.)
    if (renderedSamples <= 0 || inserts.size() == 0 || frozen != nullptr)
        return;

    buffer.clear (0, renderedSamples);
    releaseMidi.clear();   // no notes for them
    inserts.process (buffer.getWritePointer (0), buffer.getWritePointer (1), renderedSamples, releaseMidi, position);
}

void Track::showLevels (float left, float right) noexcept
{
    storeMax (peakLeft, left);
    storeMax (peakRight, right);
}

void Track::render (const juce::MidiBuffer& liveNotes, const float* const* inputs, int numInputs, bool recording,
                    const Transport& songTransport, const clap_event_transport_t& transport, int numSamples, bool dry,
                    bool hearInput) noexcept
{
    renderedSamples = juce::jmin (numSamples, buffer.getNumSamples());

    if (renderedSamples <= 0)
        return;

    buffer.clear (0, renderedSamples);

    // An FX channel or a bus: what the sends (or the tracks playing into the bus) brought in,
    // through its inserts.
    if (isMixChannel())
    {
        const auto samples = juce::jmin (renderedSamples, busInput.getNumSamples());
        buffer.copyFrom (0, 0, busInput, 0, 0, samples);
        buffer.copyFrom (1, 0, busInput, 1, 0, samples);
        blockMidi.clear();
        inserts.process (buffer.getWritePointer (0), buffer.getWritePointer (1), renderedSamples, blockMidi, transport);
        return;
    }

    // Frozen: its render, in place of everything that made it (which rests meanwhile). The Live
    // Show hears its channels live.
    if (frozen != nullptr && ! hearInput)
    {
        playClips (frozen.get(), songTransport, buffer.getWritePointer (0), buffer.getWritePointer (1), false);
        return;
    }

    if (kind == Kind::audio)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        playClips (playlist.get(), songTransport, left, right);

        // With a clip effect that's late (plugin delay compensation): the plain clips as late as
        // the slowest, each effect's as late as the slowest less its own.
        const auto clipWait = clipLatency.load (std::memory_order_relaxed);
        ++clipClock;

        if (clipsDelay != nullptr)
            clipsDelay->process (left, right, renderedSamples, clipWait, clipClock);

        // Clips with an effect of their own: each through its plugin, alone, then into the track.
        // The plugin plays every block, so its tail rings on after its clip.
        for (const auto& effect : clipEffects)
        {
            if (effect == nullptr)
                continue;

            // (No room for it: never with the device's blocks. The clip plays as it is.)
            if (effect->sound.getNumSamples() < renderedSamples)
            {
                playClips (playlist.get(), songTransport, left, right, true, effect->slotId);
                continue;
            }

            auto* effectLeft = effect->sound.getWritePointer (0);
            auto* effectRight = effect->sound.getWritePointer (1);
            effect->sound.clear (0, renderedSamples);
            playClips (playlist.get(), songTransport, effectLeft, effectRight, true, effect->slotId);

            if (auto* plugin = effect->plugin.get(); plugin != nullptr && plugin->isActive() && ! effect->away.load (std::memory_order_relaxed))
            {
                automateClipEffect (playlist.get(), songTransport, effect->slotId, *plugin);
                plugin->process (effectLeft, effectRight, effectLeft, effectRight, renderedSamples, noNotes, &transport);

                if (effect->delay != nullptr)
                    effect->delay->process (effectLeft, effectRight, renderedSamples,
                                            juce::jmax (0, clipWait - plugin->getLatencySamples()), clipClock);
            }
            else if (effect->delay != nullptr)
            {
                effect->delay->process (effectLeft, effectRight, renderedSamples, clipWait, clipClock);   // as it is: as late as the plain clips
            }

            juce::FloatVectorOperations::add (left, effectLeft, renderedSamples);
            juce::FloatVectorOperations::add (right, effectRight, renderedSamples);
        }

        // A choir track: the vocal at the lead's level, and its voices sung from the clips.
        if (choir != nullptr)
            choir->process (playlist.get(), songTransport, left, right, renderedSamples);

        const auto channel = [inputs, numInputs] (int index) -> const float*
        {
            return inputs != nullptr && juce::isPositiveAndBelow (index, numInputs) ? inputs[index] : nullptr;
        };

        // The input's channels: its left (a mono input's first), then its right or a mono input's others.
        std::array<const float*, maxInputChannels> heard {};
        int count = 0;

        if (const auto* first = channel (inputLeft.load (std::memory_order_relaxed)); first != nullptr)
        {
            heard[0] = first;
            count = 1;

            if (const auto rightIndex = inputRight.load (std::memory_order_relaxed); rightIndex >= 0)
            {
                if (const auto* second = channel (rightIndex); second != nullptr)
                    heard[static_cast<size_t> (count++)] = second;
            }
            else
            {
                for (const auto& also : inputAlso)
                    if (const auto* more = channel (also.load (std::memory_order_relaxed) - 1); more != nullptr && count < maxInputChannels)
                        heard[static_cast<size_t> (count++)] = more;
            }
        }

        // As the channel hears it: one channel alone on both sides; more, as a mono channel (all
        // of them together, connected to both sides) or a stereo one (each side its own: the
        // first, third... on the left, the second, fourth... on the right).
        const float* inLeft = heard[0];
        const float* inRight = heard[0];

        if (count > 1 && renderedSamples <= inputSum.getNumSamples())
        {
            if (monoMode.load (std::memory_order_relaxed))
            {
                auto* sum = inputSum.getWritePointer (0);
                juce::FloatVectorOperations::copy (sum, heard[0], renderedSamples);

                for (int i = 1; i < count; ++i)
                    juce::FloatVectorOperations::add (sum, heard[static_cast<size_t> (i)], renderedSamples);

                inLeft = inRight = sum;
            }
            else if (count == 2)
            {
                inRight = heard[1];
            }
            else
            {
                auto* sumLeft = inputSum.getWritePointer (0);
                auto* sumRight = inputSum.getWritePointer (1);
                juce::FloatVectorOperations::copy (sumLeft, heard[0], renderedSamples);
                juce::FloatVectorOperations::copy (sumRight, heard[1], renderedSamples);

                for (int i = 2; i < count; ++i)
                    juce::FloatVectorOperations::add (i % 2 == 0 ? sumLeft : sumRight, heard[static_cast<size_t> (i)], renderedSamples);

                inLeft = sumLeft;
                inRight = sumRight;
            }
        }

        if (inLeft != nullptr)
        {
            const auto peakOf = [this] (const float* data)
            {
                const auto range = juce::FloatVectorOperations::findMinAndMax (data, renderedSamples);
                return juce::jmax (-range.getStart(), range.getEnd());
            };

            const auto levelLeft = peakOf (inLeft);
            storeMax (inputPeakLeft, levelLeft);
            storeMax (inputPeakRight, inRight == inLeft ? levelLeft : peakOf (inRight));

            if (hearInput || monitor.load (std::memory_order_relaxed))
            {
                juce::FloatVectorOperations::add (left, inLeft, renderedSamples);
                juce::FloatVectorOperations::add (right, inRight, renderedSamples);
            }

            // The writer copies into its own queue; a background thread does the disk.
            if (recording && recorder != nullptr)
            {
                const float* data[] = { inLeft, inRight };
                recorder->write (data, renderedSamples);
            }
        }

        if (! dry)
            inserts.process (left, right, renderedSamples, blockMidi, transport);

        return;
    }

    // This block's notes: the clips' first, then the live ones.
    blockMidi.clear();
    sequencer.render (sequence.get(), songTransport, blockMidi);

    if (! liveNotes.isEmpty())
        blockMidi.addEvents (liveNotes, 0, -1, 0);

    const bool releaseNotes = notesOffRequested.exchange (false);

    if (releaseNotes)
        midiFx.requestRelease();

    // Through the MIDI FX: the notes as the instrument is to hear them.
    midiFx.process (blockMidi, songTransport, renderedSamples);

    // What it plays, for MIDI FX on other tracks that follow it (Harmonize); not while the
    // Sampling Studio holds it (which may play it on a thread of its own).
    if (! hearInput)
        MidiFx::board().publish (id, blockMidi);

    const auto& midi = blockMidi;

    // Releasing: silence the test synth at once (it may not be the one rendering) and send
    // any plugin instrument all-notes-off first, then this block's live notes, so a note
    // played in the same block is not lost.
    if (releaseNotes)
    {
        synth.allNotesOff (0, false);
        releaseMidi.clear();
        releaseMidi.addEvents (notesOffMidi, 0, -1, 0);
        releaseMidi.addEvents (midi, 0, -1, 0);
    }

    const auto& events = releaseNotes ? releaseMidi : midi;
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    auto* source = instrument != nullptr && instrument->isActive() ? instrument.get() : nullptr;

    if (source != nullptr && source->getRole() == Plugin::Role::instrument)
    {
        source->process (nullptr, nullptr, left, right, renderedSamples, events, &transport);
    }
    else if (! instrumentAway)
    {
        synth.renderNextBlock (buffer, events, 0, renderedSamples);

        // A note effect hears the notes; the test synth stays audible.
        if (source != nullptr)
            source->process (left, right, nullptr, nullptr, renderedSamples, events, &transport);
    }

    if (! dry)
        inserts.process (left, right, renderedSamples, events, transport);
}

void Track::keepKeyBlock (int numSamples) noexcept
{
    numSamples = juce::jmin (numSamples, renderedSamples, keyBlock.getNumSamples());
    keyHeld = numSamples > 0;

    for (int channel = 0; channel < 2 && keyHeld; ++channel)
        juce::FloatVectorOperations::copy (keyBlock.getWritePointer (channel), buffer.getReadPointer (channel), numSamples);
}

void Track::sendInto (float* left, float* right, int numSamples, float gain, float& lastGain, bool beforeCompensation) const noexcept
{
    numSamples = juce::jmin (numSamples, renderedSamples);

    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    // The fader's own ramp from this block (so a send follows fader moves and mutes without a
    // click), times the send's level, ramped too.
    const auto count = static_cast<float> (numSamples);
    const auto stepLeft = (lastGainLeft - rampFromLeft) / count;
    const auto stepRight = (lastGainRight - rampFromRight) / count;
    const auto stepSend = (gain - lastGain) / count;

    // A sidechain key for another track: the sound before plugin delay compensation (that track's
    // plugin hears its own before any delay too).
    const auto& source = beforeCompensation && keyHeld ? keyBlock : buffer;
    const auto* sourceLeft = source.getReadPointer (0);
    const auto* sourceRight = source.getReadPointer (1);
    auto faderLeft = rampFromLeft, faderRight = rampFromRight, send = lastGain;

    for (int i = 0; i < numSamples; ++i)
    {
        faderLeft += stepLeft;
        faderRight += stepRight;
        send += stepSend;
        left[i] += sourceLeft[i] * faderLeft * send;
        right[i] += sourceRight[i] * faderRight * send;
    }

    lastGain = gain;
}

void Track::playClips (const AudioPlaylist* list, const Transport& songTransport, float* left, float* right, bool fadeStart,
                       int effectSlot) noexcept
{
    const auto segments = songTransport.getNumSegments();

    if (list == nullptr || list->items.empty() || segments == 0)
        return;

    constexpr juce::int64 fade = 64;   // samples: clip edges ramp in and out, so they never click

    // Audio keeps its speed: a clip plays from where it starts in the song's time, sample by
    // sample, whatever the tempo does on the way.
    for (int s = 0; s < segments; ++s)
    {
        const auto& segment = songTransport.getSegment (s);
        const auto segmentEnd = segment.sample + segment.length;

        for (const auto& item : list->items)
        {
            if (item.data == nullptr || item.lengthSamples <= 0 || item.effectSlot != effectSlot)
                continue;

            const auto itemEnd = item.startSample + item.lengthSamples;

            if (itemEnd <= segment.sample || item.startSample >= segmentEnd)
                continue;

            const auto first = static_cast<int> (juce::jmax<juce::int64> (0, item.startSample - segment.sample));
            const auto last = static_cast<int> (juce::jmin<juce::int64> (segment.length, itemEnd - segment.sample));

            const auto& samples = item.data->samples;
            const auto* sourceLeft = samples.getReadPointer (0);
            const auto* sourceRight = samples.getReadPointer (samples.getNumChannels() > 1 ? 1 : 0);
            const auto available = static_cast<juce::int64> (samples.getNumSamples());

            auto intoClip = segment.sample + first - item.startSample;

            // Silence Areas: the first stretch that ends after where this part starts.
            const auto* silence = item.silence.get();
            auto quiet = silence != nullptr ? std::upper_bound (silence->begin(), silence->end(), item.offsetSamples + intoClip,
                                                                [] (juce::int64 at, const std::pair<juce::int64, juce::int64>& area) { return at < area.second; })
                                            : std::vector<std::pair<juce::int64, juce::int64>>::const_iterator();
            const auto fadeSamples = static_cast<float> (juce::jmax<juce::int64> (1, item.silenceFadeSamples));

            // The clip's own automation: its Gain and Pan lines, looked up from where this part starts.
            const auto* gainCurve = item.gainCurve.get();
            const auto* panCurve = item.panCurve.get();
            auto gainHint = gainCurve != nullptr ? gainCurve->find (item.offsetSamples + intoClip) : size_t {};
            auto panHint = panCurve != nullptr ? panCurve->find (item.offsetSamples + intoClip) : size_t {};

            for (int i = first; i < last; ++i, ++intoClip)
            {
                const auto source = item.offsetSamples + intoClip;

                if (intoClip < 0 || intoClip >= item.lengthSamples || source < 0 || source >= available)
                    continue;

                auto gain = item.gain;
                const auto untilEnd = item.lengthSamples - intoClip;

                if (intoClip < fade)
                {
                    if (fadeStart)
                        gain *= static_cast<float> (intoClip) / static_cast<float> (fade);
                }
                else if (untilEnd < fade)
                    gain *= static_cast<float> (untilEnd) / static_cast<float> (fade);

                // The clip's own fades: quarter-sine curves, which sound even as they pass.
                if (intoClip < item.fadeInSamples)
                    gain *= static_cast<float> (std::sin (juce::MathConstants<double>::halfPi * static_cast<double> (intoClip) / static_cast<double> (item.fadeInSamples)));

                if (untilEnd < item.fadeOutSamples)
                    gain *= static_cast<float> (std::sin (juce::MathConstants<double>::halfPi * static_cast<double> (untilEnd) / static_cast<double> (item.fadeOutSamples)));

                // Inside a silent stretch: down to nothing, ramping at its edges.
                if (silence != nullptr)
                {
                    while (quiet != silence->end() && quiet->second <= source)
                        ++quiet;

                    if (quiet != silence->end() && source >= quiet->first)
                    {
                        const auto depth = static_cast<float> (juce::jmin (source - quiet->first, quiet->second - source));
                        gain *= 1.0f - juce::jmin (1.0f, depth / fadeSamples);
                    }
                }

                auto gainLeft = gain, gainRight = gain;

                if (gainCurve != nullptr)
                {
                    const auto db = gainCurve->valueAt (source, gainHint);
                    const auto factor = db <= -59.9f ? 0.0f : std::exp (db * 0.11512925f);   // ln(10) / 20
                    gainLeft *= factor;
                    gainRight *= factor;
                }

                if (panCurve != nullptr)
                {
                    const auto balance = juce::jlimit (-1.0f, 1.0f, panCurve->valueAt (source, panHint));
                    gainLeft *= balance > 0.0f ? 1.0f - balance : 1.0f;
                    gainRight *= balance < 0.0f ? 1.0f + balance : 1.0f;
                }

                left[segment.offset + i] += sourceLeft[source] * gainLeft;
                right[segment.offset + i] += sourceRight[source] * gainRight;
            }
        }
    }
}

void Track::automateClipEffect (const AudioPlaylist* list, const Transport& songTransport, int effectSlot, Plugin& plugin) noexcept
{
    if (list == nullptr || songTransport.getNumSegments() == 0)
        return;

    // Where the block starts, in the song's samples: the clip's automation of its effect, there.
    const auto now = songTransport.getSegment (0).sample;

    for (const auto& item : list->items)
    {
        const auto* curves = item.paramCurves.get();

        if (item.effectSlot != effectSlot || curves == nullptr || now < item.startSample || now >= item.startSample + item.lengthSamples)
            continue;

        const auto source = item.offsetSamples + (now - item.startSample);

        for (size_t i = 0; i < curves->curves.size(); ++i)
        {
            const auto& curve = curves->curves[i];
            auto hint = curve.find (source);
            const auto value = static_cast<double> (curve.valueAt (source, hint));

            if (juce::exactlyEqual (value, curves->lastSent[i]))
                continue;

            curves->lastSent[i] = value;
            plugin.setParameterFromAudio (curves->paramIds[i], curves->indexHints[i], value);
        }

        return;
    }
}

std::pair<float, float> Track::faderGains (bool audible) const noexcept
{
    if (! audible)
        return { 0.0f, 0.0f };

    const auto db = volumeDb.load();
    const auto gain = db <= silentDb ? 0.0f : juce::Decibels::decibelsToGain (db);
    const auto balance = juce::jlimit (-1.0f, 1.0f, pan.load());

    // Polarity inverted: the gains turn negative (the fader's ramp crosses zero, so no click).
    const auto sign = polarity.load (std::memory_order_relaxed) ? -1.0f : 1.0f;
    return { sign * gain * (balance > 0.0f ? 1.0f - balance : 1.0f), sign * gain * (balance < 0.0f ? 1.0f + balance : 1.0f) };
}

void Track::settleGain (bool audible) noexcept
{
    std::tie (lastGainLeft, lastGainRight) = faderGains (audible);
    rampFromLeft = lastGainLeft;
    rampFromRight = lastGainRight;
}

void Track::mixInto (float* left, float* right, int numSamples, bool audible) noexcept
{
    numSamples = juce::jmin (numSamples, renderedSamples);

    if (numSamples <= 0 || left == nullptr)
        return;

    const auto [targetLeft, targetRight] = faderGains (audible);

    // Ramp across the block so fader moves and mutes never click.
    const auto stepLeft = (targetLeft - lastGainLeft) / static_cast<float> (numSamples);
    const auto stepRight = (targetRight - lastGainRight) / static_cast<float> (numSamples);

    const auto* sourceLeft = buffer.getReadPointer (0);
    const auto* sourceRight = buffer.getReadPointer (1);

    auto gainLeft = lastGainLeft;
    auto gainRight = lastGainRight;
    float peakL = 0.0f, peakR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        gainLeft += stepLeft;
        gainRight += stepRight;

        const auto sampleLeft = sourceLeft[i] * gainLeft;
        const auto sampleRight = sourceRight[i] * gainRight;

        if (right != nullptr)
        {
            left[i] += sampleLeft;
            right[i] += sampleRight;
        }
        else
        {
            left[i] += 0.5f * (sampleLeft + sampleRight);
        }

        peakL = juce::jmax (peakL, std::abs (sampleLeft));
        peakR = juce::jmax (peakR, std::abs (sampleRight));
    }

    rampFromLeft = lastGainLeft;
    rampFromRight = lastGainRight;
    lastGainLeft = targetLeft;
    lastGainRight = targetRight;

    storeMax (peakLeft, peakL);
    storeMax (peakRight, peakR);
}

//==============================================================================
juce::var Track::sanitiseDrumMap (const juce::var& map)
{
    if (! map.isObject())
        return {};

    juce::DynamicObject::Ptr names (new juce::DynamicObject());
    int named = 0;

    if (const auto* given = map.getProperty ("names", {}).getDynamicObject())
    {
        for (const auto& property : given->getProperties())
        {
            const auto key = property.name.toString();

            if (! key.containsOnly ("0123456789") || key.isEmpty() || key.length() > 3)
                continue;

            const auto pitch = key.getIntValue();
            const auto name = property.value.toString().trim().substring (0, 40);

            if (pitch >= 0 && pitch <= 127 && name.isNotEmpty())
            {
                names->setProperty (juce::String (pitch), name);
                ++named;
            }
        }
    }

    juce::Array<juce::var> rows;

    if (const auto* given = map.getProperty ("rows", {}).getArray())
        for (const auto& value : *given)
            if (value.isInt() || value.isInt64() || value.isDouble())
                if (const auto pitch = static_cast<int> (value); pitch >= 0 && pitch <= 127 && ! rows.contains (pitch))
                    rows.add (pitch);

    if (named == 0 && rows.isEmpty())
        return {};

    juce::DynamicObject::Ptr result (new juce::DynamicObject());
    result->setProperty ("names", juce::var (names.get()));
    result->setProperty ("rows", rows);
    return juce::var (result.get());
}
