// D:\workspace\Fanan_web_hub\src\timberwolf\TwRegistry.h
#pragma once
//==============================================================================
//  TwRegistry.h  -  every effect Timber Wolf has.
//
//  One BlockInfo per effect, in the order the add menu shows them: by family,
//  then as their files list them.  The effects are in files by where their
//  code comes from (blocks\BlocksAxis.cpp: AXIS's own; BlocksFaust.cpp: the
//  Faust libraries; BlocksChokehold.cpp, BlocksSaike.cpp: their JSFX, ported;
//  BlocksOwn.cpp: Timber Wolf's own), each adding its effects here.
//
//  Built the first time it's asked for, on the message thread (the processor
//  asks in its constructor).
//==============================================================================
#include "TwBlock.h"

namespace tw::registry
{
    const std::vector<BlockInfo>& all();
    const BlockInfo* find (const juce::String& id);

    /** The split and the blend: not effects, but the panel shows their settings as knobs. */
    const BlockInfo& splitInfo();
    const BlockInfo& blendInfo();

    // Each source's file.
    void addAxis (std::vector<BlockInfo>&);
    void addFaust (std::vector<BlockInfo>&);
    void addChokehold (std::vector<BlockInfo>&);
    void addSaike (std::vector<BlockInfo>&);
    void addOwn (std::vector<BlockInfo>&);
}
