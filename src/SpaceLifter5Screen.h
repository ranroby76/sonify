// D:\workspace\Fanan_web_hub\src\SpaceLifter5Screen.h

#pragma once

#include "BuiltinPlugins.h"

#include <memory>

//==============================================================================
/**
    Space Lifter 5's window: Sonify's plugin header (ON/OFF and PRESETS, as every plugin window
    has) over the NebulaVerb faceplate itself (NebulaFaceplate.*, compiled unchanged from the
    NebulaVerb project), on the built-in's parameters. It looks exactly as the NebulaVerb VST3
    does; only the brand words on the plate are Space Lifter 5's.

    BuiltinScreen::install() gives it to Space Lifter 5 in place of a designed panel.
*/
namespace SpaceLifter5Screen
{
    std::unique_ptr<BuiltinPlugin::Screen> create (BuiltinPlugin& plugin);
}
