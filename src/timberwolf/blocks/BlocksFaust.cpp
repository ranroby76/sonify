// D:\workspace\Fanan_web_hub\src\timberwolf\blocks\BlocksFaust.cpp
//==============================================================================
//  The effects built from the Faust libraries (grame-cncm/faustlibraries): the
//  pedal models (Klon Centaur, Fulltone OCD, MXR Phase 90, Cry Baby), filters,
//  modulations, the octaver and granular, tape stop, the echo, and the reverbs
//  (spring, zita hall, Dattorro plate, JPverb, Greyhole, Freeverb room), and
//  the OTT-style multiband.  The C++ in timberwolf\faust is the Faust
//  compiler's; FaustGlue.h has what's around it and how each is played.
//==============================================================================
#include "../faust/FaustGlue.h"
#include "../TwRegistry.h"

namespace tw::faustglue
{
    #include "../faust/klon.h"
    #include "../faust/ocd.h"
    #include "../faust/phase90.h"
    #include "../faust/crybaby.h"
    #include "../faust/wah.h"
    #include "../faust/autowah.h"
    #include "../faust/moog.h"
    #include "../faust/peq.h"
    #include "../faust/tilt.h"
    #include "../faust/exciter.h"
    #include "../faust/phaser.h"
    #include "../faust/flanger.h"
    #include "../faust/univibe.h"
    #include "../faust/dimension.h"
    #include "../faust/octave.h"
    #include "../faust/granular.h"
    #include "../faust/tapestop.h"
    #include "../faust/echo.h"
    #include "../faust/spring.h"
    #include "../faust/hall.h"
    #include "../faust/plate.h"
    #include "../faust/jpverb.h"
    #include "../faust/greyhole.h"
    #include "../faust/room.h"
    #include "../faust/ott.h"
    #include "../faust/wavefold.h"
}

namespace tw
{
    namespace
    {
        using namespace faustglue;

        juce::StringArray namesOf (const std::vector<KnobSpec>& knobs)
        {
            juce::StringArray n;
            for (const auto& k : knobs) n.add (k.name);
            return n;
        }

        /** A Faust effect as a block: its knobs drive its sliders of the same name. */
        template <typename Dsp>
        BlockInfo faustInfo (const juce::String& id, const juce::String& name, Family family, const juce::String& icon,
                             const juce::String& credit, const juce::String& description, std::vector<KnobSpec> knobs, bool trails = false)
        {
            const auto names = namesOf (knobs);
            return kit::info (id, name, family, icon, credit, description, std::move (knobs),
                              [names] { return std::unique_ptr<Block> (new FaustBlock<Dsp> (names)); }, trails);
        }

        /** The echo: its time free, or a division of the beat. */
        class EchoBlock final : public FaustBlock<Faust_Echo>
        {
        public:
            EchoBlock() : FaustBlock<Faust_Echo> ({ "Time", "Sync", "Feedback", "Tone", "Mix" }) {}

        protected:
            float valueFor (int k) const override
            {
                if (k == 0)
                    return (float) juce::jlimit (10.0, 2500.0, kit::syncedMs (knob[0], (int) knob[1], bpm()));

                return knob[(size_t) k];
            }
        };

        const juce::String faustLib = "Faust libraries (GRAME, LGPL with the Faust exception)";
    }

    void registry::addFaust (std::vector<BlockInfo>& list)
    {
        using namespace kit;

        // Drive
        list.push_back (faustInfo<Faust_Klon> ("klon", "Klon Drive", Family::drive, "gfx-overdrive",
            "Faust ve.klonCentaur: Jatin Chowdhury's ChowCentaur model (BSD-3-Clause)",
            "The Klon Centaur's transparent overdrive, its gain stage a wave digital model.",
            { percent ("Gain", 50.0f), percent ("Treble", 50.0f), percent ("Level", 50.0f) }));

        list.push_back (faustInfo<Faust_Ocd> ("ocd", "OCD Drive", Family::drive, "gfx-distortion",
            "Faust ve.fulltoneOCD: after Giampiccolo et al., DAFx (MIT)",
            "The Fulltone OCD: MOSFET and germanium clipping, high and low peak.",
            { percent ("Drive", 50.0f), percent ("Tone", 50.0f), percent ("Volume", 50.0f), choice ("Peak", { "Low", "High" }, 1) }));

        list.push_back (faustInfo<Faust_Wavefold> ("wavefold", "Wavefolder", Family::drive, "gfx-wavefolder", faustLib + ", ef.wavefold",
            "Folds the wave back on itself past the edge: synth-like overtones.",
            { knob ("Drive", 0.0f, 36.0f, 12.0f, " dB", 0.0f, 1), percent ("Width", 50.0f), knob ("Level", -24.0f, 12.0f, -6.0f, " dB", 0.0f, 1),
              percent ("Mix", 100.0f) }));

        // Dynamics
        list.push_back (faustInfo<Faust_Ott> ("ott", "OTT Multiband", Family::dynamics, "gfx-multiband",
            "Faust co.xfer_ott: David Braun (MIT)",
            "Three bands pushed up and pulled down at once: the OTT sound.",
            { percent ("Depth", 100.0f), knob ("Time", 0.0f, 1000.0f, 100.0f, " %", 100.0f), knob ("In Gain", -24.0f, 19.0f, 0.0f, " dB", 0.0f, 1),
              knob ("Out Gain", -24.0f, 19.0f, 0.0f, " dB", 0.0f, 1), knob ("Upward", 0.0f, 200.0f, 100.0f, " %"), knob ("Downward", 0.0f, 200.0f, 100.0f, " %") }));

        // Filter / EQ
        list.push_back (faustInfo<Faust_Crybaby> ("crybaby", "Cry Baby", Family::filter, "gfx-wah", faustLib + ", ve.crybaby (STK-4.3)",
            "The Cry Baby wah: put it on an expression pedal.",
            { percent ("Pedal", 50.0f), knob ("Level", -18.0f, 12.0f, 0.0f, " dB", 0.0f, 1) }));

        list.push_back (faustInfo<Faust_Wah> ("wah", "Wah", Family::filter, "gfx-wah", faustLib + ", ve.wah4 (STK-4.3)",
            "A four-pole wah with its sweep set by Low and High.",
            { percent ("Pedal", 50.0f), knob ("Low", 200.0f, 800.0f, 350.0f, " Hz"), knob ("High", 1000.0f, 4000.0f, 2200.0f, " Hz"),
              knob ("Level", -18.0f, 12.0f, 0.0f, " dB", 0.0f, 1) }));

        list.push_back (faustInfo<Faust_Autowah> ("autowah", "Auto-wah", Family::filter, "gfx-auto-wah", faustLib + ", ve.autowah (STK-4.3)",
            "A Cry Baby opened by how hard you play.",
            { knob ("Sensitivity", -24.0f, 24.0f, 0.0f, " dB", 0.0f, 1), percent ("Amount", 80.0f), knob ("Level", -18.0f, 12.0f, 0.0f, " dB", 0.0f, 1) }));

        list.push_back (faustInfo<Faust_Moog> ("moog", "Moog Filter", Family::filter, "gfx-filter-sweep", faustLib + ", ve.moogLadder (STK-4.3)",
            "A Moog ladder lowpass, swept by its LFO.",
            { knob ("Cutoff", 40.0f, 12000.0f, 1200.0f, " Hz", 1000.0f), percent ("Resonance", 30.0f), knob ("LFO Rate", 0.02f, 10.0f, 0.5f, " Hz", 1.0f),
              percent ("LFO Depth", 0.0f), knob ("Drive", 0.0f, 24.0f, 0.0f, " dB", 0.0f, 1), percent ("Mix", 100.0f) }));

        list.push_back (faustInfo<Faust_Peq> ("peq", "Parametric EQ", Family::filter, "gfx-parametric-eq", faustLib + ", fi.low_shelf, fi.peak_eq, fi.high_shelf (STK-4.3)",
            "Low shelf, a sweepable mid, high shelf.",
            { knob ("Low Freq", 30.0f, 600.0f, 120.0f, " Hz", 120.0f), knob ("Low Gain", -15.0f, 15.0f, 0.0f, " dB", 0.0f, 1),
              knob ("Mid Freq", 150.0f, 6000.0f, 800.0f, " Hz", 900.0f), knob ("Mid Gain", -15.0f, 15.0f, 0.0f, " dB", 0.0f, 1),
              knob ("Mid Q", 0.2f, 8.0f, 1.0f, {}, 1.0f), knob ("High Freq", 1500.0f, 15000.0f, 4000.0f, " Hz", 4500.0f),
              knob ("High Gain", -15.0f, 15.0f, 0.0f, " dB", 0.0f, 1), knob ("Level", -18.0f, 12.0f, 0.0f, " dB", 0.0f, 1) }));

        list.push_back (faustInfo<Faust_Tilt> ("tilt", "Tilt EQ", Family::filter, "gfx-tone", faustLib + ", fi.spectral_tilt (STK-4.3)",
            "One knob: darker one way, brighter the other.",
            { knob ("Tilt", -100.0f, 100.0f, 0.0f), knob ("Level", -18.0f, 12.0f, 0.0f, " dB", 0.0f, 1) }));

        list.push_back (faustInfo<Faust_Exciter> ("exciter", "Exciter", Family::filter, "gfx-exciter", "Timber Wolf's own, in Faust",
            "Sparkle: the highs, saturated and blended back in.",
            { knob ("Freq", 800.0f, 10000.0f, 3000.0f, " Hz", 3000.0f), knob ("Drive", 0.0f, 30.0f, 12.0f, " dB", 0.0f, 1), percent ("Amount", 30.0f),
              knob ("Level", -18.0f, 12.0f, 0.0f, " dB", 0.0f, 1) }));

        // Modulation
        list.push_back (faustInfo<Faust_Phase90> ("phase90", "Phase 90", Family::modulation, "gfx-phaser",
            "Faust ve.mxrPhase90: after Giampiccolo et al., DAFx24 (MIT)",
            "The MXR Phase 90: one knob, the JFET phaser on countless records.",
            { knob ("Rate", 0.1f, 10.0f, 1.5f, " Hz", 1.5f) }));

        list.push_back (faustInfo<Faust_Phaser> ("phaser", "Phaser", Family::modulation, "gfx-phaser", faustLib + ", pf.phaser2_stereo",
            "A stereo four-notch phaser with feedback.",
            { knob ("Rate", 0.02f, 10.0f, 0.5f, " Hz", 1.0f), percent ("Depth", 100.0f), knob ("Feedback", 0.0f, 95.0f, 50.0f, " %"),
              knob ("Low", 50.0f, 2000.0f, 200.0f, " Hz", 300.0f), knob ("High", 800.0f, 10000.0f, 3000.0f, " Hz", 3000.0f),
              knob ("Width", 50.0f, 3000.0f, 1000.0f, " Hz", 800.0f), choice ("Invert", { "Off", "On" }) }));

        list.push_back (faustInfo<Faust_Flanger> ("flanger", "Flanger", Family::modulation, "gfx-flanger", faustLib + ", pf.flanger_stereo",
            "A stereo flanger: through-zero sweeps with feedback either way.",
            { knob ("Rate", 0.02f, 10.0f, 0.3f, " Hz", 1.0f), percent ("Depth", 60.0f), knob ("Manual", 0.1f, 10.0f, 1.5f, " ms", 2.0f),
              knob ("Feedback", -95.0f, 95.0f, 50.0f, " %"), percent ("Mix", 100.0f), knob ("Spread", 0.0f, 180.0f, 90.0f, " deg"),
              choice ("Invert", { "Off", "On" }) }));

        list.push_back (faustInfo<Faust_Univibe> ("univibe", "Uni-Vibe", Family::modulation, "gfx-univibe", faustLib + ", pf.vibrato2_mono",
            "The swirling photocell vibe: Vibrato, or Chorus with the dry mixed in.",
            { knob ("Speed", 0.1f, 12.0f, 3.0f, " Hz", 3.0f), choice ("Mode", { "Vibrato", "Chorus" }, 1), percent ("Intensity", 70.0f),
              knob ("Feedback", 0.0f, 90.0f, 20.0f, " %"), knob ("Level", -18.0f, 12.0f, 0.0f, " dB", 0.0f, 1) }));

        list.push_back (faustInfo<Faust_Dimension> ("dimension", "Dimension", Family::modulation, "gfx-stereo-width",
            "Faust ef.xferDimensionExpander: David Braun (MIT)",
            "Widens what comes in: short delays spread left and right.",
            { percent ("Size", 50.0f), percent ("Mix", 50.0f) }));

        // Pitch
        list.push_back (faustInfo<Faust_Octave> ("octaver", "Octaver", Family::pitch, "gfx-octave-down", faustLib + ", ef.transpose",
            "One and two octaves down, one up, beside the dry.",
            { knob ("Dry", -60.0f, 6.0f, 0.0f, " dB", -12.0f, 1), knob ("Down 1", -60.0f, 6.0f, -6.0f, " dB", -12.0f, 1),
              knob ("Down 2", -60.0f, 6.0f, -60.0f, " dB", -12.0f, 1), knob ("Up 1", -60.0f, 6.0f, -60.0f, " dB", -12.0f, 1) }));

        list.push_back (faustInfo<Faust_Granular> ("granular", "Granular", Family::pitch, "gfx-granular", "Faust ef.granular (MIT)",
            "Grains of what you played, played back shifted and scattered.",
            { knob ("Grain", 10.0f, 400.0f, 80.0f, " ms", 80.0f), knob ("Pitch", -24.0f, 24.0f, 0.0f, " st", 0.0f, 1),
              knob ("Position", 10.0f, 1500.0f, 120.0f, " ms", 200.0f), knob ("Jitter", 0.0f, 500.0f, 30.0f, " ms", 60.0f), percent ("Mix", 50.0f) }));

        // Time
        {
            std::vector<KnobSpec> k { knob ("Time", 10.0f, 2500.0f, 380.0f, " ms", 400.0f), choice ("Sync", divisionNames()),
                                      knob ("Feedback", 0.0f, 98.0f, 35.0f, " %"), knob ("Tone", 500.0f, 16000.0f, 6000.0f, " Hz", 4000.0f), percent ("Mix", 30.0f) };
            list.push_back (info ("echo", "Echo", Family::time, "gfx-delay", faustLib + ", de.fdelay",
                "A clean digital delay, its repeats darkening with Tone.", std::move (k), maker<EchoBlock>(), true));
        }

        list.push_back (faustInfo<Faust_Tapestop> ("tapestop", "Tape Stop", Family::time, "gfx-tape-stop", faustLib + ", ef.tapeStop (STK-4.3)",
            "Stop: the tape winds down. Put Stop on a footswitch.",
            { choice ("Stop", { "Run", "Stop" }), knob ("Time", 50.0f, 4000.0f, 800.0f, " ms", 800.0f), knob ("Curve", 0.05f, 2.0f, 1.0f, {}, 1.0f),
              knob ("Fade", 0.05f, 2.0f, 1.0f, {}, 1.0f) }));

        // Ambience
        list.push_back (faustInfo<Faust_Spring> ("spring", "Spring", Family::ambience, "gfx-spring-reverb", "Faust re.springreverb: Daniel Leonov (MIT)",
            "A spring tank: drip and boing.",
            { percent ("Dwell", 50.0f), percent ("Mix", 50.0f), percent ("Tone", 50.0f), percent ("Tension", 50.0f) }, true));

        list.push_back (faustInfo<Faust_Hall> ("hall", "Hall", Family::ambience, "gfx-hall", faustLib + ", re.zita_rev1_stereo: Fons Adriaensen's zita-rev1",
            "Zita's hall: separate decay times for the lows and the mids.",
            { knob ("Pre-delay", 0.0f, 200.0f, 40.0f, " ms", 50.0f), knob ("Low X", 50.0f, 1000.0f, 200.0f, " Hz", 250.0f),
              knob ("Damp", 1500.0f, 23000.0f, 6000.0f, " Hz", 6000.0f), knob ("Low Time", 1.0f, 8.0f, 3.0f, " s", 3.0f),
              knob ("Mid Time", 1.0f, 8.0f, 2.0f, " s", 3.0f), percent ("Mix", 30.0f) }, true));

        list.push_back (faustInfo<Faust_Plate> ("plate", "Plate", Family::ambience, "gfx-plate-reverb", "Faust re.dattorro_rev: Jakob Zerbian (STK-4.3)",
            "Dattorro's plate: dense and smooth.",
            { knob ("Pre-delay", 0.0f, 200.0f, 10.0f, " ms", 40.0f), percent ("Bandwidth", 80.0f), knob ("Decay", 0.0f, 99.0f, 60.0f, " %"),
              percent ("Damping", 30.0f), percent ("Diffusion", 70.0f), percent ("Mix", 30.0f) }, true));

        list.push_back (faustInfo<Faust_Jpverb> ("jpverb", "JPverb", Family::ambience, "gfx-ambient", "Faust re.jpverb: Julian Parker (MIT)",
            "A huge, modulated algorithmic space.",
            { knob ("Decay", 0.1f, 30.0f, 3.0f, " s", 3.0f), knob ("Size", 0.5f, 3.0f, 1.0f, {}, 1.2f), knob ("Damp", 0.0f, 99.0f, 0.0f, " %"),
              knob ("Diffusion", 0.0f, 99.0f, 70.0f, " %"), percent ("Mod Depth", 10.0f), knob ("Mod Rate", 0.0f, 10.0f, 2.0f, " Hz", 2.0f),
              knob ("Low", 0.1f, 2.0f, 1.0f, "x"), knob ("High", 0.1f, 2.0f, 1.0f, "x"), percent ("Mix", 30.0f) }, true));

        list.push_back (faustInfo<Faust_Greyhole> ("greyhole", "Greyhole", Family::ambience, "gfx-ambient", "Faust re.greyhole: Julian Parker (MIT)",
            "A diffuse delay-reverb that blurs into clouds.",
            { knob ("Delay", 0.001f, 1.45f, 0.2f, " s", 0.3f), knob ("Damp", 0.0f, 99.0f, 0.0f, " %"), knob ("Size", 0.5f, 3.0f, 1.0f, {}, 1.2f),
              knob ("Diffusion", 0.0f, 99.0f, 50.0f, " %"), percent ("Feedback", 90.0f), percent ("Mod Depth", 10.0f),
              knob ("Mod Rate", 0.0f, 10.0f, 2.0f, " Hz", 2.0f), percent ("Mix", 35.0f) }, true));

        list.push_back (faustInfo<Faust_Room> ("room", "Room", Family::ambience, "gfx-room", faustLib + ", re.stereo_freeverb: Jezar's Freeverb",
            "Freeverb's room: small to big, bright to dark.",
            { percent ("Size", 60.0f), percent ("Damp", 40.0f), percent ("Mix", 25.0f) }, true));
    }
}
