#pragma once

#include "AmpTone.h"

#include <array>

// ============================================================================
//  AXIS AMP — the five amps.
//
//  Each is a set of parts on the same engine (AmpEngine.h): the gain stages
//  with their plate and cathode resistors and bypass caps, the volume pot and
//  its bright cap, the tone stack's own component values, and the power
//  section's character. No amp is copied part for part: the tone stacks are
//  exactly the published networks (Yeh and Smith's Bassman values, and the
//  standard British values), and the stages use the values typical of each
//  family - which is where a family's sound comes from: a 2.7k / 0.68uF first
//  stage trims the bass of a British amp, a 10k unbypassed "cold clipper"
//  gives the classic British lead its bite, small coupling caps keep a modern
//  high gain tight.
//
//  The CHIME's tone network is the bass / treble Top Boost family: the same
//  topology with the Top Boost's large treble pot and tiny treble cap, and a
//  fixed resistor where the mid pot would be. It has no mid control of its
//  own, so on CHIME the MID knob is a gentle mid band after it.
// ============================================================================
namespace axis::dsp::amp
{
    struct PowerSpec
    {
        double drive = 0.06;            // the phase inverter's drive per volt from the tone stack, master full up
        double tubeDrive = 1.3;         // the power tubes against the phase inverter (above 1: they go first)
        double sag = 0.3;               // how far the supply gives under load (rectifier and filter caps)
        double sagAttackMs = 6.0, sagReleaseMs = 140.0;
        double classA = 0.0;            // the bias shifting under load (cathode biased, class A): 0..1
        double presenceHz = 3200.0;     // PRESENCE: the feedback loop letting the top through
        double presenceMinDb = 0.0, presenceMaxDb = 8.0;
        double resonanceHz = 95.0, resonanceDb = 0.0;   // the feedback loop letting the bottom through
        double transformerLowHz = 48.0, transformerHighHz = 11000.0;
    };

    struct VoiceSpec
    {
        const char* name;               // on the selector
        const char* description;        // under the pedal's name

        double inputVolts = 2.5;        // volts at the first grid for a full-scale sample (a hot humbucker)
        double tightHz = 180.0;         // TIGHT: a cut below this before the first stage

        int stages = 2;
        std::array<TriodeSpec, 4> stage {};
        std::array<double, 4> after { 1.0, 1.0, 1.0, 1.0 };   // the divider after each stage

        // The bright network fixed after the first stage (0: none): a level a passing a at the
        // bottom and everything above 1 / (R C).
        double networkLevel = 0.0, networkR = 470.0e3, networkC = 470.0e-12;

        int gainAfter = 0;              // the GAIN pot comes after this stage
        double gainPotR = 1.0e6;
        double brightC = 0.0;           // BRIGHT: the cap across its top

        FollowerCurve follower;         // the cathode follower that drives the tone stack
        ToneStackParts stack;
        double midBandHz = 0.0;         // a voice with no mid pot: MID is a band here (0: it has one)

        PowerSpec power;
        double outputTrim = 1.0;        // calibrated: every amp as loud as the others at the same settings
    };

    /** The five, in the selector's order. */
    inline const std::array<VoiceSpec, 5>& voiceSpecs()
    {
        static const std::array<VoiceSpec, 5> voices = []
        {
            std::array<VoiceSpec, 5> v {};

            //------------------------------------------------------------------
            // CLEAN: the American tweed and blackface clean - two gain stages, the volume between
            // them, a cathode follower into the Bassman stack, 6L6s with plenty of headroom.
            {
                auto& a = v[0];
                a.name = "CLEAN";
                a.description = "AMERICAN CLEAN";
                a.stages = 2;
                a.stage[0] = { 100.0e3, 820.0, 25.0e-6, 300.0, 1.0e6, 34.0e3, 10.0, 18000.0 };
                a.stage[1] = { 100.0e3, 1.5e3, 25.0e-6, 300.0, 1.0e6, 68.0e3, 8.0, 16000.0 };
                a.after = { 0.08, 1.0, 1.0, 1.0 };
                a.gainAfter = 0;
                a.brightC = 120.0e-12;
                a.follower = { 45.0, 140.0 };
                a.stack = { 250.0e3, 1.0e6, 25.0e3, 56.0e3, 250.0e-12, 20.0e-9, 20.0e-9, false };
                a.power = { 0.012, 1.15, 0.30, 6.0, 140.0, 0.0, 3000.0, 0.0, 7.0, 95.0, 1.0, 45.0, 12000.0 };
                a.outputTrim = 3.5075;   // +10.9 dB
            }

            //------------------------------------------------------------------
            // CRUNCH: the late-60s British plexi - a 2.7k / 0.68uF first stage, a bright volume,
            // a cathode follower into the British stack, EL34s.
            {
                auto& a = v[1];
                a.name = "CRUNCH";
                a.description = "BRITISH CRUNCH";
                a.stages = 2;
                a.stage[0] = { 100.0e3, 2.7e3, 0.68e-6, 300.0, 1.0e6, 34.0e3, 10.0, 18000.0 };
                a.stage[1] = { 100.0e3, 820.0, 25.0e-6, 300.0, 1.0e6, 68.0e3, 10.0, 15000.0 };
                a.after = { 0.25, 1.0, 1.0, 1.0 };
                a.gainAfter = 0;
                a.brightC = 1000.0e-12;
                a.follower = { 40.0, 120.0 };
                a.stack = { 250.0e3, 1.0e6, 25.0e3, 33.0e3, 500.0e-12, 22.0e-9, 22.0e-9, false };
                a.power = { 0.020, 1.30, 0.25, 6.0, 140.0, 0.0, 3400.0, 0.0, 8.0, 100.0, 1.5, 50.0, 10500.0 };
                a.outputTrim = 0.6607;   // -3.6 dB
            }

            //------------------------------------------------------------------
            // CHIME: the British top boost - an extra gain stage into the bass / treble network,
            // four EL84s in class A with no feedback, a bright volume. Its PRESENCE is the cut
            // control the other way round: up is less cut.
            {
                auto& a = v[2];
                a.name = "CHIME";
                a.description = "BRITISH CHIME";
                a.stages = 2;
                a.stage[0] = { 100.0e3, 1.5e3, 25.0e-6, 290.0, 1.0e6, 34.0e3, 10.0, 18000.0 };
                a.stage[1] = { 100.0e3, 1.5e3, 25.0e-6, 290.0, 1.0e6, 68.0e3, 10.0, 16000.0 };
                a.after = { 0.18, 1.0, 1.0, 1.0 };
                a.gainAfter = 0;
                a.brightC = 100.0e-12;
                a.follower = { 40.0, 120.0 };
                a.stack = { 1.0e6, 1.0e6, 10.0e3, 100.0e3, 50.0e-12, 22.0e-9, 22.0e-9, true };
                a.midBandHz = 800.0;
                a.power = { 0.022, 1.45, 0.45, 8.0, 180.0, 0.35, 3800.0, -10.0, 0.0, 100.0, 0.0, 55.0, 9500.0 };
                a.outputTrim = 1.3646;   // +2.7 dB
            }

            //------------------------------------------------------------------
            // LEAD: the 80s British master-volume lead - 2.7k / 0.68uF, the 470k / 470pF bright
            // network, the gain pot, a 10k unbypassed cold clipper, a tight third stage, the
            // follower and the British stack, EL34s on a firm supply.
            {
                auto& a = v[3];
                a.name = "LEAD";
                a.description = "BRITISH LEAD";
                a.stages = 3;
                a.stage[0] = { 100.0e3, 2.7e3, 0.68e-6, 300.0, 1.0e6, 34.0e3, 10.0, 18000.0 };
                a.stage[1] = { 100.0e3, 10.0e3, 0.0, 300.0, 1.0e6, 68.0e3, 15.0, 15000.0 };
                a.stage[2] = { 100.0e3, 820.0, 0.68e-6, 300.0, 1.0e6, 470.0e3, 15.0, 12000.0 };
                a.after = { 1.0, 0.14, 1.0, 1.0 };
                a.networkLevel = 0.68;
                a.gainAfter = 0;
                a.brightC = 470.0e-12;
                a.follower = { 38.0, 115.0 };
                a.stack = { 220.0e3, 1.0e6, 25.0e3, 33.0e3, 470.0e-12, 22.0e-9, 22.0e-9, false };
                a.power = { 0.020, 1.30, 0.12, 5.0, 110.0, 0.0, 3400.0, 0.0, 8.0, 100.0, 2.0, 55.0, 10000.0 };
                a.outputTrim = 0.3162;   // -10.0 dB
            }

            //------------------------------------------------------------------
            // MODERN: four cascaded stages with small coupling caps and a cold clipper among them
            // (the tight modern high gain), the stack after them all, a firm supply, and the
            // feedback loop's resonance holding the low end up.
            {
                auto& a = v[4];
                a.name = "MODERN";
                a.description = "MODERN HIGH GAIN";
                a.stages = 4;
                a.stage[0] = { 100.0e3, 1.5e3, 1.0e-6, 310.0, 1.0e6, 34.0e3, 20.0, 18000.0 };
                a.stage[1] = { 100.0e3, 1.5e3, 0.68e-6, 310.0, 1.0e6, 68.0e3, 60.0, 14000.0 };
                a.stage[2] = { 100.0e3, 6.8e3, 0.0, 310.0, 1.0e6, 470.0e3, 80.0, 12000.0 };
                a.stage[3] = { 100.0e3, 1.5e3, 22.0e-6, 310.0, 1.0e6, 470.0e3, 60.0, 9000.0 };
                a.after = { 1.0, 0.03, 0.25, 1.0 };
                a.gainAfter = 0;
                a.brightC = 470.0e-12;
                a.follower = { 38.0, 115.0 };
                a.stack = { 250.0e3, 1.0e6, 25.0e3, 47.0e3, 250.0e-12, 47.0e-9, 22.0e-9, false };
                a.power = { 0.020, 1.25, 0.12, 5.0, 100.0, 0.0, 3600.0, 0.0, 8.0, 90.0, 4.0, 60.0, 9000.0 };
                a.outputTrim = 0.2371;   // -12.5 dB
            }

            return v;
        }();

        return voices;
    }
}
