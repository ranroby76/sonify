// C:\workspace\SONIFY PLUGIN natura\src\NaturaPanels.h
#pragma once
//==============================================================================
//  NaturaPanels.h  -  the controls bound to parameters, and the frame layout
//  every tab is built from.
//
//  ParamSlider  a Grex GoldSlider wired to a host parameter (drags are one
//               automation gesture, right double-click resets)
//  ParamToggle  a square Grex button for an on/off parameter
//  ParamRadio   a column of square buttons for a choice parameter
//  Page         rows of titled frames (Grex's frame style); each frame lays
//               its controls out left to right
//==============================================================================
#include "PluginProcessor.h"
#include "GrexWidgets.h"

namespace natura
{
    inline juce::String guiLabel (P p)
    {
        switch (p)
        {
            case P::exc_strength: return "POWER";      case P::exc_hardness: return "HARD";
            case P::exc_noise: return "NOISE";         case P::exc_transient: return "ATTACK";
            case P::exc_sample: return "SAMPLED";      case P::exc_source: return "SOURCE";
            case P::vib_wheel: return "WHEEL";
            case P::tl_mix: return "MIX";              case P::tl_source: return "SOURCE";
            case P::tl_color: return "COLOR";          case P::tl_formant: return "FORMANT";
            case P::tl_detune: return "DETUNE";        case P::tl_sub: return "SUB";
            case P::res_decay: return "DECAY";         case P::res_damping: return "DAMPING";
            case P::res_inharm: return "STIFF";        case P::res_oddeven: return "ODD/EVEN";
            case P::res_unison: return "UNISON";       case P::res_detune: return "DETUNE";
            case P::body_size: return "SIZE";          case P::body_material: return "MATERIAL";
            case P::body_reso: return "RESO";          case P::body_mix: return "MIX";
            case P::chr_drive: return "DRIVE";         case P::chr_sat: return "WARMTH";   case P::chr_tone: return "TONE";
            case P::ss_hammerpos: return "HAMMER";     case P::ss_strings: return "STRINGS";
            case P::ss_detune: return "BEATING";       case P::ss_symp: return "SYMP";
            case P::ss_keyoff: return "KEY OFF";       case P::ss_pedalnoise: return "PEDAL";  case P::ss_lid: return "LID";
            case P::ep_type: return "TYPE";            case P::ep_chime: return "TINE";
            case P::ep_pickup: return "PICKUP";        case P::ep_bark: return "BARK";
            case P::ep_tremrate: return "TREM RT";     case P::ep_tremdepth: return "TREM";    case P::ep_keyoff: return "KEY OFF";
            case P::sb_mallet: return "MALLET";        case P::sb_material: return "BAR";
            case P::sb_strike: return "STRIKE";        case P::sb_tube: return "TUBE";
            case P::sb_motorrate: return "MOTOR RT";   case P::sb_motordepth: return "MOTOR";
            case P::tw_click: return "CLICK";          case P::tw_leak: return "LEAK";
            case P::tw_perc: return "PERC";            case P::tw_percdecay: return "P.DECAY";
            case P::tw_percvol: return "P.VOL";        case P::tw_vib: return "VIB/CHO";
            case P::po_principal: return "PRINC";      case P::po_flute: return "FLUTE";
            case P::po_reed: return "REED";            case P::po_mixture: return "MIXTURE";
            case P::po_bourdon: return "BOURDON";      case P::po_chiff: return "CHIFF";
            case P::po_wind: return "WIND";            case P::po_tremrate: return "TREM RT";  case P::po_tremdepth: return "TREM";
            case P::fr_bellows: return "BELLOWS";      case P::fr_register: return "REGISTER";
            case P::fr_musette: return "MUSETTE";      case P::fr_tone: return "REED";        case P::fr_chamber: return "CHAMBER";
            case P::pl_position: return "PLUCK";       case P::pl_plectrum: return "PICK";
            case P::pl_fret: return "FRET";            case P::pl_mute: return "MUTE";
            case P::pl_pickup: return "PICKUP";        case P::pl_ring: return "RING";
            case P::pl_amp: return "AMP";              case P::pl_drive: return "DRIVE";
            case P::pl_amptone: return "TONE";         case P::pl_keyoff: return "RELEASE";
            case P::bw_pressure: return "FORCE";       case P::bw_speed: return "SPEED";
            case P::bw_position: return "POSITION";    case P::bw_rosin: return "ROSIN";      case P::bw_attack: return "ATTACK";
            case P::lr_tension: return "TENSION";      case P::lr_growl: return "GROWL";
            case P::lr_bell: return "BELL";            case P::lr_mute: return "MUTE";
            case P::lr_wah: return "WAH";              case P::lr_bore: return "BORE";        case P::lr_buzz: return "BUZZ";
            case P::sr_stiffness: case P::dr_stiffness: return "REED";
            case P::sr_bite: case P::dr_bite: return "BITE";
            case P::sr_bore: return "BORE";            case P::dr_bore: return "BORE";
            case P::sr_subtone: return "SUBTONE";      case P::dr_nasal: return "NASAL";
            case P::sr_click: case P::dr_click: return "KEYS";
            case P::aj_breath: return "BREATH";        case P::aj_jet: return "JET";
            case P::aj_flutter: return "FLUTTER";      case P::aj_overblow: return "OVERBLOW"; case P::aj_pipe: return "PIPE";
            case P::s1_wave: case P::s2_wave: return "WAVE";
            case P::s1_oct: case P::s2_oct: return "OCT";
            case P::s1_semi: case P::s2_semi: return "SEMI";
            case P::s1_fine: case P::s2_fine: return "FINE";
            case P::s1_pw: case P::s2_pw: return "PW";
            case P::s1_level: case P::s2_level: return "LEVEL";
            case P::sy_fm: return "FM";                case P::sy_ring: return "RING";
            case P::sy_sync: return "SYNC";            case P::sy_sub: return "SUB";
            case P::sy_noise: return "NOISE";          case P::sy_drive: return "DRIVE";
            case P::sy_cutoff: return "CUTOFF";        case P::sy_reso: return "RESO";
            case P::sy_key: return "KEY";              case P::sy_env: return "ENV";          case P::sy_vel: return "VEL";
            case P::sy_a: return "A";                  case P::sy_d: return "D";
            case P::sy_s: return "S";                  case P::sy_r: return "R";              case P::sy_level: return "LEVEL";
            case P::amp_a: case P::fenv_a: return "A"; case P::amp_d: case P::fenv_d: return "D";
            case P::amp_s: case P::fenv_s: return "S"; case P::amp_r: case P::fenv_r: return "R";
            case P::amp_shape: return "SHAPE";         case P::amp_gain: return "GAIN";       case P::amp_var: return "VAR";
            case P::flt_cut: return "CUT";             case P::flt_res: return "RES";         case P::flt_key: return "KEY";
            case P::fenv_amt: return "AMT";            case P::port: return "PORT";           case P::voices: return "VOICES";
            case P::octave: return "OCT";              case P::range_lo: return "LOW";        case P::range_hi: return "HIGH";
            case P::alfo_rate: case P::flfo_rate: case P::vib_rate: return "RATE";
            case P::alfo_depth: case P::flfo_depth: case P::vib_depth: return "DEPTH";
            case P::alfo_delay: case P::flfo_delay: case P::vib_delay: return "DELAY";
            case P::ag_mode: return "MODE";            case P::ag_depth: return "DEPTH";
            case P::ag_time: return "TIME";            case P::ag_shape: return "SHAPE";
            case P::ag_every: return "EVERY";          case P::ag_odds: return "ODDS";        case P::ag_vel: return "VEL";
            case P::humanize: return "HUMANIZE";       case P::pb_range: return "BEND";
            case P::dyn_src: return "DYNAMICS";        case P::mw_target: return "M.WHEEL";
            case P::at_target: return "AFTERT.";       case P::leg_vel: return "LEG.VEL";
            case P::art_ks_base: return "BASE";        case P::art_cc_num: return "CC";
            case P::art_vel_split: return "SPLIT";     case P::art_vel_lo: return "SOFT";
            case P::art_vel_hi: return "HARD";         case P::art_auto_ms: return "REPEAT";
            case P::art_trill: return "TRILL";         case P::art_rate: return "RATE";
            case P::rot_drive: return "DRIVE";         case P::rot_balance: return "HORN";    case P::rot_mix: return "MIX";
            case P::pha_rate: case P::cho_rate: return "RATE";
            case P::pha_depth: case P::cho_depth: return "DEPTH";
            case P::pha_fb: return "FDBK";             case P::pha_mix: case P::cho_mix: case P::dly_mix: case P::rev_mix: return "MIX";
            case P::dly_time: return "TIME";           case P::dly_fb: return "FDBK";         case P::dly_tone: return "TONE";
            case P::rev_size: return "SIZE";           case P::rev_damp: return "DAMP";       case P::rev_predelay: return "PRE";
            case P::eq_low: return "LOW";              case P::eq_high: return "HIGH";
            case P::out_width: return "WIDTH";         case P::out_volume: return "VOLUME";
            default: break;
        }
        return juce::String (info (p).name).toUpperCase();
    }

    //==========================================================================
    class ParamSlider final : public GoldSlider
    {
    public:
        ParamSlider (NaturaProcessor& proc, P p)
            : GoldSlider (guiLabel (p), info (p).minV, info (p).maxV, info (p).defV, ""),
              id (p),
              attach (*proc.param (p), [this] (float v) { setValue (v, false); }, nullptr)
        {
            const auto& in = info (p);
            if (in.kind != Kind::Float || in.unit == Unit::Pct || in.unit == Unit::Bip || in.unit == Unit::VibDepth) setStep (1.0f);
            displayFn = [p] (float v) { return displayText (p, v); };
            onDragStart = [this] { attach.beginGesture(); };
            onChange    = [this] (float v) { attach.setValueAsPartOfGesture (v); };
            onDragEnd   = [this] { attach.endGesture(); };
            attach.sendInitialUpdate();
            setTooltip (in.name);
        }

        void setTooltip (const juce::String& t) { tip = t; }
        const P id;

    private:
        juce::ParameterAttachment attach;
        juce::String tip;
    };

    //==========================================================================
    class ParamToggle final : public juce::TextButton
    {
    public:
        ParamToggle (NaturaProcessor& proc, P p, const juce::String& caption)
            : juce::TextButton (caption),
              attach (*proc.param (p), [this] (float v) { state = v >= 0.5f; InstrEditStyle::styleSquareButton (*this, state); repaint(); }, nullptr)
        {
            onClick = [this] { attach.setValueAsCompleteGesture (state ? 0.0f : 1.0f); };
            attach.sendInitialUpdate();
        }

    private:
        bool state = false;
        juce::ParameterAttachment attach;
    };

    //==========================================================================
    class ParamRadio final : public juce::Component
    {
    public:
        ParamRadio (NaturaProcessor& proc, P p, const juce::StringArray& labels)
            : attach (*proc.param (p), [this] (float v) { select ((int) std::lround (v)); }, nullptr)
        {
            for (int i = 0; i < labels.size(); ++i)
            {
                auto* b = buttons.add (new juce::TextButton (labels[i]));
                b->onClick = [this, i] { attach.setValueAsCompleteGesture ((float) i); };
                addAndMakeVisible (b);
            }
            attach.sendInitialUpdate();
        }

        void resized() override
        {
            if (buttons.isEmpty()) return;
            auto r = getLocalBounds();
            const int h = juce::jmin (30, r.getHeight() / buttons.size());
            const int gap = juce::jmax (2, (r.getHeight() - h * buttons.size()) / juce::jmax (1, buttons.size() + 1));
            int y = gap;
            for (auto* b : buttons) { b->setBounds (0, y, r.getWidth(), h); y += h + gap; }
        }

    private:
        void select (int index)
        {
            for (int i = 0; i < buttons.size(); ++i) InstrEditStyle::styleSquareButton (*buttons[i], i == index);
        }
        juce::OwnedArray<juce::TextButton> buttons;
        juce::ParameterAttachment attach;
    };

    //==========================================================================
    //  Frames and pages
    //==========================================================================
    struct Frame
    {
        juce::String title;
        std::vector<juce::Component*> items;
        std::vector<float> units;         // width of each item, in slider widths
        juce::Rectangle<int> bounds;
        bool visible = true;

        float totalUnits() const { float t = 0.0f; for (float u : units) t += u; return t; }
    };

    class Page : public juce::Component
    {
    public:
        explicit Page (NaturaProcessor& p) : proc (p) {}

        void paint (juce::Graphics& g) override
        {
            for (const auto& f : frames)
                if (f.visible) InstrEditStyle::paintComponentFrame (g, f.bounds, f.title);
        }

        /** Rows of frame indices; each row shares the width by its units. */
        void layoutRows (const std::vector<std::vector<int>>& rows, juce::Rectangle<int> area)
        {
            const int gap = 10;
            const int rowH = (area.getHeight() - gap * ((int) rows.size() - 1)) / juce::jmax (1, (int) rows.size());
            int y = area.getY();
            for (const auto& row : rows)
            {
                float units = 0.0f;
                int count = 0;
                for (int fi : row) if (frames[(size_t) fi].visible) { units += frames[(size_t) fi].totalUnits(); ++count; }
                const int pad = InstrEditStyle::kFrameSidePad * 2;
                const float avail = (float) (area.getWidth() - gap * juce::jmax (0, count - 1) - pad * count);
                const float unitW = juce::jmin (88.0f, avail / juce::jmax (1.0f, units));
                // centre the row when the frames do not need the full width
                const float used = unitW * units + (float) (pad * count + gap * juce::jmax (0, count - 1));
                float x = (float) area.getX() + ((float) area.getWidth() - used) * 0.5f;
                for (int fi : row)
                {
                    auto& f = frames[(size_t) fi];
                    if (! f.visible) continue;
                    const int w = (int) std::lround (unitW * f.totalUnits()) + pad;
                    f.bounds = { (int) x, y, w, rowH };
                    placeItems (f);
                    x += (float) (w + gap);
                }
                y += rowH + gap;
            }
            repaint();
        }

        void setFrameVisible (int index, bool v)
        {
            auto& f = frames[(size_t) index];
            f.visible = v;
            for (auto* c : f.items) c->setVisible (v);
        }

    protected:
        int addFrame (const juce::String& title, std::vector<juce::Component*> items, std::vector<float> units = {})
        {
            Frame f;
            f.title = title;
            f.items = std::move (items);
            f.units = units.empty() ? std::vector<float> (f.items.size(), 1.0f) : std::move (units);
            for (auto* c : f.items) addAndMakeVisible (c);
            frames.push_back (std::move (f));
            return (int) frames.size() - 1;
        }

        ParamSlider* slider (P p) { return sliders.add (new ParamSlider (proc, p)); }
        ParamToggle* toggle (P p, const juce::String& caption) { return toggles.add (new ParamToggle (proc, p, caption)); }

        /** Toggles stacked in one column, as Grex does it. */
        juce::Component* column (std::vector<juce::Component*> items)
        {
            auto* c = columns.add (new Column());
            for (auto* i : items) c->addAndMakeVisible (i);
            c->items = std::move (items);
            return c;
        }

        NaturaProcessor& proc;
        std::vector<Frame> frames;
        juce::OwnedArray<ParamSlider> sliders;
        juce::OwnedArray<ParamToggle> toggles;

    private:
        struct Column : juce::Component
        {
            std::vector<juce::Component*> items;
            void resized() override
            {
                const int n = (int) items.size();
                if (n == 0) return;
                const int h = juce::jmin (30, getHeight() / n - 6);
                const int gap = (getHeight() - h * n) / (n + 1);
                int y = gap;
                for (auto* c : items) { c->setBounds (2, y, getWidth() - 4, h); y += h + gap; }
            }
        };

        void placeItems (const Frame& f)
        {
            const auto content = InstrEditStyle::componentFrameContent (f.bounds);
            const float total = juce::jmax (0.001f, f.totalUnits());
            float x = (float) content.getX();
            for (size_t i = 0; i < f.items.size(); ++i)
            {
                const float w = (float) content.getWidth() * f.units[i] / total;
                f.items[i]->setBounds ((int) x, content.getY(), (int) w, content.getHeight());
                x += w;
            }
        }

        juce::OwnedArray<Column> columns;
    };
}
