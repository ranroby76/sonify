// C:\workspace\Fanan_web_hub\src\helpers.h

#pragma once

// Stand-in for webview-gui's helpers. wclap-bridge only needs guessMediaType(), used
// when a plugin serves a web UI file without naming its type.

#include <cstring>
#include <string>

namespace webview_gui
{
namespace helpers
{

inline std::string guessMediaType (const char* path)
{
    struct Entry { const char* extension; const char* type; };

    static constexpr Entry types[] = {
        { "html", "text/html;charset=utf-8" },       { "htm",  "text/html;charset=utf-8" },
        { "js",   "text/javascript;charset=utf-8" }, { "mjs",  "text/javascript;charset=utf-8" },
        { "css",  "text/css;charset=utf-8" },        { "json", "application/json" },
        { "txt",  "text/plain;charset=utf-8" },      { "svg",  "image/svg+xml" },
        { "png",  "image/png" },                     { "jpg",  "image/jpeg" },
        { "jpeg", "image/jpeg" },                    { "gif",  "image/gif" },
        { "webp", "image/webp" },                    { "ico",  "image/x-icon" },
        { "wasm", "application/wasm" },              { "woff", "font/woff" },
        { "woff2", "font/woff2" },                   { "ttf",  "font/ttf" },
        { "otf",  "font/otf" },                      { "wav",  "audio/wav" },
        { "mp3",  "audio/mpeg" },                    { "ogg",  "audio/ogg" },
    };

    std::string name (path != nullptr ? path : "");
    name = name.substr (0, name.find_first_of ("?#"));

    const auto slash = name.find_last_of ('/');
    const auto dot = name.find_last_of ('.');

    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return "text/html;charset=utf-8";

    auto extension = name.substr (dot + 1);

    for (auto& c : extension)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char> (c - 'A' + 'a');

    for (const auto& entry : types)
        if (extension == entry.extension)
            return entry.type;

    return "application/octet-stream";
}

} // namespace helpers
} // namespace webview_gui
