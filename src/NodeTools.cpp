// D:\workspace\Fanan_web_hub\src\NodeTools.cpp
//
// The Node Rack's Latcher, Step Seq and Transient Splitter (Colosseum's system tools).

#include "NodeTools.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cmath>
#include <cstring>

namespace NodeTools
{
    //==============================================================================
    // Latcher
    Latcher::Pad Latcher::defaultPad (int index)
    {
        Pad pad;
        pad.triggerNote = 60 + index;   // C4 upwards, one pad a semitone
        pad.outputNote = 60 + index;
        return pad;
    }

    Latcher::Latcher()
        : juce::AudioProcessor (BusesProperties())   // MIDI only
    {
        for (int i = 0; i < numPads; ++i)
        {
            pads[static_cast<size_t> (i)] = defaultPad (i);
            latched[static_cast<size_t> (i)].store (false);
        }

        output.ensureSize (8192);
    }

    bool Latcher::isPadLatched (int index) const noexcept
    {
        return index >= 0 && index < numPads && latched[static_cast<size_t> (index)].load();
    }

    int Latcher::countLatched() const noexcept
    {
        int count = 0;

        for (const auto& pad : latched)
            count += pad.load() ? 1 : 0;

        return count;
    }

    void Latcher::togglePadFromUI (int index)
    {
        if (index < 0 || index >= numPads)
            return;

        auto& state = latched[static_cast<size_t> (index)];
        state.store (! state.load());
        pendingToggles.fetch_or (1u << static_cast<juce::uint32> (index));
    }

    void Latcher::allNotesOff()
    {
        for (int i = 0; i < numPads; ++i)
            if (latched[static_cast<size_t> (i)].exchange (false))
                pendingToggles.fetch_or (1u << static_cast<juce::uint32> (i));
    }

    void Latcher::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        buffer.clear();
        output.clear();

        const auto send = [this] (const Pad& pad, bool on, int sample)
        {
            output.addEvent (on ? juce::MidiMessage::noteOn (pad.midiChannel, pad.outputNote, static_cast<juce::uint8> (pad.velocity))
                                : juce::MidiMessage::noteOff (pad.midiChannel, pad.outputNote, static_cast<juce::uint8> (pad.velocity)),
                             sample);
        };

        // Pads pressed with the mouse (already shown): their notes now.
        if (const auto toggles = pendingToggles.exchange (0); toggles != 0)
            for (int i = 0; i < numPads; ++i)
                if ((toggles & (1u << static_cast<juce::uint32> (i))) != 0)
                    send (pads[static_cast<size_t> (i)], latched[static_cast<size_t> (i)].load(), 0);

        // A trigger note pressed toggles its pad (the first that has it); its release is swallowed.
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            bool consumed = false;

            if (message.isNoteOn())
            {
                for (int i = 0; i < numPads; ++i)
                {
                    const auto& pad = pads[static_cast<size_t> (i)];

                    if (pad.triggerNote != message.getNoteNumber())
                        continue;

                    auto& state = latched[static_cast<size_t> (i)];
                    const auto nowOn = ! state.load();
                    state.store (nowOn);
                    send (pad, nowOn, metadata.samplePosition);
                    consumed = true;
                    break;
                }
            }
            else if (message.isNoteOff())
            {
                for (const auto& pad : pads)
                    if (pad.triggerNote == message.getNoteNumber())
                        consumed = true;
            }

            if (! consumed)
                output.addEvent (message, metadata.samplePosition);
        }

        midi.swapWith (output);
    }

    void Latcher::getStateInformation (juce::MemoryBlock& destination)
    {
        juce::ValueTree state ("LatcherState");

        for (int i = 0; i < numPads; ++i)
        {
            const auto& pad = pads[static_cast<size_t> (i)];
            juce::ValueTree padState ("Pad");
            padState.setProperty ("index", i, nullptr);
            padState.setProperty ("triggerNote", pad.triggerNote, nullptr);
            padState.setProperty ("outputNote", pad.outputNote, nullptr);
            padState.setProperty ("velocity", pad.velocity, nullptr);
            padState.setProperty ("midiChannel", pad.midiChannel, nullptr);
            state.addChild (padState, -1, nullptr);
        }

        juce::MemoryOutputStream stream (destination, false);
        state.writeToStream (stream);
    }

    void Latcher::setStateInformation (const void* data, int size)
    {
        const auto state = juce::ValueTree::readFromData (data, static_cast<size_t> (size));

        if (! state.isValid() || ! state.hasType ("LatcherState"))
            return;

        for (const auto& padState : state)
        {
            const auto index = static_cast<int> (padState.getProperty ("index", -1));

            if (! padState.hasType ("Pad") || index < 0 || index >= numPads)
                continue;

            auto& pad = pads[static_cast<size_t> (index)];
            pad.triggerNote = juce::jlimit (0, 127, static_cast<int> (padState.getProperty ("triggerNote", 60 + index)));
            pad.outputNote = juce::jlimit (0, 127, static_cast<int> (padState.getProperty ("outputNote", 60 + index)));
            pad.velocity = juce::jlimit (1, 127, static_cast<int> (padState.getProperty ("velocity", 100)));
            pad.midiChannel = juce::jlimit (1, 16, static_cast<int> (padState.getProperty ("midiChannel", 1)));
            latched[static_cast<size_t> (index)].store (false);
        }
    }

    //==============================================================================
    // Step Seq
    StepSeq::StepSeq()
        : juce::AudioProcessor (BusesProperties())   // MIDI only
    {
        for (int i = 0; i < maxSlots; ++i)
        {
            slots[static_cast<size_t> (i)].midiChannel = i + 1;   // lane N on channel N
            shownStep[static_cast<size_t> (i)].store (0);
        }
    }

    void StepSeq::prepareToPlay (double rate, int)
    {
        sampleRate = rate > 0.0 ? rate : 44100.0;
    }

    double StepSeq::rateBeats (RateDiv rate)
    {
        switch (rate)
        {
            case rate1_1:   return 4.0;
            case rate1_2:   return 2.0;
            case rate1_4:   return 1.0;
            case rate1_8:   return 0.5;
            case rate1_16:  return 0.25;
            case rate1_32:  return 0.125;
            case rate1_64:  return 0.0625;
            case rate1_128: return 0.03125;
            case numRates:
            default:        return 1.0;
        }
    }

    double StepSeq::typeMultiplier (RateType type)
    {
        return type == typeTriplet ? 2.0 / 3.0 : type == typeDotted ? 1.5 : 1.0;
    }

    double StepSeq::speedMultiplier (SpeedMode speed)
    {
        return speed == speedHalf ? 2.0 : speed == speedDouble ? 0.5 : 1.0;   // of the step's length
    }

    int StepSeq::nextStep (Slot& slot)
    {
        const auto count = juce::jmax (1, slot.stepCount);
        const auto current = slot.currentStep;

        switch (slot.order)
        {
            case orderReverse:
                return (current - 1 + count) % count;

            case orderPingPong:
                if (slot.pingPongForward)
                {
                    if (current + 1 >= count) { slot.pingPongForward = false; return juce::jmax (0, current - 1); }
                    return current + 1;
                }

                if (current - 1 < 0) { slot.pingPongForward = true; return juce::jmin (count - 1, current + 1); }
                return current - 1;

            case orderRandom:
                return random.nextInt (count);

            case orderDrunk:
            {
                auto next = current + random.nextInt (3) - 1;
                return next < 0 ? count - 1 : next >= count ? 0 : next;
            }

            case orderForward:
            case numOrders:
            default:
                return (current + 1) % count;
        }
    }

    int StepSeq::valueOf (const Slot& slot)
    {
        const auto current = slot.steps[static_cast<size_t> (juce::jlimit (0, maxSteps - 1, slot.currentStep))];

        if (! slot.smooth)
            return juce::jlimit (0, 127, current);

        const auto previous = slot.steps[static_cast<size_t> (juce::jlimit (0, maxSteps - 1, slot.previousStep))];
        return juce::jlimit (0, 127, juce::roundToInt (previous + (current - previous) * slot.phase));
    }

    void StepSeq::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        buffer.clear();

        if (resetOwed.exchange (false))
            for (int i = 0; i < maxSlots; ++i)
            {
                slots[static_cast<size_t> (i)].resetPlayback();
                shownStep[static_cast<size_t> (i)].store (0);
            }

        if (! playing.load())
            return;

        // The song's tempo and metre (the rack's play head), or its own.
        auto bpm = ownBpm.load();

        if (auto* head = getPlayHead())
            if (const auto position = head->getPosition(); position.hasValue())
                if (syncTempo.load())
                    if (const auto songBpm = position->getBpm(); songBpm.hasValue() && *songBpm > 0.0)
                        bpm = *songBpm;

        effectiveBpm.store (bpm);

        const auto numSamples = buffer.getNumSamples();
        const auto beatsPerSample = bpm / 60.0 / sampleRate;

        for (int index = 0; index < maxSlots; ++index)
        {
            auto& slot = slots[static_cast<size_t> (index)];

            if (! slot.enabled || slot.stepCount < 2)
                continue;

            slot.currentStep = juce::jlimit (0, slot.stepCount - 1, slot.currentStep);
            slot.previousStep = juce::jlimit (0, slot.stepCount - 1, slot.previousStep);

            const auto stepBeats = rateBeats (slot.rate) * typeMultiplier (slot.type) * speedMultiplier (slot.speed);
            const auto swing = static_cast<double> (slot.swing) / 100.0;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                // Swing: even steps longer, odd steps shorter.
                auto length = stepBeats;

                if (swing > 0.0)
                    length *= (slot.currentStep % 2 == 0) ? 1.0 + swing * 0.5 : 1.0 - swing * 0.5;

                slot.phase += beatsPerSample / juce::jmax (0.001, length);

                if (slot.phase >= 1.0)
                {
                    slot.phase -= 1.0;
                    slot.previousStep = slot.currentStep;
                    slot.currentStep = nextStep (slot);
                }

                const auto value = valueOf (slot);

                if (value != slot.lastSent)
                {
                    midi.addEvent (juce::MidiMessage::controllerEvent (slot.midiChannel, slot.ccNumber, value), sample);
                    slot.lastSent = value;
                }
            }

            shownStep[static_cast<size_t> (index)].store (slot.currentStep);
        }
    }

    int StepSeq::getSlotCurrentStep (int index) const noexcept
    {
        return index >= 0 && index < maxSlots ? shownStep[static_cast<size_t> (index)].load() : 0;
    }

    int StepSeq::countEnabled() const noexcept
    {
        int count = 0;

        for (const auto& slot : slots)
            count += slot.enabled ? 1 : 0;

        return count;
    }

    bool StepSeq::slotHasData (int index) const
    {
        const auto& slot = getSlot (index);

        for (int i = 0; i < slot.stepCount; ++i)
            if (slot.steps[static_cast<size_t> (i)] != 0)
                return true;

        return false;
    }

    void StepSeq::clearSlot (int index)
    {
        getSlot (index).steps.fill (0);
    }

    void StepSeq::copySettings (const Slot& from, Slot& to)
    {
        to.enabled = from.enabled;
        to.stepCount = from.stepCount;
        to.midiChannel = from.midiChannel;
        to.ccNumber = from.ccNumber;
        to.rate = from.rate;
        to.type = from.type;
        to.order = from.order;
        to.speed = from.speed;
        to.smooth = from.smooth;
        to.swing = from.swing;
        to.steps = from.steps;
    }

    void StepSeq::copySlotToClipboard (int index)
    {
        copySettings (getSlot (index), clipboard);
        clipboardValid = true;
    }

    bool StepSeq::pasteSlotFromClipboard (int index)
    {
        if (! clipboardValid)
            return false;

        copySettings (clipboard, getSlot (index));
        return true;
    }

    juce::String StepSeq::rateName (RateDiv rate)
    {
        static const char* names[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128" };
        return rate >= 0 && rate < numRates ? names[rate] : "1/4";
    }

    juce::String StepSeq::typeName (RateType type)
    {
        return type == typeTriplet ? "Triplet" : type == typeDotted ? "Dotted" : "Normal";
    }

    juce::String StepSeq::orderName (StepOrder order)
    {
        static const char* names[] = { "Forward", "Reverse", "PingPong", "Random", "Drunk" };
        return order >= 0 && order < numOrders ? names[order] : "Forward";
    }

    juce::String StepSeq::speedName (SpeedMode speed)
    {
        return speed == speedHalf ? "Half" : speed == speedDouble ? "Double" : "Normal";
    }

    void StepSeq::getStateInformation (juce::MemoryBlock& destination)
    {
        juce::ValueTree state ("StepSeqState");
        state.setProperty ("bpm", ownBpm.load(), nullptr);
        state.setProperty ("syncBpm", syncTempo.load(), nullptr);
        state.setProperty ("syncTs", syncTimeSig.load(), nullptr);
        state.setProperty ("tsNum", numerator.load(), nullptr);
        state.setProperty ("tsDen", denominator.load(), nullptr);
        state.setProperty ("playing", playing.load(), nullptr);

        for (int i = 0; i < maxSlots; ++i)
        {
            const auto& slot = slots[static_cast<size_t> (i)];
            juce::ValueTree lane ("Slot");
            lane.setProperty ("index", i, nullptr);
            lane.setProperty ("enabled", slot.enabled, nullptr);
            lane.setProperty ("stepCount", slot.stepCount, nullptr);
            lane.setProperty ("midiChannel", slot.midiChannel, nullptr);
            lane.setProperty ("ccNumber", slot.ccNumber, nullptr);
            lane.setProperty ("rate", static_cast<int> (slot.rate), nullptr);
            lane.setProperty ("type", static_cast<int> (slot.type), nullptr);
            lane.setProperty ("order", static_cast<int> (slot.order), nullptr);
            lane.setProperty ("speed", static_cast<int> (slot.speed), nullptr);
            lane.setProperty ("smooth", slot.smooth, nullptr);
            lane.setProperty ("swing", slot.swing, nullptr);

            juce::StringArray values;

            for (int s = 0; s < slot.stepCount; ++s)
                values.add (juce::String (slot.steps[static_cast<size_t> (s)]));

            lane.setProperty ("steps", values.joinIntoString (","), nullptr);
            state.addChild (lane, -1, nullptr);
        }

        juce::MemoryOutputStream stream (destination, false);
        state.writeToStream (stream);
    }

    void StepSeq::setStateInformation (const void* data, int size)
    {
        const auto state = juce::ValueTree::readFromData (data, static_cast<size_t> (size));

        if (! state.isValid() || ! state.hasType ("StepSeqState"))
            return;

        setBpm (static_cast<double> (state.getProperty ("bpm", 120.0)));
        syncTempo.store (static_cast<bool> (state.getProperty ("syncBpm", true)));
        syncTimeSig.store (static_cast<bool> (state.getProperty ("syncTs", true)));
        setTimeSigNumerator (static_cast<int> (state.getProperty ("tsNum", 4)));
        setTimeSigDenominator (static_cast<int> (state.getProperty ("tsDen", 4)));
        playing.store (static_cast<bool> (state.getProperty ("playing", false)));
        resetOwed.store (true);

        for (const auto& lane : state)
        {
            const auto index = static_cast<int> (lane.getProperty ("index", -1));

            if (! lane.hasType ("Slot") || index < 0 || index >= maxSlots)
                continue;

            auto& slot = slots[static_cast<size_t> (index)];
            slot.enabled = static_cast<bool> (lane.getProperty ("enabled", false));

            const auto count = static_cast<int> (lane.getProperty ("stepCount", 16));
            slot.stepCount = std::find (allowedStepCounts.begin(), allowedStepCounts.end(), count) != allowedStepCounts.end() ? count : 16;
            slot.midiChannel = juce::jlimit (1, 16, static_cast<int> (lane.getProperty ("midiChannel", index + 1)));
            slot.ccNumber = juce::jlimit (0, 127, static_cast<int> (lane.getProperty ("ccNumber", 1)));
            slot.rate = static_cast<RateDiv> (juce::jlimit (0, static_cast<int> (numRates) - 1, static_cast<int> (lane.getProperty ("rate", static_cast<int> (rate1_8)))));
            slot.type = static_cast<RateType> (juce::jlimit (0, static_cast<int> (numRateTypes) - 1, static_cast<int> (lane.getProperty ("type", 0))));
            slot.order = static_cast<StepOrder> (juce::jlimit (0, static_cast<int> (numOrders) - 1, static_cast<int> (lane.getProperty ("order", 0))));
            slot.speed = static_cast<SpeedMode> (juce::jlimit (0, static_cast<int> (numSpeedModes) - 1, static_cast<int> (lane.getProperty ("speed", static_cast<int> (speedNormal)))));
            slot.smooth = static_cast<bool> (lane.getProperty ("smooth", false));
            slot.swing = juce::jlimit (0.0f, 100.0f, static_cast<float> (static_cast<double> (lane.getProperty ("swing", 0.0))));
            slot.steps.fill (0);

            juce::StringArray values;
            values.addTokens (lane.getProperty ("steps", {}).toString(), ",", "");

            for (int s = 0; s < juce::jmin (values.size(), slot.stepCount); ++s)
                slot.steps[static_cast<size_t> (s)] = juce::jlimit (0, 127, values[s].getIntValue());
        }
    }

    //==============================================================================
    // Transient Splitter
    TransientSplitter::TransientSplitter()
        : juce::AudioProcessor (BusesProperties()
                                  .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput ("Output", juce::AudioChannelSet::discreteChannels (4), true))
    {
    }

    bool TransientSplitter::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        return layouts.getMainInputChannelSet().size() >= 2 && layouts.getMainOutputChannelSet().size() >= 4;
    }

    void TransientSplitter::prepareToPlay (double rate, int samplesPerBlock)
    {
        sampleRate = rate > 0.0 ? rate : 44100.0;
        dry.setSize (2, juce::jmax (1, samplesPerBlock), false, false, true);
        fastL = fastR = slowL = slowR = 0.0f;
        gateL = gateR = smoothL = smoothR = 0.0f;
        holdL = holdR = 0;
        highPassL.reset(); highPassR.reset(); lowPassL.reset(); lowPassR.reset();
        lastHighPass = lastLowPass = -1.0f;
        updateFilters();
    }

    void TransientSplitter::updateFilters()
    {
        const auto nyquist = static_cast<float> (sampleRate * 0.49);
        const auto hp = juce::jmin (nyquist, juce::jlimit (20.0f, 20000.0f, focusHighPass.load()));
        const auto lp = juce::jmin (nyquist, juce::jlimit (20.0f, 20000.0f, focusLowPass.load()));

        if (std::abs (hp - lastHighPass) > 0.1f)
        {
            highPass = SonifyDsp::Biquad::highPass (sampleRate, hp, 0.7071);
            lastHighPass = hp;
        }

        if (std::abs (lp - lastLowPass) > 0.1f)
        {
            lowPass = SonifyDsp::Biquad::lowPass (sampleRate, lp, 0.7071);
            lastLowPass = lp;
        }
    }

    void TransientSplitter::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        const auto numSamples = buffer.getNumSamples();

        if (buffer.getNumChannels() < 4 || numSamples <= 0)
            return;

        if (dry.getNumSamples() < numSamples)
            dry.setSize (2, numSamples, false, false, true);   // only if the host broke its promise

        updateFilters();

        const auto sens = juce::jlimit (0.01f, 1.0f, sensitivity.load());
        const auto decayMs = juce::jlimit (1.0f, 500.0f, decay.load());
        const auto holdMs = juce::jlimit (0.0f, 100.0f, holdTime.load());
        const auto smoothMs = juce::jlimit (0.1f, 50.0f, smoothing.load());
        const auto bal = juce::jlimit (-1.0f, 1.0f, balance.load());
        const auto tGain = juce::Decibels::decibelsToGain (juce::jlimit (-60.0f, 12.0f, transientGainDb.load()), -60.0f);
        const auto sGain = juce::Decibels::decibelsToGain (juce::jlimit (-60.0f, 12.0f, sustainGainDb.load()), -60.0f);
        const auto linked = stereoLinked.load(), gate = gateMode.load(), invert = invertMode.load();

        const auto fastAttack = coefficientFor (0.2f), fastRelease = coefficientFor (5.0f);
        const auto slowAttack = coefficientFor (20.0f), slowRelease = coefficientFor (100.0f);
        const auto smoothCoeff = coefficientFor (smoothMs), decayCoeff = coefficientFor (decayMs);
        const auto threshold = 1.0f + (1.0f - sens) * 4.0f;   // fast over slow: 1 (most sensitive) .. 5
        const auto holdSamples = static_cast<int> (holdMs * 0.001f * static_cast<float> (sampleRate));
        const auto balTransient = bal > 0.0f ? 1.0f - bal : 1.0f;
        const auto balSustain = bal < 0.0f ? 1.0f + bal : 1.0f;

        dry.copyFrom (0, 0, buffer, 0, 0, numSamples);
        dry.copyFrom (1, 0, buffer, 1, 0, numSamples);
        const auto* inL = dry.getReadPointer (0);
        const auto* inR = dry.getReadPointer (1);
        auto* transL = buffer.getWritePointer (0);
        auto* transR = buffer.getWritePointer (1);
        auto* sustL = buffer.getWritePointer (2);
        auto* sustR = buffer.getWritePointer (3);

        float tL = 0.0f, tR = 0.0f, sL = 0.0f, sR = 0.0f, activity = 0.0f;

        const auto follow = [] (float& envelope, float input, float attack, float release)
        {
            envelope = input + (input > envelope ? attack : release) * (envelope - input);
        };

        const auto gateStep = [&] (float ratio, float& gateValue, int& hold)
        {
            if (ratio > threshold)  { gateValue = 1.0f; hold = holdSamples; }
            else if (hold > 0)      { --hold; gateValue = 1.0f; }
            else                    { gateValue *= decayCoeff; if (gateValue < 0.001f) gateValue = 0.0f; }
        };

        for (int i = 0; i < numSamples; ++i)
        {
            auto detL = std::abs (lowPassL.process (lowPass, highPassL.process (highPass, inL[i])));
            auto detR = std::abs (lowPassR.process (lowPass, highPassR.process (highPass, inR[i])));

            if (linked)
                detL = detR = juce::jmax (detL, detR);

            follow (fastL, detL, fastAttack, fastRelease);
            follow (fastR, detR, fastAttack, fastRelease);
            follow (slowL, detL, slowAttack, slowRelease);
            follow (slowR, detR, slowAttack, slowRelease);

            gateStep (fastL / juce::jmax (slowL, 1.0e-10f), gateL, holdL);
            gateStep (fastR / juce::jmax (slowR, 1.0e-10f), gateR, holdR);

            const auto targetL = gate ? (gateL > 0.5f ? 1.0f : 0.0f) : gateL;
            const auto targetR = gate ? (gateR > 0.5f ? 1.0f : 0.0f) : gateR;
            smoothL += (targetL - smoothL) * (1.0f - smoothCoeff);
            smoothR += (targetR - smoothR) * (1.0f - smoothCoeff);

            auto gL = juce::jlimit (0.0f, 1.0f, smoothL);
            auto gR = juce::jlimit (0.0f, 1.0f, smoothR);

            if (invert)
            {
                gL = 1.0f - gL;
                gR = 1.0f - gR;
            }

            transL[i] = inL[i] * gL * tGain * balTransient;
            transR[i] = inR[i] * gR * tGain * balTransient;
            sustL[i] = inL[i] * (1.0f - gL) * sGain * balSustain;
            sustR[i] = inR[i] * (1.0f - gR) * sGain * balSustain;

            tL += transL[i] * transL[i];
            tR += transR[i] * transR[i];
            sL += sustL[i] * sustL[i];
            sR += sustR[i] * sustR[i];
            activity = juce::jmax (activity, gL, gR);
        }

        const auto scale = 1.0f / static_cast<float> (numSamples);
        transientRmsL.store (std::sqrt (tL * scale));
        transientRmsR.store (std::sqrt (tR * scale));
        sustainRmsL.store (std::sqrt (sL * scale));
        sustainRmsR.store (std::sqrt (sR * scale));
        transientActivity.store (activity);
    }

    void TransientSplitter::getStateInformation (juce::MemoryBlock& destination)
    {
        juce::XmlElement xml ("TransientSplitterState");
        xml.setAttribute ("sensitivity", static_cast<double> (sensitivity.load()));
        xml.setAttribute ("decay", static_cast<double> (decay.load()));
        xml.setAttribute ("holdTime", static_cast<double> (holdTime.load()));
        xml.setAttribute ("smoothing", static_cast<double> (smoothing.load()));
        xml.setAttribute ("focusHPFreq", static_cast<double> (focusHighPass.load()));
        xml.setAttribute ("focusLPFreq", static_cast<double> (focusLowPass.load()));
        xml.setAttribute ("transientGainDb", static_cast<double> (transientGainDb.load()));
        xml.setAttribute ("sustainGainDb", static_cast<double> (sustainGainDb.load()));
        xml.setAttribute ("balance", static_cast<double> (balance.load()));
        xml.setAttribute ("stereoLinked", stereoLinked.load());
        xml.setAttribute ("gateMode", gateMode.load());
        xml.setAttribute ("invertMode", invertMode.load());
        copyXmlToBinary (xml, destination);
    }

    void TransientSplitter::setStateInformation (const void* data, int size)
    {
        const auto xml = getXmlFromBinary (data, size);

        if (xml == nullptr || ! xml->hasTagName ("TransientSplitterState"))
            return;

        sensitivity.store (static_cast<float> (xml->getDoubleAttribute ("sensitivity", 0.5)));
        decay.store (static_cast<float> (xml->getDoubleAttribute ("decay", 50.0)));
        holdTime.store (static_cast<float> (xml->getDoubleAttribute ("holdTime", 10.0)));
        smoothing.store (static_cast<float> (xml->getDoubleAttribute ("smoothing", 2.0)));
        focusHighPass.store (static_cast<float> (xml->getDoubleAttribute ("focusHPFreq", 20.0)));
        focusLowPass.store (static_cast<float> (xml->getDoubleAttribute ("focusLPFreq", 20000.0)));
        transientGainDb.store (static_cast<float> (xml->getDoubleAttribute ("transientGainDb", 0.0)));
        sustainGainDb.store (static_cast<float> (xml->getDoubleAttribute ("sustainGainDb", 0.0)));
        balance.store (static_cast<float> (xml->getDoubleAttribute ("balance", 0.0)));
        stereoLinked.store (xml->getBoolAttribute ("stereoLinked", true));
        gateMode.store (xml->getBoolAttribute ("gateMode", false));
        invertMode.store (xml->getBoolAttribute ("invertMode", false));
    }
}
