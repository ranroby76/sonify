#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================================
//  AXIS — parameter definitions
//
//  Single source of truth. Every id here matches an element id in the web UI,
//  so the bridge binds by name with no translation layer.
// ============================================================================
namespace axis
{

// ---- modules, in signal order ---------------------------------------------
enum class ModuleId { gate = 0, comp, drive, amp, space, count };

inline constexpr int numModules = static_cast<int> (ModuleId::count);

inline juce::StringArray moduleNames()
{
    return { "GATE", "COMP", "DRIVE", "AMP", "SPACE" };
}

// ---- parameter ids ---------------------------------------------------------
namespace pid
{
    // gate
    inline constexpr auto gateMode     = "GATE_MODE";
    inline constexpr auto gateThresh   = "GATE_THRESH";
    inline constexpr auto gateDepth    = "GATE_DEPTH";
    inline constexpr auto gateRelease  = "GATE_RELEASE";
    inline constexpr auto gateHold     = "GATE_HOLD";
    inline constexpr auto gateScHpf    = "GATE_SC_HPF";
    inline constexpr auto gateHiss     = "GATE_HISS";
    inline constexpr auto gateLearn    = "GATE_LEARN";
    inline constexpr auto gateRange    = "GATE_RANGE";
    inline constexpr auto gateBypass   = "GATE_BYPASS";

    // comp
    inline constexpr auto compCircuit  = "COMP_CIRCUIT";
    inline constexpr auto compThresh   = "COMP_THRESH";
    inline constexpr auto compRatio    = "COMP_RATIO";
    inline constexpr auto compMakeup   = "COMP_MAKEUP";
    inline constexpr auto compAttack   = "COMP_ATTACK";
    inline constexpr auto compRelease  = "COMP_RELEASE";
    inline constexpr auto compAuto     = "COMP_AUTO";
    inline constexpr auto compScHpf    = "COMP_SC_HPF";
    inline constexpr auto compParal    = "COMP_PARAL";
    inline constexpr auto compMix      = "COMP_MIX";
    inline constexpr auto compBypass   = "COMP_BYPASS";

    // drive
    inline constexpr auto drvVoice     = "DRV_VOICE";
    inline constexpr auto drvTone      = "DRV_TONE";
    inline constexpr auto drvGain      = "DRV_GAIN";
    inline constexpr auto drvLevel     = "DRV_LEVEL";
    inline constexpr auto drvBody      = "DRV_BODY";
    inline constexpr auto drvBite      = "DRV_BITE";
    inline constexpr auto drvDynamic   = "DRV_DYNAMIC";
    inline constexpr auto drvStereo    = "DRV_STEREO";
    inline constexpr auto drvMorph     = "DRV_MORPH";
    inline constexpr auto drvMix       = "DRV_MIX";
    inline constexpr auto drvBypass    = "DRV_BYPASS";

    // amp (AmpModel.h): the amp, and its cabinet
    inline constexpr auto ampVoice     = "AMP_VOICE";
    inline constexpr auto ampGain      = "AMP_GAIN";
    inline constexpr auto ampBass      = "AMP_BASS";
    inline constexpr auto ampMid       = "AMP_MID";
    inline constexpr auto ampTreble    = "AMP_TREBLE";
    inline constexpr auto ampPresence  = "AMP_PRESENCE";
    inline constexpr auto ampMaster    = "AMP_MASTER";
    inline constexpr auto ampBright    = "AMP_BRIGHT";
    inline constexpr auto ampTight     = "AMP_TIGHT";
    inline constexpr auto ampCab       = "AMP_CAB";
    inline constexpr auto ampBypass    = "AMP_BYPASS";

    // space
    inline constexpr auto spcType      = "SPC_TYPE";
    inline constexpr auto spcPredly    = "SPC_PREDLY";
    inline constexpr auto spcDecay     = "SPC_DECAY";
    inline constexpr auto spcSize      = "SPC_SIZE";
    inline constexpr auto spcDamp      = "SPC_DAMP";
    inline constexpr auto spcWidth     = "SPC_WIDTH";
    inline constexpr auto spcShimmer   = "SPC_SHIMMER";
    inline constexpr auto spcFreeze    = "SPC_FREEZE";
    inline constexpr auto spcDuck      = "SPC_DUCK";
    inline constexpr auto spcMix       = "SPC_MIX";
    inline constexpr auto spcBypass    = "SPC_BYPASS";

    // the cabinet: one of the modelled ones, or the IR (the AMP's, on the rail)
    inline constexpr auto cabType      = "CAB_TYPE";
    inline constexpr auto cabMicX      = "CAB_MIC_X";
    inline constexpr auto cabMicY      = "CAB_MIC_Y";
}

// ---- helpers ---------------------------------------------------------------
inline juce::NormalisableRange<float> linear (float lo, float hi)
{
    return { lo, hi };
}

/** Range skewed so the lower values get more of the travel — right for times
    and frequencies, where the useful detail sits at the bottom. */
inline juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r { lo, hi };
    r.setSkewForCentre (centre);
    return r;
}

inline std::unique_ptr<juce::AudioParameterFloat> makeFloat (
    juce::StringRef id, juce::StringRef name,
    juce::NormalisableRange<float> range, float def, juce::StringRef unit)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def,
        juce::AudioParameterFloatAttributes().withLabel (unit));
}

inline std::unique_ptr<juce::AudioParameterBool> makeBool (
    juce::StringRef id, juce::StringRef name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { id, 1 }, name, def);
}

inline std::unique_ptr<juce::AudioParameterChoice> makeChoice (
    juce::StringRef id, juce::StringRef name,
    juce::StringArray options, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { id, 1 }, name, std::move (options), def);
}

// ---- the full layout --------------------------------------------------------
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---------------- GATE ----------------
    layout.add (makeChoice (pid::gateMode,    "Gate Mode",    { "FAST", "SMOOTH", "ADAPT" }, 2));
    layout.add (makeFloat  (pid::gateThresh,  "Gate Thresh",  linear (-80.0f, 0.0f),      -46.0f, "dB"));
    layout.add (makeFloat  (pid::gateDepth,   "Gate Depth",   linear (0.0f, 100.0f),       88.0f, "%"));
    layout.add (makeFloat  (pid::gateRelease, "Gate Release", skewed (5.0f, 800.0f, 150.0f), 160.0f, "ms"));
    layout.add (makeFloat  (pid::gateHold,    "Gate Hold",    skewed (0.0f, 500.0f, 80.0f),  40.0f, "ms"));
    layout.add (makeFloat  (pid::gateScHpf,   "Gate SC HPF",  skewed (20.0f, 800.0f, 160.0f), 120.0f, "Hz"));
    layout.add (makeBool   (pid::gateHiss,    "Gate Hiss Filter", true));
    layout.add (makeBool   (pid::gateLearn,   "Gate Auto-Learn",  false));
    layout.add (makeFloat  (pid::gateRange,   "Gate Range",   linear (0.0f, 100.0f),      100.0f, "%"));
    layout.add (makeBool   (pid::gateBypass,  "Gate Bypass",  false));

    // ---------------- COMP ----------------
    layout.add (makeChoice (pid::compCircuit, "Comp Circuit", { "OPTO", "FET", "VCA" }, 0));
    layout.add (makeFloat  (pid::compThresh,  "Comp Thresh",  linear (-48.0f, 0.0f),      -18.0f, "dB"));
    layout.add (makeFloat  (pid::compRatio,   "Comp Ratio",   skewed (1.0f, 20.0f, 4.0f),   4.0f, ":1"));
    layout.add (makeFloat  (pid::compMakeup,  "Comp Makeup",  linear (0.0f, 24.0f),         6.0f, "dB"));
    layout.add (makeFloat  (pid::compAttack,  "Comp Attack",  skewed (1.0f, 120.0f, 20.0f), 14.0f, "ms"));
    layout.add (makeFloat  (pid::compRelease, "Comp Release", skewed (20.0f, 1200.0f, 250.0f), 280.0f, "ms"));
    layout.add (makeBool   (pid::compAuto,    "Comp Auto Gain", true));
    layout.add (makeBool   (pid::compScHpf,   "Comp SC HPF",    false));
    layout.add (makeBool   (pid::compParal,   "Comp Parallel",  false));
    layout.add (makeFloat  (pid::compMix,     "Comp Mix",     linear (0.0f, 100.0f),      100.0f, "%"));
    layout.add (makeBool   (pid::compBypass,  "Comp Bypass",  false));

    // ---------------- DRIVE ----------------
    layout.add (makeChoice (pid::drvVoice,    "Drive Voice",  { "WARM", "OPEN", "TIGHT" }, 0));
    layout.add (makeFloat  (pid::drvTone,     "Drive Tone",   linear (0.0f, 100.0f), 42.0f, ""));
    layout.add (makeFloat  (pid::drvGain,     "Drive Gain",   linear (0.0f, 100.0f), 64.0f, ""));
    layout.add (makeFloat  (pid::drvLevel,    "Drive Level",  linear (0.0f, 100.0f), 55.0f, ""));
    layout.add (makeFloat  (pid::drvBody,     "Drive Body",   linear (0.0f, 100.0f), 38.0f, ""));
    layout.add (makeFloat  (pid::drvBite,     "Drive Bite",   linear (0.0f, 100.0f), 71.0f, ""));
    layout.add (makeBool   (pid::drvDynamic,  "Drive Dynamic Touch", true));
    layout.add (makeBool   (pid::drvStereo,   "Drive True Stereo",   false));
    layout.add (makeBool   (pid::drvMorph,    "Drive Morph Link",    false));
    layout.add (makeFloat  (pid::drvMix,      "Drive Mix",    linear (0.0f, 100.0f), 100.0f, "%"));
    layout.add (makeBool   (pid::drvBypass,   "Drive Bypass", false));

    // ---------------- AMP ----------------
    layout.add (makeChoice (pid::ampVoice,    "Amp Voice",    { "CLEAN", "CRUNCH", "CHIME", "LEAD", "MODERN" }, 1));
    layout.add (makeFloat  (pid::ampGain,     "Amp Gain",     linear (0.0f, 100.0f),  50.0f, ""));
    layout.add (makeFloat  (pid::ampBass,     "Amp Bass",     linear (0.0f, 100.0f),  50.0f, ""));
    layout.add (makeFloat  (pid::ampMid,      "Amp Mid",      linear (0.0f, 100.0f),  50.0f, ""));
    layout.add (makeFloat  (pid::ampTreble,   "Amp Treble",   linear (0.0f, 100.0f),  50.0f, ""));
    layout.add (makeFloat  (pid::ampPresence, "Amp Presence", linear (0.0f, 100.0f),  50.0f, ""));
    layout.add (makeFloat  (pid::ampMaster,   "Amp Master",   linear (0.0f, 100.0f),  50.0f, ""));
    layout.add (makeBool   (pid::ampBright,   "Amp Bright",   false));
    layout.add (makeBool   (pid::ampTight,    "Amp Tight",    false));
    layout.add (makeBool   (pid::ampCab,      "Amp Cabinet",  true));
    layout.add (makeBool   (pid::ampBypass,   "Amp Bypass",   false));

    // ---------------- SPACE ----------------
    layout.add (makeChoice (pid::spcType,     "Space Type",   { "PLATE", "HALL", "SPRING" }, 2));
    layout.add (makeFloat  (pid::spcPredly,   "Space Pre-Delay", skewed (0.0f, 250.0f, 50.0f), 24.0f, "ms"));
    layout.add (makeFloat  (pid::spcDecay,    "Space Decay",  linear (0.0f, 100.0f), 46.0f, ""));
    layout.add (makeFloat  (pid::spcSize,     "Space Size",   linear (0.0f, 100.0f), 58.0f, ""));
    layout.add (makeFloat  (pid::spcDamp,     "Space Damp",   linear (0.0f, 100.0f), 62.0f, ""));
    layout.add (makeFloat  (pid::spcWidth,    "Space Width",  linear (0.0f, 100.0f), 80.0f, ""));
    layout.add (makeBool   (pid::spcShimmer,  "Space Shimmer", false));
    layout.add (makeBool   (pid::spcFreeze,   "Space Freeze",  false));
    layout.add (makeBool   (pid::spcDuck,     "Space Duck",    true));
    layout.add (makeFloat  (pid::spcMix,      "Space Mix",    linear (0.0f, 100.0f), 28.0f, "%"));
    layout.add (makeBool   (pid::spcBypass,   "Space Bypass", false));

    // ---------------- CABINET ----------------
    layout.add (makeChoice (pid::cabType,     "Cabinet",      { "1x12 OPEN", "2x12 ALNICO", "4x12 VINTAGE", "4x12 MODERN", "IR" }, 2));
    layout.add (makeFloat  (pid::cabMicX,     "Cab Mic X",    linear (0.0f, 100.0f), 36.0f, "%"));
    layout.add (makeFloat  (pid::cabMicY,     "Cab Mic Y",    linear (0.0f, 100.0f), 50.0f, "%"));

    return layout;
}

} // namespace axis
