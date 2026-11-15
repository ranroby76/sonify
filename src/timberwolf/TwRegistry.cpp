// D:\workspace\Fanan_web_hub\src\timberwolf\TwRegistry.cpp
#include "TwRegistry.h"

#include <algorithm>

namespace tw::registry
{
    const std::vector<BlockInfo>& all()
    {
        static const std::vector<BlockInfo> list = []
        {
            std::vector<BlockInfo> l;
            addAxis (l);
            addFaust (l);
            addChokehold (l);
            addSaike (l);
            addOwn (l);

            // By family, each family's effects in the order their files gave them.
            std::stable_sort (l.begin(), l.end(), [] (const BlockInfo& a, const BlockInfo& b)
                              { return (int) a.family < (int) b.family; });

           #if JUCE_DEBUG
            for (size_t i = 0; i < l.size(); ++i)
            {
                jassert (l[i].knobs.size() <= (size_t) maxKnobs);
                for (size_t j = i + 1; j < l.size(); ++j)
                    jassert (l[i].id != l[j].id);   // ids are kept in presets: they must be unique
            }
           #endif

            return l;
        }();

        return list;
    }

    const BlockInfo* find (const juce::String& id)
    {
        for (const auto& b : all())
            if (b.id == id)
                return &b;

        return nullptr;
    }

    const BlockInfo& splitInfo()
    {
        static const BlockInfo info = []
        {
            BlockInfo b;
            b.id = "split";
            b.name = "Split";
            b.family = Family::utility;
            b.icon = "gfx-ab-split";
            b.description = "Sends the signal down two paths: all of it to both, or the lows to A and the highs to B.";
            b.knobs = { choice ("Type", { "Y split", "Crossover" }), knob ("Crossover", 80.0f, 8000.0f, 700.0f, " Hz", 700.0f) };
            b.hidden = true;
            return b;
        }();

        return info;
    }

    const BlockInfo& blendInfo()
    {
        static const BlockInfo info = []
        {
            BlockInfo b;
            b.id = "blend";
            b.name = "Blend";
            b.family = Family::utility;
            b.icon = "gfx-blend";
            b.description = "Where the two paths meet: each at its level and pan.";
            b.knobs = { knob ("Level A", -60.0f, 12.0f, 0.0f, " dB", -6.0f, 1), knob ("Level B", -60.0f, 12.0f, 0.0f, " dB", -6.0f, 1),
                        knob ("Pan A", -100.0f, 100.0f, 0.0f), knob ("Pan B", -100.0f, 100.0f, 0.0f),
                        choice ("B polarity", { "Normal", "Inverted" }) };
            b.hidden = true;
            return b;
        }();

        return info;
    }
}
