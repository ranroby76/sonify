// C:\workspace\Fanan_web_hub\src\clap-webview-gui.h

#pragma once

// Stand-in for wclap-bridge's webview-gui module, with the same API.
//
// The real module opens a plugin's web UI in its own native window. The Hub shows
// plugin UIs inside its own web view instead, so every native-window call here
// answers "not supported". Messages a plugin sends to its UI are still forwarded
// to the host's CLAP_EXT_WEBVIEW extension whenever the host provides one.

#include "clap/all.h"   // includes the draft webview extension

#include <cstdint>
#include <cstring>

namespace webview_gui
{

struct ClapWebviewGui
{
    // Plugin GUI extension handed to the host: reports no native GUI support.
    clap_plugin_gui* extPluginGui;

    // Used by the bridge instead of querying the host directly.
    const clap_host_webview* extHostWebview;

    ClapWebviewGui (const clap_plugin* = nullptr, const clap_host* = nullptr)
    {
        extPluginGui = &pluginGuiProxy;
        extHostWebview = &hostWebviewProxy;
    }

    void init (const clap_plugin*, const clap_host*) {}
    void init() {}

    bool isApiSupported (const char*, bool)            { return false; }
    bool getPreferredApi (const char**, bool*)         { return false; }
    bool create (const char*, bool)                    { return false; }
    void destroy()                                     {}
    bool setScale (double)                             { return true; }

    bool getSize (uint32_t* width, uint32_t* height)
    {
        *width = 640;
        *height = 400;
        return true;
    }

    bool canResize()                                   { return false; }

    bool getResizeHints (clap_gui_resize_hints_t* hints)
    {
        *hints = { false, false, false, 0, 0 };
        return true;
    }

    bool adjustSize (uint32_t*, uint32_t*)             { return false; }
    bool setSize (uint32_t, uint32_t)                  { return false; }
    bool setParent (const clap_window*)                { return false; }
    bool setTransient (const clap_window*)             { return false; }
    void suggestTitle (const char*)                    {}
    bool show()                                        { return false; }
    bool hide()                                        { return false; }

private:
    static bool hostWebviewSend (const clap_host_t* host, const void* buffer, uint32_t size)
    {
        if (host == nullptr || host->get_extension == nullptr)
            return false;

        const auto* ext = static_cast<const clap_host_webview*> (host->get_extension (host, CLAP_EXT_WEBVIEW));
        return ext != nullptr && ext->send != nullptr && ext->send (host, buffer, size);
    }

    static bool guiIsApiSupported (const clap_plugin*, const char*, bool)          { return false; }
    static bool guiGetPreferredApi (const clap_plugin*, const char**, bool*)       { return false; }
    static bool guiCreate (const clap_plugin*, const char*, bool)                  { return false; }
    static void guiDestroy (const clap_plugin*)                                    {}
    static bool guiSetScale (const clap_plugin*, double)                           { return true; }

    static bool guiGetSize (const clap_plugin*, uint32_t* width, uint32_t* height)
    {
        *width = 640;
        *height = 400;
        return true;
    }

    static bool guiCanResize (const clap_plugin*)                                  { return false; }

    static bool guiGetResizeHints (const clap_plugin*, clap_gui_resize_hints_t* hints)
    {
        *hints = { false, false, false, 0, 0 };
        return true;
    }

    static bool guiAdjustSize (const clap_plugin*, uint32_t*, uint32_t*)           { return false; }
    static bool guiSetSize (const clap_plugin*, uint32_t, uint32_t)                { return false; }
    static bool guiSetParent (const clap_plugin*, const clap_window*)              { return false; }
    static bool guiSetTransient (const clap_plugin*, const clap_window*)           { return false; }
    static void guiSuggestTitle (const clap_plugin*, const char*)                  {}
    static bool guiShow (const clap_plugin*)                                       { return false; }
    static bool guiHide (const clap_plugin*)                                       { return false; }

    clap_plugin_gui pluginGuiProxy { guiIsApiSupported, guiGetPreferredApi, guiCreate, guiDestroy,
                                     guiSetScale, guiGetSize, guiCanResize, guiGetResizeHints,
                                     guiAdjustSize, guiSetSize, guiSetParent, guiSetTransient,
                                     guiSuggestTitle, guiShow, guiHide };

    clap_host_webview hostWebviewProxy { hostWebviewSend };
};

} // namespace webview_gui
