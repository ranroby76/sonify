// D:\workspace\Fanan_web_hub\src\MaterialArchetypes.h
// The built-in material looks: ready-made surfaces (material, colours, grain, patterns and
// layer style) with the light, background and text colours that go with them. Edit UI's
// Material Look list and Fanan AI's edit_layout put them on; a look only sets values, the
// painter (stella_paint.h) draws them as it draws any shape.
// Each look also has its matching controls: a 3D knob, slider, push button and lever (the 3D
// Studio's settings, Model3D), which Looks makes into built-in looks ("Aged Brass knob"...),
// and its displays and fonts: lamps, selectors, readouts, LED digits, keyboards, meters, the
// fonts and title lettering, and the panel camera; and its painted panel (PanelPainter's
// recipe, its parts placed from the window's layout).

#pragma once

#include "GuiLayout.h"
#include "ObjectPaint.h"
#include "PanelParts.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace MaterialArchetypes
{
    struct Entry
    {
        const char* name;
        const char* family;   // metal, plastic, cloth, wood, stone, composite or glass
        const char* json;
    };

    inline const std::vector<Entry>& all()
    {
        static const std::vector<Entry> looks {
            { "Dark Brushed Aluminium", "metal", R"ARCH({
  "format": 1,
  "name": "Dark Brushed Aluminium",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF30373D",
    "bottom": "#FF171C21"
  },
  "surface": {
    "material": "aluminium",
    "finish": "brushed",
    "color": "#FF4A5158",
    "color2": "#FF22282E",
    "grain": 0.8,
    "scale": 1,
    "shine": 0.5,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 38.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 36.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 50.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 38.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FFE6A34A"
})ARCH" },
            { "Black Anodized", "metal", R"ARCH({
  "format": 1,
  "name": "Black Anodized",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF1B212A",
    "bottom": "#FF090C12"
  },
  "surface": {
    "material": "anodized",
    "finish": "satin",
    "color": "#FF1C232E",
    "color2": "#FF0A0E15",
    "grain": 0.14,
    "scale": 0.9,
    "shine": 0.5,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.07,
        "size": 7,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 28.0,
        "direction": "up",
        "size": 2.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 28.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 42.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 48.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FF55C7D9"
})ARCH" },
            { "Aged Brass", "metal", R"ARCH({
  "format": 1,
  "name": "Aged Brass",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF3A3024",
    "bottom": "#FF17120D"
  },
  "surface": {
    "material": "brass",
    "finish": "satin",
    "color": "#FF665238",
    "color2": "#FF342719",
    "grain": 0.42,
    "scale": 1.1,
    "shine": 0.42,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.09,
        "size": 8,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 42.0,
        "direction": "up",
        "size": 4.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 42.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 54.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 42.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FFFFC36A"
})ARCH" },
            { "Silver Faceplate", "metal", R"ARCH({
  "format": 1,
  "name": "Silver Faceplate",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FFD5D8DC",
    "bottom": "#FFB5BAC0"
  },
  "surface": {
    "material": "silver",
    "finish": "brushed",
    "color": "#FFB9BDC2",
    "color2": "#FFA6ABB1",
    "grain": 0.38,
    "scale": 1.0,
    "shine": 0.54,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.065,
        "size": 7,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 38.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 38.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 38.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 22.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF252B32",
  "accentColor": "#FF852B39"
})ARCH" },
            { "Workshop Hammertone", "metal", R"ARCH({
  "format": 1,
  "name": "Workshop Hammertone",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF33495B",
    "bottom": "#FF1C2D3C"
  },
  "surface": {
    "material": "hammertone",
    "finish": "hammered",
    "color": "#FF3B5367",
    "color2": "#FF2B4052",
    "grain": 0.85,
    "scale": 1.12,
    "shine": 0.32,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.08,
        "size": 7,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 45.0,
        "direction": "up",
        "size": 4.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 32.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 48.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 34.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFFFF2D9",
  "accentColor": "#FFFFB94E"
})ARCH" },
            { "Red Anodized", "metal", R"ARCH({
  "format": 1,
  "name": "Red Anodized",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 34
  },
  "background": {
    "top": "#FF53242E",
    "bottom": "#FF231118"
  },
  "surface": {
    "material": "anodized",
    "finish": "satin",
    "color": "#FF682B36",
    "color2": "#FF35151E",
    "grain": 0.24,
    "scale": 1.05,
    "shine": 0.58,
    "radius": 5,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.045,
        "size": 5,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 18.0,
        "direction": "up",
        "size": 2.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 34.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 22.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 32.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 24.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 3.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF7ECED",
  "accentColor": "#FF95F3EA"
})ARCH" },
            { "Blue Anodized", "metal", R"ARCH({
  "format": 1,
  "name": "Blue Anodized",
  "family": "metal",
  "light": {
    "angle": 118,
    "altitude": 39
  },
  "background": {
    "top": "#FF183D61",
    "bottom": "#FF101D34"
  },
  "surface": {
    "material": "anodized",
    "finish": "satin",
    "color": "#FF234D76",
    "color2": "#FF172A4B",
    "grain": 0.2,
    "scale": 1.0,
    "shine": 0.53,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.058,
        "size": 7,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 25.0,
        "direction": "up",
        "size": 3.0,
        "soften": 2.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 39.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 29.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 38.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 31.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0F6FF",
  "accentColor": "#FFFFCB74"
})ARCH" },
            { "Gold Anodized", "metal", R"ARCH({
  "format": 1,
  "name": "Gold Anodized",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 44
  },
  "background": {
    "top": "#FFDED1B2",
    "bottom": "#FFB4A076"
  },
  "surface": {
    "material": "anodized",
    "finish": "satin",
    "color": "#FFC6AB73",
    "color2": "#FF997B45",
    "grain": 0.27,
    "scale": 1.0,
    "shine": 0.52,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.071,
        "size": 9,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 32.0,
        "direction": "up",
        "size": 4.0,
        "soften": 3.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 44.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 36.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 44.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 38.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF29241C",
  "accentColor": "#FF263B68"
})ARCH" },
            { "Mirror Chrome", "metal", R"ARCH({
  "format": 1,
  "name": "Mirror Chrome",
  "family": "metal",
  "light": {
    "angle": 118,
    "altitude": 49
  },
  "background": {
    "top": "#FFDAE3E9",
    "bottom": "#FF9CAAB8"
  },
  "surface": {
    "material": "chrome",
    "finish": "polished",
    "color": "#FFB8C4D0",
    "color2": "#FF84939F",
    "grain": 0.05,
    "scale": 0.95,
    "shine": 0.95,
    "radius": 8,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.084,
        "size": 11,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 39.0,
        "direction": "up",
        "size": 5.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 49.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 43.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 50.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 6.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF1D2932",
  "accentColor": "#FF602642"
})ARCH" },
            { "Brushed Steel", "metal", R"ARCH({
  "format": 1,
  "name": "Brushed Steel",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 54
  },
  "background": {
    "top": "#FFBFC7CB",
    "bottom": "#FF879397"
  },
  "surface": {
    "material": "steel",
    "finish": "brushed",
    "color": "#FF909A9F",
    "color2": "#FF7E888E",
    "grain": 0.42,
    "scale": 1.1,
    "shine": 0.49,
    "radius": 9,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.097,
        "size": 13,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 46.0,
        "direction": "up",
        "size": 6.0,
        "soften": 2.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 54.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 50.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 56.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 52.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 7.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF1B2529",
  "accentColor": "#FF194D72"
})ARCH" },
            { "Gunmetal Grey", "metal", R"ARCH({
  "format": 1,
  "name": "Gunmetal Grey",
  "family": "metal",
  "light": {
    "angle": 118,
    "altitude": 34
  },
  "background": {
    "top": "#FF2D3746",
    "bottom": "#FF161D26"
  },
  "surface": {
    "material": "gunmetal",
    "finish": "satin",
    "color": "#FF333F50",
    "color2": "#FF1B2936",
    "grain": 0.28,
    "scale": 0.94,
    "shine": 0.56,
    "radius": 5,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.045,
        "size": 5,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 53.0,
        "direction": "up",
        "size": 2.0,
        "soften": 3.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 34.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 22.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 62.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 59.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "choke": 0.0,
        "size": 3.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0F5F7",
  "accentColor": "#FFFFBD6F"
})ARCH" },
            { "Liquid Titanium", "metal", R"ARCH({
  "format": 1,
  "name": "Liquid Titanium",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 39
  },
  "background": {
    "top": "#FF373C4A",
    "bottom": "#FF1B202B"
  },
  "surface": {
    "material": "titanium",
    "finish": "liquid",
    "color": "#FF454857",
    "color2": "#FF354F68",
    "grain": 0.08,
    "scale": 1.26,
    "shine": 0.86,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 19.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 39.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 29.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 32.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 24.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF1F4FB",
  "accentColor": "#FFCDAFFF"
})ARCH" },
            { "Copper Patina", "metal", R"ARCH({
  "format": 1,
  "name": "Copper Patina",
  "family": "metal",
  "light": {
    "angle": 118,
    "altitude": 44
  },
  "background": {
    "top": "#FF60392D",
    "bottom": "#FF25191A"
  },
  "surface": {
    "material": "copper",
    "finish": "satin",
    "color": "#FF8A5137",
    "color2": "#FF543829",
    "grain": 0.36,
    "scale": 1.04,
    "shine": 0.6,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.071,
        "size": 9,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 26.0,
        "direction": "up",
        "size": 4.0,
        "soften": 2.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 44.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 36.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 38.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 31.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFFFF4E6",
  "accentColor": "#FFB5EBE0"
})ARCH" },
            { "Cast Iron", "metal", R"ARCH({
  "format": 1,
  "name": "Cast Iron",
  "family": "metal",
  "light": {
    "angle": 125,
    "altitude": 49
  },
  "background": {
    "top": "#FF2A3032",
    "bottom": "#FF111719"
  },
  "surface": {
    "material": "iron",
    "finish": "natural",
    "color": "#FF34393C",
    "color2": "#FF262A2B",
    "grain": 0.79,
    "scale": 1.31,
    "shine": 0.17,
    "radius": 8,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 33.0,
        "direction": "up",
        "size": 5.0,
        "soften": 3.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 49.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 43.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 44.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 38.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "choke": 0.0,
        "size": 6.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFEAE6DB",
  "accentColor": "#FFFFBE65"
})ARCH" },
            { "Matte Black Polymer", "plastic", R"ARCH({
  "format": 1,
  "name": "Matte Black Polymer",
  "family": "plastic",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF242424",
    "bottom": "#FF0E0E0E"
  },
  "surface": {
    "material": "matteplastic",
    "finish": "matte",
    "color": "#FF262626",
    "color2": "#FF121212",
    "grain": 0.32,
    "scale": 0.85,
    "shine": 0.05,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.06,
        "size": 5,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 18.0,
        "direction": "up",
        "size": 2.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 22.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 38.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 48.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FF79B7A6"
})ARCH" },
            { "Vintage Bakelite", "plastic", R"ARCH({
  "format": 1,
  "name": "Vintage Bakelite",
  "family": "plastic",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF28160E",
    "bottom": "#FF0E0705"
  },
  "surface": {
    "material": "bakelite",
    "finish": "polished",
    "color": "#FF3A1F13",
    "color2": "#FF170B06",
    "grain": 0.12,
    "scale": 0.9,
    "shine": 0.62,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.04,
        "size": 9,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 48.0,
        "direction": "up",
        "size": 5.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 48.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 58.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 34.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FFFFB65B"
})ARCH" },
            { "Soft Touch Rubber", "plastic", R"ARCH({
  "format": 1,
  "name": "Soft Touch Rubber",
  "family": "plastic",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF262320",
    "bottom": "#FF0E0C0A"
  },
  "surface": {
    "material": "rubber",
    "finish": "matte",
    "color": "#FF2B2723",
    "color2": "#FF14110F",
    "grain": 0.55,
    "scale": 0.8,
    "shine": 0.02,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "dots",
        "blend": "overlay",
        "opacity": 0.14,
        "size": 8,
        "angle": 45
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 14.0,
        "direction": "up",
        "size": 2.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 18.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 34.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 54.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FF8BC4D0"
})ARCH" },
            { "Cream Console", "plastic", R"ARCH({
  "format": 1,
  "name": "Cream Console",
  "family": "plastic",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FFF2E8D3",
    "bottom": "#FFDDCFB4"
  },
  "surface": {
    "material": "powder",
    "finish": "satin",
    "color": "#FFE5D6B7",
    "color2": "#FFD6C5A2",
    "grain": 0.22,
    "scale": 1.0,
    "shine": 0.27,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.065,
        "size": 7,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 26.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 25.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 33.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 18.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF453222",
  "accentColor": "#FF98501A"
})ARCH" },
            { "Burnt Orange Seventies", "plastic", R"ARCH({
  "format": 1,
  "name": "Burnt Orange Seventies",
  "family": "plastic",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF77300E",
    "bottom": "#FF4F1C08"
  },
  "surface": {
    "material": "powder",
    "finish": "satin",
    "color": "#FF883610",
    "color2": "#FF732B0B",
    "grain": 0.25,
    "scale": 1.0,
    "shine": 0.27,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.08,
        "size": 7,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 32.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 28.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 43.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 28.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFFFF1D4",
  "accentColor": "#FFFFDE79"
})ARCH" },
            { "White Studio", "plastic", R"ARCH({
  "format": 1,
  "name": "White Studio",
  "family": "plastic",
  "light": {
    "angle": 118,
    "altitude": 54
  },
  "background": {
    "top": "#FFF6F9FA",
    "bottom": "#FFD6E0E5"
  },
  "surface": {
    "material": "powder",
    "finish": "matte",
    "color": "#FFE7EDF0",
    "color2": "#FFCDD8DD",
    "grain": 0.13,
    "scale": 0.85,
    "shine": 0.1,
    "radius": 9,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.097,
        "size": 13,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 40.0,
        "direction": "up",
        "size": 6.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 54.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 50.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 50.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 7.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF27323A",
  "accentColor": "#FF125F82"
})ARCH" },
            { "Mint Retro", "plastic", R"ARCH({
  "format": 1,
  "name": "Mint Retro",
  "family": "plastic",
  "light": {
    "angle": 125,
    "altitude": 34
  },
  "background": {
    "top": "#FFDCF0E5",
    "bottom": "#FFABC9BB"
  },
  "surface": {
    "material": "powder",
    "finish": "satin",
    "color": "#FFB8D9CC",
    "color2": "#FF8DBEAA",
    "grain": 0.22,
    "scale": 0.96,
    "shine": 0.42,
    "radius": 5,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.045,
        "size": 5,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 47.0,
        "direction": "up",
        "size": 2.0,
        "soften": 2.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 34.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 22.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 56.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 52.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 3.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF2B4338",
  "accentColor": "#FF7C3C2C"
})ARCH" },
            { "Mustard Seventies", "plastic", R"ARCH({
  "format": 1,
  "name": "Mustard Seventies",
  "family": "plastic",
  "light": {
    "angle": 118,
    "altitude": 39
  },
  "background": {
    "top": "#FF845C21",
    "bottom": "#FF4B3418"
  },
  "surface": {
    "material": "powder",
    "finish": "satin",
    "color": "#FF79521A",
    "color2": "#FF53380F",
    "grain": 0.34,
    "scale": 1.13,
    "shine": 0.4,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.058,
        "size": 7,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 54.0,
        "direction": "up",
        "size": 3.0,
        "soften": 3.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 39.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 29.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 62.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 59.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFFFF2DA",
  "accentColor": "#FFFFE6B8"
})ARCH" },
            { "Red Lacquer", "plastic", R"ARCH({
  "format": 1,
  "name": "Red Lacquer",
  "family": "plastic",
  "light": {
    "angle": 125,
    "altitude": 44
  },
  "background": {
    "top": "#FF4B141E",
    "bottom": "#FF230B10"
  },
  "surface": {
    "material": "plastic",
    "finish": "polished",
    "color": "#FF641D28",
    "color2": "#FF370D16",
    "grain": 0.06,
    "scale": 1.03,
    "shine": 0.87,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.071,
        "size": 9,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 20.0,
        "direction": "up",
        "size": 4.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 44.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 36.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 32.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 24.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFFFF2EB",
  "accentColor": "#FFFFC879"
})ARCH" },
            { "Ivory Ceramic", "plastic", R"ARCH({
  "format": 1,
  "name": "Ivory Ceramic",
  "family": "plastic",
  "light": {
    "angle": 118,
    "altitude": 49
  },
  "background": {
    "top": "#FFFAF4E9",
    "bottom": "#FFE0D3BD"
  },
  "surface": {
    "material": "ceramic",
    "finish": "polished",
    "color": "#FFF1E6D2",
    "color2": "#FFD6C6AB",
    "grain": 0.08,
    "scale": 0.88,
    "shine": 0.74,
    "radius": 8,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.084,
        "size": 11,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 27.0,
        "direction": "up",
        "size": 5.0,
        "soften": 2.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 49.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 43.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 38.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 31.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 6.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF453423",
  "accentColor": "#FF3F5770"
})ARCH" },
            { "Black Tolex", "cloth", R"ARCH({
  "format": 1,
  "name": "Black Tolex",
  "family": "cloth",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF242527",
    "bottom": "#FF0E0F11"
  },
  "surface": {
    "material": "tolex",
    "finish": "natural",
    "color": "#FF27282A",
    "color2": "#FF171819",
    "grain": 0.9,
    "scale": 1.2,
    "shine": 0.09,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 22.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 17.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 42.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 35.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF3EBDD",
  "accentColor": "#FFFF5959"
})ARCH" },
            { "Oxblood Leather", "cloth", R"ARCH({
  "format": 1,
  "name": "Oxblood Leather",
  "family": "cloth",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF481A26",
    "bottom": "#FF260D16"
  },
  "surface": {
    "material": "leather",
    "finish": "satin",
    "color": "#FF57202B",
    "color2": "#FF3C1420",
    "grain": 0.5,
    "scale": 1.05,
    "shine": 0.27,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "softlight",
        "opacity": 0.08,
        "size": 7,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 34.0,
        "direction": "up",
        "size": 4.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 28.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 46.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 31.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFFFF0DC",
  "accentColor": "#FFE7B96E"
})ARCH" },
            { "Grey Felt", "cloth", R"ARCH({
  "format": 1,
  "name": "Grey Felt",
  "family": "cloth",
  "light": {
    "angle": 118,
    "altitude": 44
  },
  "background": {
    "top": "#FF363B3F",
    "bottom": "#FF1C2024"
  },
  "surface": {
    "material": "felt",
    "finish": "natural",
    "color": "#FF373C40",
    "color2": "#FF25292C",
    "grain": 0.91,
    "scale": 1.14,
    "shine": 0.03,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 55.0,
        "direction": "up",
        "size": 4.0,
        "soften": 3.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 44.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 36.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 62.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 59.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0F2F2",
  "accentColor": "#FFB5D5CF"
})ARCH" },
            { "Walnut Cheek", "wood", R"ARCH({
  "format": 1,
  "name": "Walnut Cheek",
  "family": "wood",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF34241B",
    "bottom": "#FF160E0A"
  },
  "surface": {
    "material": "walnut",
    "finish": "satin",
    "color": "#FF5A3A26",
    "color2": "#FF22140D",
    "grain": 1.05,
    "scale": 1.3,
    "shine": 0.22,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 24.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 28.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 44.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 30.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FFE5A45F"
})ARCH" },
            { "Ebony Studio", "wood", R"ARCH({
  "format": 1,
  "name": "Ebony Studio",
  "family": "wood",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF24201D",
    "bottom": "#FF0D0B09"
  },
  "surface": {
    "material": "ebony",
    "finish": "satin",
    "color": "#FF302923",
    "color2": "#FF15120F",
    "grain": 0.55,
    "scale": 1,
    "shine": 0.3,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "pinstripes",
        "blend": "overlay",
        "opacity": 0.08,
        "size": 9,
        "angle": 90
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 30.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 30.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 48.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 42.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FFD5B27A"
})ARCH" },
            { "Rosewood Console", "wood", R"ARCH({
  "format": 1,
  "name": "Rosewood Console",
  "family": "wood",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF2C1412",
    "bottom": "#FF100707"
  },
  "surface": {
    "material": "rosewood",
    "finish": "polished",
    "color": "#FF3A130F",
    "color2": "#FF130605",
    "grain": 0.9,
    "scale": 1.2,
    "shine": 0.3,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 34.0,
        "direction": "up",
        "size": 4.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 38.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 48.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 34.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FFFFB878"
})ARCH" },
            { "Natural Oak", "wood", R"ARCH({
  "format": 1,
  "name": "Natural Oak",
  "family": "wood",
  "light": {
    "angle": 125,
    "altitude": 54
  },
  "background": {
    "top": "#FFE6C99D",
    "bottom": "#FFBB945E"
  },
  "surface": {
    "material": "oak",
    "finish": "satin",
    "color": "#FFD2AF74",
    "color2": "#FFAB814A",
    "grain": 0.76,
    "scale": 1.35,
    "shine": 0.27,
    "radius": 9,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 34.0,
        "direction": "up",
        "size": 6.0,
        "soften": 3.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 54.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 50.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 44.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 38.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "choke": 0.0,
        "size": 7.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF35281B",
  "accentColor": "#FF174D45"
})ARCH" },
            { "Burl Luxe", "wood", R"ARCH({
  "format": 1,
  "name": "Burl Luxe",
  "family": "wood",
  "light": {
    "angle": 118,
    "altitude": 34
  },
  "background": {
    "top": "#FF4B2C1C",
    "bottom": "#FF22140E"
  },
  "surface": {
    "material": "burl",
    "finish": "polished",
    "color": "#FF633A23",
    "color2": "#FF3D2317",
    "grain": 0.88,
    "scale": 1.2,
    "shine": 0.79,
    "radius": 5,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 41.0,
        "direction": "up",
        "size": 2.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 34.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 22.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 50.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 3.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFFFF0D5",
  "accentColor": "#FFFFCC75"
})ARCH" },
            { "Flame Maple", "wood", R"ARCH({
  "format": 1,
  "name": "Flame Maple",
  "family": "wood",
  "light": {
    "angle": 125,
    "altitude": 39
  },
  "background": {
    "top": "#FFE0AE57",
    "bottom": "#FFBD8E48"
  },
  "surface": {
    "material": "guitarwood",
    "finish": "polished",
    "color": "#FFC78E37",
    "color2": "#FFB07A36",
    "grain": 0.72,
    "scale": 1.4,
    "shine": 0.67,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 48.0,
        "direction": "up",
        "size": 3.0,
        "soften": 2.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 39.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 29.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 56.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 52.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF302113",
  "accentColor": "#FF273A58"
})ARCH" },
            { "Black Marble", "stone", R"ARCH({
  "format": 1,
  "name": "Black Marble",
  "family": "stone",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF22252A",
    "bottom": "#FF0B0E12"
  },
  "surface": {
    "material": "blackmarble",
    "finish": "polished",
    "color": "#FF282B30",
    "color2": "#FFC9B98F",
    "grain": 0.85,
    "scale": 1.0,
    "shine": 0.77,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 46.0,
        "direction": "up",
        "size": 4.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 44.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 48.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 24.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF4ECDD",
  "accentColor": "#FFE6BF70"
})ARCH" },
            { "White Marble", "stone", R"ARCH({
  "format": 1,
  "name": "White Marble",
  "family": "stone",
  "light": {
    "angle": 125,
    "altitude": 49
  },
  "background": {
    "top": "#FFF5F4F1",
    "bottom": "#FFD7D9D7"
  },
  "surface": {
    "material": "marble",
    "finish": "polished",
    "color": "#FFE8E7E3",
    "color2": "#FFB8BDC2",
    "grain": 0.34,
    "scale": 1.18,
    "shine": 0.75,
    "radius": 8,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 21.0,
        "direction": "up",
        "size": 5.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 49.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 43.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 32.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 24.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 6.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FF2E353B",
  "accentColor": "#FF72532B"
})ARCH" },
            { "Slate Stone", "stone", R"ARCH({
  "format": 1,
  "name": "Slate Stone",
  "family": "stone",
  "light": {
    "angle": 118,
    "altitude": 54
  },
  "background": {
    "top": "#FF30363D",
    "bottom": "#FF161C23"
  },
  "surface": {
    "material": "slate",
    "finish": "natural",
    "color": "#FF353A41",
    "color2": "#FF252C34",
    "grain": 0.74,
    "scale": 1.15,
    "shine": 0.14,
    "radius": 9,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 28.0,
        "direction": "up",
        "size": 6.0,
        "soften": 2.0,
        "globalLight": true,
        "angle": 118.0,
        "altitude": 54.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 50.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 38.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 31.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 7.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 40.0,
        "angle": 118.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0F4EF",
  "accentColor": "#FFBDDE83"
})ARCH" },
            { "Carbon Fibre", "composite", R"ARCH({
  "format": 1,
  "name": "Carbon Fibre",
  "family": "composite",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF22282D",
    "bottom": "#FF0B1015"
  },
  "surface": {
    "material": "carbon",
    "finish": "satin",
    "color": "#FF0F1215",
    "color2": "#FF07090B",
    "grain": 0.9,
    "scale": 1.0,
    "shine": 0.12,
    "radius": 6,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 29.0,
        "direction": "up",
        "size": 3.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 26.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 43.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 31.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF5FAFF",
  "accentColor": "#FF00DDF2"
})ARCH" },
            { "Dark Screen Glass", "glass", R"ARCH({
  "format": 1,
  "name": "Dark Screen Glass",
  "family": "glass",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF303940",
    "bottom": "#FF0A1017"
  },
  "surface": {
    "material": "glass",
    "finish": "polished",
    "color": "#FF1A242C",
    "color2": "#FF0A1016",
    "grain": 0.04,
    "scale": 0.8,
    "shine": 0.85,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.035,
        "size": 12,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 62.0,
        "direction": "up",
        "size": 6.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 62.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 58.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 24.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 1.0,
        "choke": 0.0,
        "size": 3.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FF65D9EE"
})ARCH" },
            { "Frosted Smoke", "glass", R"ARCH({
  "format": 1,
  "name": "Frosted Smoke",
  "family": "glass",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF3C444A",
    "bottom": "#FF191E22"
  },
  "surface": {
    "material": "frosted",
    "finish": "matte",
    "color": "#FF565F67",
    "color2": "#FF2E353B",
    "grain": 0.5,
    "scale": 1,
    "shine": 0.2,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.07,
        "size": 8,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 32.0,
        "direction": "up",
        "size": 4.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 40.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 48.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 38.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 5.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FF9CCAD0"
})ARCH" },
            { "Amber Tinted Glass", "glass", R"ARCH({
  "format": 1,
  "name": "Amber Tinted Glass",
  "family": "glass",
  "light": {
    "angle": 125,
    "altitude": 38
  },
  "background": {
    "top": "#FF3B2C1B",
    "bottom": "#FF160F08"
  },
  "surface": {
    "material": "tintedglass",
    "finish": "polished",
    "color": "#FF684B28",
    "color2": "#FF302012",
    "grain": 0.08,
    "scale": 0.9,
    "shine": 0.72,
    "radius": 7,
    "depth": 0.0,
    "shadow": 0.0,
    "patterns": [
      {
        "pattern": "noise",
        "blend": "overlay",
        "opacity": 0.045,
        "size": 10,
        "angle": 0
      }
    ],
    "effects": {
      "bevel": {
        "enabled": true,
        "style": "inner",
        "technique": "smooth",
        "depth": 58.0,
        "direction": "up",
        "size": 5.0,
        "soften": 1.0,
        "globalLight": true,
        "angle": 125.0,
        "altitude": 38.0,
        "glossContour": "0.0000,0.0000,1;0.2500,0.1800,1;0.5000,0.5000,1;0.7500,0.8200,1;1.0000,1.0000,1",
        "antiAliased": true,
        "highlightMode": "screen",
        "highlightColor": "#FFFFFFFF",
        "highlightOpacity": 58.0,
        "shadowMode": "multiply",
        "shadowColor": "#FF000000",
        "shadowOpacity": 56.0,
        "contour": false,
        "contourShape": "0.0000,0.0000,0;0.5000,0.9200,0;1.0000,1.0000,0",
        "contourRange": 100.0
      },
      "innerShadow": {
        "enabled": true,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 30.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 2.0,
        "choke": 0.0,
        "size": 4.0
      },
      "dropShadow": {
        "enabled": false,
        "blend": "multiply",
        "color": "#FF000000",
        "opacity": 45.0,
        "angle": 125.0,
        "globalLight": true,
        "distance": 3.0,
        "spread": 0.0,
        "size": 5.0,
        "knockout": true
      }
    }
  },
  "textColor": "#FFF0EEE8",
  "accentColor": "#FFFFC15A"
})ARCH" },
        };

        return looks;
    }

    /** Each look's matching 3D controls: "knob", "slider", "button" and "toggle", each a 3D
        Studio model's settings by name (Model3D::setByName). */
    struct ControlSet
    {
        const char* name;   // its material look's
        const char* json;
    };

    inline const std::vector<ControlSet>& controlSets()
    {
        static const std::vector<ControlSet> sets {
            { "Dark Brushed Aluminium", R"CTRL({
  "format": 1,
  "name": "Dark Brushed Aluminium",
  "knob": {
    "material": "Spun aluminium",
    "colour": "#FFE0E4E5",
    "diameter": 70,
    "height": 29,
    "taper": 8,
    "edge": 16,
    "top": "Dish",
    "topDepth": 4,
    "skirt": true,
    "skirtDiameter": 123,
    "skirtHeight": 16,
    "skirtBevel": 42,
    "skirtMaterial": "Brushed aluminium",
    "skirtColour": "#FF353C41",
    "skirtLine": true,
    "grip": "Knurled",
    "gripCount": 52,
    "gripDepth": 1.8,
    "cap": false,
    "pointer": "Groove",
    "pointerColour": "#FF1C2025",
    "pointerLength": 71,
    "pointerWidth": 5,
    "pointerSide": true,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 90,
    "trackWidth": 8,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFE6E7E6",
    "cap": "Classic fader",
    "capWidth": 74,
    "capLength": 15,
    "capHeight": 22,
    "grooves": 5,
    "material": "Brushed aluminium",
    "colour": "#FFE0E4E5",
    "lineColour": "#FF1C2025"
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 74,
    "depth": 54,
    "corner": 24,
    "height": 18,
    "edge": 26,
    "top": "Flat",
    "material": "Matte plastic",
    "colour": "#FF222426",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 9,
    "bezelHeight": 28,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF89939C",
    "light": "Bar",
    "lightColour": "#FFE6A34A",
    "lightOff": 16
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 52,
    "baseMaterial": "Steel",
    "baseColour": "#FF89939C",
    "leverLength": 40,
    "leverWidth": 10,
    "tip": "Plain",
    "angle": 34,
    "leverMaterial": "Chrome",
    "leverColour": "#FFE0E4E5"
  }
})CTRL" },
            { "Black Anodized", R"CTRL({
  "format": 1,
  "name": "Black Anodized",
  "knob": {
    "material": "Anodized",
    "colour": "#FF20282F",
    "diameter": 66,
    "height": 47,
    "taper": 15,
    "edge": 12,
    "top": "Flat",
    "skirt": false,
    "grip": "Ribs",
    "gripCount": 22,
    "gripDepth": 3.6,
    "cap": false,
    "pointer": "Dot",
    "pointerColour": "#FFF9FAF7",
    "pointerLength": 57,
    "pointerWidth": 10,
    "pointerSide": false,
    "pointerY": 0,
    "ring": true,
    "ringSegments": 21,
    "ringWidth": 7,
    "ringGap": 5,
    "ringFrom": "Its start",
    "ringColour": "#FF55C7D9"
  },
  "slider": {
    "trackLength": 86,
    "trackWidth": 6,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFE6E7E6",
    "cap": "Block",
    "capWidth": 56,
    "capLength": 23,
    "capHeight": 29,
    "material": "Anodized",
    "colour": "#FF2D3236",
    "lineColour": "#FFF9FAF7"
  },
  "button": {
    "shape": "Round",
    "width": 62,
    "depth": 62,
    "height": 18,
    "edge": 26,
    "top": "Convex",
    "topDepth": 7,
    "material": "Gloss plastic",
    "colour": "#FF20282F",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 5,
    "bezelHeight": 18,
    "bezelMaterial": "Anodized",
    "bezelColour": "#FF38454C",
    "light": "Ring round it",
    "lightColour": "#FF55C7D9",
    "lightOff": 16
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 49,
    "baseMaterial": "Anodized",
    "baseColour": "#FF20282F",
    "leverLength": 31,
    "leverWidth": 13,
    "tip": "Paddle",
    "angle": 27,
    "leverMaterial": "Steel",
    "leverColour": "#FF89939C"
  }
})CTRL" },
            { "Aged Brass", R"CTRL({
  "format": 1,
  "name": "Aged Brass",
  "knob": {
    "material": "Brass",
    "colour": "#FFC6A066",
    "diameter": 65,
    "height": 40,
    "taper": -4,
    "edge": 23,
    "top": "Dome",
    "topDepth": 10,
    "skirt": true,
    "skirtDiameter": 117,
    "skirtHeight": 22,
    "skirtBevel": 53,
    "skirtMaterial": "Brass",
    "skirtColour": "#FF6B4C2C",
    "skirtLine": false,
    "grip": "Flutes",
    "gripCount": 18,
    "gripDepth": 3.4,
    "cap": true,
    "capDiameter": 56,
    "capMaterial": "Bakelite",
    "capColour": "#FF36251E",
    "capGroove": true,
    "pointer": "Line",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 53,
    "pointerWidth": 7,
    "pointerSide": false,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 82,
    "trackWidth": 10,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 65,
    "capLength": 18,
    "capHeight": 28,
    "grooves": 3,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "lineColour": "#FFF5EEE0"
  },
  "button": {
    "shape": "Round",
    "width": 61,
    "depth": 61,
    "height": 18,
    "edge": 26,
    "top": "Convex",
    "topDepth": 9,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 10,
    "bezelHeight": 24,
    "bezelMaterial": "Brass",
    "bezelColour": "#FFC6A066",
    "light": "Dot",
    "lightPlace": "Top",
    "lightColour": "#FFFFC36A",
    "lightOff": 16
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 56,
    "baseMaterial": "Brass",
    "baseColour": "#FF6B4C2C",
    "leverLength": 36,
    "leverWidth": 12,
    "tip": "Ball",
    "angle": 32,
    "leverMaterial": "Brass",
    "leverColour": "#FFC6A066"
  }
})CTRL" },
            { "Silver Faceplate", R"CTRL({
  "format": 1,
  "name": "Silver Faceplate",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF222426",
    "diameter": 72,
    "height": 42,
    "taper": 12,
    "edge": 19,
    "top": "Dome",
    "topDepth": 6,
    "skirt": true,
    "skirtDiameter": 123,
    "skirtHeight": 14,
    "skirtBevel": 42,
    "skirtMaterial": "Brushed aluminium",
    "skirtColour": "#FF89939C",
    "skirtLine": false,
    "grip": "Flutes",
    "gripCount": 36,
    "gripDepth": 3.2,
    "cap": false,
    "pointer": "Line",
    "pointerColour": "#FFF9FAF7",
    "pointerLength": 67,
    "pointerWidth": 7,
    "pointerSide": true,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 88,
    "trackWidth": 8,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FF1C2025",
    "cap": "Classic fader",
    "capWidth": 73,
    "capLength": 14,
    "capHeight": 29,
    "grooves": 4,
    "material": "Bakelite",
    "colour": "#FF222426",
    "lineColour": "#FFF9FAF7"
  },
  "button": {
    "shape": "Square",
    "width": 63,
    "depth": 63,
    "height": 19,
    "edge": 13,
    "top": "Flat",
    "material": "Bakelite",
    "colour": "#FF222426",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF89939C",
    "light": "Dot",
    "lightPlace": "Top",
    "lightColour": "#FF852B39",
    "lightOff": 16
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 49,
    "baseMaterial": "Chrome",
    "baseColour": "#FFE0E4E5",
    "leverLength": 40,
    "leverWidth": 10,
    "tip": "Plain",
    "angle": 34,
    "leverMaterial": "Chrome",
    "leverColour": "#FFE0E4E5"
  }
})CTRL" },
            { "Workshop Hammertone", R"CTRL({
  "format": 1,
  "name": "Workshop Hammertone",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF222426",
    "diameter": 60,
    "height": 55,
    "taper": -3,
    "edge": 27,
    "top": "Dome",
    "topDepth": 14,
    "skirt": true,
    "skirtDiameter": 112,
    "skirtHeight": 18,
    "skirtBevel": 42,
    "skirtMaterial": "Painted metal",
    "skirtColour": "#FF34495A",
    "skirtLine": false,
    "grip": "Flutes",
    "gripCount": 15,
    "gripDepth": 3.8,
    "cap": true,
    "capDiameter": 55,
    "capMaterial": "Spun aluminium",
    "capColour": "#FFE8DAB9",
    "capGroove": true,
    "pointer": "Groove",
    "pointerColour": "#FF332118",
    "pointerLength": 54,
    "pointerWidth": 7,
    "pointerSide": false,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 83,
    "trackWidth": 11,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 64,
    "capLength": 16,
    "capHeight": 25,
    "grooves": 3,
    "material": "Painted metal",
    "colour": "#FF34495A",
    "lineColour": "#FFF5EEE0"
  },
  "button": {
    "shape": "Round",
    "width": 61,
    "depth": 61,
    "height": 18,
    "edge": 26,
    "top": "Convex",
    "topDepth": 9,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF89939C",
    "light": "Dot",
    "lightPlace": "Middle",
    "lightColour": "#FFFFB94E",
    "lightOff": 16
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 57,
    "baseMaterial": "Steel",
    "baseColour": "#FF89939C",
    "leverLength": 43,
    "leverWidth": 12,
    "tip": "Ball",
    "angle": 38,
    "leverMaterial": "Chrome",
    "leverColour": "#FFE0E4E5"
  }
})CTRL" },
            { "Red Anodized", R"CTRL({
  "format": 1,
  "name": "Red Anodized",
  "knob": {
    "material": "Anodized",
    "colour": "#FF252B33",
    "diameter": 63,
    "height": 32,
    "taper": 8,
    "edge": 9,
    "top": "Flat",
    "skirt": false,
    "grip": "Knurled",
    "cap": true,
    "pointer": "Line",
    "ring": true,
    "gripCount": 52,
    "gripDepth": 2.0,
    "capDiameter": 47,
    "capMaterial": "Spun aluminium",
    "capColour": "#FFB8C3C8",
    "capGroove": false,
    "pointerColour": "#FF17191C",
    "pointerLength": 64,
    "pointerWidth": 7,
    "pointerSide": true,
    "pointerY": -4,
    "ringColour": "#FF95F3EA",
    "ringSegments": 24,
    "ringWidth": 5,
    "ringGap": 4,
    "ringFrom": "Its start"
  },
  "slider": {
    "trackLength": 76,
    "trackWidth": 7,
    "trackColour": "#FF0B1016",
    "ticks": 9,
    "cap": "Block",
    "capWidth": 52,
    "capHeight": 18,
    "material": "Anodized",
    "colour": "#FF29323D",
    "lineColour": "#FFFFFFFF",
    "tickLength": 12,
    "tickColour": "#FFF7ECED",
    "capLength": 12
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 54,
    "depth": 52,
    "height": 12,
    "edge": 14,
    "top": "Flat",
    "material": "Anodized",
    "colour": "#FF26313C",
    "travel": 20,
    "bezel": true,
    "bezelWidth": 5,
    "bezelHeight": 14,
    "bezelMaterial": "Anodized",
    "bezelColour": "#FF646B73",
    "light": "Bar",
    "corner": 22,
    "lightColour": "#FF95F3EA",
    "lightOff": 7
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 40,
    "baseMaterial": "Anodized",
    "baseColour": "#FF69727C",
    "leverLength": 28,
    "leverWidth": 8,
    "tip": "Paddle",
    "angle": 24,
    "leverMaterial": "Anodized",
    "leverColour": "#FF69727C"
  }
})CTRL" },
            { "Blue Anodized", R"CTRL({
  "format": 1,
  "name": "Blue Anodized",
  "knob": {
    "material": "Spun aluminium",
    "colour": "#FFCDD7DD",
    "diameter": 70,
    "height": 27,
    "taper": -4,
    "edge": 24,
    "top": "Dish",
    "skirt": true,
    "grip": "Ribs",
    "cap": false,
    "pointer": "Groove",
    "ring": true,
    "topDepth": 8,
    "skirtDiameter": 112,
    "skirtHeight": 13,
    "skirtBevel": 26,
    "skirtMaterial": "Spun aluminium",
    "skirtColour": "#FF515F6B",
    "skirtLine": true,
    "gripCount": 18,
    "gripDepth": 1.8,
    "pointerColour": "#FF17191C",
    "pointerLength": 55,
    "pointerWidth": 7,
    "pointerSide": false,
    "pointerY": 0,
    "ringColour": "#FFFFCB74",
    "ringSegments": 32,
    "ringWidth": 6,
    "ringGap": 5,
    "ringFrom": "The middle"
  },
  "slider": {
    "trackLength": 79,
    "trackWidth": 8,
    "trackColour": "#FF0B1016",
    "ticks": 11,
    "cap": "Classic fader",
    "capWidth": 57,
    "capHeight": 23,
    "material": "Spun aluminium",
    "colour": "#FFD2DDE4",
    "lineColour": "#FF17191C",
    "tickLength": 15,
    "tickColour": "#FFF0F6FF",
    "capLength": 16,
    "grooves": 3
  },
  "button": {
    "shape": "Rounded square",
    "width": 57,
    "depth": 59,
    "height": 17,
    "edge": 19,
    "top": "Concave",
    "material": "Satin plastic",
    "colour": "#FF273B52",
    "travel": 23,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 19,
    "bezelMaterial": "Chrome",
    "bezelColour": "#FFADBCCC",
    "light": "Dot",
    "topDepth": 6,
    "lightColour": "#FFFFCB74",
    "lightOff": 10,
    "lightPlace": "Left"
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 47,
    "baseMaterial": "Chrome",
    "baseColour": "#FFC9D0D9",
    "leverLength": 34,
    "leverWidth": 10,
    "tip": "Plain",
    "angle": 29,
    "leverMaterial": "Chrome",
    "leverColour": "#FFC9D0D9"
  }
})CTRL" },
            { "Gold Anodized", R"CTRL({
  "format": 1,
  "name": "Gold Anodized",
  "knob": {
    "material": "Satin plastic",
    "colour": "#FF292B2B",
    "diameter": 60,
    "height": 43,
    "taper": 14,
    "edge": 19,
    "top": "Dome",
    "skirt": true,
    "grip": "Flutes",
    "cap": true,
    "pointer": "Dot",
    "ring": false,
    "topDepth": 9,
    "skirtDiameter": 127,
    "skirtHeight": 28,
    "skirtBevel": 60,
    "skirtMaterial": "Gold",
    "skirtColour": "#FF6A5B42",
    "skirtLine": false,
    "gripCount": 30,
    "gripDepth": 2.8,
    "capDiameter": 45,
    "capMaterial": "Gold",
    "capColour": "#FFC4A36D",
    "capGroove": true,
    "pointerColour": "#FF17191C",
    "pointerLength": 34,
    "pointerWidth": 11,
    "pointerSide": false,
    "pointerY": 3
  },
  "slider": {
    "trackLength": 82,
    "trackWidth": 9,
    "trackColour": "#FF171B21",
    "ticks": 7,
    "cap": "Round",
    "capWidth": 62,
    "capHeight": 28,
    "material": "Satin plastic",
    "colour": "#FF2B2A28",
    "lineColour": "#FFFFFFFF",
    "tickLength": 18,
    "tickColour": "#FF29241C"
  },
  "button": {
    "shape": "Round",
    "width": 60,
    "depth": 66,
    "height": 22,
    "edge": 24,
    "top": "Convex",
    "material": "Bakelite",
    "colour": "#FF292620",
    "travel": 26,
    "bezel": true,
    "bezelWidth": 9,
    "bezelHeight": 24,
    "bezelMaterial": "Gold",
    "bezelColour": "#FFA58B61",
    "light": "Dot",
    "topDepth": 7,
    "lightColour": "#FF263B68",
    "lightOff": 13,
    "lightPlace": "Middle"
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 54,
    "baseMaterial": "Brass",
    "baseColour": "#FFD7B980",
    "leverLength": 40,
    "leverWidth": 12,
    "tip": "Ball",
    "angle": 34,
    "leverMaterial": "Brass",
    "leverColour": "#FFD7B980"
  }
})CTRL" },
            { "Mirror Chrome", R"CTRL({
  "format": 1,
  "name": "Mirror Chrome",
  "knob": {
    "material": "Chrome",
    "colour": "#FFDDE3E6",
    "diameter": 68,
    "height": 24,
    "taper": -6,
    "edge": 27,
    "top": "Flat",
    "skirt": true,
    "grip": "Knurled",
    "cap": true,
    "pointer": "Ridge",
    "ring": false,
    "skirtDiameter": 108,
    "skirtHeight": 10,
    "skirtBevel": 42,
    "skirtMaterial": "Chrome",
    "skirtColour": "#FF78838C",
    "skirtLine": false,
    "gripCount": 46,
    "gripDepth": 1.6,
    "capDiameter": 51,
    "capMaterial": "Bakelite",
    "capColour": "#FF282D31",
    "capGroove": false,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 70,
    "pointerWidth": 5,
    "pointerSide": false,
    "pointerY": 5
  },
  "slider": {
    "trackLength": 85,
    "trackWidth": 10,
    "trackColour": "#FF171B21",
    "ticks": 9,
    "cap": "Block",
    "capWidth": 67,
    "capHeight": 33,
    "material": "Chrome",
    "colour": "#FFD8DEE3",
    "lineColour": "#FF17191C",
    "tickLength": 21,
    "tickColour": "#FF1D2932",
    "capLength": 12
  },
  "button": {
    "shape": "Pill",
    "width": 63,
    "depth": 73,
    "height": 27,
    "edge": 29,
    "top": "Flat",
    "material": "Chrome",
    "colour": "#FFBEC7CE",
    "travel": 29,
    "bezel": true,
    "bezelWidth": 11,
    "bezelHeight": 29,
    "bezelMaterial": "Chrome",
    "bezelColour": "#FFD8DFE1",
    "light": "Ring round it",
    "lightColour": "#FF602642",
    "lightOff": 16
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 61,
    "baseMaterial": "Chrome",
    "baseColour": "#FFE5E9EC",
    "leverLength": 46,
    "leverWidth": 14,
    "tip": "Paddle",
    "angle": 39,
    "leverMaterial": "Chrome",
    "leverColour": "#FFE5E9EC"
  }
})CTRL" },
            { "Brushed Steel", R"CTRL({
  "format": 1,
  "name": "Brushed Steel",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF262B2D",
    "diameter": 61,
    "height": 49,
    "taper": 10,
    "edge": 15,
    "top": "Dish",
    "skirt": true,
    "grip": "Flutes",
    "cap": false,
    "pointer": "Line",
    "ring": false,
    "topDepth": 12,
    "skirtDiameter": 137,
    "skirtHeight": 31,
    "skirtBevel": 59,
    "skirtMaterial": "Steel",
    "skirtColour": "#FF31383B",
    "skirtLine": true,
    "gripCount": 24,
    "gripDepth": 4.2,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 72,
    "pointerWidth": 6,
    "pointerSide": false,
    "pointerY": -3
  },
  "slider": {
    "trackLength": 88,
    "trackWidth": 11,
    "trackColour": "#FF171B21",
    "ticks": 13,
    "cap": "Classic fader",
    "capWidth": 72,
    "capHeight": 18,
    "material": "Steel",
    "colour": "#FF353A3D",
    "lineColour": "#FFFFFFFF",
    "tickLength": 12,
    "tickColour": "#FF1B2529",
    "capLength": 16,
    "grooves": 6
  },
  "button": {
    "shape": "Square",
    "width": 66,
    "depth": 53,
    "height": 32,
    "edge": 34,
    "top": "Flat",
    "material": "Bakelite",
    "colour": "#FF292C2C",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 13,
    "bezelHeight": 34,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF9EAAAD",
    "light": "Dot",
    "lightColour": "#FF194D72",
    "lightOff": 19,
    "lightPlace": "Left"
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 68,
    "baseMaterial": "Steel",
    "baseColour": "#FF9EA6AC",
    "leverLength": 52,
    "leverWidth": 16,
    "tip": "Plain",
    "angle": 44,
    "leverMaterial": "Steel",
    "leverColour": "#FF9EA6AC"
  }
})CTRL" },
            { "Gunmetal Grey", R"CTRL({
  "format": 1,
  "name": "Gunmetal Grey",
  "knob": {
    "material": "Steel",
    "colour": "#FF353D48",
    "diameter": 75,
    "height": 33,
    "taper": -7,
    "edge": 10,
    "top": "Flat",
    "skirt": true,
    "grip": "Ribs",
    "cap": true,
    "pointer": "Groove",
    "ring": false,
    "skirtDiameter": 118,
    "skirtHeight": 19,
    "skirtBevel": 64,
    "skirtMaterial": "Steel",
    "skirtColour": "#FF222B37",
    "skirtLine": false,
    "gripCount": 42,
    "gripDepth": 2.9,
    "capDiameter": 56,
    "capMaterial": "Spun aluminium",
    "capColour": "#FFABB7C4",
    "capGroove": true,
    "pointerColour": "#FF17191C",
    "pointerLength": 61,
    "pointerWidth": 7,
    "pointerSide": true,
    "pointerY": -4
  },
  "slider": {
    "trackLength": 91,
    "trackWidth": 12,
    "trackColour": "#FF0B1016",
    "ticks": 11,
    "cap": "Block",
    "capWidth": 52,
    "capHeight": 23,
    "material": "Steel",
    "colour": "#FF303A47",
    "lineColour": "#FFFFFFFF",
    "tickLength": 15,
    "tickColour": "#FFF0F5F7",
    "capLength": 20
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 69,
    "depth": 60,
    "height": 12,
    "edge": 39,
    "top": "Concave",
    "material": "Painted metal",
    "colour": "#FF29313C",
    "travel": 35,
    "bezel": true,
    "bezelWidth": 5,
    "bezelHeight": 39,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF8796A6",
    "light": "Bar",
    "corner": 22,
    "topDepth": 10,
    "lightColour": "#FFFFBD6F",
    "lightOff": 22
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 40,
    "baseMaterial": "Steel",
    "baseColour": "#FF7B8B98",
    "leverLength": 28,
    "leverWidth": 8,
    "tip": "Plain",
    "angle": 24,
    "leverMaterial": "Steel",
    "leverColour": "#FF7B8B98"
  }
})CTRL" },
            { "Liquid Titanium", R"CTRL({
  "format": 1,
  "name": "Liquid Titanium",
  "knob": {
    "material": "Chrome",
    "colour": "#FFB9C4D0",
    "diameter": 72,
    "height": 21,
    "taper": -9,
    "edge": 31,
    "top": "Dome",
    "skirt": false,
    "grip": "Smooth",
    "cap": true,
    "pointer": "Dot",
    "ring": true,
    "topDepth": 11,
    "capDiameter": 54,
    "capMaterial": "Anodized",
    "capColour": "#FF2D3445",
    "capGroove": false,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 44,
    "pointerWidth": 9,
    "pointerSide": false,
    "pointerY": 0,
    "ringColour": "#FFCDAFFF",
    "ringSegments": 0,
    "ringWidth": 5,
    "ringGap": 4,
    "ringFrom": "Its start"
  },
  "slider": {
    "trackLength": 94,
    "trackWidth": 13,
    "trackColour": "#FF0B1016",
    "ticks": 0,
    "cap": "Round",
    "capWidth": 57,
    "capHeight": 28,
    "material": "Anodized",
    "colour": "#FF303747",
    "lineColour": "#FFFFFFFF"
  },
  "button": {
    "shape": "Round",
    "width": 72,
    "depth": 67,
    "height": 17,
    "edge": 44,
    "top": "Convex",
    "material": "Anodized",
    "colour": "#FF282E42",
    "travel": 38,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 14,
    "bezelMaterial": "Anodized",
    "bezelColour": "#FF8B83AA",
    "light": "The whole cap",
    "topDepth": 5,
    "lightColour": "#FFCDAFFF",
    "lightOff": 25
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 47,
    "baseMaterial": "Chrome",
    "baseColour": "#FFBAC0D2",
    "leverLength": 34,
    "leverWidth": 10,
    "tip": "Ball",
    "angle": 29,
    "leverMaterial": "Chrome",
    "leverColour": "#FFBAC0D2"
  }
})CTRL" },
            { "Copper Patina", R"CTRL({
  "format": 1,
  "name": "Copper Patina",
  "knob": {
    "material": "Copper",
    "colour": "#FFAA7456",
    "diameter": 65,
    "height": 44,
    "taper": 3,
    "edge": 20,
    "top": "Dome",
    "skirt": true,
    "grip": "Flutes",
    "cap": true,
    "pointer": "Ridge",
    "ring": false,
    "topDepth": 13,
    "skirtDiameter": 115,
    "skirtHeight": 24,
    "skirtBevel": 47,
    "skirtMaterial": "Copper",
    "skirtColour": "#FF704D38",
    "skirtLine": false,
    "gripCount": 19,
    "gripDepth": 3.5,
    "capDiameter": 49,
    "capMaterial": "Bakelite",
    "capColour": "#FF38271D",
    "capGroove": true,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 58,
    "pointerWidth": 6,
    "pointerSide": true,
    "pointerY": 3
  },
  "slider": {
    "trackLength": 76,
    "trackWidth": 7,
    "trackColour": "#FF0B1016",
    "ticks": 9,
    "cap": "Classic fader",
    "capWidth": 62,
    "capHeight": 33,
    "material": "Bakelite",
    "colour": "#FF33251E",
    "lineColour": "#FFFFFFFF",
    "tickLength": 21,
    "tickColour": "#FFFFF4E6",
    "capLength": 16,
    "grooves": 4
  },
  "button": {
    "shape": "Rounded square",
    "width": 75,
    "depth": 74,
    "height": 22,
    "edge": 16,
    "top": "Convex",
    "material": "Bakelite",
    "colour": "#FF3A271D",
    "travel": 41,
    "bezel": true,
    "bezelWidth": 9,
    "bezelHeight": 19,
    "bezelMaterial": "Copper",
    "bezelColour": "#FFC79672",
    "light": "Dot",
    "topDepth": 6,
    "lightColour": "#FFB5EBE0",
    "lightOff": 9,
    "lightPlace": "Left"
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 54,
    "baseMaterial": "Copper",
    "baseColour": "#FFAD8265",
    "leverLength": 40,
    "leverWidth": 12,
    "tip": "Ball",
    "angle": 34,
    "leverMaterial": "Copper",
    "leverColour": "#FFAD8265"
  }
})CTRL" },
            { "Cast Iron", R"CTRL({
  "format": 1,
  "name": "Cast Iron",
  "knob": {
    "material": "Painted metal",
    "colour": "#FF313537",
    "diameter": 78,
    "height": 60,
    "taper": -3,
    "edge": 4,
    "top": "Flat",
    "skirt": true,
    "grip": "Knurled",
    "cap": false,
    "pointer": "Groove",
    "ring": false,
    "skirtDiameter": 149,
    "skirtHeight": 36,
    "skirtBevel": 28,
    "skirtMaterial": "Painted metal",
    "skirtColour": "#FF292D2F",
    "skirtLine": true,
    "gripCount": 70,
    "gripDepth": 4.2,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 78,
    "pointerWidth": 10,
    "pointerSide": true,
    "pointerY": 5
  },
  "slider": {
    "trackLength": 79,
    "trackWidth": 8,
    "trackColour": "#FF0B1016",
    "ticks": 11,
    "cap": "Block",
    "capWidth": 67,
    "capHeight": 18,
    "material": "Painted metal",
    "colour": "#FF2C3133",
    "lineColour": "#FFFFFFFF",
    "tickLength": 12,
    "tickColour": "#FFEAE6DB",
    "capLength": 20
  },
  "button": {
    "shape": "Square",
    "width": 78,
    "depth": 54,
    "height": 27,
    "edge": 21,
    "top": "Concave",
    "material": "Painted metal",
    "colour": "#FF353839",
    "travel": 44,
    "bezel": true,
    "bezelWidth": 11,
    "bezelHeight": 24,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF7A8487",
    "light": "None",
    "topDepth": 7
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 61,
    "baseMaterial": "Steel",
    "baseColour": "#FF90969A",
    "leverLength": 46,
    "leverWidth": 14,
    "tip": "Plain",
    "angle": 39,
    "leverMaterial": "Steel",
    "leverColour": "#FF90969A"
  }
})CTRL" },
            { "Matte Black Polymer", R"CTRL({
  "format": 1,
  "name": "Matte Black Polymer",
  "knob": {
    "material": "Matte plastic",
    "colour": "#FF2D3236",
    "diameter": 78,
    "height": 23,
    "taper": -8,
    "edge": 34,
    "top": "Dish",
    "topDepth": 13,
    "skirt": false,
    "grip": "Smooth",
    "cap": false,
    "pointer": "Groove",
    "pointerColour": "#FFF9FAF7",
    "pointerLength": 73,
    "pointerWidth": 7,
    "pointerSide": false,
    "pointerY": 0,
    "ring": true,
    "ringSegments": 0,
    "ringWidth": 5,
    "ringGap": 8,
    "ringFrom": "Its start",
    "ringColour": "#FF79B7A6"
  },
  "slider": {
    "trackLength": 75,
    "trackWidth": 6,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Round",
    "capWidth": 53,
    "capHeight": 19,
    "material": "Rubber",
    "colour": "#FF20282F",
    "lineColour": "#FFF9FAF7"
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 72,
    "depth": 51,
    "corner": 44,
    "height": 12,
    "edge": 43,
    "top": "Flat",
    "material": "Matte plastic",
    "colour": "#FF2D3236",
    "travel": 32,
    "bezel": false,
    "light": "The whole cap",
    "lightColour": "#FF79B7A6",
    "lightOff": 16
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 46,
    "baseMaterial": "Anodized",
    "baseColour": "#FF20282F",
    "leverLength": 32,
    "leverWidth": 15,
    "tip": "Paddle",
    "angle": 25,
    "leverMaterial": "Matte plastic",
    "leverColour": "#FF353C41"
  }
})CTRL" },
            { "Vintage Bakelite", R"CTRL({
  "format": 1,
  "name": "Vintage Bakelite",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF36251E",
    "diameter": 68,
    "height": 53,
    "taper": 9,
    "edge": 30,
    "top": "Dome",
    "topDepth": 15,
    "skirt": true,
    "skirtDiameter": 118,
    "skirtHeight": 15,
    "skirtBevel": 63,
    "skirtMaterial": "Brass",
    "skirtColour": "#FF6B4C2C",
    "skirtLine": false,
    "grip": "Smooth",
    "cap": false,
    "pointer": "Line",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 61,
    "pointerWidth": 8,
    "pointerSide": false,
    "pointerY": 4,
    "ring": false
  },
  "slider": {
    "trackLength": 87,
    "trackWidth": 10,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 78,
    "capLength": 19,
    "capHeight": 31,
    "grooves": 2,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "lineColour": "#FFF5EEE0"
  },
  "button": {
    "shape": "Round",
    "width": 67,
    "depth": 67,
    "height": 27,
    "edge": 39,
    "top": "Convex",
    "topDepth": 11,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Brass",
    "bezelColour": "#FF6B4C2C",
    "light": "Dot",
    "lightPlace": "Top",
    "lightColour": "#FFFFB65B",
    "lightOff": 16
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 54,
    "baseMaterial": "Brass",
    "baseColour": "#FF6B4C2C",
    "leverLength": 34,
    "leverWidth": 12,
    "tip": "Ball",
    "angle": 36,
    "leverMaterial": "Brass",
    "leverColour": "#FFC6A066"
  }
})CTRL" },
            { "Soft Touch Rubber", R"CTRL({
  "format": 1,
  "name": "Soft Touch Rubber",
  "knob": {
    "material": "Rubber",
    "colour": "#FF2D3236",
    "diameter": 73,
    "height": 26,
    "taper": -5,
    "edge": 38,
    "top": "Dish",
    "topDepth": 11,
    "skirt": false,
    "grip": "Ribs",
    "gripCount": 16,
    "gripDepth": 3.5,
    "cap": true,
    "capDiameter": 68,
    "capMaterial": "Matte plastic",
    "capColour": "#FF20282F",
    "capGroove": true,
    "pointer": "Line",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 64,
    "pointerWidth": 8,
    "pointerSide": false,
    "pointerY": 0,
    "ring": true,
    "ringSegments": 0,
    "ringWidth": 4,
    "ringGap": 5,
    "ringFrom": "Its start",
    "ringColour": "#FF8BC4D0"
  },
  "slider": {
    "trackLength": 72,
    "trackWidth": 7,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Block",
    "capWidth": 67,
    "capLength": 24,
    "capHeight": 18,
    "material": "Rubber",
    "colour": "#FF2D3236",
    "lineColour": "#FFF5EEE0"
  },
  "button": {
    "shape": "Pill",
    "width": 78,
    "depth": 53,
    "height": 12,
    "edge": 49,
    "top": "Convex",
    "topDepth": 4,
    "material": "Rubber",
    "colour": "#FF2D3236",
    "travel": 32,
    "bezel": false,
    "light": "Dot",
    "lightPlace": "Left",
    "lightColour": "#FF8BC4D0",
    "lightOff": 16
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 45,
    "baseMaterial": "Anodized",
    "baseColour": "#FF20282F",
    "leverLength": 35,
    "leverWidth": 16,
    "tip": "Paddle",
    "angle": 22,
    "leverMaterial": "Rubber",
    "leverColour": "#FF2D3236"
  }
})CTRL" },
            { "Cream Console", R"CTRL({
  "format": 1,
  "name": "Cream Console",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF36251E",
    "diameter": 75,
    "height": 31,
    "taper": 8,
    "edge": 21,
    "top": "Flat",
    "skirt": true,
    "skirtDiameter": 127,
    "skirtHeight": 16,
    "skirtBevel": 42,
    "skirtMaterial": "Bakelite",
    "skirtColour": "#FF332118",
    "skirtLine": true,
    "grip": "Ribs",
    "gripCount": 30,
    "gripDepth": 3.1,
    "cap": true,
    "capDiameter": 59,
    "capMaterial": "Painted metal",
    "capColour": "#FFE8DAB9",
    "capGroove": true,
    "pointer": "Line",
    "pointerColour": "#FF332118",
    "pointerLength": 69,
    "pointerWidth": 6,
    "pointerSide": false,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 92,
    "trackWidth": 9,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FF332118",
    "cap": "Classic fader",
    "capWidth": 75,
    "capLength": 18,
    "capHeight": 26,
    "grooves": 5,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "lineColour": "#FFF5EEE0"
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 74,
    "depth": 57,
    "corner": 22,
    "height": 18,
    "edge": 22,
    "top": "Flat",
    "material": "Painted metal",
    "colour": "#FFB65A22",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF89939C",
    "light": "Dot",
    "lightPlace": "Left",
    "lightColour": "#FF98501A",
    "lightOff": 16
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 55,
    "baseMaterial": "Steel",
    "baseColour": "#FF89939C",
    "leverLength": 38,
    "leverWidth": 12,
    "tip": "Paddle",
    "angle": 29,
    "leverMaterial": "Bakelite",
    "leverColour": "#FF332118"
  }
})CTRL" },
            { "Burnt Orange Seventies", R"CTRL({
  "format": 1,
  "name": "Burnt Orange Seventies",
  "knob": {
    "material": "Painted metal",
    "colour": "#FFE8DAB9",
    "diameter": 76,
    "height": 28,
    "taper": 18,
    "edge": 26,
    "top": "Dish",
    "topDepth": 8,
    "skirt": true,
    "skirtDiameter": 126,
    "skirtHeight": 19,
    "skirtBevel": 27,
    "skirtMaterial": "Bakelite",
    "skirtColour": "#FF36251E",
    "skirtLine": false,
    "grip": "Smooth",
    "cap": false,
    "pointer": "Dot",
    "pointerColour": "#FF332118",
    "pointerLength": 70,
    "pointerWidth": 9,
    "pointerSide": false,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 87,
    "trackWidth": 9,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFE8DAB9",
    "cap": "Block",
    "capWidth": 71,
    "capLength": 24,
    "capHeight": 21,
    "material": "Painted metal",
    "colour": "#FFE8DAB9",
    "lineColour": "#FF332118"
  },
  "button": {
    "shape": "Rounded square",
    "width": 65,
    "depth": 65,
    "height": 18,
    "edge": 32,
    "top": "Convex",
    "topDepth": 5,
    "material": "Painted metal",
    "colour": "#FFE8DAB9",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Bakelite",
    "bezelColour": "#FF332118",
    "light": "Bar",
    "lightColour": "#FFFFDE79",
    "lightOff": 16
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 49,
    "baseMaterial": "Brass",
    "baseColour": "#FF6B4C2C",
    "leverLength": 35,
    "leverWidth": 14,
    "tip": "Paddle",
    "angle": 33,
    "leverMaterial": "Painted metal",
    "leverColour": "#FFE8DAB9"
  }
})CTRL" },
            { "White Studio", R"CTRL({
  "format": 1,
  "name": "White Studio",
  "knob": {
    "material": "Satin plastic",
    "colour": "#FFE9F0F3",
    "diameter": 60,
    "height": 22,
    "taper": 4,
    "edge": 24,
    "top": "Flat",
    "skirt": false,
    "grip": "Smooth",
    "cap": false,
    "pointer": "Line",
    "ring": true,
    "pointerColour": "#FF17191C",
    "pointerLength": 60,
    "pointerWidth": 6,
    "pointerSide": false,
    "pointerY": -3,
    "ringColour": "#FF125F82",
    "ringSegments": 0,
    "ringWidth": 5,
    "ringGap": 4,
    "ringFrom": "Its start"
  },
  "slider": {
    "trackLength": 82,
    "trackWidth": 9,
    "trackColour": "#FF171B21",
    "ticks": 9,
    "cap": "Round",
    "capWidth": 72,
    "capHeight": 23,
    "material": "Satin plastic",
    "colour": "#FFF0F3F3",
    "lineColour": "#FF17191C",
    "tickLength": 15,
    "tickColour": "#FF27323A"
  },
  "button": {
    "shape": "Pill",
    "width": 56,
    "depth": 61,
    "height": 32,
    "edge": 26,
    "top": "Flat",
    "material": "Satin plastic",
    "colour": "#FFE5EDF0",
    "travel": 20,
    "bezel": true,
    "bezelWidth": 13,
    "bezelHeight": 29,
    "bezelMaterial": "Painted metal",
    "bezelColour": "#FFB0BEC9",
    "light": "Dot",
    "lightColour": "#FF125F82",
    "lightOff": 15,
    "lightPlace": "Top"
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 68,
    "baseMaterial": "Chrome",
    "baseColour": "#FFB5C0C7",
    "leverLength": 52,
    "leverWidth": 16,
    "tip": "Paddle",
    "angle": 44,
    "leverMaterial": "Chrome",
    "leverColour": "#FFB5C0C7"
  }
})CTRL" },
            { "Mint Retro", R"CTRL({
  "format": 1,
  "name": "Mint Retro",
  "knob": {
    "material": "Bakelite",
    "colour": "#FFF0E9DA",
    "diameter": 67,
    "height": 28,
    "taper": 16,
    "edge": 31,
    "top": "Dome",
    "skirt": true,
    "grip": "Smooth",
    "cap": false,
    "pointer": "Dot",
    "ring": false,
    "topDepth": 10,
    "skirtDiameter": 110,
    "skirtHeight": 12,
    "skirtBevel": 70,
    "skirtMaterial": "Bakelite",
    "skirtColour": "#FFB2A391",
    "skirtLine": false,
    "pointerColour": "#FF17191C",
    "pointerLength": 42,
    "pointerWidth": 10,
    "pointerSide": false,
    "pointerY": -4
  },
  "slider": {
    "trackLength": 85,
    "trackWidth": 10,
    "trackColour": "#FF171B21",
    "ticks": 7,
    "cap": "Classic fader",
    "capWidth": 52,
    "capHeight": 28,
    "material": "Bakelite",
    "colour": "#FFF0E5D7",
    "lineColour": "#FF17191C",
    "tickLength": 18,
    "tickColour": "#FF2B4338",
    "capLength": 16,
    "grooves": 2
  },
  "button": {
    "shape": "Rounded square",
    "width": 59,
    "depth": 68,
    "height": 12,
    "edge": 31,
    "top": "Convex",
    "material": "Bakelite",
    "colour": "#FFE9E0D0",
    "travel": 23,
    "bezel": true,
    "bezelWidth": 5,
    "bezelHeight": 34,
    "bezelMaterial": "Chrome",
    "bezelColour": "#FFAFB7AA",
    "light": "Dot",
    "topDepth": 9,
    "lightColour": "#FF7C3C2C",
    "lightOff": 18,
    "lightPlace": "Left"
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 40,
    "baseMaterial": "Chrome",
    "baseColour": "#FFC8D3CC",
    "leverLength": 28,
    "leverWidth": 8,
    "tip": "Ball",
    "angle": 24,
    "leverMaterial": "Chrome",
    "leverColour": "#FFC8D3CC"
  }
})CTRL" },
            { "Mustard Seventies", R"CTRL({
  "format": 1,
  "name": "Mustard Seventies",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF493321",
    "diameter": 76,
    "height": 35,
    "taper": -4,
    "edge": 17,
    "top": "Dish",
    "skirt": true,
    "grip": "Ribs",
    "cap": true,
    "pointer": "Line",
    "ring": false,
    "topDepth": 14,
    "skirtDiameter": 140,
    "skirtHeight": 16,
    "skirtBevel": 52,
    "skirtMaterial": "Brass",
    "skirtColour": "#FF7C5729",
    "skirtLine": true,
    "gripCount": 26,
    "gripDepth": 2.4,
    "capDiameter": 57,
    "capMaterial": "Brass",
    "capColour": "#FFE0BB78",
    "capGroove": true,
    "pointerColour": "#FF17191C",
    "pointerLength": 60,
    "pointerWidth": 9,
    "pointerSide": false,
    "pointerY": 0
  },
  "slider": {
    "trackLength": 88,
    "trackWidth": 11,
    "trackColour": "#FF0B1016",
    "ticks": 11,
    "cap": "Block",
    "capWidth": 57,
    "capHeight": 33,
    "material": "Bakelite",
    "colour": "#FF452B19",
    "lineColour": "#FFFFFFFF",
    "tickLength": 21,
    "tickColour": "#FFFFF2DA",
    "capLength": 20
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 62,
    "depth": 75,
    "height": 17,
    "edge": 36,
    "top": "Convex",
    "material": "Bakelite",
    "colour": "#FF493019",
    "travel": 26,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 39,
    "bezelMaterial": "Brass",
    "bezelColour": "#FFC3A166",
    "light": "Bar",
    "corner": 35,
    "topDepth": 10,
    "lightColour": "#FFFFE6B8",
    "lightOff": 21
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 47,
    "baseMaterial": "Brass",
    "baseColour": "#FFC49F57",
    "leverLength": 34,
    "leverWidth": 10,
    "tip": "Paddle",
    "angle": 29,
    "leverMaterial": "Brass",
    "leverColour": "#FFC49F57"
  }
})CTRL" },
            { "Red Lacquer", R"CTRL({
  "format": 1,
  "name": "Red Lacquer",
  "knob": {
    "material": "Gloss plastic",
    "colour": "#FF982A37",
    "diameter": 58,
    "height": 37,
    "taper": 20,
    "edge": 28,
    "top": "Dome",
    "skirt": false,
    "grip": "Knurled",
    "cap": true,
    "pointer": "Line",
    "ring": false,
    "topDepth": 14,
    "gripCount": 36,
    "gripDepth": 2.2,
    "capDiameter": 44,
    "capMaterial": "Chrome",
    "capColour": "#FFCFD9DD",
    "capGroove": false,
    "pointerColour": "#FF17191C",
    "pointerLength": 53,
    "pointerWidth": 7,
    "pointerSide": false,
    "pointerY": 3
  },
  "slider": {
    "trackLength": 91,
    "trackWidth": 12,
    "trackColour": "#FF0B1016",
    "ticks": 9,
    "cap": "Round",
    "capWidth": 62,
    "capHeight": 18,
    "material": "Chrome",
    "colour": "#FFD2D7D8",
    "lineColour": "#FF17191C",
    "tickLength": 12,
    "tickColour": "#FFFFF2EB"
  },
  "button": {
    "shape": "Round",
    "width": 65,
    "depth": 55,
    "height": 22,
    "edge": 41,
    "top": "Convex",
    "material": "Gloss plastic",
    "colour": "#FF8D2530",
    "travel": 29,
    "bezel": true,
    "bezelWidth": 9,
    "bezelHeight": 14,
    "bezelMaterial": "Chrome",
    "bezelColour": "#FFD6DEE0",
    "light": "Ring round it",
    "topDepth": 5,
    "lightColour": "#FFFFC879",
    "lightOff": 24
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 54,
    "baseMaterial": "Chrome",
    "baseColour": "#FFD5DEE4",
    "leverLength": 40,
    "leverWidth": 12,
    "tip": "Plain",
    "angle": 34,
    "leverMaterial": "Chrome",
    "leverColour": "#FFD5DEE4"
  }
})CTRL" },
            { "Ivory Ceramic", R"CTRL({
  "format": 1,
  "name": "Ivory Ceramic",
  "knob": {
    "material": "Satin plastic",
    "colour": "#FFF1E7D3",
    "diameter": 66,
    "height": 29,
    "taper": 13,
    "edge": 33,
    "top": "Flat",
    "skirt": true,
    "grip": "Flutes",
    "cap": false,
    "pointer": "Dot",
    "ring": false,
    "skirtDiameter": 115,
    "skirtHeight": 19,
    "skirtBevel": 69,
    "skirtMaterial": "Satin plastic",
    "skirtColour": "#FFC4B69D",
    "skirtLine": false,
    "gripCount": 16,
    "gripDepth": 2.2,
    "pointerColour": "#FF17191C",
    "pointerLength": 36,
    "pointerWidth": 9,
    "pointerSide": false,
    "pointerY": 5
  },
  "slider": {
    "trackLength": 94,
    "trackWidth": 13,
    "trackColour": "#FF171B21",
    "ticks": 7,
    "cap": "Classic fader",
    "capWidth": 67,
    "capHeight": 23,
    "material": "Satin plastic",
    "colour": "#FFF2E7D4",
    "lineColour": "#FF17191C",
    "tickLength": 15,
    "tickColour": "#FF453423",
    "capLength": 16,
    "grooves": 5
  },
  "button": {
    "shape": "Rounded square",
    "width": 68,
    "depth": 62,
    "height": 27,
    "edge": 46,
    "top": "Flat",
    "material": "Satin plastic",
    "colour": "#FFECE5D6",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 11,
    "bezelHeight": 19,
    "bezelMaterial": "Brass",
    "bezelColour": "#FFCFBD9C",
    "light": "Dot",
    "lightColour": "#FF3F5770",
    "lightOff": 8,
    "lightPlace": "Left"
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 61,
    "baseMaterial": "Brass",
    "baseColour": "#FFCBBC9D",
    "leverLength": 46,
    "leverWidth": 14,
    "tip": "Ball",
    "angle": 39,
    "leverMaterial": "Brass",
    "leverColour": "#FFCBBC9D"
  }
})CTRL" },
            { "Black Tolex", R"CTRL({
  "format": 1,
  "name": "Black Tolex",
  "knob": {
    "material": "Brushed aluminium",
    "colour": "#FFE0E4E5",
    "diameter": 78,
    "height": 37,
    "taper": 6,
    "edge": 15,
    "top": "Flat",
    "skirt": true,
    "skirtDiameter": 129,
    "skirtHeight": 16,
    "skirtBevel": 45,
    "skirtMaterial": "Steel",
    "skirtColour": "#FF38454C",
    "skirtLine": false,
    "grip": "Ribs",
    "gripCount": 44,
    "gripDepth": 2.9,
    "cap": true,
    "capDiameter": 61,
    "capMaterial": "Matte plastic",
    "capColour": "#FF20282F",
    "capGroove": true,
    "pointer": "Line",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 74,
    "pointerWidth": 7,
    "pointerSide": true,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 89,
    "trackWidth": 8,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 78,
    "capLength": 14,
    "capHeight": 27,
    "grooves": 6,
    "material": "Brushed aluminium",
    "colour": "#FFE0E4E5",
    "lineColour": "#FF1C2025"
  },
  "button": {
    "shape": "Rounded square",
    "width": 63,
    "depth": 63,
    "height": 18,
    "edge": 26,
    "top": "Concave",
    "topDepth": 5,
    "material": "Matte plastic",
    "colour": "#FF20282F",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF89939C",
    "light": "Dot",
    "lightPlace": "Top",
    "lightColour": "#FFFF5959",
    "lightOff": 16
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 56,
    "baseMaterial": "Steel",
    "baseColour": "#FF89939C",
    "leverLength": 46,
    "leverWidth": 12,
    "tip": "Ball",
    "angle": 31,
    "leverMaterial": "Chrome",
    "leverColour": "#FFE0E4E5"
  }
})CTRL" },
            { "Oxblood Leather", R"CTRL({
  "format": 1,
  "name": "Oxblood Leather",
  "knob": {
    "material": "Brass",
    "colour": "#FFC6A066",
    "diameter": 63,
    "height": 35,
    "taper": 2,
    "edge": 17,
    "top": "Dish",
    "topDepth": 7,
    "skirt": true,
    "skirtDiameter": 119,
    "skirtHeight": 17,
    "skirtBevel": 61,
    "skirtMaterial": "Brass",
    "skirtColour": "#FF6B4C2C",
    "skirtLine": false,
    "grip": "Knurled",
    "gripCount": 48,
    "gripDepth": 2.1,
    "cap": true,
    "capDiameter": 58,
    "capMaterial": "Bakelite",
    "capColour": "#FF381A1B",
    "capGroove": true,
    "pointer": "Groove",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 55,
    "pointerWidth": 6,
    "pointerSide": false,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 84,
    "trackWidth": 9,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 65,
    "capLength": 15,
    "capHeight": 25,
    "grooves": 4,
    "material": "Bakelite",
    "colour": "#FF381A1B",
    "lineColour": "#FFF5EEE0"
  },
  "button": {
    "shape": "Round",
    "width": 65,
    "depth": 65,
    "height": 22,
    "edge": 29,
    "top": "Concave",
    "topDepth": 8,
    "material": "Bakelite",
    "colour": "#FF381A1B",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Brass",
    "bezelColour": "#FFC6A066",
    "light": "Bar",
    "lightColour": "#FFE7B96E",
    "lightOff": 16
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 58,
    "baseMaterial": "Brass",
    "baseColour": "#FFC6A066",
    "leverLength": 40,
    "leverWidth": 11,
    "tip": "Ball",
    "angle": 35,
    "leverMaterial": "Brass",
    "leverColour": "#FFC6A066"
  }
})CTRL" },
            { "Grey Felt", R"CTRL({
  "format": 1,
  "name": "Grey Felt",
  "knob": {
    "material": "Rubber",
    "colour": "#FF4A5358",
    "diameter": 55,
    "height": 24,
    "taper": 0,
    "edge": 27,
    "top": "Dish",
    "skirt": false,
    "grip": "Smooth",
    "cap": false,
    "pointer": "Line",
    "ring": false,
    "topDepth": 6,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 54,
    "pointerWidth": 5,
    "pointerSide": false,
    "pointerY": 3
  },
  "slider": {
    "trackLength": 85,
    "trackWidth": 10,
    "trackColour": "#FF0B1016",
    "ticks": 0,
    "cap": "Round",
    "capWidth": 62,
    "capHeight": 23,
    "material": "Rubber",
    "colour": "#FF4B5358",
    "lineColour": "#FFFFFFFF"
  },
  "button": {
    "shape": "Pill",
    "width": 55,
    "depth": 63,
    "height": 22,
    "edge": 33,
    "top": "Concave",
    "material": "Rubber",
    "colour": "#FF485056",
    "travel": 44,
    "bezel": true,
    "bezelWidth": 9,
    "bezelHeight": 39,
    "bezelMaterial": "Painted metal",
    "bezelColour": "#FF677079",
    "light": "None",
    "topDepth": 10
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 54,
    "baseMaterial": "Painted metal",
    "baseColour": "#FF768086",
    "leverLength": 40,
    "leverWidth": 12,
    "tip": "Paddle",
    "angle": 34,
    "leverMaterial": "Steel",
    "leverColour": "#FFA5ACB2"
  }
})CTRL" },
            { "Walnut Cheek", R"CTRL({
  "format": 1,
  "name": "Walnut Cheek",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF222426",
    "diameter": 75,
    "height": 44,
    "taper": 14,
    "edge": 16,
    "top": "Flat",
    "skirt": true,
    "skirtDiameter": 127,
    "skirtHeight": 12,
    "skirtBevel": 40,
    "skirtMaterial": "Brass",
    "skirtColour": "#FF6B4C2C",
    "skirtLine": false,
    "grip": "Flutes",
    "gripCount": 32,
    "gripDepth": 4.4,
    "cap": false,
    "pointer": "Ridge",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 75,
    "pointerWidth": 8,
    "pointerSide": true,
    "pointerY": 5,
    "ring": false
  },
  "slider": {
    "trackLength": 92,
    "trackWidth": 9,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 75,
    "capLength": 15,
    "capHeight": 24,
    "grooves": 5,
    "material": "Bakelite",
    "colour": "#FF222426",
    "lineColour": "#FFF5EEE0"
  },
  "button": {
    "shape": "Rounded square",
    "width": 63,
    "depth": 63,
    "height": 18,
    "edge": 26,
    "top": "Convex",
    "topDepth": 5,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Brass",
    "bezelColour": "#FFC6A066",
    "light": "Dot",
    "lightPlace": "Left",
    "lightColour": "#FFE5A45F",
    "lightOff": 16
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 48,
    "baseMaterial": "Brass",
    "baseColour": "#FFC6A066",
    "leverLength": 43,
    "leverWidth": 11,
    "tip": "Ball",
    "angle": 29,
    "leverMaterial": "Steel",
    "leverColour": "#FFE0E4E5"
  }
})CTRL" },
            { "Ebony Studio", R"CTRL({
  "format": 1,
  "name": "Ebony Studio",
  "knob": {
    "material": "Brushed aluminium",
    "colour": "#FFE0E4E5",
    "diameter": 67,
    "height": 33,
    "taper": 3,
    "edge": 13,
    "top": "Dish",
    "topDepth": 8,
    "skirt": true,
    "skirtDiameter": 113,
    "skirtHeight": 17,
    "skirtBevel": 42,
    "skirtMaterial": "Brushed aluminium",
    "skirtColour": "#FF89939C",
    "skirtLine": true,
    "grip": "Knurled",
    "gripCount": 62,
    "gripDepth": 1.9,
    "cap": true,
    "capDiameter": 64,
    "capMaterial": "Matte plastic",
    "capColour": "#FF20282F",
    "capGroove": true,
    "pointer": "Dot",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 56,
    "pointerWidth": 10,
    "pointerSide": false,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 86,
    "trackWidth": 8,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 73,
    "capLength": 13,
    "capHeight": 24,
    "grooves": 6,
    "material": "Brushed aluminium",
    "colour": "#FFE0E4E5",
    "lineColour": "#FF1C2025"
  },
  "button": {
    "shape": "Square",
    "width": 56,
    "depth": 56,
    "height": 18,
    "edge": 12,
    "top": "Flat",
    "material": "Matte plastic",
    "colour": "#FF20282F",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Brass",
    "bezelColour": "#FF6B4C2C",
    "light": "Bar",
    "lightColour": "#FFD5B27A",
    "lightOff": 16
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 52,
    "baseMaterial": "Brass",
    "baseColour": "#FF6B4C2C",
    "leverLength": 36,
    "leverWidth": 12,
    "tip": "Paddle",
    "angle": 31,
    "leverMaterial": "Brass",
    "leverColour": "#FFC6A066"
  }
})CTRL" },
            { "Rosewood Console", R"CTRL({
  "format": 1,
  "name": "Rosewood Console",
  "knob": {
    "material": "Copper",
    "colour": "#FF785044",
    "diameter": 72,
    "height": 30,
    "taper": 24,
    "edge": 21,
    "top": "Dome",
    "topDepth": 6,
    "skirt": false,
    "grip": "Flutes",
    "gripCount": 24,
    "gripDepth": 2.6,
    "cap": false,
    "pointer": "Line",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 72,
    "pointerWidth": 6,
    "pointerSide": true,
    "pointerY": -2,
    "ring": false
  },
  "slider": {
    "trackLength": 84,
    "trackWidth": 8,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Block",
    "capWidth": 64,
    "capLength": 21,
    "capHeight": 27,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "lineColour": "#FFF5EEE0"
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 76,
    "depth": 55,
    "corner": 26,
    "height": 18,
    "edge": 26,
    "top": "Convex",
    "topDepth": 4,
    "material": "Bakelite",
    "colour": "#FF381A1B",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Copper",
    "bezelColour": "#FF785044",
    "light": "Dot",
    "lightPlace": "Top",
    "lightColour": "#FFFFB878",
    "lightOff": 16
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 54,
    "baseMaterial": "Copper",
    "baseColour": "#FF785044",
    "leverLength": 39,
    "leverWidth": 13,
    "tip": "Ball",
    "angle": 33,
    "leverMaterial": "Brass",
    "leverColour": "#FFC6A066"
  }
})CTRL" },
            { "Natural Oak", R"CTRL({
  "format": 1,
  "name": "Natural Oak",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF29251F",
    "diameter": 64,
    "height": 48,
    "taper": 4,
    "edge": 13,
    "top": "Dish",
    "skirt": true,
    "grip": "Flutes",
    "cap": true,
    "pointer": "Ridge",
    "ring": false,
    "topDepth": 9,
    "skirtDiameter": 129,
    "skirtHeight": 23,
    "skirtBevel": 46,
    "skirtMaterial": "Brass",
    "skirtColour": "#FF765833",
    "skirtLine": true,
    "gripCount": 39,
    "gripDepth": 2.5,
    "capDiameter": 48,
    "capMaterial": "Brass",
    "capColour": "#FFCCA66F",
    "capGroove": true,
    "pointerColour": "#FF17191C",
    "pointerLength": 63,
    "pointerWidth": 7,
    "pointerSide": true,
    "pointerY": -3
  },
  "slider": {
    "trackLength": 76,
    "trackWidth": 7,
    "trackColour": "#FF171B21",
    "ticks": 11,
    "cap": "Classic fader",
    "capWidth": 72,
    "capHeight": 28,
    "material": "Bakelite",
    "colour": "#FF2A2520",
    "lineColour": "#FFFFFFFF",
    "tickLength": 18,
    "tickColour": "#FF35281B",
    "capLength": 20,
    "grooves": 6
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 71,
    "depth": 69,
    "height": 32,
    "edge": 18,
    "top": "Concave",
    "material": "Bakelite",
    "colour": "#FF2C2620",
    "travel": 35,
    "bezel": true,
    "bezelWidth": 13,
    "bezelHeight": 24,
    "bezelMaterial": "Brass",
    "bezelColour": "#FFC9AB74",
    "light": "Dot",
    "corner": 74,
    "topDepth": 7,
    "lightColour": "#FF174D45",
    "lightOff": 11,
    "lightPlace": "Middle"
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 68,
    "baseMaterial": "Brass",
    "baseColour": "#FFD2B17F",
    "leverLength": 52,
    "leverWidth": 16,
    "tip": "Ball",
    "angle": 44,
    "leverMaterial": "Brass",
    "leverColour": "#FFD2B17F"
  }
})CTRL" },
            { "Burl Luxe", R"CTRL({
  "format": 1,
  "name": "Burl Luxe",
  "knob": {
    "material": "Gold",
    "colour": "#FFD3AD6C",
    "diameter": 72,
    "height": 33,
    "taper": -7,
    "edge": 25,
    "top": "Dome",
    "skirt": true,
    "grip": "Knurled",
    "cap": true,
    "pointer": "Dot",
    "ring": false,
    "topDepth": 8,
    "skirtDiameter": 121,
    "skirtHeight": 21,
    "skirtBevel": 62,
    "skirtMaterial": "Gold",
    "skirtColour": "#FF72552D",
    "skirtLine": false,
    "gripCount": 65,
    "gripDepth": 1.5,
    "capDiameter": 54,
    "capMaterial": "Bakelite",
    "capColour": "#FF2E241D",
    "capGroove": false,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 40,
    "pointerWidth": 8,
    "pointerSide": false,
    "pointerY": -4
  },
  "slider": {
    "trackLength": 79,
    "trackWidth": 8,
    "trackColour": "#FF0B1016",
    "ticks": 9,
    "cap": "Classic fader",
    "capWidth": 52,
    "capHeight": 33,
    "material": "Gold",
    "colour": "#FFCFB074",
    "lineColour": "#FF17191C",
    "tickLength": 21,
    "tickColour": "#FFFFF0D5",
    "capLength": 12,
    "grooves": 2
  },
  "button": {
    "shape": "Round",
    "width": 74,
    "depth": 76,
    "height": 12,
    "edge": 23,
    "top": "Convex",
    "material": "Gold",
    "colour": "#FFB58D58",
    "travel": 38,
    "bezel": true,
    "bezelWidth": 5,
    "bezelHeight": 29,
    "bezelMaterial": "Gold",
    "bezelColour": "#FFE2C58D",
    "light": "Dot",
    "topDepth": 8,
    "lightColour": "#FFFFCC75",
    "lightOff": 14,
    "lightPlace": "Top"
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 40,
    "baseMaterial": "Gold",
    "baseColour": "#FFE5C98E",
    "leverLength": 28,
    "leverWidth": 8,
    "tip": "Ball",
    "angle": 24,
    "leverMaterial": "Gold",
    "leverColour": "#FFE5C98E"
  }
})CTRL" },
            { "Flame Maple", R"CTRL({
  "format": 1,
  "name": "Flame Maple",
  "knob": {
    "material": "Brushed aluminium",
    "colour": "#FFD1D4D0",
    "diameter": 69,
    "height": 39,
    "taper": 5,
    "edge": 15,
    "top": "Flat",
    "skirt": false,
    "grip": "Ribs",
    "cap": true,
    "pointer": "Groove",
    "ring": false,
    "gripCount": 20,
    "gripDepth": 3.0,
    "capDiameter": 52,
    "capMaterial": "Bakelite",
    "capColour": "#FF292725",
    "capGroove": true,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 57,
    "pointerWidth": 5,
    "pointerSide": false,
    "pointerY": 0
  },
  "slider": {
    "trackLength": 82,
    "trackWidth": 9,
    "trackColour": "#FF171B21",
    "ticks": 9,
    "cap": "Block",
    "capWidth": 57,
    "capHeight": 18,
    "material": "Brushed aluminium",
    "colour": "#FFDDDBD3",
    "lineColour": "#FF17191C",
    "tickLength": 12,
    "tickColour": "#FF302113",
    "capLength": 16
  },
  "button": {
    "shape": "Rounded square",
    "width": 77,
    "depth": 56,
    "height": 17,
    "edge": 28,
    "top": "Flat",
    "material": "Satin plastic",
    "colour": "#FF282622",
    "travel": 41,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 34,
    "bezelMaterial": "Brushed aluminium",
    "bezelColour": "#FFAEAFAC",
    "light": "Dot",
    "lightColour": "#FF273A58",
    "lightOff": 17,
    "lightPlace": "Left"
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 47,
    "baseMaterial": "Chrome",
    "baseColour": "#FFD6D9D8",
    "leverLength": 34,
    "leverWidth": 10,
    "tip": "Plain",
    "angle": 29,
    "leverMaterial": "Chrome",
    "leverColour": "#FFD6D9D8"
  }
})CTRL" },
            { "Black Marble", R"CTRL({
  "format": 1,
  "name": "Black Marble",
  "knob": {
    "material": "Gold",
    "colour": "#FFC6A066",
    "diameter": 69,
    "height": 24,
    "taper": 22,
    "edge": 34,
    "top": "Dome",
    "topDepth": 5,
    "skirt": false,
    "grip": "Smooth",
    "cap": true,
    "capDiameter": 61,
    "capMaterial": "Bakelite",
    "capColour": "#FF222426",
    "capGroove": true,
    "pointer": "Dot",
    "pointerColour": "#FFF5EEE0",
    "pointerLength": 60,
    "pointerWidth": 8,
    "pointerSide": false,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 94,
    "trackWidth": 7,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 76,
    "capLength": 12,
    "capHeight": 19,
    "grooves": 8,
    "material": "Gold",
    "colour": "#FFC6A066",
    "lineColour": "#FF332118"
  },
  "button": {
    "shape": "Rounded square",
    "width": 64,
    "depth": 64,
    "height": 18,
    "edge": 26,
    "top": "Convex",
    "topDepth": 4,
    "material": "Gloss plastic",
    "colour": "#FF222426",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Gold",
    "bezelColour": "#FFC6A066",
    "light": "Ring round it",
    "lightColour": "#FFE6BF70",
    "lightOff": 16
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 54,
    "baseMaterial": "Gold",
    "baseColour": "#FFC6A066",
    "leverLength": 38,
    "leverWidth": 11,
    "tip": "Plain",
    "angle": 34,
    "leverMaterial": "Gold",
    "leverColour": "#FFC6A066"
  }
})CTRL" },
            { "White Marble", R"CTRL({
  "format": 1,
  "name": "White Marble",
  "knob": {
    "material": "Chrome",
    "colour": "#FFD6DCE0",
    "diameter": 74,
    "height": 38,
    "taper": -5,
    "edge": 19,
    "top": "Dome",
    "skirt": true,
    "grip": "Flutes",
    "cap": true,
    "pointer": "Groove",
    "ring": false,
    "topDepth": 12,
    "skirtDiameter": 133,
    "skirtHeight": 20,
    "skirtBevel": 55,
    "skirtMaterial": "Chrome",
    "skirtColour": "#FF747D81",
    "skirtLine": true,
    "gripCount": 27,
    "gripDepth": 1.9,
    "capDiameter": 56,
    "capMaterial": "Bakelite",
    "capColour": "#FF2B2C2E",
    "capGroove": false,
    "pointerColour": "#FFFFFFFF",
    "pointerLength": 67,
    "pointerWidth": 6,
    "pointerSide": false,
    "pointerY": 5
  },
  "slider": {
    "trackLength": 88,
    "trackWidth": 11,
    "trackColour": "#FF171B21",
    "ticks": 11,
    "cap": "Classic fader",
    "capWidth": 67,
    "capHeight": 28,
    "material": "Chrome",
    "colour": "#FFD6DADE",
    "lineColour": "#FF17191C",
    "tickLength": 18,
    "tickColour": "#FF2E353B",
    "capLength": 12,
    "grooves": 5
  },
  "button": {
    "shape": "Round",
    "width": 58,
    "depth": 70,
    "height": 27,
    "edge": 38,
    "top": "Convex",
    "material": "Satin plastic",
    "colour": "#FFCCD1D2",
    "travel": 20,
    "bezel": true,
    "bezelWidth": 11,
    "bezelHeight": 14,
    "bezelMaterial": "Chrome",
    "bezelColour": "#FFCDD6D5",
    "light": "Dot",
    "topDepth": 5,
    "lightColour": "#FF72532B",
    "lightOff": 23,
    "lightPlace": "Top"
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 61,
    "baseMaterial": "Chrome",
    "baseColour": "#FFD9E0DF",
    "leverLength": 46,
    "leverWidth": 14,
    "tip": "Ball",
    "angle": 39,
    "leverMaterial": "Chrome",
    "leverColour": "#FFD9E0DF"
  }
})CTRL" },
            { "Slate Stone", R"CTRL({
  "format": 1,
  "name": "Slate Stone",
  "knob": {
    "material": "Brushed aluminium",
    "colour": "#FF939DA2",
    "diameter": 64,
    "height": 35,
    "taper": 4,
    "edge": 14,
    "top": "Flat",
    "skirt": false,
    "grip": "Knurled",
    "cap": false,
    "pointer": "Ridge",
    "ring": true,
    "gripCount": 84,
    "gripDepth": 1.8,
    "pointerColour": "#FF17191C",
    "pointerLength": 51,
    "pointerWidth": 8,
    "pointerSide": false,
    "pointerY": -3,
    "ringColour": "#FFBDDE83",
    "ringSegments": 18,
    "ringWidth": 6,
    "ringGap": 5,
    "ringFrom": "The middle"
  },
  "slider": {
    "trackLength": 91,
    "trackWidth": 12,
    "trackColour": "#FF0B1016",
    "ticks": 9,
    "cap": "Block",
    "capWidth": 72,
    "capHeight": 33,
    "material": "Brushed aluminium",
    "colour": "#FF919DA3",
    "lineColour": "#FF17191C",
    "tickLength": 21,
    "tickColour": "#FFF0F4EF",
    "capLength": 16
  },
  "button": {
    "shape": "Rounded rectangle",
    "width": 61,
    "depth": 77,
    "height": 32,
    "edge": 43,
    "top": "Flat",
    "material": "Painted metal",
    "colour": "#FF414B50",
    "travel": 23,
    "bezel": true,
    "bezelWidth": 13,
    "bezelHeight": 19,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF888F98",
    "light": "Bar",
    "corner": 74,
    "lightColour": "#FFBDDE83",
    "lightOff": 7
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 68,
    "baseMaterial": "Steel",
    "baseColour": "#FFABB5B9",
    "leverLength": 52,
    "leverWidth": 16,
    "tip": "Paddle",
    "angle": 44,
    "leverMaterial": "Steel",
    "leverColour": "#FFABB5B9"
  }
})CTRL" },
            { "Carbon Fibre", R"CTRL({
  "format": 1,
  "name": "Carbon Fibre",
  "knob": {
    "material": "Anodized",
    "colour": "#FF20282F",
    "diameter": 71,
    "height": 35,
    "taper": -6,
    "edge": 12,
    "top": "Dish",
    "topDepth": 9,
    "skirt": true,
    "skirtDiameter": 114,
    "skirtHeight": 14,
    "skirtBevel": 21,
    "skirtMaterial": "Anodized",
    "skirtColour": "#FF353C41",
    "skirtLine": false,
    "grip": "Knurled",
    "gripCount": 72,
    "gripDepth": 1.5,
    "cap": true,
    "capDiameter": 74,
    "capMaterial": "Spun aluminium",
    "capColour": "#FFE0E4E5",
    "capGroove": true,
    "pointer": "Groove",
    "pointerColour": "#FF1C2025",
    "pointerLength": 66,
    "pointerWidth": 5,
    "pointerSide": false,
    "pointerY": 0,
    "ring": true,
    "ringSegments": 32,
    "ringWidth": 4,
    "ringGap": 5,
    "ringFrom": "The middle",
    "ringColour": "#FF00DDF2"
  },
  "slider": {
    "trackLength": 96,
    "trackWidth": 6,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF9FAF7",
    "cap": "Block",
    "capWidth": 60,
    "capLength": 22,
    "capHeight": 20,
    "material": "Anodized",
    "colour": "#FF20282F",
    "lineColour": "#FFF9FAF7"
  },
  "button": {
    "shape": "Pill",
    "width": 78,
    "depth": 52,
    "height": 14,
    "edge": 33,
    "top": "Flat",
    "material": "Anodized",
    "colour": "#FF20282F",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Steel",
    "bezelColour": "#FF38454C",
    "light": "Bar",
    "lightColour": "#FF00DDF2",
    "lightOff": 16
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 51,
    "baseMaterial": "Anodized",
    "baseColour": "#FF20282F",
    "leverLength": 34,
    "leverWidth": 14,
    "tip": "Paddle",
    "angle": 26,
    "leverMaterial": "Steel",
    "leverColour": "#FFE0E4E5"
  }
})CTRL" },
            { "Dark Screen Glass", R"CTRL({
  "format": 1,
  "name": "Dark Screen Glass",
  "knob": {
    "material": "Anodized",
    "colour": "#FF20282F",
    "diameter": 71,
    "height": 18,
    "taper": 7,
    "edge": 31,
    "top": "Flat",
    "skirt": false,
    "grip": "Smooth",
    "cap": true,
    "capDiameter": 79,
    "capMaterial": "Anodized",
    "capColour": "#FF20282F",
    "capGroove": false,
    "pointer": "Dot",
    "pointerColour": "#FFF9FAF7",
    "pointerLength": 68,
    "pointerWidth": 8,
    "pointerSide": false,
    "pointerY": 0,
    "ring": true,
    "ringSegments": 36,
    "ringWidth": 5,
    "ringGap": 4,
    "ringFrom": "The middle",
    "ringColour": "#FF65D9EE"
  },
  "slider": {
    "trackLength": 94,
    "trackWidth": 4,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Block",
    "capWidth": 52,
    "capLength": 18,
    "capHeight": 16,
    "material": "Anodized",
    "colour": "#FF20282F",
    "lineColour": "#FFF9FAF7"
  },
  "button": {
    "shape": "Rounded square",
    "width": 59,
    "depth": 59,
    "height": 11,
    "edge": 21,
    "top": "Flat",
    "material": "Gloss plastic",
    "colour": "#FF20282F",
    "travel": 32,
    "bezel": false,
    "light": "Ring round it",
    "lightColour": "#FF65D9EE",
    "lightOff": 16
  },
  "toggle": {
    "base": "Plate",
    "baseDiameter": 51,
    "baseMaterial": "Anodized",
    "baseColour": "#FF20282F",
    "leverLength": 30,
    "leverWidth": 16,
    "tip": "Paddle",
    "angle": 20,
    "leverMaterial": "Steel",
    "leverColour": "#FF38454C"
  }
})CTRL" },
            { "Frosted Smoke", R"CTRL({
  "format": 1,
  "name": "Frosted Smoke",
  "knob": {
    "material": "Spun aluminium",
    "colour": "#FFE0E4E5",
    "diameter": 64,
    "height": 27,
    "taper": 19,
    "edge": 28,
    "top": "Dish",
    "topDepth": 5,
    "skirt": true,
    "skirtDiameter": 111,
    "skirtHeight": 11,
    "skirtBevel": 42,
    "skirtMaterial": "Anodized",
    "skirtColour": "#FF38454C",
    "skirtLine": false,
    "grip": "Knurled",
    "gripCount": 46,
    "gripDepth": 1.3,
    "cap": true,
    "capDiameter": 73,
    "capMaterial": "Spun aluminium",
    "capColour": "#FFE0E4E5",
    "capGroove": true,
    "pointer": "Line",
    "pointerColour": "#FF1C2025",
    "pointerLength": 57,
    "pointerWidth": 5,
    "pointerSide": false,
    "pointerY": 0,
    "ring": true,
    "ringSegments": 20,
    "ringWidth": 5,
    "ringGap": 6,
    "ringFrom": "Its start",
    "ringColour": "#FF9CCAD0"
  },
  "slider": {
    "trackLength": 88,
    "trackWidth": 7,
    "trackColour": "#FF20282F",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 69,
    "capLength": 14,
    "capHeight": 21,
    "grooves": 7,
    "material": "Spun aluminium",
    "colour": "#FFE0E4E5",
    "lineColour": "#FF1C2025"
  },
  "button": {
    "shape": "Rounded square",
    "width": 64,
    "depth": 64,
    "height": 18,
    "edge": 26,
    "top": "Concave",
    "topDepth": 7,
    "material": "Anodized",
    "colour": "#FF20282F",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Spun aluminium",
    "bezelColour": "#FFE0E4E5",
    "light": "Dot",
    "lightPlace": "Middle",
    "lightColour": "#FF9CCAD0",
    "lightOff": 16
  },
  "toggle": {
    "base": "Round nut",
    "baseDiameter": 48,
    "baseMaterial": "Steel",
    "baseColour": "#FFE0E4E5",
    "leverLength": 37,
    "leverWidth": 11,
    "tip": "Plain",
    "angle": 30,
    "leverMaterial": "Steel",
    "leverColour": "#FFE0E4E5"
  }
})CTRL" },
            { "Amber Tinted Glass", R"CTRL({
  "format": 1,
  "name": "Amber Tinted Glass",
  "knob": {
    "material": "Bakelite",
    "colour": "#FF36251E",
    "diameter": 70,
    "height": 38,
    "taper": 30,
    "edge": 16,
    "top": "Flat",
    "skirt": true,
    "skirtDiameter": 120,
    "skirtHeight": 14,
    "skirtBevel": 38,
    "skirtMaterial": "Brass",
    "skirtColour": "#FF6B4C2C",
    "skirtLine": false,
    "grip": "Ribs",
    "gripCount": 26,
    "gripDepth": 3,
    "cap": true,
    "capDiameter": 50,
    "capMaterial": "Brass",
    "capColour": "#FFC6A066",
    "capGroove": true,
    "pointer": "Ridge",
    "pointerColour": "#FF1C2025",
    "pointerLength": 58,
    "pointerWidth": 6,
    "pointerSide": true,
    "pointerY": 0,
    "ring": false
  },
  "slider": {
    "trackLength": 84,
    "trackWidth": 8,
    "trackColour": "#FF15191C",
    "ticks": 11,
    "tickLength": 16,
    "tickColour": "#FFF5EEE0",
    "cap": "Classic fader",
    "capWidth": 65,
    "capLength": 17,
    "capHeight": 28,
    "grooves": 5,
    "material": "Brass",
    "colour": "#FFC6A066",
    "lineColour": "#FF1C2025"
  },
  "button": {
    "shape": "Rounded square",
    "width": 62,
    "depth": 62,
    "height": 18,
    "edge": 26,
    "top": "Convex",
    "topDepth": 7,
    "material": "Bakelite",
    "colour": "#FF36251E",
    "travel": 32,
    "bezel": true,
    "bezelWidth": 7,
    "bezelHeight": 24,
    "bezelMaterial": "Brass",
    "bezelColour": "#FF6B4C2C",
    "light": "The whole cap",
    "lightColour": "#FFFFC15A",
    "lightOff": 16
  },
  "toggle": {
    "base": "Hex nut",
    "baseDiameter": 50,
    "baseMaterial": "Brass",
    "baseColour": "#FFC6A066",
    "leverLength": 39,
    "leverWidth": 9,
    "tip": "Plain",
    "angle": 32,
    "leverMaterial": "Brass",
    "leverColour": "#FFC6A066"
  }
})CTRL" },
        };

        return sets;
    }

    /** The parts of a control set, the model each is (its JSON key) and the look it makes. */
    struct ControlPart
    {
        const char* key;      // in the set's JSON: "knob", "slider", "button", "toggle"
        const char* suffix;   // the look's name after the set's: "knob", "slider", "slider across"...
        bool across;          // a slider across (its model on its side)
    };

    inline const std::vector<ControlPart>& controlParts()
    {
        static const std::vector<ControlPart> parts { { "knob", "knob", false }, { "slider", "slider", false },
                                                      { "slider", "slider across", true }, { "button", "button", false },
                                                      { "toggle", "lever", false } };
        return parts;
    }

    /** A control look's name: "Aged Brass knob", "Aged Brass slider across"... */
    inline juce::String controlLookName (const juce::String& set, const juce::String& suffix)
    {
        return set + " " + suffix;
    }

    /** A look's control set, as parsed JSON (void: none). */
    inline juce::var findControls (const juce::String& name)
    {
        for (const auto& set : controlSets())
            if (name.trim().equalsIgnoreCase (set.name))
                return juce::JSON::parse (juce::String (set.json));

        return {};
    }

    /** Every knob, slider and switch on the window wears the look's matching controls (a switch:
        its push button). Their place, size and parameter stay as they are. Returns how many. */
    inline int applyControls (GuiLayout& layout, const juce::String& lookName)
    {
        const auto set = juce::String (lookName).trim();

        if (! findControls (set).isObject())
            return 0;

        int controls = 0;

        for (auto& w : layout.widgets)
        {
            using Type = GuiWidget::Type;

            if (w.type == Type::knob)
                w.style = controlLookName (set, "knob");
            else if (w.type == Type::slider)
                w.style = controlLookName (set, w.vertical ? "slider" : "slider across");
            else if (w.type == Type::toggle)
                w.style = controlLookName (set, "button");
            else
                continue;

            ++controls;
        }

        return controls;
    }

    inline juce::StringArray names()
    {
        juce::StringArray list;

        for (const auto& look : all())
            list.add (look.name);

        return list;
    }

    /** The look of that name, as parsed JSON (void: there's none of that name). */
    inline juce::var find (const juce::String& name)
    {
        for (const auto& look : all())
            if (name.trim().equalsIgnoreCase (look.name))
                return juce::JSON::parse (juce::String (look.json));

        return {};
    }

    inline juce::Colour colourOf (const juce::var& text)
    {
        return juce::Colour::fromString (text.toString().removeCharacters ("#"));
    }

    /** Puts a look on one shape: its material, colours, grain, patterns and layer style.
        Its place, size, corners and anything else it holds stay as they are. */
    inline void applyToWidget (GuiWidget& w, const juce::var& look)
    {
        const auto surface = look.getProperty ("surface", {});
        w.colour = colourOf (surface.getProperty ("color", {}));

        for (const auto* key : { "material", "finish", "color", "colour", "color2", "colour2", "scale", "grain", "shine", "flow",
                                 "pattern", "patterns", "patternColor", "patternColour", "patternOpacity", "patternBlend",
                                 "patternSize", "patternScale", "patternAngle", "patternWidth", "patternDepth",
                                 "depth", "shadow", "effects" })
            w.settings.erase (key);

        for (const auto* key : { "material", "finish", "color2", "scale", "grain", "shine", "depth", "shadow", "patterns", "effects" })
            if (surface.hasProperty (key))
                w.settings[key] = surface.getProperty (key, {}).clone();
    }

    /** A panel: a filled rectangle wide and tall enough not to be a thin divider. */
    inline bool isPanel (const GuiWidget& w)
    {
        constexpr int minPanelSide = 24;

        return w.type == GuiWidget::Type::shape && w.shapeKind() == "rectangle" && ObjectPaint::isFilled (w)
            && juce::jmin (w.bounds.getWidth(), w.bounds.getHeight()) >= minPanelSide;
    }

    /** Puts a look on the whole window: every panel, the background, the light and the text
        colours. Lines and other shapes stay as they are. Returns how many panels took it. */
    inline int applyToLayout (GuiLayout& layout, const juce::var& look)
    {
        const auto light = look.getProperty ("light", {});
        const auto background = look.getProperty ("background", {});
        const auto text = colourOf (look.getProperty ("textColor", {}));
        const auto accent = colourOf (look.getProperty ("accentColor", {}));

        layout.lightAngle = (float) (double) light.getProperty ("angle", layout.lightAngle);
        layout.lightAltitude = (float) (double) light.getProperty ("altitude", layout.lightAltitude);
        layout.backgroundTop = colourOf (background.getProperty ("top", {}));
        layout.backgroundBottom = colourOf (background.getProperty ("bottom", {}));

        int panels = 0;

        for (auto& w : layout.widgets)
        {
            using Type = GuiWidget::Type;

            if (isPanel (w))
            {
                applyToWidget (w, look);
                w.settings.erase ("light");      // lit by the window's one light from now on
                w.settings.erase ("altitude");
                ++panels;
                continue;
            }

            switch (w.type)
            {
                case Type::knob:   case Type::slider: case Type::toggle:   case Type::selector:
                case Type::meter:  case Type::scope:  case Type::lamp:     case Type::envelope:
                case Type::filter: case Type::xy:     case Type::keyboard:
                    w.textColour = text;
                    break;

                case Type::label:
                    if (PanelParts::labelDisplayOf (w) != "led")   // LED digits keep their own colour
                        w.colour = text;
                    break;

                case Type::group:
                    w.colour = text;
                    break;

                case Type::tabs:
                    w.colour = accent;
                    break;

                default:
                    break;
            }
        }

        return panels;
    }

    /** Each look's displays and fonts (applyTheme). */
    struct Theme
    {
        const char* name;   // its material look's
        const char* json;
    };

    inline const std::vector<Theme>& themes()
    {
        static const std::vector<Theme> list {
            { "Dark Brushed Aluminium", R"THEME({
  "format": 1,
  "name": "Dark Brushed Aluminium",
  "fonts": {
    "title": "Michroma",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 49,
  "lamp": {
    "display": "dome",
    "color": "#FFE6A34A",
    "bezel": "#FF1A2025"
  },
  "selector": {
    "display": "keys",
    "color": "#FFE6A34A",
    "background": "#FF181D21"
  },
  "readout": {
    "display": "readout",
    "color": "#FFF0EEE8",
    "background": "#FF171D22"
  },
  "digits": {
    "color": "#FFE6A34A",
    "window": "3d",
    "background": "#FF101418",
    "ghost": 11,
    "glow": 46,
    "slant": 3,
    "thickness": 14
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFE6A34A",
    "whiteKeys": "#FFE9E7E0",
    "blackKeys": "#FF191E23"
  },
  "meter": {
    "display": "segments",
    "colorLow": "#FF4ECF8A",
    "colorMid": "#FFFFC354",
    "colorHigh": "#FFF15945",
    "background": "#FF11171A",
    "segments": 16,
    "peakHold": true,
    "scale": true,
    "glow": 0.42
  }
})THEME" },
            { "Black Anodized", R"THEME({
  "format": 1,
  "name": "Black Anodized",
  "fonts": {
    "title": "Orbitron",
    "captions": "Rajdhani",
    "values": "Share Tech Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": true,
  "captionSize": 12.0,
  "camera": 45,
  "lamp": {
    "display": "square",
    "color": "#FF55C7D9",
    "bezel": "#FF0B131C"
  },
  "selector": {
    "display": "vfd",
    "color": "#FF55C7D9",
    "background": "#FF081016"
  },
  "readout": {
    "display": "vfd",
    "color": "#FF85F2F6",
    "background": "#FF09121A"
  },
  "digits": {
    "color": "#FF55C7D9",
    "window": "flat",
    "background": "#FF071119",
    "ghost": 9,
    "glow": 75,
    "slant": 7,
    "thickness": 12
  },
  "keyboard": {
    "display": "modern",
    "color": "#FF55C7D9",
    "whiteKeys": "#FF3B4750",
    "blackKeys": "#FF10161D"
  },
  "meter": {
    "display": "dots",
    "colorLow": "#FF58DFAA",
    "colorMid": "#FFE9D15F",
    "colorHigh": "#FFFF665B",
    "background": "#FF0A1218",
    "segments": 22,
    "peakHold": true,
    "scale": false,
    "glow": 0.8
  }
})THEME" },
            { "Aged Brass", R"THEME({
  "format": 1,
  "name": "Aged Brass",
  "fonts": {
    "title": "Cinzel",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 59,
  "lamp": {
    "display": "jewel",
    "color": "#FFFFC36A",
    "bezel": "#FF493B25"
  },
  "selector": {
    "display": "lamps",
    "color": "#FFFFC36A",
    "background": "#FF2B231A"
  },
  "readout": {
    "display": "readout",
    "color": "#FFF5E8C6",
    "background": "#FF2E2419"
  },
  "digits": {
    "color": "#FFFFC36A",
    "window": "3d",
    "background": "#FF24180D",
    "ghost": 17,
    "glow": 35,
    "slant": 0,
    "thickness": 17
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFFFC36A",
    "whiteKeys": "#FFF2E5C7",
    "blackKeys": "#FF201A16"
  },
  "meter": {
    "display": "porthole",
    "colorLow": "#FF5B986E",
    "colorMid": "#FFD4A34F",
    "colorHigh": "#FFD95342",
    "background": "#FFF0DFC0",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.12
  }
})THEME" },
            { "Silver Faceplate", R"THEME({
  "format": 1,
  "name": "Silver Faceplate",
  "fonts": {
    "title": "Oswald",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.0,
  "camera": 47,
  "lamp": {
    "display": "jewel",
    "color": "#FF852B39",
    "bezel": "#FF858B92"
  },
  "selector": {
    "display": "keys",
    "color": "#FF852B39",
    "background": "#FFF0EBE5"
  },
  "readout": {
    "display": "lcd",
    "color": "#FF303F37",
    "background": "#FFD8E0D0"
  },
  "digits": {
    "color": "#FF852B39",
    "window": "flat",
    "background": "#FFF1E9E5",
    "ghost": 7,
    "glow": 20,
    "slant": 0,
    "thickness": 13
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF852B39",
    "whiteKeys": "#FFEBE9E4",
    "blackKeys": "#FF292D32"
  },
  "meter": {
    "display": "edge",
    "colorLow": "#FF39917A",
    "colorMid": "#FFDF9E3F",
    "colorHigh": "#FFC64A45",
    "background": "#FFF1EDE1",
    "segments": 0,
    "peakHold": true,
    "scale": true,
    "glow": 0.08
  }
})THEME" },
            { "Workshop Hammertone", R"THEME({
  "format": 1,
  "name": "Workshop Hammertone",
  "fonts": {
    "title": "Oswald",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": true,
  "captionSize": 12.5,
  "camera": 57,
  "lamp": {
    "display": "jewel",
    "color": "#FFFFB94E",
    "bezel": "#FF26394A"
  },
  "selector": {
    "display": "keys",
    "color": "#FFFFB94E",
    "background": "#FF182935"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFF2D9",
    "background": "#FF162532"
  },
  "digits": {
    "color": "#FFFFB94E",
    "window": "3d",
    "background": "#FF14202B",
    "ghost": 22,
    "glow": 33,
    "slant": 2,
    "thickness": 20
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFFFB94E",
    "whiteKeys": "#FFE9E1CF",
    "blackKeys": "#FF26343D"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF588570",
    "colorMid": "#FFE0B24C",
    "colorHigh": "#FFDC5443",
    "background": "#FFE6D9B9",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.07
  }
})THEME" },
            { "Red Anodized", R"THEME({
  "format": 1,
  "name": "Red Anodized",
  "fonts": {
    "title": "Michroma",
    "captions": "Rajdhani",
    "values": "Share Tech Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 53,
  "lamp": {
    "display": "square",
    "color": "#FF95F3EA",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "cells",
    "color": "#FF95F3EA",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FF95F3EA",
    "window": "flat",
    "background": "#FF0C1216",
    "ghost": 9,
    "glow": 25,
    "slant": 0,
    "thickness": 12
  },
  "keyboard": {
    "display": "modern",
    "color": "#FF95F3EA",
    "whiteKeys": "#FF48555F",
    "blackKeys": "#FF182028"
  },
  "meter": {
    "display": "segments",
    "colorLow": "#FF44CDBA",
    "colorMid": "#FFFFC963",
    "colorHigh": "#FFE04B40",
    "background": "#FF0C1519",
    "segments": 20,
    "peakHold": true,
    "scale": false,
    "glow": 0.55
  }
})THEME" },
            { "Blue Anodized", R"THEME({
  "format": 1,
  "name": "Blue Anodized",
  "fonts": {
    "title": "Orbitron",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "glow",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12,
  "camera": 57,
  "lamp": {
    "display": "square",
    "color": "#FFFFCB74",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "pill",
    "color": "#FFFFCB74",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFFFCB74",
    "window": "3d",
    "background": "#FF0C1216",
    "ghost": 14,
    "glow": 33,
    "slant": 2,
    "thickness": 14
  },
  "keyboard": {
    "display": "modern",
    "color": "#FFFFCB74",
    "whiteKeys": "#FF48555F",
    "blackKeys": "#FF182028"
  },
  "meter": {
    "display": "arc",
    "colorLow": "#FF48C979",
    "colorMid": "#FFF3AB40",
    "colorHigh": "#FFFF6046",
    "background": "#FF0C1519",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.65
  }
})THEME" },
            { "Gold Anodized", R"THEME({
  "format": 1,
  "name": "Gold Anodized",
  "fonts": {
    "title": "Cinzel",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 46,
  "lamp": {
    "display": "jewel",
    "color": "#FF263B68",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "keys",
    "color": "#FF263B68",
    "background": "#FFE2E7E8"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FF263B68",
    "window": "3d",
    "background": "#FFE8E9E7",
    "ghost": 19,
    "glow": 41,
    "slant": 4,
    "thickness": 16
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF263B68",
    "whiteKeys": "#FFEEE5D1",
    "blackKeys": "#FF28231E"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF53946F",
    "colorMid": "#FFDFC060",
    "colorHigh": "#FFCF3E37",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.1
  }
})THEME" },
            { "Mirror Chrome", R"THEME({
  "format": 1,
  "name": "Mirror Chrome",
  "fonts": {
    "title": "Montserrat",
    "captions": "Inter",
    "values": "Share Tech Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12,
  "camera": 58,
  "lamp": {
    "display": "dome",
    "color": "#FF602642",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "pill",
    "color": "#FF602642",
    "background": "#FFE2E7E8"
  },
  "readout": {
    "display": "lcd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FF602642",
    "window": "flat",
    "background": "#FFE8E9E7",
    "ghost": 24,
    "glow": 49,
    "slant": 6,
    "thickness": 18
  },
  "keyboard": {
    "display": "graphite",
    "color": "#FF602642",
    "whiteKeys": "#FF404850",
    "blackKeys": "#FFA7B3B6"
  },
  "meter": {
    "display": "edge",
    "colorLow": "#FF39BCA3",
    "colorMid": "#FFFFC963",
    "colorHigh": "#FFE04B40",
    "background": "#FF10171C",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.15
  }
})THEME" },
            { "Brushed Steel", R"THEME({
  "format": 1,
  "name": "Brushed Steel",
  "fonts": {
    "title": "Oswald",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": true,
  "captionSize": 12.5,
  "camera": 55,
  "lamp": {
    "display": "dome",
    "color": "#FF194D72",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "keys",
    "color": "#FF194D72",
    "background": "#FFE2E7E8"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FF194D72",
    "window": "3d",
    "background": "#FFE8E9E7",
    "ghost": 29,
    "glow": 57,
    "slant": 8,
    "thickness": 20
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF194D72",
    "whiteKeys": "#FFECECE7",
    "blackKeys": "#FF1D2024"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF44CDBA",
    "colorMid": "#FFF3AB40",
    "colorHigh": "#FFFF6046",
    "background": "#FF10171C",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.12
  }
})THEME" },
            { "Gunmetal Grey", R"THEME({
  "format": 1,
  "name": "Gunmetal Grey",
  "fonts": {
    "title": "Rajdhani",
    "captions": "Rajdhani",
    "values": "Share Tech Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12,
  "camera": 54,
  "lamp": {
    "display": "dot",
    "color": "#FFFFBD6F",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "cells",
    "color": "#FFFFBD6F",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFFFBD6F",
    "window": "flat",
    "background": "#FF0C1216",
    "ghost": 34,
    "glow": 65,
    "slant": 10,
    "thickness": 22
  },
  "keyboard": {
    "display": "graphite",
    "color": "#FFFFBD6F",
    "whiteKeys": "#FF404850",
    "blackKeys": "#FFA7B3B6"
  },
  "meter": {
    "display": "bar",
    "colorLow": "#FF48C979",
    "colorMid": "#FFDFC060",
    "colorHigh": "#FFCF3E37",
    "background": "#FF0C1519",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.28
  }
})THEME" },
            { "Liquid Titanium", R"THEME({
  "format": 1,
  "name": "Liquid Titanium",
  "fonts": {
    "title": "Exo 2",
    "captions": "Rajdhani",
    "values": "Share Tech Mono"
  },
  "titleStyle": "glow",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12,
  "camera": 65,
  "lamp": {
    "display": "square",
    "color": "#FFCDAFFF",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "vfd",
    "color": "#FFCDAFFF",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFCDAFFF",
    "window": "3d",
    "background": "#FF0C1216",
    "ghost": 9,
    "glow": 73,
    "slant": 12,
    "thickness": 12
  },
  "keyboard": {
    "display": "modern",
    "color": "#FFCDAFFF",
    "whiteKeys": "#FF48555F",
    "blackKeys": "#FF182028"
  },
  "meter": {
    "display": "neon",
    "colorLow": "#FF53946F",
    "colorMid": "#FFFFC963",
    "colorHigh": "#FFE04B40",
    "background": "#FF0C1519",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.8
  }
})THEME" },
            { "Copper Patina", R"THEME({
  "format": 1,
  "name": "Copper Patina",
  "fonts": {
    "title": "Cinzel",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 57,
  "lamp": {
    "display": "jewel",
    "color": "#FFB5EBE0",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "lamps",
    "color": "#FFB5EBE0",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFB5EBE0",
    "window": "3d",
    "background": "#FF0C1216",
    "ghost": 14,
    "glow": 25,
    "slant": 1,
    "thickness": 14
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFB5EBE0",
    "whiteKeys": "#FFEEE5D1",
    "blackKeys": "#FF212833"
  },
  "meter": {
    "display": "porthole",
    "colorLow": "#FF39BCA3",
    "colorMid": "#FFF3AB40",
    "colorHigh": "#FFFF6046",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.14
  }
})THEME" },
            { "Cast Iron", R"THEME({
  "format": 1,
  "name": "Cast Iron",
  "fonts": {
    "title": "Russo One",
    "captions": "Oswald",
    "values": "Roboto Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": true,
  "captionSize": 12,
  "camera": 60,
  "lamp": {
    "display": "jewel",
    "color": "#FFFFBE65",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "keys",
    "color": "#FFFFBE65",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFFFBE65",
    "window": "3d",
    "background": "#FF0C1216",
    "ghost": 19,
    "glow": 33,
    "slant": 3,
    "thickness": 16
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFFFBE65",
    "whiteKeys": "#FFE3EDF2",
    "blackKeys": "#FF1D2024"
  },
  "meter": {
    "display": "edge",
    "colorLow": "#FF44CDBA",
    "colorMid": "#FFDFC060",
    "colorHigh": "#FFCF3E37",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.1
  }
})THEME" },
            { "Matte Black Polymer", R"THEME({
  "format": 1,
  "name": "Matte Black Polymer",
  "fonts": {
    "title": "Exo 2",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 11.5,
  "camera": 36,
  "lamp": {
    "display": "dot",
    "color": "#FF79B7A6",
    "bezel": "#FF15181A"
  },
  "selector": {
    "display": "pill",
    "color": "#FF79B7A6",
    "background": "#FF101715"
  },
  "readout": {
    "display": "lcd",
    "color": "#FFC5F6E0",
    "background": "#FF15251F"
  },
  "digits": {
    "color": "#FF79B7A6",
    "window": "flat",
    "background": "#FF0B1511",
    "ghost": 0,
    "glow": 35,
    "slant": 1,
    "thickness": 12
  },
  "keyboard": {
    "display": "modern",
    "color": "#FF79B7A6",
    "whiteKeys": "#FF353D3C",
    "blackKeys": "#FF121719"
  },
  "meter": {
    "display": "bar",
    "colorLow": "#FF70C59A",
    "colorMid": "#FFE1C46B",
    "colorHigh": "#FFFF6A56",
    "background": "#FF111717",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.24
  }
})THEME" },
            { "Vintage Bakelite", R"THEME({
  "format": 1,
  "name": "Vintage Bakelite",
  "fonts": {
    "title": "Righteous",
    "captions": "Barlow Condensed",
    "values": "VT323"
  },
  "titleStyle": "embossed",
  "titleBold": false,
  "captionBold": false,
  "captionSize": 13.0,
  "camera": 62,
  "lamp": {
    "display": "jewel",
    "color": "#FFFFB65B",
    "bezel": "#FF2B170F"
  },
  "selector": {
    "display": "lamps",
    "color": "#FFFFB65B",
    "background": "#FF26130E"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFE1AD",
    "background": "#FF24140F"
  },
  "digits": {
    "color": "#FFFFB65B",
    "window": "3d",
    "background": "#FF1C0F0B",
    "ghost": 21,
    "glow": 46,
    "slant": 2,
    "thickness": 17
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFFFB65B",
    "whiteKeys": "#FFEEDCC2",
    "blackKeys": "#FF251B15"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF70956A",
    "colorMid": "#FFE1AE4D",
    "colorHigh": "#FFE15A42",
    "background": "#FFE9D7B4",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.09
  }
})THEME" },
            { "Soft Touch Rubber", R"THEME({
  "format": 1,
  "name": "Soft Touch Rubber",
  "fonts": {
    "title": "Rajdhani",
    "captions": "Inter",
    "values": "Share Tech Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 32,
  "lamp": {
    "display": "dot",
    "color": "#FF8BC4D0",
    "bezel": "#FF151819"
  },
  "selector": {
    "display": "cells",
    "color": "#FF8BC4D0",
    "background": "#FF161B1D"
  },
  "readout": {
    "display": "lcd",
    "color": "#FFD0EFF4",
    "background": "#FF142226"
  },
  "digits": {
    "color": "#FF8BC4D0",
    "window": "none",
    "background": "#FF10181B",
    "ghost": 4,
    "glow": 45,
    "slant": 0,
    "thickness": 11
  },
  "keyboard": {
    "display": "graphite",
    "color": "#FF8BC4D0",
    "whiteKeys": "#FF353A3D",
    "blackKeys": "#FFB6C5C9"
  },
  "meter": {
    "display": "arc",
    "colorLow": "#FF60BF9F",
    "colorMid": "#FFF2BC63",
    "colorHigh": "#FFFF6656",
    "background": "#FF11181C",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.36
  }
})THEME" },
            { "Cream Console", R"THEME({
  "format": 1,
  "name": "Cream Console",
  "fonts": {
    "title": "Oswald",
    "captions": "Roboto",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 52,
  "lamp": {
    "display": "dome",
    "color": "#FF98501A",
    "bezel": "#FF8D7659"
  },
  "selector": {
    "display": "keys",
    "color": "#FF98501A",
    "background": "#FFF1E5D0"
  },
  "readout": {
    "display": "lcd",
    "color": "#FF433A26",
    "background": "#FFDACCA9"
  },
  "digits": {
    "color": "#FF98501A",
    "window": "flat",
    "background": "#FFF0E4CA",
    "ghost": 9,
    "glow": 17,
    "slant": 0,
    "thickness": 15
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF98501A",
    "whiteKeys": "#FFF8F0DC",
    "blackKeys": "#FF392D22"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF45806C",
    "colorMid": "#FFBB8742",
    "colorHigh": "#FFC34B32",
    "background": "#FFEEE0BE",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.05
  }
})THEME" },
            { "Burnt Orange Seventies", R"THEME({
  "format": 1,
  "name": "Burnt Orange Seventies",
  "fonts": {
    "title": "Righteous",
    "captions": "Barlow Condensed",
    "values": "VT323"
  },
  "titleStyle": "shadow",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 13.5,
  "camera": 61,
  "lamp": {
    "display": "dome",
    "color": "#FFFFDE79",
    "bezel": "#FF5C230C"
  },
  "selector": {
    "display": "tiles",
    "color": "#FFFFDE79",
    "background": "#FF41170A"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFEFCD",
    "background": "#FF471B0C"
  },
  "digits": {
    "color": "#FFFFDE79",
    "window": "3d",
    "background": "#FF3A1407",
    "ghost": 18,
    "glow": 54,
    "slant": 8,
    "thickness": 22
  },
  "keyboard": {
    "display": "reverse",
    "color": "#FFFFDE79",
    "whiteKeys": "#FF332119",
    "blackKeys": "#FFF4DFB9"
  },
  "meter": {
    "display": "edge",
    "colorLow": "#FF6DCE8F",
    "colorMid": "#FFFFC864",
    "colorHigh": "#FFFF6448",
    "background": "#FFF6DEA9",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.12
  }
})THEME" },
            { "White Studio", R"THEME({
  "format": 1,
  "name": "White Studio",
  "fonts": {
    "title": "Montserrat",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12,
  "camera": 42,
  "lamp": {
    "display": "square",
    "color": "#FF125F82",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "pill",
    "color": "#FF125F82",
    "background": "#FFE2E7E8"
  },
  "readout": {
    "display": "lcd",
    "color": "#FF17191C",
    "background": "#FFD7E1D9"
  },
  "digits": {
    "color": "#FF125F82",
    "window": "flat",
    "background": "#FFF0ECE0",
    "ghost": 24,
    "glow": 41,
    "slant": 5,
    "thickness": 18
  },
  "keyboard": {
    "display": "modern",
    "color": "#FF125F82",
    "whiteKeys": "#FF48555F",
    "blackKeys": "#FF182028"
  },
  "meter": {
    "display": "bar",
    "colorLow": "#FF48C979",
    "colorMid": "#FFFFC963",
    "colorHigh": "#FFE04B40",
    "background": "#FF0C1519",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.25
  }
})THEME" },
            { "Mint Retro", R"THEME({
  "format": 1,
  "name": "Mint Retro",
  "fonts": {
    "title": "Righteous",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 13,
  "camera": 51,
  "lamp": {
    "display": "dome",
    "color": "#FF7C3C2C",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "keys",
    "color": "#FF7C3C2C",
    "background": "#FFF0F1EB"
  },
  "readout": {
    "display": "lcd",
    "color": "#FF17191C",
    "background": "#FFD0DACB"
  },
  "digits": {
    "color": "#FF7C3C2C",
    "window": "3d",
    "background": "#FFF0ECE0",
    "ghost": 29,
    "glow": 49,
    "slant": 7,
    "thickness": 20
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF7C3C2C",
    "whiteKeys": "#FFF5F0E4",
    "blackKeys": "#FF28231E"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF53946F",
    "colorMid": "#FFF3AB40",
    "colorHigh": "#FFFF6046",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.12
  }
})THEME" },
            { "Mustard Seventies", R"THEME({
  "format": 1,
  "name": "Mustard Seventies",
  "fonts": {
    "title": "Bebas Neue",
    "captions": "Barlow Condensed",
    "values": "VT323"
  },
  "titleStyle": "shadow",
  "titleBold": true,
  "captionBold": true,
  "captionSize": 13,
  "camera": 49,
  "lamp": {
    "display": "dome",
    "color": "#FFFFE6B8",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "tiles",
    "color": "#FFFFE6B8",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "lcd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFFFE6B8",
    "window": "3d",
    "background": "#FF0C1216",
    "ghost": 34,
    "glow": 57,
    "slant": 9,
    "thickness": 22
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFFFE6B8",
    "whiteKeys": "#FFF1F4EF",
    "blackKeys": "#FF212833"
  },
  "meter": {
    "display": "segments",
    "colorLow": "#FF39BCA3",
    "colorMid": "#FFDFC060",
    "colorHigh": "#FFCF3E37",
    "background": "#FF0C1519",
    "segments": 12,
    "peakHold": true,
    "scale": true,
    "glow": 0.3
  }
})THEME" },
            { "Red Lacquer", R"THEME({
  "format": 1,
  "name": "Red Lacquer",
  "fonts": {
    "title": "Audiowide",
    "captions": "Rajdhani",
    "values": "Share Tech Mono"
  },
  "titleStyle": "shadow",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 59,
  "lamp": {
    "display": "jewel",
    "color": "#FFFFC879",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "keys",
    "color": "#FFFFC879",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFFFC879",
    "window": "3d",
    "background": "#FF0C1216",
    "ghost": 9,
    "glow": 65,
    "slant": 11,
    "thickness": 12
  },
  "keyboard": {
    "display": "graphite",
    "color": "#FFFFC879",
    "whiteKeys": "#FF404850",
    "blackKeys": "#FFA7B3B6"
  },
  "meter": {
    "display": "arc",
    "colorLow": "#FF44CDBA",
    "colorMid": "#FFFFC963",
    "colorHigh": "#FFE04B40",
    "background": "#FF0C1519",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.55
  }
})THEME" },
            { "Ivory Ceramic", R"THEME({
  "format": 1,
  "name": "Ivory Ceramic",
  "fonts": {
    "title": "Playfair Display",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 13,
  "camera": 49,
  "lamp": {
    "display": "jewel",
    "color": "#FF3F5770",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "lamps",
    "color": "#FF3F5770",
    "background": "#FFF0F1EB"
  },
  "readout": {
    "display": "readout",
    "color": "#FF17191C",
    "background": "#FFE1DED6"
  },
  "digits": {
    "color": "#FF3F5770",
    "window": "3d",
    "background": "#FFF0ECE0",
    "ghost": 14,
    "glow": 73,
    "slant": 0,
    "thickness": 14
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF3F5770",
    "whiteKeys": "#FFE3EDF2",
    "blackKeys": "#FF17202A"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF48C979",
    "colorMid": "#FFF3AB40",
    "colorHigh": "#FFFF6046",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.12
  }
})THEME" },
            { "Black Tolex", R"THEME({
  "format": 1,
  "name": "Black Tolex",
  "fonts": {
    "title": "Bebas Neue",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": true,
  "captionSize": 13.0,
  "camera": 64,
  "lamp": {
    "display": "jewel",
    "color": "#FFFF5959",
    "bezel": "#FF646467"
  },
  "selector": {
    "display": "lamps",
    "color": "#FFFF5959",
    "background": "#FF17191B"
  },
  "readout": {
    "display": "readout",
    "color": "#FFF3EBDD",
    "background": "#FF18181A"
  },
  "digits": {
    "color": "#FFFF5959",
    "window": "3d",
    "background": "#FF131416",
    "ghost": 24,
    "glow": 52,
    "slant": 3,
    "thickness": 17
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFFF5959",
    "whiteKeys": "#FFF0E6D5",
    "blackKeys": "#FF202122"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF62A77D",
    "colorMid": "#FFD6A05B",
    "colorHigh": "#FFDB4035",
    "background": "#FFE9D8B9",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.14
  }
})THEME" },
            { "Oxblood Leather", R"THEME({
  "format": 1,
  "name": "Oxblood Leather",
  "fonts": {
    "title": "Cinzel",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.0,
  "camera": 68,
  "lamp": {
    "display": "jewel",
    "color": "#FFE7B96E",
    "bezel": "#FF5C353B"
  },
  "selector": {
    "display": "keys",
    "color": "#FFE7B96E",
    "background": "#FF2E171D"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFEAD3",
    "background": "#FF30161D"
  },
  "digits": {
    "color": "#FFE7B96E",
    "window": "3d",
    "background": "#FF270E15",
    "ghost": 14,
    "glow": 29,
    "slant": 0,
    "thickness": 16
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFE7B96E",
    "whiteKeys": "#FFF6E8D2",
    "blackKeys": "#FF24161B"
  },
  "meter": {
    "display": "porthole",
    "colorLow": "#FF60967C",
    "colorMid": "#FFDFB052",
    "colorHigh": "#FFD14B42",
    "background": "#FFF0E2C5",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.08
  }
})THEME" },
            { "Grey Felt", R"THEME({
  "format": 1,
  "name": "Grey Felt",
  "fonts": {
    "title": "Inter",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12,
  "camera": 43,
  "lamp": {
    "display": "dot",
    "color": "#FFB5D5CF",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "pill",
    "color": "#FFB5D5CF",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFB5D5CF",
    "window": "flat",
    "background": "#FF0C1216",
    "ghost": 34,
    "glow": 49,
    "slant": 8,
    "thickness": 22
  },
  "keyboard": {
    "display": "graphite",
    "color": "#FFB5D5CF",
    "whiteKeys": "#FF404850",
    "blackKeys": "#FFA7B3B6"
  },
  "meter": {
    "display": "bar",
    "colorLow": "#FF48C979",
    "colorMid": "#FFDFC060",
    "colorHigh": "#FFCF3E37",
    "background": "#FF0C1519",
    "segments": 0,
    "peakHold": false,
    "scale": false,
    "glow": 0.22
  }
})THEME" },
            { "Walnut Cheek", R"THEME({
  "format": 1,
  "name": "Walnut Cheek",
  "fonts": {
    "title": "Playfair Display",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.0,
  "camera": 60,
  "lamp": {
    "display": "dome",
    "color": "#FFE5A45F",
    "bezel": "#FF38251A"
  },
  "selector": {
    "display": "keys",
    "color": "#FFE5A45F",
    "background": "#FF24170F"
  },
  "readout": {
    "display": "readout",
    "color": "#FFF5E6D1",
    "background": "#FF25180F"
  },
  "digits": {
    "color": "#FFE5A45F",
    "window": "flat",
    "background": "#FF21150E",
    "ghost": 17,
    "glow": 32,
    "slant": 0,
    "thickness": 16
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFE5A45F",
    "whiteKeys": "#FFF0E1C8",
    "blackKeys": "#FF27211C"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF518E69",
    "colorMid": "#FFE4AF54",
    "colorHigh": "#FFD05B43",
    "background": "#FFF1E3C8",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.07
  }
})THEME" },
            { "Ebony Studio", R"THEME({
  "format": 1,
  "name": "Ebony Studio",
  "fonts": {
    "title": "Montserrat",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 11.5,
  "camera": 53,
  "lamp": {
    "display": "dome",
    "color": "#FFD5B27A",
    "bezel": "#FF171411"
  },
  "selector": {
    "display": "pill",
    "color": "#FFD5B27A",
    "background": "#FF15120F"
  },
  "readout": {
    "display": "lcd",
    "color": "#FFF0DCB7",
    "background": "#FF232017"
  },
  "digits": {
    "color": "#FFD5B27A",
    "window": "flat",
    "background": "#FF15120F",
    "ghost": 9,
    "glow": 24,
    "slant": 1,
    "thickness": 12
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFD5B27A",
    "whiteKeys": "#FFE5E1D6",
    "blackKeys": "#FF171615"
  },
  "meter": {
    "display": "edge",
    "colorLow": "#FF58A18A",
    "colorMid": "#FFE8B965",
    "colorHigh": "#FFE16B4E",
    "background": "#FFECE4D2",
    "segments": 0,
    "peakHold": true,
    "scale": true,
    "glow": 0.05
  }
})THEME" },
            { "Rosewood Console", R"THEME({
  "format": 1,
  "name": "Rosewood Console",
  "fonts": {
    "title": "Playfair Display",
    "captions": "Roboto",
    "values": "VT323"
  },
  "titleStyle": "shadow",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 13.0,
  "camera": 65,
  "lamp": {
    "display": "jewel",
    "color": "#FFFFB878",
    "bezel": "#FF381817"
  },
  "selector": {
    "display": "lamps",
    "color": "#FFFFB878",
    "background": "#FF270E0C"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFBE3CC",
    "background": "#FF29110E"
  },
  "digits": {
    "color": "#FFFFB878",
    "window": "3d",
    "background": "#FF200A09",
    "ghost": 20,
    "glow": 45,
    "slant": 3,
    "thickness": 17
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFFFB878",
    "whiteKeys": "#FFF6E6D6",
    "blackKeys": "#FF261719"
  },
  "meter": {
    "display": "porthole",
    "colorLow": "#FF659B7C",
    "colorMid": "#FFE3B45B",
    "colorHigh": "#FFDF5F4A",
    "background": "#FFF4E5C9",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.1
  }
})THEME" },
            { "Natural Oak", R"THEME({
  "format": 1,
  "name": "Natural Oak",
  "fonts": {
    "title": "Cinzel",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 50,
  "lamp": {
    "display": "dome",
    "color": "#FF174D45",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "keys",
    "color": "#FF174D45",
    "background": "#FFF0F1EB"
  },
  "readout": {
    "display": "readout",
    "color": "#FF17191C",
    "background": "#FFD8CBAD"
  },
  "digits": {
    "color": "#FF174D45",
    "window": "3d",
    "background": "#FFF0ECE0",
    "ghost": 19,
    "glow": 25,
    "slant": 2,
    "thickness": 16
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF174D45",
    "whiteKeys": "#FFECECE7",
    "blackKeys": "#FF28231E"
  },
  "meter": {
    "display": "vu",
    "colorLow": "#FF53946F",
    "colorMid": "#FFDFC060",
    "colorHigh": "#FFCF3E37",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.1
  }
})THEME" },
            { "Burl Luxe", R"THEME({
  "format": 1,
  "name": "Burl Luxe",
  "fonts": {
    "title": "Playfair Display",
    "captions": "Barlow Condensed",
    "values": "Roboto Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 13,
  "camera": 57,
  "lamp": {
    "display": "jewel",
    "color": "#FFFFCC75",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "lamps",
    "color": "#FFFFCC75",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "readout",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFFFCC75",
    "window": "3d",
    "background": "#FF0C1216",
    "ghost": 24,
    "glow": 33,
    "slant": 4,
    "thickness": 18
  },
  "keyboard": {
    "display": "classic",
    "color": "#FFFFCC75",
    "whiteKeys": "#FFF5F0E4",
    "blackKeys": "#FF212833"
  },
  "meter": {
    "display": "porthole",
    "colorLow": "#FF39BCA3",
    "colorMid": "#FFFFC963",
    "colorHigh": "#FFE04B40",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.12
  }
})THEME" },
            { "Flame Maple", R"THEME({
  "format": 1,
  "name": "Flame Maple",
  "fonts": {
    "title": "Righteous",
    "captions": "Roboto",
    "values": "Roboto Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": true,
  "captionSize": 12.5,
  "camera": 53,
  "lamp": {
    "display": "dome",
    "color": "#FF273A58",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "keys",
    "color": "#FF273A58",
    "background": "#FFE2E7E8"
  },
  "readout": {
    "display": "lcd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FF273A58",
    "window": "3d",
    "background": "#FFE8E9E7",
    "ghost": 29,
    "glow": 41,
    "slant": 6,
    "thickness": 20
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF273A58",
    "whiteKeys": "#FFF1F4EF",
    "blackKeys": "#FF1D2024"
  },
  "meter": {
    "display": "edge",
    "colorLow": "#FF44CDBA",
    "colorMid": "#FFF3AB40",
    "colorHigh": "#FFFF6046",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.14
  }
})THEME" },
            { "Black Marble", R"THEME({
  "format": 1,
  "name": "Black Marble",
  "fonts": {
    "title": "Cinzel",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "embossed",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 11.5,
  "camera": 55,
  "lamp": {
    "display": "jewel",
    "color": "#FFE6BF70",
    "bezel": "#FF15171A"
  },
  "selector": {
    "display": "pill",
    "color": "#FFE6BF70",
    "background": "#FF12161A"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFF2E7C6",
    "background": "#FF15191B"
  },
  "digits": {
    "color": "#FFE6BF70",
    "window": "flat",
    "background": "#FF0D1012",
    "ghost": 6,
    "glow": 25,
    "slant": 0,
    "thickness": 13
  },
  "keyboard": {
    "display": "graphite",
    "color": "#FFE6BF70",
    "whiteKeys": "#FF30353B",
    "blackKeys": "#FFCFD0C7"
  },
  "meter": {
    "display": "porthole",
    "colorLow": "#FF4AA78B",
    "colorMid": "#FFE2B65B",
    "colorHigh": "#FFDB5A48",
    "background": "#FFF0E9D4",
    "segments": 0,
    "peakHold": true,
    "scale": true,
    "glow": 0.08
  }
})THEME" },
            { "White Marble", R"THEME({
  "format": 1,
  "name": "White Marble",
  "fonts": {
    "title": "Cinzel",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "engraved",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 47,
  "lamp": {
    "display": "jewel",
    "color": "#FF72532B",
    "bezel": "#FF28313B"
  },
  "selector": {
    "display": "keys",
    "color": "#FF72532B",
    "background": "#FFF0F1EB"
  },
  "readout": {
    "display": "readout",
    "color": "#FF17191C",
    "background": "#FFE3E6E5"
  },
  "digits": {
    "color": "#FF72532B",
    "window": "3d",
    "background": "#FFF0ECE0",
    "ghost": 9,
    "glow": 57,
    "slant": 10,
    "thickness": 12
  },
  "keyboard": {
    "display": "classic",
    "color": "#FF72532B",
    "whiteKeys": "#FFE3EDF2",
    "blackKeys": "#FF28231E"
  },
  "meter": {
    "display": "porthole",
    "colorLow": "#FF53946F",
    "colorMid": "#FFFFC963",
    "colorHigh": "#FFE04B40",
    "background": "#FFF0E2C7",
    "segments": 0,
    "peakHold": false,
    "scale": true,
    "glow": 0.12
  }
})THEME" },
            { "Slate Stone", R"THEME({
  "format": 1,
  "name": "Slate Stone",
  "fonts": {
    "title": "Michroma",
    "captions": "Rajdhani",
    "values": "Share Tech Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12,
  "camera": 55,
  "lamp": {
    "display": "dot",
    "color": "#FFBDDE83",
    "bezel": "#FF1D2124"
  },
  "selector": {
    "display": "cells",
    "color": "#FFBDDE83",
    "background": "#FF0C121A"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFFFFFFF",
    "background": "#FF111B22"
  },
  "digits": {
    "color": "#FFBDDE83",
    "window": "flat",
    "background": "#FF0C1216",
    "ghost": 14,
    "glow": 65,
    "slant": 12,
    "thickness": 14
  },
  "keyboard": {
    "display": "graphite",
    "color": "#FFBDDE83",
    "whiteKeys": "#FF404850",
    "blackKeys": "#FFA7B3B6"
  },
  "meter": {
    "display": "arc",
    "colorLow": "#FF39BCA3",
    "colorMid": "#FFF3AB40",
    "colorHigh": "#FFFF6046",
    "background": "#FF0C1519",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.45
  }
})THEME" },
            { "Carbon Fibre", R"THEME({
  "format": 1,
  "name": "Carbon Fibre",
  "fonts": {
    "title": "Michroma",
    "captions": "Rajdhani",
    "values": "Share Tech Mono"
  },
  "titleStyle": "plain",
  "titleBold": true,
  "captionBold": true,
  "captionSize": 11.5,
  "camera": 40,
  "lamp": {
    "display": "square",
    "color": "#FF00DDF2",
    "bezel": "#FF111B21"
  },
  "selector": {
    "display": "cells",
    "color": "#FF00DDF2",
    "background": "#FF09151A"
  },
  "readout": {
    "display": "vfd",
    "color": "#FF8EF7FF",
    "background": "#FF09141B"
  },
  "digits": {
    "color": "#FF00DDF2",
    "window": "flat",
    "background": "#FF061116",
    "ghost": 2,
    "glow": 85,
    "slant": 7,
    "thickness": 11
  },
  "keyboard": {
    "display": "modern",
    "color": "#FF00DDF2",
    "whiteKeys": "#FF303B43",
    "blackKeys": "#FF0B1117"
  },
  "meter": {
    "display": "segments",
    "colorLow": "#FF30E8AA",
    "colorMid": "#FFEDCE4C",
    "colorHigh": "#FFFF4A44",
    "background": "#FF051219",
    "segments": 32,
    "peakHold": true,
    "scale": true,
    "glow": 0.88
  }
})THEME" },
            { "Dark Screen Glass", R"THEME({
  "format": 1,
  "name": "Dark Screen Glass",
  "fonts": {
    "title": "Orbitron",
    "captions": "Exo 2",
    "values": "Share Tech Mono"
  },
  "titleStyle": "glow",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.5,
  "camera": 43,
  "lamp": {
    "display": "dot",
    "color": "#FF65D9EE",
    "bezel": "#FF0B1922"
  },
  "selector": {
    "display": "vfd",
    "color": "#FF65D9EE",
    "background": "#FF06141D"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFB5F4FF",
    "background": "#FF06131E"
  },
  "digits": {
    "color": "#FF65D9EE",
    "window": "3d",
    "background": "#FF06121D",
    "ghost": 4,
    "glow": 88,
    "slant": 5,
    "thickness": 10
  },
  "keyboard": {
    "display": "modern",
    "color": "#FF65D9EE",
    "whiteKeys": "#FF273E4E",
    "blackKeys": "#FF0A1925"
  },
  "meter": {
    "display": "neon",
    "colorLow": "#FF39DDBF",
    "colorMid": "#FFFFD66F",
    "colorHigh": "#FFFF536A",
    "background": "#FF07151E",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.92
  }
})THEME" },
            { "Frosted Smoke", R"THEME({
  "format": 1,
  "name": "Frosted Smoke",
  "fonts": {
    "title": "Exo 2",
    "captions": "Inter",
    "values": "Roboto Mono"
  },
  "titleStyle": "glow",
  "titleBold": true,
  "captionBold": false,
  "captionSize": 12.0,
  "camera": 39,
  "lamp": {
    "display": "square",
    "color": "#FF9CCAD0",
    "bezel": "#FF222B31"
  },
  "selector": {
    "display": "vfd",
    "color": "#FF9CCAD0",
    "background": "#FF16242B"
  },
  "readout": {
    "display": "lcd",
    "color": "#FFBFE9E8",
    "background": "#FF25333C"
  },
  "digits": {
    "color": "#FF9CCAD0",
    "window": "flat",
    "background": "#FF1A282E",
    "ghost": 11,
    "glow": 35,
    "slant": 0,
    "thickness": 15
  },
  "keyboard": {
    "display": "graphite",
    "color": "#FF9CCAD0",
    "whiteKeys": "#FF4D575E",
    "blackKeys": "#FFD1DEE0"
  },
  "meter": {
    "display": "tube",
    "colorLow": "#FF52C2B2",
    "colorMid": "#FFFFBF6E",
    "colorHigh": "#FFFD6C5D",
    "background": "#FF1A2930",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.44
  }
})THEME" },
            { "Amber Tinted Glass", R"THEME({
  "format": 1,
  "name": "Amber Tinted Glass",
  "fonts": {
    "title": "Audiowide",
    "captions": "Rajdhani",
    "values": "VT323"
  },
  "titleStyle": "glow",
  "titleBold": false,
  "captionBold": false,
  "captionSize": 13.0,
  "camera": 48,
  "lamp": {
    "display": "tube",
    "color": "#FFFFC15A",
    "bezel": "#FF2C1B0B"
  },
  "selector": {
    "display": "vfd",
    "color": "#FFFFC15A",
    "background": "#FF241708"
  },
  "readout": {
    "display": "vfd",
    "color": "#FFFFDE9C",
    "background": "#FF2A1B0C"
  },
  "digits": {
    "color": "#FFFFC15A",
    "window": "3d",
    "background": "#FF211407",
    "ghost": 23,
    "glow": 75,
    "slant": 8,
    "thickness": 21
  },
  "keyboard": {
    "display": "reverse",
    "color": "#FFFFC15A",
    "whiteKeys": "#FF2F251C",
    "blackKeys": "#FFEAD5A6"
  },
  "meter": {
    "display": "tube",
    "colorLow": "#FF65C792",
    "colorMid": "#FFF7C153",
    "colorHigh": "#FFFF6842",
    "background": "#FF241A0E",
    "segments": 0,
    "peakHold": true,
    "scale": false,
    "glow": 0.72
  }
})THEME" },
        };

        return list;
    }

    /** A look's displays and fonts, as parsed JSON (void: none). */
    inline juce::var findTheme (const juce::String& name)
    {
        for (const auto& theme : themes())
            if (name.trim().equalsIgnoreCase (theme.name))
                return juce::JSON::parse (juce::String (theme.json));

        return {};
    }

    /** A title's lettering as text effects (Photoshop's layer style on its words); void: plain. */
    inline juce::var titleEffects (const juce::String& style, juce::Colour accent)
    {
        juce::String json;

        if (style == "engraved")
            json = R"({ "bevel": { "style": "inner", "direction": "down", "size": 1, "depth": 120, "highlightOpacity": 45, "shadowOpacity": 70 },
                        "innerShadow": { "opacity": 45, "distance": 1, "size": 1 } })";
        else if (style == "embossed")
            json = R"({ "bevel": { "style": "emboss", "direction": "up", "size": 2, "depth": 100, "soften": 1, "highlightOpacity": 60, "shadowOpacity": 60 } })";
        else if (style == "glow")
            json = R"({ "dropShadow": { "blend": "screen", "color": ")" + ("#" + accent.toDisplayString (true))
                 + R"(", "opacity": 70, "distance": 0, "spread": 0, "size": 8 } })";
        else if (style == "shadow")
            json = R"({ "dropShadow": { "opacity": 60, "distance": 2, "size": 4 } })";

        return json.isEmpty() ? juce::var() : juce::JSON::parse (json);
    }

    /** A meter's display for its box: a needle meter or a ring that wouldn't fit its shape
        becomes its nearest one that does (an upright VU in a tall box...). */
    inline juce::String meterDisplayFor (const juce::String& wanted, const GuiWidget& w)
    {
        const auto height = juce::jmax (1, w.bounds.getHeight() - (w.label.isNotEmpty() ? GuiLayout::captionHeight : 0));
        const auto aspect = (float) w.bounds.getWidth() / (float) height;
        const bool tall = aspect < 0.75f, wide = aspect > 1.34f;

        if (wanted == "vu")         return tall ? "vuside" : wanted;
        if (wanted == "porthole")   return tall ? "vuside" : wide ? "vu" : wanted;
        if (wanted == "arc")        return tall ? "segments" : wide ? "bar" : wanted;
        return wanted;
    }

    /** Dresses the window's displays and words in a look's theme: lamps, selectors, readouts,
        LED digits, keyboards and meters; the titles', captions' and values' fonts and the titles'
        lettering; the panel camera. Places, sizes, parameters and words stay as they are; a
        drop-down selector stays a drop-down, a selector set and an icon stay as they are.
        Returns how many elements changed. */
    inline int applyTheme (GuiLayout& layout, const juce::String& lookName)
    {
        const auto theme = findTheme (lookName);

        if (! theme.isObject())
            return 0;

        const auto fonts = theme.getProperty ("fonts", {});
        const auto titleFont = fonts.getProperty ("title", {}).toString();
        const auto captionFont = fonts.getProperty ("captions", {}).toString();
        const auto valueFont = fonts.getProperty ("values", {}).toString();
        const bool titleBold = theme.getProperty ("titleBold", true), captionBold = theme.getProperty ("captionBold", false);
        const auto captionSize = (float) (double) theme.getProperty ("captionSize", 0.0);
        const auto effects = titleEffects (theme.getProperty ("titleStyle", "plain").toString(),
                                           colourOf (find (lookName).getProperty ("accentColor", {})));

        const auto part = [&theme] (const char* name) { return theme.getProperty (name, {}); };
        const auto lamp = part ("lamp"), selector = part ("selector"), readout = part ("readout"), digits = part ("digits"),
                   keyboard = part ("keyboard"), meter = part ("meter");

        const auto setTitle = [&] (GuiWidget& w)
        {
            w.font = titleFont;
            w.bold = titleBold;

            if (effects.isObject())
                w.settings["textEffects"] = effects.clone();
            else
                w.settings.erase ("textEffects");
        };

        const auto setCaption = [&] (GuiWidget& w)
        {
            w.font = captionFont;
            w.bold = captionBold;

            if (captionSize > 0.0f)
                w.fontSize = captionSize;
        };

        const auto copy = [] (GuiWidget& w, const juce::var& from, std::initializer_list<const char*> keys)
        {
            for (const auto* key : keys)
                if (from.hasProperty (key))
                    w.settings[key] = from.getProperty (key, {});
        };

        int changed = 0;

        for (auto& w : layout.widgets)
        {
            using Type = GuiWidget::Type;
            bool did = true;

            switch (w.type)
            {
                case Type::knob: case Type::slider: case Type::toggle: case Type::scope: case Type::envelope:
                case Type::filter: case Type::xy:
                    setCaption (w);
                    break;

                case Type::lamp:
                    setCaption (w);
                    w.colour = colourOf (lamp.getProperty ("color", {}));
                    copy (w, lamp, { "display", "bezel" });
                    break;

                case Type::selector:
                    setCaption (w);

                    if (w.settings.count ("set") == 0 && w.settingText ("display").trim().toLowerCase() != "dropdown")
                    {
                        w.colour = colourOf (selector.getProperty ("color", {}));
                        copy (w, selector, { "display", "background" });
                    }
                    break;

                case Type::keyboard:
                    w.colour = colourOf (keyboard.getProperty ("color", {}));
                    copy (w, keyboard, { "display", "whiteKeys", "blackKeys" });
                    break;

                case Type::meter:
                    setCaption (w);
                    copy (w, meter, { "colorLow", "colorMid", "colorHigh", "background", "segments", "peakHold", "scale", "glow" });
                    w.settings["display"] = meterDisplayFor (meter.getProperty ("display", "bar").toString(), w);
                    break;

                case Type::label:
                {
                    const auto display = PanelParts::labelDisplayOf (w);

                    if (display == "readout" || display == "vfd" || display == "lcd")
                    {
                        w.font = valueFont;
                        w.colour = colourOf (readout.getProperty ("color", {}));
                        copy (w, readout, { "display", "background" });
                    }
                    else if (display == "led")
                    {
                        w.colour = colourOf (digits.getProperty ("color", {}));
                        copy (w, digits, { "window", "background", "ghost", "glow", "slant", "thickness" });
                    }
                    else if (display == "icon")
                    {
                        did = false;
                    }
                    else
                    {
                        setTitle (w);
                    }
                    break;
                }

                case Type::group:
                    setTitle (w);
                    break;

                case Type::tabs:
                case Type::preset:
                    w.font = captionFont;
                    break;

                default:
                    did = false;
                    break;
            }

            if (did)
                ++changed;
        }

        layout.camera = (float) juce::jlimit (0.0, 100.0, (double) theme.getProperty ("camera", layout.camera));
        return changed;
    }

    //==========================================================================
    /** Each look's painted panel: how PanelPainter paints the window's background in it, part
        by part (its sections, title plate, screens, wells, dividers, vents, accent light). The
        parts are placed from the window's own layout (painterSettings). */
    struct PaintedPanel
    {
        const char* name;   // its material look's
        const char* json;
    };

    inline const std::vector<PaintedPanel>& paintedPanels()
    {
        static const std::vector<PaintedPanel> list {
            { "Dark Brushed Aluminium", R"PAINT({
  "format": 1,
  "name": "Dark Brushed Aluminium",
  "panel": {
    "key": 0.56,
    "vignette": 0.39,
    "rim": 5,
    "corner": 7,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 7,
    "depth": 4,
    "strength": 0.75,
    "surface": {
      "material": "gunmetal",
      "finish": "satin",
      "color": "#FF353D44",
      "color2": "#FF272E35",
      "grain": 0.28,
      "shine": 0.3,
      "scale": 1.0,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "aluminium",
      "finish": "brushed",
      "color": "#FFB2B8BD",
      "color2": "#FF858D94",
      "grain": 0.42,
      "shine": 0.45,
      "scale": 1.0,
      "pattern": "pinstripes",
      "patternBlend": "softlight",
      "patternOpacity": 0.095,
      "patternSize": 7,
      "patternAngle": 0
    },
    "textStyle": "engraved",
    "textColor": "#FF1B252D"
  },
  "screens": {
    "color": "#FF081217",
    "glow": "#FFE6A34A"
  },
  "wells": {
    "use": true,
    "minKnob": 62
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": true,
    "slots": 9,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.2
  }
})PAINT" },
            { "Black Anodized", R"PAINT({
  "format": 1,
  "name": "Black Anodized",
  "panel": {
    "key": 0.48,
    "vignette": 0.47,
    "rim": 4,
    "corner": 6,
    "screws": true
  },
  "sections": {
    "kind": "inlay",
    "radius": 6,
    "depth": 2,
    "strength": 0.58,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FF161D26",
      "color2": "#FF202B33",
      "grain": 0.13,
      "shine": 0.29,
      "scale": 0.85,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FF17262F",
      "color2": "#FF0F1B25",
      "grain": 0.12,
      "shine": 0.38,
      "scale": 0.8,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FFDAF5FA"
  },
  "screens": {
    "color": "#FF050D16",
    "glow": "#FF55C7D9"
  },
  "wells": {
    "use": false,
    "minKnob": 74
  },
  "dividers": {
    "kind": "groove",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.25
  }
})PAINT" },
            { "Aged Brass", R"PAINT({
  "format": 1,
  "name": "Aged Brass",
  "panel": {
    "key": 0.51,
    "vignette": 0.46,
    "rim": 6,
    "corner": 5,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 5,
    "depth": 5,
    "strength": 0.73,
    "surface": {
      "material": "brass",
      "finish": "satin",
      "color": "#FF473827",
      "color2": "#FF35291B",
      "grain": 0.45,
      "shine": 0.33,
      "scale": 1.1,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "brass",
      "finish": "satin",
      "color": "#FFCCB17C",
      "color2": "#FF9F8151",
      "grain": 0.4,
      "shine": 0.52,
      "scale": 1.05,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.09,
      "patternSize": 10,
      "patternAngle": 0
    },
    "textStyle": "engraved",
    "textColor": "#FF322112"
  },
  "screens": {
    "color": "#FF211B10",
    "glow": "#FFFFB05C"
  },
  "wells": {
    "use": true,
    "minKnob": 52
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": false,
    "slots": 7,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.22
  }
})PAINT" },
            { "Silver Faceplate", R"PAINT({
  "format": 1,
  "name": "Silver Faceplate",
  "panel": {
    "key": 0.64,
    "vignette": 0.3,
    "rim": 5,
    "corner": 5,
    "screws": true
  },
  "sections": {
    "kind": "raise",
    "radius": 5,
    "depth": 2,
    "strength": 0.56,
    "surface": {
      "material": "silver",
      "finish": "satin",
      "color": "#FFB4B8BD",
      "color2": "#FFA9AEB4",
      "grain": 0.18,
      "shine": 0.28,
      "scale": 1.0,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "gunmetal",
      "finish": "satin",
      "color": "#FF303A46",
      "color2": "#FF202B34",
      "grain": 0.17,
      "shine": 0.39,
      "scale": 0.85,
      "pattern": "none"
    },
    "textStyle": "engraved",
    "textColor": "#FFF2F2ED"
  },
  "screens": {
    "color": "#FF0F1D28",
    "glow": "#FF852B39"
  },
  "wells": {
    "use": true,
    "minKnob": 66
  },
  "dividers": {
    "kind": "groove",
    "width": 1
  },
  "vents": {
    "use": true,
    "slots": 8,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.15
  }
})PAINT" },
            { "Workshop Hammertone", R"PAINT({
  "format": 1,
  "name": "Workshop Hammertone",
  "panel": {
    "key": 0.52,
    "vignette": 0.42,
    "rim": 6,
    "corner": 4,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 4,
    "depth": 5,
    "strength": 0.85,
    "surface": {
      "material": "hammertone",
      "finish": "hammered",
      "color": "#FF344D5F",
      "color2": "#FF273A4A",
      "grain": 0.35,
      "shine": 0.32,
      "scale": 1.08,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "steel",
      "finish": "brushed",
      "color": "#FFC3C7C7",
      "color2": "#FF9EA6A8",
      "grain": 0.4,
      "shine": 0.4,
      "scale": 1.0,
      "pattern": "none"
    },
    "textStyle": "embossed",
    "textColor": "#FF252E36"
  },
  "screens": {
    "color": "#FF13222B",
    "glow": "#FFFFB94E"
  },
  "wells": {
    "use": true,
    "minKnob": 59
  },
  "dividers": {
    "kind": "ridge",
    "width": 2
  },
  "vents": {
    "use": true,
    "slots": 12,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.2
  }
})PAINT" },
            { "Red Anodized", R"PAINT({
  "format": 1,
  "name": "Red Anodized",
  "panel": {
    "key": 0.4,
    "vignette": 0.26,
    "rim": 5,
    "corner": 6,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 5,
    "depth": 4,
    "strength": 0.68,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FF421A26",
      "color2": "#FF29141B",
      "grain": 0.19,
      "shine": 0.43,
      "scale": 0.97,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FFBCC6CB",
      "color2": "#FF919EA6",
      "grain": 0.24,
      "shine": 0.49,
      "scale": 1.05,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FF242B32"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FF95F3EA"
  },
  "wells": {
    "use": true,
    "minKnob": 62
  },
  "dividers": {
    "kind": "groove",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 5,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.15
  }
})PAINT" },
            { "Blue Anodized", R"PAINT({
  "format": 1,
  "name": "Blue Anodized",
  "panel": {
    "key": 0.455,
    "vignette": 0.32,
    "rim": 4,
    "corner": 8,
    "screws": true
  },
  "sections": {
    "kind": "inlay",
    "radius": 6,
    "depth": 2,
    "strength": 0.57,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FF183452",
      "color2": "#FF11253C",
      "grain": 0.16,
      "shine": 0.4,
      "scale": 0.92,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "silver",
      "finish": "brushed",
      "color": "#FFD0DADF",
      "color2": "#FFABB8C1",
      "grain": 0.2,
      "shine": 0.45,
      "scale": 1.0,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FF243342"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFFFCB74"
  },
  "wells": {
    "use": false,
    "minKnob": 64
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": false
  },
  "accentLight": {
    "use": true,
    "strength": 0.23
  }
})PAINT" },
            { "Gold Anodized", R"PAINT({
  "format": 1,
  "name": "Gold Anodized",
  "panel": {
    "key": 0.51,
    "vignette": 0.38,
    "rim": 5,
    "corner": 6,
    "screws": true
  },
  "sections": {
    "kind": "raise",
    "radius": 7,
    "depth": 3,
    "strength": 0.56,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FFD7BD8F",
      "color2": "#FFB99B66",
      "grain": 0.21,
      "shine": 0.39,
      "scale": 0.92,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FF554634",
      "color2": "#FF342A20",
      "grain": 0.27,
      "shine": 0.44,
      "scale": 1.0,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.08499999999999999,
      "patternSize": 9,
      "patternAngle": 0
    },
    "textStyle": "engraved",
    "textColor": "#FFFFEBCD"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF263B68"
  },
  "wells": {
    "use": true,
    "minKnob": 60
  },
  "dividers": {
    "kind": "groove",
    "width": 3
  },
  "vents": {
    "use": false,
    "slots": 7,
    "across": true
  },
  "accentLight": {
    "use": false,
    "strength": 0.31
  }
})PAINT" },
            { "Mirror Chrome", R"PAINT({
  "format": 1,
  "name": "Mirror Chrome",
  "panel": {
    "key": 0.35,
    "vignette": 0.44,
    "rim": 5,
    "corner": 8,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 8,
    "depth": 3,
    "strength": 0.48,
    "surface": {
      "material": "chrome",
      "finish": "polished",
      "color": "#FFA9B4BE",
      "color2": "#FF96A2AD",
      "grain": 0.04,
      "shine": 0.5,
      "scale": 0.87,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "steel",
      "finish": "satin",
      "color": "#FF29343D",
      "color2": "#FF18232B",
      "grain": 0.05,
      "shine": 0.8,
      "scale": 0.95,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.055,
      "patternSize": 10,
      "patternAngle": 0
    },
    "textStyle": "engraved",
    "textColor": "#FFF4F8F9"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF602642"
  },
  "wells": {
    "use": false,
    "minKnob": 64
  },
  "dividers": {
    "kind": "groove",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 8,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.39
  }
})PAINT" },
            { "Brushed Steel", R"PAINT({
  "format": 1,
  "name": "Brushed Steel",
  "panel": {
    "key": 0.62,
    "vignette": 0.5,
    "rim": 6,
    "corner": 4,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 9,
    "depth": 5,
    "strength": 0.75,
    "surface": {
      "material": "steel",
      "finish": "brushed",
      "color": "#FFA3ADB2",
      "color2": "#FF89949A",
      "grain": 0.33,
      "shine": 0.37,
      "scale": 1.01,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FF2A353B",
      "color2": "#FF1A252B",
      "grain": 0.42,
      "shine": 0.41,
      "scale": 1.1,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.07,
      "patternSize": 11,
      "patternAngle": 0
    },
    "textStyle": "engraved",
    "textColor": "#FFF2F5F5"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF194D72"
  },
  "wells": {
    "use": true,
    "minKnob": 58
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": true,
    "slots": 9,
    "across": true
  },
  "accentLight": {
    "use": false,
    "strength": 0.15
  }
})PAINT" },
            { "Gunmetal Grey", R"PAINT({
  "format": 1,
  "name": "Gunmetal Grey",
  "panel": {
    "key": 0.675,
    "vignette": 0.26,
    "rim": 5,
    "corner": 5,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 10,
    "depth": 4,
    "strength": 0.7,
    "surface": {
      "material": "gunmetal",
      "finish": "satin",
      "color": "#FF283341",
      "color2": "#FF202A37",
      "grain": 0.22,
      "shine": 0.42,
      "scale": 0.86,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "silver",
      "finish": "brushed",
      "color": "#FFC5CDD1",
      "color2": "#FF9EAAB2",
      "grain": 0.28,
      "shine": 0.47,
      "scale": 0.94,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FF263039"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFFFBD6F"
  },
  "wells": {
    "use": true,
    "minKnob": 60
  },
  "dividers": {
    "kind": "groove",
    "width": 3
  },
  "vents": {
    "use": true,
    "slots": 10,
    "across": false
  },
  "accentLight": {
    "use": true,
    "strength": 0.23
  }
})PAINT" },
            { "Liquid Titanium", R"PAINT({
  "format": 1,
  "name": "Liquid Titanium",
  "panel": {
    "key": 0.4,
    "vignette": 0.32,
    "rim": 3,
    "corner": 12,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 11,
    "depth": 2,
    "strength": 0.49,
    "surface": {
      "material": "titanium",
      "finish": "liquid",
      "color": "#FF353A4C",
      "color2": "#FF2C3344",
      "grain": 0.06,
      "shine": 0.65,
      "scale": 1.16,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "titanium",
      "finish": "polished",
      "color": "#FFB4AFBF",
      "color2": "#FF878DA2",
      "grain": 0.08,
      "shine": 0.72,
      "scale": 1.26,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FF202838"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFCDAFFF"
  },
  "wells": {
    "use": false,
    "minKnob": 64
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 11,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.31
  }
})PAINT" },
            { "Copper Patina", R"PAINT({
  "format": 1,
  "name": "Copper Patina",
  "panel": {
    "key": 0.455,
    "vignette": 0.38,
    "rim": 5,
    "corner": 6,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 12,
    "depth": 3,
    "strength": 0.61,
    "surface": {
      "material": "copper",
      "finish": "satin",
      "color": "#FF683F2D",
      "color2": "#FF523425",
      "grain": 0.28,
      "shine": 0.45,
      "scale": 0.96,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "copper",
      "finish": "brushed",
      "color": "#FFBC896D",
      "color2": "#FF87644D",
      "grain": 0.36,
      "shine": 0.5,
      "scale": 1.04,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.07,
      "patternSize": 9,
      "patternAngle": 0
    },
    "textStyle": "engraved",
    "textColor": "#FF2E2119"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFB5EBE0"
  },
  "wells": {
    "use": true,
    "minKnob": 60
  },
  "dividers": {
    "kind": "ridge",
    "width": 2
  },
  "vents": {
    "use": false,
    "slots": 12,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.39
  }
})PAINT" },
            { "Cast Iron", R"PAINT({
  "format": 1,
  "name": "Cast Iron",
  "panel": {
    "key": 0.51,
    "vignette": 0.44,
    "rim": 6,
    "corner": 3,
    "screws": true
  },
  "sections": {
    "kind": "raise",
    "radius": 5,
    "depth": 5,
    "strength": 0.77,
    "surface": {
      "material": "iron",
      "finish": "natural",
      "color": "#FF292F31",
      "color2": "#FF212629",
      "grain": 0.62,
      "shine": 0.13,
      "scale": 1.21,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "aluminium",
      "finish": "brushed",
      "color": "#FFC0C3BE",
      "color2": "#FF939B98",
      "grain": 0.79,
      "shine": 0.14,
      "scale": 1.31,
      "pattern": "none"
    },
    "textStyle": "embossed",
    "textColor": "#FF282E2C"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFFFBE65"
  },
  "wells": {
    "use": true,
    "minKnob": 64
  },
  "dividers": {
    "kind": "groove",
    "width": 3
  },
  "vents": {
    "use": true,
    "slots": 13,
    "across": true
  },
  "accentLight": {
    "use": false,
    "strength": 0.15
  }
})PAINT" },
            { "Matte Black Polymer", R"PAINT({
  "format": 1,
  "name": "Matte Black Polymer",
  "panel": {
    "key": 0.46,
    "vignette": 0.44,
    "rim": 2,
    "corner": 10,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 10,
    "depth": 2,
    "strength": 0.4,
    "surface": {
      "material": "matteplastic",
      "finish": "matte",
      "color": "#FF202629",
      "color2": "#FF1A2022",
      "grain": 0.16,
      "shine": 0.08,
      "scale": 0.85,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "matteplastic",
      "finish": "satin",
      "color": "#FF18292D",
      "color2": "#FF101B20",
      "grain": 0.09,
      "shine": 0.17,
      "scale": 0.85,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FFBCEDE2"
  },
  "screens": {
    "color": "#FF071416",
    "glow": "#FF79B7A6"
  },
  "wells": {
    "use": false,
    "minKnob": 78
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 5,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.21
  }
})PAINT" },
            { "Vintage Bakelite", R"PAINT({
  "format": 1,
  "name": "Vintage Bakelite",
  "panel": {
    "key": 0.55,
    "vignette": 0.32,
    "rim": 3,
    "corner": 11,
    "screws": false
  },
  "sections": {
    "kind": "recess",
    "radius": 9,
    "depth": 4,
    "strength": 0.8,
    "surface": {
      "material": "bakelite",
      "finish": "satin",
      "color": "#FF48281D",
      "color2": "#FF30190F",
      "grain": 0.16,
      "shine": 0.22,
      "scale": 1.0,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "brass",
      "finish": "satin",
      "color": "#FFCBA56C",
      "color2": "#FF9D774A",
      "grain": 0.29,
      "shine": 0.51,
      "scale": 0.9,
      "pattern": "none"
    },
    "textStyle": "embossed",
    "textColor": "#FF382313"
  },
  "screens": {
    "color": "#FF1E120C",
    "glow": "#FFFFB65B"
  },
  "wells": {
    "use": true,
    "minKnob": 60
  },
  "dividers": {
    "kind": "ridge",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 7,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.18
  }
})PAINT" },
            { "Soft Touch Rubber", R"PAINT({
  "format": 1,
  "name": "Soft Touch Rubber",
  "panel": {
    "key": 0.44,
    "vignette": 0.49,
    "rim": 2,
    "corner": 13,
    "screws": false
  },
  "sections": {
    "kind": "recess",
    "radius": 12,
    "depth": 2,
    "strength": 0.55,
    "surface": {
      "material": "rubber",
      "finish": "matte",
      "color": "#FF252321",
      "color2": "#FF1C1B19",
      "grain": 0.24,
      "shine": 0.06,
      "scale": 0.8,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "anodized",
      "finish": "matte",
      "color": "#FF1A262D",
      "color2": "#FF121B21",
      "grain": 0.16,
      "shine": 0.18,
      "scale": 0.9,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FFE8F0EB"
  },
  "screens": {
    "color": "#FF0D161B",
    "glow": "#FF8BC4D0"
  },
  "wells": {
    "use": false,
    "minKnob": 72
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 4,
    "across": true
  },
  "accentLight": {
    "use": false,
    "strength": 0.12
  }
})PAINT" },
            { "Cream Console", R"PAINT({
  "format": 1,
  "name": "Cream Console",
  "panel": {
    "key": 0.64,
    "vignette": 0.26,
    "rim": 5,
    "corner": 6,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 7,
    "depth": 3,
    "strength": 0.64,
    "surface": {
      "material": "powder",
      "finish": "satin",
      "color": "#FFD8C9AE",
      "color2": "#FFCEC0A7",
      "grain": 0.15,
      "shine": 0.22,
      "scale": 0.95,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "silver",
      "finish": "brushed",
      "color": "#FFE6E3D7",
      "color2": "#FFC7C7C1",
      "grain": 0.28,
      "shine": 0.37,
      "scale": 0.93,
      "pattern": "none"
    },
    "textStyle": "engraved",
    "textColor": "#FF443325"
  },
  "screens": {
    "color": "#FF343026",
    "glow": "#FFCF7E35"
  },
  "wells": {
    "use": true,
    "minKnob": 56
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": true,
    "slots": 8,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.18
  }
})PAINT" },
            { "Burnt Orange Seventies", R"PAINT({
  "format": 1,
  "name": "Burnt Orange Seventies",
  "panel": {
    "key": 0.59,
    "vignette": 0.33,
    "rim": 4,
    "corner": 12,
    "screws": true
  },
  "sections": {
    "kind": "raise",
    "radius": 10,
    "depth": 3,
    "strength": 0.52,
    "surface": {
      "material": "powder",
      "finish": "satin",
      "color": "#FF77310F",
      "color2": "#FF6D280B",
      "grain": 0.2,
      "shine": 0.29,
      "scale": 1.0,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "powder",
      "finish": "satin",
      "color": "#FFECD7AF",
      "color2": "#FFD1AE7E",
      "grain": 0.17,
      "shine": 0.3,
      "scale": 1.05,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FF4E2712"
  },
  "screens": {
    "color": "#FF301609",
    "glow": "#FFFFDE79"
  },
  "wells": {
    "use": true,
    "minKnob": 60
  },
  "dividers": {
    "kind": "ridge",
    "width": 2
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.2
  }
})PAINT" },
            { "White Studio", R"PAINT({
  "format": 1,
  "name": "White Studio",
  "panel": {
    "key": 0.35,
    "vignette": 0.5,
    "rim": 2,
    "corner": 12,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 6,
    "depth": 2,
    "strength": 0.48,
    "surface": {
      "material": "powder",
      "finish": "matte",
      "color": "#FFDDE4E8",
      "color2": "#FFCFD8DD",
      "grain": 0.1,
      "shine": 0.08,
      "scale": 0.78,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "powder",
      "finish": "matte",
      "color": "#FFD3DEE3",
      "color2": "#FFB9C7CF",
      "grain": 0.13,
      "shine": 0.08,
      "scale": 0.85,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FF25333E"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF125F82"
  },
  "wells": {
    "use": false,
    "minKnob": 70
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 5,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.23
  }
})PAINT" },
            { "Mint Retro", R"PAINT({
  "format": 1,
  "name": "Mint Retro",
  "panel": {
    "key": 0.35,
    "vignette": 0.26,
    "rim": 4,
    "corner": 12,
    "screws": false
  },
  "sections": {
    "kind": "raise",
    "radius": 7,
    "depth": 3,
    "strength": 0.5,
    "surface": {
      "material": "powder",
      "finish": "satin",
      "color": "#FFADCFBF",
      "color2": "#FF97BFAE",
      "grain": 0.17,
      "shine": 0.22,
      "scale": 0.88,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "powder",
      "finish": "satin",
      "color": "#FFE3EEE2",
      "color2": "#FFB9D3C1",
      "grain": 0.22,
      "shine": 0.35,
      "scale": 0.96,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FF283B32"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF7C3C2C"
  },
  "wells": {
    "use": false,
    "minKnob": 70
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": true
  },
  "accentLight": {
    "use": false,
    "strength": 0.31
  }
})PAINT" },
            { "Mustard Seventies", R"PAINT({
  "format": 1,
  "name": "Mustard Seventies",
  "panel": {
    "key": 0.675,
    "vignette": 0.32,
    "rim": 5,
    "corner": 7,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 8,
    "depth": 4,
    "strength": 0.7,
    "surface": {
      "material": "powder",
      "finish": "satin",
      "color": "#FF64441A",
      "color2": "#FF4D3416",
      "grain": 0.27,
      "shine": 0.3,
      "scale": 1.04,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "bakelite",
      "finish": "polished",
      "color": "#FF402A16",
      "color2": "#FF25180E",
      "grain": 0.34,
      "shine": 0.34,
      "scale": 1.13,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.08499999999999999,
      "patternSize": 8,
      "patternAngle": 0
    },
    "textStyle": "printed",
    "textColor": "#FFFFEDD2"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFFFE6B8"
  },
  "wells": {
    "use": true,
    "minKnob": 62
  },
  "dividers": {
    "kind": "ridge",
    "width": 3
  },
  "vents": {
    "use": true,
    "slots": 7,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.39
  }
})PAINT" },
            { "Red Lacquer", R"PAINT({
  "format": 1,
  "name": "Red Lacquer",
  "panel": {
    "key": 0.4,
    "vignette": 0.38,
    "rim": 4,
    "corner": 10,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 9,
    "depth": 2,
    "strength": 0.61,
    "surface": {}
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "chrome",
      "finish": "polished",
      "color": "#FFCDD3D5",
      "color2": "#FFA0ADB2",
      "grain": 0.06,
      "shine": 0.73,
      "scale": 1.03,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.055,
      "patternSize": 9,
      "patternAngle": 0
    },
    "textStyle": "printed",
    "textColor": "#FF293039"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFFFC879"
  },
  "wells": {
    "use": true,
    "minKnob": 60
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 8,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.15
  }
})PAINT" },
            { "Ivory Ceramic", R"PAINT({
  "format": 1,
  "name": "Ivory Ceramic",
  "panel": {
    "key": 0.35,
    "vignette": 0.44,
    "rim": 3,
    "corner": 12,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 10,
    "depth": 2,
    "strength": 0.46,
    "surface": {
      "material": "ceramic",
      "finish": "polished",
      "color": "#FFE6D8BF",
      "color2": "#FFD9C9AE",
      "grain": 0.06,
      "shine": 0.3,
      "scale": 0.81,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "ceramic",
      "finish": "polished",
      "color": "#FFF1EBE1",
      "color2": "#FFDED2BE",
      "grain": 0.08,
      "shine": 0.62,
      "scale": 0.88,
      "pattern": "none"
    },
    "textStyle": "engraved",
    "textColor": "#FF443526"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF3F5770"
  },
  "wells": {
    "use": false,
    "minKnob": 70
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": false,
    "slots": 9,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.23
  }
})PAINT" },
            { "Black Tolex", R"PAINT({
  "format": 1,
  "name": "Black Tolex",
  "panel": {
    "key": 0.44,
    "vignette": 0.49,
    "rim": 5,
    "corner": 5,
    "screws": true
  },
  "sections": {
    "kind": "inlay",
    "radius": 5,
    "depth": 2,
    "strength": 0.54,
    "surface": {
      "material": "tolex",
      "finish": "natural",
      "color": "#FF202125",
      "color2": "#FF19191C",
      "grain": 0.4,
      "shine": 0.1,
      "scale": 1.1,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "aluminium",
      "finish": "brushed",
      "color": "#FFC4C6C7",
      "color2": "#FF979A9D",
      "grain": 0.37,
      "shine": 0.43,
      "scale": 0.95,
      "pattern": "none"
    },
    "textStyle": "embossed",
    "textColor": "#FF282B30"
  },
  "screens": {
    "color": "#FF101014",
    "glow": "#FFFF5959"
  },
  "wells": {
    "use": true,
    "minKnob": 64
  },
  "dividers": {
    "kind": "ridge",
    "width": 2
  },
  "vents": {
    "use": true,
    "slots": 7,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.14
  }
})PAINT" },
            { "Oxblood Leather", R"PAINT({
  "format": 1,
  "name": "Oxblood Leather",
  "panel": {
    "key": 0.53,
    "vignette": 0.42,
    "rim": 2,
    "corner": 10,
    "screws": false
  },
  "sections": {
    "kind": "recess",
    "radius": 10,
    "depth": 3,
    "strength": 0.57,
    "surface": {
      "material": "leather",
      "finish": "satin",
      "color": "#FF431923",
      "color2": "#FF35131C",
      "grain": 0.34,
      "shine": 0.24,
      "scale": 1.0,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "brass",
      "finish": "satin",
      "color": "#FFBFA679",
      "color2": "#FF96764E",
      "grain": 0.25,
      "shine": 0.51,
      "scale": 1.0,
      "pattern": "none"
    },
    "textStyle": "embossed",
    "textColor": "#FF2D1F17"
  },
  "screens": {
    "color": "#FF211014",
    "glow": "#FFE7B96E"
  },
  "wells": {
    "use": true,
    "minKnob": 62
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.18
  }
})PAINT" },
            { "Grey Felt", R"PAINT({
  "format": 1,
  "name": "Grey Felt",
  "panel": {
    "key": 0.675,
    "vignette": 0.38,
    "rim": 1,
    "corner": 12,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 6,
    "depth": 2,
    "strength": 0.45,
    "surface": {
      "material": "felt",
      "finish": "natural",
      "color": "#FF2F363B",
      "color2": "#FF252B30",
      "grain": 0.71,
      "shine": 0.02,
      "scale": 1.05,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "aluminium",
      "finish": "satin",
      "color": "#FF293237",
      "color2": "#FF1C262A",
      "grain": 0.91,
      "shine": 0.03,
      "scale": 1.14,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FFEFF5F6"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFB5D5CF"
  },
  "wells": {
    "use": false,
    "minKnob": 70
  },
  "dividers": {
    "kind": "none",
    "width": 3
  },
  "vents": {
    "use": false,
    "slots": 13,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.23
  }
})PAINT" },
            { "Walnut Cheek", R"PAINT({
  "format": 1,
  "name": "Walnut Cheek",
  "panel": {
    "key": 0.56,
    "vignette": 0.38,
    "rim": 3,
    "corner": 8,
    "screws": false
  },
  "sections": {
    "kind": "recess",
    "radius": 7,
    "depth": 4,
    "strength": 0.58,
    "surface": {
      "material": "walnut",
      "finish": "satin",
      "color": "#FF492E1F",
      "color2": "#FF352014",
      "grain": 0.44,
      "shine": 0.21,
      "scale": 1.1,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "brass",
      "finish": "brushed",
      "color": "#FFD2B686",
      "color2": "#FFA78A5F",
      "grain": 0.32,
      "shine": 0.45,
      "scale": 1.0,
      "pattern": "none"
    },
    "textStyle": "engraved",
    "textColor": "#FF352319"
  },
  "screens": {
    "color": "#FF160F0C",
    "glow": "#FFE5A45F"
  },
  "wells": {
    "use": true,
    "minKnob": 56
  },
  "dividers": {
    "kind": "groove",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.18
  }
})PAINT" },
            { "Ebony Studio", R"PAINT({
  "format": 1,
  "name": "Ebony Studio",
  "panel": {
    "key": 0.52,
    "vignette": 0.4,
    "rim": 3,
    "corner": 7,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 7,
    "depth": 2,
    "strength": 0.49,
    "surface": {
      "material": "ebony",
      "finish": "satin",
      "color": "#FF28221E",
      "color2": "#FF1D1A17",
      "grain": 0.37,
      "shine": 0.23,
      "scale": 0.95,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "aluminium",
      "finish": "brushed",
      "color": "#FFC9CDD0",
      "color2": "#FF999FA4",
      "grain": 0.22,
      "shine": 0.48,
      "scale": 0.93,
      "pattern": "none"
    },
    "textStyle": "engraved",
    "textColor": "#FF1A272C"
  },
  "screens": {
    "color": "#FF0E1416",
    "glow": "#FFD5B27A"
  },
  "wells": {
    "use": false,
    "minKnob": 70
  },
  "dividers": {
    "kind": "groove",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.17
  }
})PAINT" },
            { "Rosewood Console", R"PAINT({
  "format": 1,
  "name": "Rosewood Console",
  "panel": {
    "key": 0.51,
    "vignette": 0.4,
    "rim": 4,
    "corner": 6,
    "screws": true
  },
  "sections": {
    "kind": "recess",
    "radius": 5,
    "depth": 4,
    "strength": 0.69,
    "surface": {
      "material": "rosewood",
      "finish": "satin",
      "color": "#FF301512",
      "color2": "#FF270F0D",
      "grain": 0.48,
      "shine": 0.32,
      "scale": 1.18,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": true,
    "surface": {
      "material": "brass",
      "finish": "satin",
      "color": "#FFD0AD81",
      "color2": "#FFAD8159",
      "grain": 0.24,
      "shine": 0.51,
      "scale": 1.0,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FF3D2216"
  },
  "screens": {
    "color": "#FF190D0C",
    "glow": "#FFFFB878"
  },
  "wells": {
    "use": true,
    "minKnob": 55
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": true,
    "slots": 6,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.17
  }
})PAINT" },
            { "Natural Oak", R"PAINT({
  "format": 1,
  "name": "Natural Oak",
  "panel": {
    "key": 0.51,
    "vignette": 0.5,
    "rim": 3,
    "corner": 7,
    "screws": false
  },
  "sections": {
    "kind": "recess",
    "radius": 11,
    "depth": 3,
    "strength": 0.48,
    "surface": {
      "material": "oak",
      "finish": "satin",
      "color": "#FFD7B680",
      "color2": "#FFC09B64",
      "grain": 0.59,
      "shine": 0.2,
      "scale": 1.24,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "walnut",
      "finish": "satin",
      "color": "#FF4D3525",
      "color2": "#FF302117",
      "grain": 0.76,
      "shine": 0.23,
      "scale": 1.35,
      "pattern": "none"
    },
    "textStyle": "engraved",
    "textColor": "#FFFFF1DA"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF174D45"
  },
  "wells": {
    "use": true,
    "minKnob": 62
  },
  "dividers": {
    "kind": "groove",
    "width": 3
  },
  "vents": {
    "use": false,
    "slots": 10,
    "across": true
  },
  "accentLight": {
    "use": false,
    "strength": 0.31
  }
})PAINT" },
            { "Burl Luxe", R"PAINT({
  "format": 1,
  "name": "Burl Luxe",
  "panel": {
    "key": 0.565,
    "vignette": 0.26,
    "rim": 4,
    "corner": 6,
    "screws": false
  },
  "sections": {
    "kind": "recess",
    "radius": 12,
    "depth": 4,
    "strength": 0.64,
    "surface": {
      "material": "smooth",
      "finish": "satin",
      "color": "#FF38251A",
      "color2": "#FF2A1C15",
      "grain": 0.69,
      "shine": 0.59,
      "scale": 1.1,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "burl",
      "finish": "polished",
      "color": "#FFBF9366",
      "color2": "#FF885B37",
      "grain": 0.88,
      "shine": 0.66,
      "scale": 1.2,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.055,
      "patternSize": 7,
      "patternAngle": 0
    },
    "textStyle": "engraved",
    "textColor": "#FF2E1D14"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFFFCC75"
  },
  "wells": {
    "use": true,
    "minKnob": 64
  },
  "dividers": {
    "kind": "ridge",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 11,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.39
  }
})PAINT" },
            { "Flame Maple", R"PAINT({
  "format": 1,
  "name": "Flame Maple",
  "panel": {
    "key": 0.62,
    "vignette": 0.32,
    "rim": 3,
    "corner": 8,
    "screws": false
  },
  "sections": {
    "kind": "recess",
    "radius": 5,
    "depth": 3,
    "strength": 0.52,
    "surface": {
      "material": "smooth",
      "finish": "satin",
      "color": "#FFD8A24E",
      "color2": "#FFC08A35",
      "grain": 0.56,
      "shine": 0.5,
      "scale": 1.29,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "guitarwood",
      "finish": "polished",
      "color": "#FFF2D69B",
      "color2": "#FFBF9048",
      "grain": 0.72,
      "shine": 0.56,
      "scale": 1.4,
      "pattern": "noise",
      "patternBlend": "softlight",
      "patternOpacity": 0.07,
      "patternSize": 8,
      "patternAngle": 0
    },
    "textStyle": "engraved",
    "textColor": "#FF35261B"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF273A58"
  },
  "wells": {
    "use": true,
    "minKnob": 60
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": false,
    "slots": 12,
    "across": true
  },
  "accentLight": {
    "use": false,
    "strength": 0.15
  }
})PAINT" },
            { "Black Marble", R"PAINT({
  "format": 1,
  "name": "Black Marble",
  "panel": {
    "key": 0.58,
    "vignette": 0.45,
    "rim": 2,
    "corner": 4,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 3,
    "depth": 2,
    "strength": 0.5,
    "surface": {
      "material": "slate",
      "finish": "satin",
      "color": "#FF22262B",
      "color2": "#FF171B20",
      "grain": 0.1,
      "shine": 0.3,
      "scale": 0.95,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "blackmarble",
      "finish": "polished",
      "color": "#FF373B40",
      "color2": "#FF5D6263",
      "grain": 0.22,
      "shine": 0.79,
      "scale": 1.15,
      "pattern": "damascus",
      "patternBlend": "softlight",
      "patternOpacity": 0.16,
      "patternSize": 23,
      "patternAngle": 13
    },
    "textStyle": "embossed",
    "textColor": "#FFFFF1D8"
  },
  "screens": {
    "color": "#FF090F12",
    "glow": "#FFE6BF70"
  },
  "wells": {
    "use": false,
    "minKnob": 78
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 5,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.14
  }
})PAINT" },
            { "White Marble", R"PAINT({
  "format": 1,
  "name": "White Marble",
  "panel": {
    "key": 0.35,
    "vignette": 0.44,
    "rim": 2,
    "corner": 10,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 7,
    "depth": 2,
    "strength": 0.47,
    "surface": {
      "material": "ceramic",
      "finish": "satin",
      "color": "#FFD9DAD7",
      "color2": "#FFC9CCCC",
      "grain": 0.27,
      "shine": 0.3,
      "scale": 1.09,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "marble",
      "finish": "polished",
      "color": "#FFE5E5E2",
      "color2": "#FFB9BEC0",
      "grain": 0.34,
      "shine": 0.63,
      "scale": 1.18,
      "pattern": "none"
    },
    "textStyle": "engraved",
    "textColor": "#FF283138"
  },
  "screens": {
    "color": "#FF17252A",
    "glow": "#FF72532B"
  },
  "wells": {
    "use": false,
    "minKnob": 72
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 5,
    "across": true
  },
  "accentLight": {
    "use": false,
    "strength": 0.31
  }
})PAINT" },
            { "Slate Stone", R"PAINT({
  "format": 1,
  "name": "Slate Stone",
  "panel": {
    "key": 0.455,
    "vignette": 0.5,
    "rim": 3,
    "corner": 6,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 8,
    "depth": 2,
    "strength": 0.58,
    "surface": {
      "material": "slate",
      "finish": "natural",
      "color": "#FF2B323A",
      "color2": "#FF202730",
      "grain": 0.58,
      "shine": 0.11,
      "scale": 1.06,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "steel",
      "finish": "brushed",
      "color": "#FFB9C2BD",
      "color2": "#FF87948D",
      "grain": 0.74,
      "shine": 0.12,
      "scale": 1.15,
      "pattern": "none"
    },
    "textStyle": "engraved",
    "textColor": "#FF29332D"
  },
  "screens": {
    "color": "#FF0C151C",
    "glow": "#FFBDDE83"
  },
  "wells": {
    "use": false,
    "minKnob": 68
  },
  "dividers": {
    "kind": "groove",
    "width": 2
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": false
  },
  "accentLight": {
    "use": false,
    "strength": 0.39
  }
})PAINT" },
            { "Carbon Fibre", R"PAINT({
  "format": 1,
  "name": "Carbon Fibre",
  "panel": {
    "key": 0.48,
    "vignette": 0.51,
    "rim": 2,
    "corner": 5,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 5,
    "depth": 2,
    "strength": 0.5,
    "surface": {
      "material": "carbon",
      "finish": "satin",
      "color": "#FF161C20",
      "color2": "#FF0C1318",
      "grain": 0.22,
      "shine": 0.2,
      "scale": 1.0,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "anodized",
      "finish": "satin",
      "color": "#FF17272E",
      "color2": "#FF0C171D",
      "grain": 0.12,
      "shine": 0.35,
      "scale": 0.9,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FFB5F8FF"
  },
  "screens": {
    "color": "#FF06141C",
    "glow": "#FF00DDF2"
  },
  "wells": {
    "use": false,
    "minKnob": 72
  },
  "dividers": {
    "kind": "groove",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 9,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.32
  }
})PAINT" },
            { "Dark Screen Glass", R"PAINT({
  "format": 1,
  "name": "Dark Screen Glass",
  "panel": {
    "key": 0.43,
    "vignette": 0.5,
    "rim": 2,
    "corner": 7,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 9,
    "depth": 2,
    "strength": 0.38,
    "surface": {
      "material": "glass",
      "finish": "polished",
      "color": "#FF13212A",
      "color2": "#FF0A131B",
      "grain": 0.03,
      "shine": 0.7,
      "scale": 0.88,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "glass",
      "finish": "polished",
      "color": "#FF101D26",
      "color2": "#FF0C1620",
      "grain": 0.02,
      "shine": 0.82,
      "scale": 0.88,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FFBDF5FF"
  },
  "screens": {
    "color": "#FF030E16",
    "glow": "#FF65D9EE"
  },
  "wells": {
    "use": false,
    "minKnob": 78
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 6,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.42
  }
})PAINT" },
            { "Frosted Smoke", R"PAINT({
  "format": 1,
  "name": "Frosted Smoke",
  "panel": {
    "key": 0.5,
    "vignette": 0.4,
    "rim": 1,
    "corner": 12,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 12,
    "depth": 2,
    "strength": 0.44,
    "surface": {
      "material": "frosted",
      "finish": "matte",
      "color": "#FF444D54",
      "color2": "#FF343D45",
      "grain": 0.22,
      "shine": 0.17,
      "scale": 1.1,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "frosted",
      "finish": "satin",
      "color": "#FF303940",
      "color2": "#FF252D33",
      "grain": 0.18,
      "shine": 0.37,
      "scale": 0.9,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FFF4FFFF"
  },
  "screens": {
    "color": "#FF111F29",
    "glow": "#FF9CCAD0"
  },
  "wells": {
    "use": false,
    "minKnob": 74
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 4,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.26
  }
})PAINT" },
            { "Amber Tinted Glass", R"PAINT({
  "format": 1,
  "name": "Amber Tinted Glass",
  "panel": {
    "key": 0.51,
    "vignette": 0.38,
    "rim": 2,
    "corner": 10,
    "screws": false
  },
  "sections": {
    "kind": "inlay",
    "radius": 10,
    "depth": 2,
    "strength": 0.5,
    "surface": {
      "material": "glass",
      "finish": "polished",
      "color": "#FF432F1C",
      "color2": "#FF302217",
      "grain": 0.1,
      "shine": 0.67,
      "scale": 1.05,
      "pattern": "none"
    }
  },
  "titlePlate": {
    "use": true,
    "screws": false,
    "surface": {
      "material": "glass",
      "finish": "polished",
      "color": "#FF211710",
      "color2": "#FF120E0A",
      "grain": 0.03,
      "shine": 0.83,
      "scale": 0.9,
      "pattern": "none"
    },
    "textStyle": "printed",
    "textColor": "#FFFFDB8C"
  },
  "screens": {
    "color": "#FF1B120B",
    "glow": "#FFFFC15A"
  },
  "wells": {
    "use": false,
    "minKnob": 76
  },
  "dividers": {
    "kind": "none",
    "width": 1
  },
  "vents": {
    "use": false,
    "slots": 7,
    "across": true
  },
  "accentLight": {
    "use": true,
    "strength": 0.31
  }
})PAINT" },
        };

        return list;
    }

    /** A look's painted panel, as parsed JSON (void: none). */
    inline juce::var findPaintedPanel (const juce::String& name)
    {
        for (const auto& panel : paintedPanels())
            if (name.trim().equalsIgnoreCase (panel.name))
                return juce::JSON::parse (juce::String (panel.json));

        return {};
    }

    /** The window's title: its biggest plain words (the topmost of them); -1: none. */
    inline int titleIndex (const GuiLayout& layout)
    {
        int best = -1;

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
        {
            const auto& w = layout.widgets[(size_t) i];

            if (w.type != GuiWidget::Type::label || w.label.trim().isEmpty() || w.opacity <= 0.0f || PanelParts::labelDisplayOf (w) != "plain")
                continue;

            if (best < 0)
            {
                best = i;
                continue;
            }

            const auto& b = layout.widgets[(size_t) best];

            if (w.fontSize > b.fontSize || (w.fontSize == b.fontSize && w.bounds.getY() < b.bounds.getY()))
                best = i;
        }

        return best;
    }

    /** PanelPainter's settings for a look's painted panel on this window: the look's surface lit
        as its recipe says, a section under each group, a plate under the title, a screen under
        each display, a well under each big knob, dividers between rows of groups, vents in a free
        corner, the accent's light behind the title. Void: the look has no painted panel. */
    inline juce::var painterSettings (const GuiLayout& layout, const juce::String& lookName)
    {
        const auto look = find (lookName);
        const auto recipe = findPaintedPanel (lookName);

        if (! look.isObject() || ! recipe.isObject())
            return {};

        using Type = GuiWidget::Type;
        auto* settings = new juce::DynamicObject();
        juce::var result (settings);

        const auto surface = look.getProperty ("surface", {});

        for (const auto* key : { "material", "finish", "color2", "grain", "shine", "scale", "patterns" })
            if (surface.hasProperty (key))
                settings->setProperty (key, surface.getProperty (key, {}).clone());

        const auto panel = recipe.getProperty ("panel", {});

        for (const auto* key : { "key", "vignette", "rim", "corner" })
            if (panel.hasProperty (key))
                settings->setProperty (key, panel.getProperty (key, {}));

        settings->setProperty ("screws", (bool) panel.getProperty ("screws", false) ? 1 : 0);
        settings->setProperty ("light", std::round (layout.lightAngle * 10.0f) / 10.0f);
        settings->setProperty ("altitude", std::round (layout.lightAltitude * 10.0f) / 10.0f);

        juce::Array<juce::var> parts, lines, vents;

        const auto box = [] (juce::Rectangle<int> r, int pad, const char* kind)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("kind", kind);
            o->setProperty ("x", r.getX() - pad);
            o->setProperty ("y", r.getY() - pad);
            o->setProperty ("w", r.getWidth() + 2 * pad);
            o->setProperty ("h", r.getHeight() + 2 * pad);
            return o;
        };

        const auto addSurface = [] (juce::DynamicObject* o, const juce::var& own)
        {
            if (auto* fields = own.getDynamicObject())
                for (const auto& f : fields->getProperties())
                    if (! (f.name.toString() == "pattern" && f.value.toString() == "none"))
                        o->setProperty (f.name, f.value);
        };

        const auto visible = [] (const GuiWidget& w) { return w.opacity > 0.0f && w.type != Type::image && ! (w.type == Type::shape); };
        const auto title = titleIndex (layout);

        // The accent's light behind the title (lights go first).
        if (const auto accent = recipe.getProperty ("accentLight", {}); (bool) accent.getProperty ("use", false) && title >= 0)
        {
            const auto r = layout.widgets[(size_t) title].bounds.toFloat();
            auto* o = new juce::DynamicObject();
            o->setProperty ("kind", "light");
            o->setProperty ("shape", "circle");
            o->setProperty ("x", r.getCentreX() - r.getWidth() * 0.8f);
            o->setProperty ("y", r.getCentreY() - r.getHeight() * 3.0f);
            o->setProperty ("w", r.getWidth() * 1.6f);
            o->setProperty ("h", r.getHeight() * 6.0f);
            o->setProperty ("color", look.getProperty ("accentColor", {}));
            o->setProperty ("strength", accent.getProperty ("strength", 0.3));
            parts.add (juce::var (o));
        }

        // Sections under the groups.
        const auto sections = recipe.getProperty ("sections", {});
        const auto sectionKind = sections.getProperty ("kind", "none").toString();
        std::vector<juce::Rectangle<int>> groups;

        for (const auto& w : layout.widgets)
            if (w.type == Type::group && w.opacity > 0.0f)
                groups.push_back (w.bounds);

        if (sectionKind != "none")
        {
            for (const auto& g : groups)
            {
                auto* o = box (g, 6, sectionKind.toRawUTF8());

                for (const auto* key : { "radius", "depth", "strength" })
                    if (sections.hasProperty (key))
                        o->setProperty (key, sections.getProperty (key, {}));

                addSurface (o, sections.getProperty ("surface", {}));
                parts.add (juce::var (o));
            }
        }

        // Screens under the displays, wells under the big knobs.
        const auto screens = recipe.getProperty ("screens", {});
        const auto wells = recipe.getProperty ("wells", {});
        const bool useWells = wells.getProperty ("use", false);
        const auto minKnob = (int) wells.getProperty ("minKnob", 64);

        for (const auto& w : layout.widgets)
        {
            if (w.opacity <= 0.0f)
                continue;

            if (w.type == Type::meter || w.type == Type::scope || w.type == Type::envelope || w.type == Type::filter)
            {
                auto area = w.bounds;

                if (w.label.isNotEmpty())
                    area = area.withTrimmedBottom (GuiLayout::captionHeight);

                auto* o = box (area, 5, "screen");
                o->setProperty ("color", screens.getProperty ("color", "#FF0A1216"));
                o->setProperty ("glow", screens.getProperty ("glow", {}));
                parts.add (juce::var (o));
            }
            else if (w.type == Type::knob && useWells && w.bounds.getWidth() >= minKnob)
            {
                parts.add (juce::var (box ({ w.bounds.getX(), w.bounds.getY(), w.bounds.getWidth(), w.bounds.getWidth() }, 10, "well")));
            }
        }

        // The title's plate.
        if (const auto plate = recipe.getProperty ("titlePlate", {}); (bool) plate.getProperty ("use", false) && title >= 0)
        {
            const auto r = layout.widgets[(size_t) title].bounds;
            auto* o = new juce::DynamicObject();
            o->setProperty ("kind", "plate");
            o->setProperty ("x", r.getX() - 12);
            o->setProperty ("y", r.getY() - 7);
            o->setProperty ("w", r.getWidth() + 24);
            o->setProperty ("h", r.getHeight() + 14);
            o->setProperty ("radius", 4);
            o->setProperty ("screws", plate.getProperty ("screws", false));
            addSurface (o, plate.getProperty ("surface", {}));
            parts.add (juce::var (o));
        }

        // Dividers between rows of groups.
        if (const auto dividers = recipe.getProperty ("dividers", {}); dividers.getProperty ("kind", "none").toString() != "none" && groups.size() > 1)
        {
            int left = layout.width, right = 0;

            for (const auto& g : groups)
            {
                left = juce::jmin (left, g.getX());
                right = juce::jmax (right, g.getRight());
            }

            std::vector<juce::Rectangle<int>> sorted (groups);
            std::sort (sorted.begin(), sorted.end(), [] (const auto& a, const auto& b) { return a.getY() < b.getY(); });
            int bottom = sorted.front().getBottom();

            for (size_t i = 1; i < sorted.size() && lines.size() < 4; ++i)
            {
                if (sorted[i].getY() >= bottom + 8)
                {
                    auto* o = new juce::DynamicObject();
                    const auto y = (bottom + sorted[i].getY()) / 2;
                    o->setProperty ("x1", left);
                    o->setProperty ("y1", y);
                    o->setProperty ("x2", right);
                    o->setProperty ("y2", y);
                    o->setProperty ("width", dividers.getProperty ("width", 2));
                    o->setProperty ("kind", dividers.getProperty ("kind", "groove"));
                    lines.add (juce::var (o));
                }

                bottom = juce::jmax (bottom, sorted[i].getBottom());
            }
        }

        // Vents in a corner nothing else uses.
        if (const auto v = recipe.getProperty ("vents", {}); (bool) v.getProperty ("use", false))
        {
            constexpr int ventW = 110, ventH = 28, margin = 20;
            const juce::Rectangle<int> corners[] { { layout.width - margin - ventW, layout.height - margin - ventH, ventW, ventH },
                                                   { margin, layout.height - margin - ventH, ventW, ventH },
                                                   { layout.width - margin - ventW, margin, ventW, ventH },
                                                   { margin, margin, ventW, ventH } };

            for (const auto& c : corners)
            {
                bool free = true;

                for (const auto& w : layout.widgets)
                    free = free && ! (visible (w) && w.bounds.expanded (6).intersects (c));

                if (free)
                {
                    auto* o = box (c, 0, "vent");
                    o->removeProperty ("kind");
                    o->setProperty ("slots", v.getProperty ("slots", 8));
                    o->setProperty ("across", v.getProperty ("across", true));
                    vents.add (juce::var (o));
                    break;
                }
            }
        }

        settings->setProperty ("parts", parts);

        if (! lines.isEmpty())   settings->setProperty ("lines", lines);
        if (! vents.isEmpty())   settings->setProperty ("vents", vents);

        return result;
    }

    /** paint_picture's recipe for a look's painted panel on this window (void: none). */
    inline juce::var painterRecipe (const GuiLayout& layout, const juce::String& lookName, const juce::String& file)
    {
        const auto settings = painterSettings (layout, lookName);

        if (! settings.isObject())
            return {};

        auto* recipe = new juce::DynamicObject();
        recipe->setProperty ("element", "PanelPainter");
        recipe->setProperty ("file", file);
        recipe->setProperty ("width", layout.width);
        recipe->setProperty ("height", layout.height);
        recipe->setProperty ("color", find (lookName).getProperty ("surface", {}).getProperty ("color", {}));
        recipe->setProperty ("settings", settings);
        return juce::var (recipe);
    }

    /** Before a look's painted panel goes behind everything: the panels (shapes) step back
        (hidden, kept: their own menu or Ctrl+Z brings them back), and the title takes the
        plate's lettering. Returns how many panels were hidden. */
    inline int prepareForPainting (GuiLayout& layout, const juce::String& lookName)
    {
        const auto recipe = findPaintedPanel (lookName);

        if (! recipe.isObject())
            return 0;

        int hidden = 0;

        for (auto& w : layout.widgets)
        {
            if (isPanel (w) && w.opacity > 0.0f)
            {
                w.opacity = 0.0f;
                ++hidden;
            }
        }

        const auto plate = recipe.getProperty ("titlePlate", {});

        if (const auto title = titleIndex (layout); title >= 0 && (bool) plate.getProperty ("use", false))
        {
            auto& w = layout.widgets[(size_t) title];
            w.colour = colourOf (plate.getProperty ("textColor", {}));

            const auto style = plate.getProperty ("textStyle", "printed").toString();
            const auto effects = titleEffects (style == "printed" ? juce::String ("plain") : style, {});

            if (effects.isObject())
                w.settings["textEffects"] = effects;
            else
                w.settings.erase ("textEffects");
        }

        return hidden;
    }
}
