// D:\workspace\Fanan_web_hub\src\stella_gui_x11.cpp
//
// An exported plugin's window on Linux: an X11 window inside the host's, showing the Editor's
// pixels and passing the mouse to it. It talks to the X server over a connection of its own,
// and runs on the host's timer and its watch on that connection (see Window::idle): redrawn
// about 30 times a second, so meters, scopes and automation stay live. While Registration is
// open it takes the keyboard focus (a click on it) and the keys (XLookupString), and hands the
// focus back when it closes. The clipboard is the CLIPBOARD selection: a paste is asked of its
// owner and arrives with the next events (never waited for); a copy makes this window its owner.
//
// libX11 is loaded when the window opens (what it uses is declared here, as Xlib declares it),
// so building the plugin needs no X11 development files and the plugin loads without X too.

#include "stella_gui.h"

#if defined (__linux__)

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>

namespace stella::gui
{
    namespace
    {
        //======================================================================
        // Xlib, as it declares it (its ABI hasn't changed in decades).
        struct Display;
        struct Visual;
        struct GCRecord;
        using XID = unsigned long;
        using XWindow = XID;
        using Atom = XID;
        using KeySym = XID;
        struct XErrorEvent;
        using ErrorHandler = int (*) (Display*, XErrorEvent*);
        using Colormap = XID;
        using XTime = unsigned long;
        using GC = GCRecord*;
        using Bool = int;

        struct XImage
        {
            int width, height;
            int xoffset;
            int format;
            char* data;
            int byte_order;
            int bitmap_unit;
            int bitmap_bit_order;
            int bitmap_pad;
            int depth;
            int bytes_per_line;
            int bits_per_pixel;
            unsigned long red_mask, green_mask, blue_mask;
            void* obdata;

            struct Functions
            {
                XImage* (*create_image) (Display*, Visual*, unsigned int, int, int, char*, unsigned int, unsigned int, int, int);
                int (*destroy_image) (XImage*);
                unsigned long (*get_pixel) (XImage*, int, int);
                int (*put_pixel) (XImage*, int, int, unsigned long);
                XImage* (*sub_image) (XImage*, int, int, unsigned int, unsigned int);
                int (*add_pixel) (XImage*, long);
            } f;
        };

        struct XSetWindowAttributes
        {
            XID background_pixmap;
            unsigned long background_pixel;
            XID border_pixmap;
            unsigned long border_pixel;
            int bit_gravity;
            int win_gravity;
            int backing_store;
            unsigned long backing_planes;
            unsigned long backing_pixel;
            Bool save_under;
            long event_mask;
            long do_not_propagate_mask;
            Bool override_redirect;
            Colormap colormap;
            XID cursor;
        };

        struct XAnyEvent
        {
            int type;
            unsigned long serial;
            Bool send_event;
            Display* display;
            XWindow window;
        };

        struct XPointerEvent   // XButtonEvent and XMotionEvent share this beginning
        {
            int type;
            unsigned long serial;
            Bool send_event;
            Display* display;
            XWindow window, root, subwindow;
            XTime time;
            int x, y;
            int x_root, y_root;
            unsigned int state;
            unsigned int button;   // a motion event's is_hint here
        };

        struct XKeyEvent
        {
            int type;
            unsigned long serial;
            Bool send_event;
            Display* display;
            XWindow window, root, subwindow;
            XTime time;
            int x, y;
            int x_root, y_root;
            unsigned int state;
            unsigned int keycode;
            Bool same_screen;
        };

        struct XSelectionRequestEvent
        {
            int type;
            unsigned long serial;
            Bool send_event;
            Display* display;
            XWindow owner, requestor;
            Atom selection, target, property;
            XTime time;
        };

        struct XSelectionEvent   // SelectionNotify
        {
            int type;
            unsigned long serial;
            Bool send_event;
            Display* display;
            XWindow requestor;
            Atom selection, target, property;
            XTime time;
        };

        union XEvent
        {
            int type;
            XAnyEvent any;
            XPointerEvent pointer;
            XKeyEvent key;
            XSelectionRequestEvent request;
            XSelectionEvent selection;
            long pad[24];
        };

        constexpr int keyPress = 2, buttonPress = 4, buttonRelease = 5, motionNotify = 6, leaveNotify = 8, focusIn = 9, focusOut = 10,
                      expose = 12, selectionClear = 29, selectionRequest = 30, selectionNotify = 31;
        constexpr long exposureMask = 1L << 15, buttonPressMask = 1L << 2, buttonReleaseMask = 1L << 3, pointerMotionMask = 1L << 6,
                       enterWindowMask = 1L << 4, leaveWindowMask = 1L << 5, keyPressMask = 1L << 0, keyReleaseMask = 1L << 1,
                       focusChangeMask = 1L << 21;
        constexpr unsigned int shiftMask = 1u << 0, controlMask = 1u << 2, button1Mask = 1u << 8;
        constexpr Atom xaAtom = 4, xaString = 31, anyPropertyType = 0;
        constexpr int revertToParent = 2, propModeReplace = 0;
        constexpr unsigned long cwBackPixel = 1ul << 1, cwBorderPixel = 1ul << 3, cwEventMask = 1ul << 11, cwColormap = 1ul << 13;
        constexpr int zPixmap = 2, inputOutput = 1, allocNone = 0;

        /** libX11's functions, loaded once. */
        struct X11
        {
            Display* (*openDisplay) (const char*) = nullptr;
            int (*closeDisplay) (Display*) = nullptr;
            int (*defaultScreen) (Display*) = nullptr;
            Visual* (*defaultVisual) (Display*, int) = nullptr;
            int (*defaultDepth) (Display*, int) = nullptr;
            XWindow (*rootWindow) (Display*, int) = nullptr;
            Colormap (*createColormap) (Display*, XWindow, Visual*, int) = nullptr;
            int (*freeColormap) (Display*, Colormap) = nullptr;
            XWindow (*createWindow) (Display*, XWindow, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual*,
                                     unsigned long, XSetWindowAttributes*) = nullptr;
            int (*destroyWindow) (Display*, XWindow) = nullptr;
            int (*mapWindow) (Display*, XWindow) = nullptr;
            int (*unmapWindow) (Display*, XWindow) = nullptr;
            GC (*createGC) (Display*, XID, unsigned long, void*) = nullptr;
            int (*freeGC) (Display*, GC) = nullptr;
            XImage* (*createImage) (Display*, Visual*, unsigned int, int, int, char*, unsigned int, unsigned int, int, int) = nullptr;
            int (*putImage) (Display*, XID, GC, XImage*, int, int, int, int, unsigned int, unsigned int) = nullptr;
            int (*pending) (Display*) = nullptr;
            int (*nextEvent) (Display*, XEvent*) = nullptr;
            int (*flush) (Display*) = nullptr;
            int (*connectionNumber) (Display*) = nullptr;

            // The keyboard and the clipboard: without them the window still works, without those.
            int (*lookupString) (XKeyEvent*, char*, int, KeySym*, void*) = nullptr;
            int (*setInputFocus) (Display*, XWindow, int, XTime) = nullptr;
            int (*getInputFocus) (Display*, XWindow*, int*) = nullptr;
            ErrorHandler (*setErrorHandler) (ErrorHandler) = nullptr;
            int (*sync) (Display*, Bool) = nullptr;
            Atom (*internAtom) (Display*, const char*, Bool) = nullptr;
            int (*convertSelection) (Display*, Atom, Atom, Atom, XWindow, XTime) = nullptr;
            int (*getWindowProperty) (Display*, XWindow, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long*, unsigned long*, unsigned char**) = nullptr;
            int (*free) (void*) = nullptr;
            int (*setSelectionOwner) (Display*, Atom, XWindow, XTime) = nullptr;
            XWindow (*getSelectionOwner) (Display*, Atom) = nullptr;
            int (*changeProperty) (Display*, XWindow, Atom, Atom, int, int, const unsigned char*, int) = nullptr;
            int (*sendEvent) (Display*, XWindow, Bool, long, XEvent*) = nullptr;

            bool loaded = false, hasKeys = false, hasClipboard = false;

            static X11& get()
            {
                static X11 x11;
                static bool tried = false;

                if (! tried)
                {
                    tried = true;
                    x11.load();
                }

                return x11;
            }

        private:
            template <typename Function>
            static bool find (void* library, const char* name, Function& function)
            {
                function = reinterpret_cast<Function> (dlsym (library, name));
                return function != nullptr;
            }

            void load()
            {
                auto* library = dlopen ("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);

                if (library == nullptr)
                    library = dlopen ("libX11.so", RTLD_LAZY | RTLD_LOCAL);

                if (library == nullptr)
                    return;

                loaded = find (library, "XOpenDisplay", openDisplay) && find (library, "XCloseDisplay", closeDisplay)
                      && find (library, "XDefaultScreen", defaultScreen) && find (library, "XDefaultVisual", defaultVisual)
                      && find (library, "XDefaultDepth", defaultDepth) && find (library, "XRootWindow", rootWindow)
                      && find (library, "XCreateColormap", createColormap) && find (library, "XFreeColormap", freeColormap)
                      && find (library, "XCreateWindow", createWindow) && find (library, "XDestroyWindow", destroyWindow)
                      && find (library, "XMapWindow", mapWindow) && find (library, "XUnmapWindow", unmapWindow)
                      && find (library, "XCreateGC", createGC) && find (library, "XFreeGC", freeGC)
                      && find (library, "XCreateImage", createImage) && find (library, "XPutImage", putImage)
                      && find (library, "XPending", pending) && find (library, "XNextEvent", nextEvent)
                      && find (library, "XFlush", flush) && find (library, "XConnectionNumber", connectionNumber);

                hasKeys = find (library, "XLookupString", lookupString) && find (library, "XSetInputFocus", setInputFocus)
                       && find (library, "XGetInputFocus", getInputFocus) && find (library, "XSetErrorHandler", setErrorHandler)
                       && find (library, "XSync", sync);

                hasClipboard = find (library, "XInternAtom", internAtom) && find (library, "XConvertSelection", convertSelection)
                            && find (library, "XGetWindowProperty", getWindowProperty) && find (library, "XFree", free)
                            && find (library, "XSetSelectionOwner", setSelectionOwner) && find (library, "XGetSelectionOwner", getSelectionOwner)
                            && find (library, "XChangeProperty", changeProperty) && find (library, "XSendEvent", sendEvent);
            }
        };

        /** While the focus moves: an error (a window gone meanwhile) is ignored, not fatal. */
        int ignoreErrors (Display*, XErrorEvent*)    { return 0; }

        //======================================================================
        class X11Window final : public Window,
                                private Clipboard
        {
        public:
            X11Window (Host& host, Display* d)
                : x (X11::get()), display (d), editor (host)
            {
                pixels.resize ((size_t) editor.getWidth() * (size_t) editor.getHeight());
                editor.setClipboard (this);
            }

            ~X11Window() override
            {
                editor.setClipboard (nullptr);

                if (focused)
                {
                    focused = false;
                    giveFocusBack();
                }

                if (image != nullptr)
                {
                    image->data = nullptr;   // the pixels are ours
                    image->f.destroy_image (image);
                }

                if (gc != nullptr)
                    x.freeGC (display, gc);

                if (window != 0)
                    x.destroyWindow (display, window);

                if (colormap != 0)
                    x.freeColormap (display, colormap);

                x.flush (display);
                x.closeDisplay (display);
            }

            bool attach (void* parent) override
            {
                if (window != 0 || parent == nullptr)
                    return false;

                const auto screen = x.defaultScreen (display);
                auto* visual = x.defaultVisual (display, screen);
                const auto depth = x.defaultDepth (display, screen);

                if (depth != 24 && depth != 32)
                    return false;   // the pixels are 32-bit true colour

                // Its own colours, so it can sit in a host window of any kind.
                colormap = x.createColormap (display, x.rootWindow (display, screen), visual, allocNone);

                XSetWindowAttributes attributes {};
                attributes.background_pixel = 0;
                attributes.border_pixel = 0;
                attributes.colormap = colormap;
                attributes.event_mask = exposureMask | buttonPressMask | buttonReleaseMask | pointerMotionMask | enterWindowMask | leaveWindowMask
                                      | keyPressMask | keyReleaseMask | focusChangeMask;

                parentWindow = (XWindow) reinterpret_cast<std::uintptr_t> (parent);
                window = x.createWindow (display, parentWindow, 0, 0,
                                         (unsigned int) editor.getWidth(), (unsigned int) editor.getHeight(), 0, depth, inputOutput, visual,
                                         cwBackPixel | cwBorderPixel | cwColormap | cwEventMask, &attributes);

                if (window == 0)
                    return false;

                gc = x.createGC (display, window, 0, nullptr);
                image = x.createImage (display, visual, (unsigned int) depth, zPixmap, 0, reinterpret_cast<char*> (pixels.data()),
                                       (unsigned int) editor.getWidth(), (unsigned int) editor.getHeight(), 32, editor.getWidth() * 4);

                if (gc == nullptr || image == nullptr)
                    return false;

                if (x.hasClipboard)
                {
                    clipboardAtom = x.internAtom (display, "CLIPBOARD", 0);
                    utf8Atom = x.internAtom (display, "UTF8_STRING", 0);
                    targetsAtom = x.internAtom (display, "TARGETS", 0);
                    pasteAtom = x.internAtom (display, "STELLA_PASTE", 0);
                }

                x.mapWindow (display, window);
                x.flush (display);
                return true;
            }

            void setVisible (bool visible) override
            {
                if (window == 0)
                    return;

                if (visible)
                    x.mapWindow (display, window);
                else
                    x.unmapWindow (display, window);

                x.flush (display);
            }

            int getFileDescriptor() const override    { return x.connectionNumber (display); }

            // Keys the host hands the plugin's view.
            bool wantsKeys() override    { return editor.wantsKeys(); }

            bool keyDown (Key key, std::uint32_t character, int modifiers) override
            {
                const bool taken = editor.keyDown (key, character, modifiers, true);

                if (taken && window != 0)
                {
                    followKeys (false);
                    paint();
                }

                return taken;
            }

            void idle() override
            {
                if (window == 0)
                    return;

                bool exposed = false;

                while (x.pending (display) > 0)
                {
                    XEvent event {};
                    x.nextEvent (display, &event);

                    if (event.any.window != window)
                        continue;

                    exposed = handle (event) || exposed;
                }

                // About 30 times a second, and at once when the server asks.
                const auto now = std::chrono::steady_clock::now();

                if (exposed || now - lastPaint >= std::chrono::milliseconds (30))
                {
                    lastPaint = now;
                    paint();
                }
            }

        private:
            //==================================================================
            // The clipboard (Editor's Clipboard): the CLIPBOARD selection.
            bool getText (std::string& text) override
            {
                text.clear();

                if (ownsClipboard)
                {
                    text = copied;   // ours: no need to ask
                    return true;
                }

                if (! x.hasClipboard || window == 0 || clipboardAtom == 0 || x.getSelectionOwner (display, clipboardAtom) == 0)
                    return true;     // nothing to paste

                askForPaste (utf8Atom);
                return false;        // it comes with the next events (handle: selectionNotify)
            }

            void setText (const std::string& text) override
            {
                if (! x.hasClipboard || window == 0 || clipboardAtom == 0)
                    return;

                copied = text;
                x.setSelectionOwner (display, clipboardAtom, window, lastTime);
                ownsClipboard = x.getSelectionOwner (display, clipboardAtom) == window;
                x.flush (display);
            }

            void askForPaste (Atom target)
            {
                pasteTarget = target;
                x.convertSelection (display, clipboardAtom, target, pasteAtom, window, lastTime);
                x.flush (display);
            }

            /** The clipboard's owner answered (or couldn't). */
            void pasteArrived (const XSelectionEvent& e)
            {
                if (e.property == 0)
                {
                    // No UTF-8 from it: plain text then.
                    if (pasteTarget == utf8Atom && utf8Atom != xaString)
                        askForPaste (xaString);

                    return;
                }

                Atom type = 0;
                int format = 0;
                unsigned long count = 0, after = 0;
                unsigned char* data = nullptr;

                if (x.getWindowProperty (display, window, pasteAtom, 0, 1024, 1, anyPropertyType, &type, &format, &count, &after, &data) != 0)
                    return;   // not Success

                std::string text;

                if (data != nullptr && format == 8 && (type == utf8Atom || type == xaString))
                {
                    for (unsigned long i = 0; i < count; ++i)
                    {
                        const auto c = data[i];

                        // STRING is Latin-1: as UTF-8.
                        if (type == xaString && c >= 0x80)
                        {
                            text += (char) (0xc0 | (c >> 6));
                            text += (char) (0x80 | (c & 0x3f));
                        }
                        else
                        {
                            text += (char) c;
                        }
                    }
                }

                if (data != nullptr)
                    x.free (data);

                if (! text.empty())
                    editor.pasteText (text);
            }

            /** Someone asks for what this window copied. */
            void answer (const XSelectionRequestEvent& r)
            {
                XEvent reply {};
                auto& n = reply.selection;
                n.type = selectionNotify;
                n.display = display;
                n.requestor = r.requestor;
                n.selection = r.selection;
                n.target = r.target;
                n.property = 0;   // refused, unless it's something it has
                n.time = r.time;

                const auto property = r.property != 0 ? r.property : r.target;   // old clients name no property

                if (ownsClipboard && r.selection == clipboardAtom)
                {
                    if (r.target == targetsAtom)
                    {
                        const Atom targets[] { targetsAtom, utf8Atom, xaString };
                        x.changeProperty (display, r.requestor, property, xaAtom, 32, propModeReplace,
                                          reinterpret_cast<const unsigned char*> (targets), 3);
                        n.property = property;
                    }
                    else if (r.target == utf8Atom || r.target == xaString)
                    {
                        x.changeProperty (display, r.requestor, property, r.target, 8, propModeReplace,
                                          reinterpret_cast<const unsigned char*> (copied.data()), (int) copied.size());
                        n.property = property;
                    }
                }

                x.sendEvent (display, r.requestor, 0, 0, &reply);
                x.flush (display);
            }

            //==================================================================
            // The keyboard focus: taken while Registration is open (from a click on it), handed
            // back to whoever had it when it closes.
            void setFocusSafely (XWindow to, XTime time)
            {
                // The window may have gone: an error then mustn't end the host (Xlib's own
                // handler exits).
                const auto previous = x.setErrorHandler (ignoreErrors);
                x.setInputFocus (display, to, revertToParent, time);
                x.sync (display, 0);
                x.setErrorHandler (previous);
            }

            void giveFocusBack()
            {
                if (x.hasKeys && window != 0)
                    setFocusSafely (focusBefore != 0 && focusBefore != window ? focusBefore : parentWindow, lastTime);

                focusBefore = 0;
            }

            void followKeys (bool mayTake)
            {
                if (! x.hasKeys || window == 0)
                    return;

                const bool wants = editor.wantsKeys();

                if (wants && mayTake && ! focused)
                {
                    XWindow before = 0;
                    int revert = 0;
                    x.getInputFocus (display, &before, &revert);

                    if (before != window)
                        focusBefore = before > 1 ? before : 0;   // not None or PointerRoot

                    setFocusSafely (window, lastTime);
                    focused = true;   // FocusIn follows
                }
                else if (! wants && focused)
                {
                    focused = false;
                    giveFocusBack();
                }
            }

            /** A key going down while Registration is open: true if it took it. */
            bool keyPressed (const XKeyEvent& event)
            {
                if (! editor.wantsKeys() || ! x.hasKeys)
                    return false;

                auto copy = event;
                char typed[32] {};
                KeySym symbol = 0;
                const auto length = x.lookupString (&copy, typed, (int) sizeof (typed) - 1, &symbol, nullptr);
                const int modifiers = ((event.state & shiftMask) != 0 ? shiftKey : 0) | ((event.state & controlMask) != 0 ? commandKey : 0);

                switch (symbol)
                {
                    case 0xff08:                return editor.keyDown (Key::backspace, 0, modifiers);
                    case 0xffff: case 0xff9f:   return editor.keyDown (Key::forwardDelete, 0, modifiers);
                    case 0xff51: case 0xff96:   return editor.keyDown (Key::left, 0, modifiers);
                    case 0xff53: case 0xff98:   return editor.keyDown (Key::right, 0, modifiers);
                    case 0xff50: case 0xff95:   return editor.keyDown (Key::home, 0, modifiers);
                    case 0xff57: case 0xff9c:   return editor.keyDown (Key::end, 0, modifiers);
                    case 0xff0d: case 0xff8d:   return editor.keyDown (Key::enter, 0, modifiers);
                    case 0xff1b:                return editor.keyDown (Key::escape, 0, modifiers);
                    case 0xff09: case 0xfe20:   return editor.keyDown (Key::tab, 0, modifiers);

                    case 0xff63:   // Insert: Shift pastes, Ctrl copies
                        if ((modifiers & shiftKey) != 0)   return editor.keyDown (Key::character, 'v', commandKey);
                        if ((modifiers & commandKey) != 0) return editor.keyDown (Key::character, 'c', commandKey);
                        return false;

                    default:
                        break;
                }

                // Ctrl and a letter: a shortcut, by its key's symbol (what it types is a control character).
                if ((modifiers & commandKey) != 0)
                    return symbol >= 0x20 && symbol < 0x7f && editor.keyDown (Key::character, (std::uint32_t) symbol, modifiers);

                bool taken = false;

                for (int i = 0; i < length && i < (int) sizeof (typed); ++i)
                {
                    const auto c = (std::uint32_t) (unsigned char) typed[i];   // Latin-1

                    if (c >= 0x20 && c != 0x7f)
                        taken = editor.keyDown (Key::character, c, 0) || taken;
                }

                return taken;
            }

            //==================================================================
            /** True when it should be drawn at once. */
            bool handle (const XEvent& event)
            {
                const auto& p = event.pointer;
                const bool fine = (p.state & (shiftMask | controlMask)) != 0;

                switch (event.type)
                {
                    case expose:
                        return true;

                    case keyPress:
                    {
                        lastTime = event.key.time;
                        const bool taken = keyPressed (event.key);
                        followKeys (false);
                        return taken;
                    }

                    case focusIn:
                        focused = true;
                        return false;

                    case focusOut:
                        focused = false;
                        return false;

                    case selectionRequest:
                        answer (event.request);
                        return false;

                    case selectionClear:
                        ownsClipboard = false;
                        return false;

                    case selectionNotify:
                        if (event.selection.property == pasteAtom || event.selection.property == 0)
                            pasteArrived (event.selection);

                        return true;

                    case buttonPress:
                        lastTime = p.time;

                        if (p.button == 1)
                        {
                            // A second click soon after, near the first: a double click.
                            const bool twice = p.time - lastClick < 400 && std::abs (p.x - clickX) < 5 && std::abs (p.y - clickY) < 5;
                            lastClick = twice ? 0 : p.time;
                            clickX = p.x;
                            clickY = p.y;
                            editor.mouseDown (p.x, p.y, fine, twice);
                            followKeys (true);
                            return true;
                        }

                        if (p.button == 4 || p.button == 5)
                        {
                            editor.mouseWheel (p.x, p.y, p.button == 4 ? 1.0f : -1.0f);
                            return true;
                        }

                        return false;

                    case buttonRelease:
                        lastTime = p.time;

                        if (p.button != 1)
                            return false;

                        editor.mouseUp();
                        followKeys (false);
                        return true;

                    case motionNotify:
                        if ((p.state & button1Mask) != 0)
                            editor.mouseDrag (p.x, p.y, fine);
                        else
                            editor.mouseMove (p.x, p.y);

                        return (p.state & button1Mask) != 0;

                    case leaveNotify:
                        editor.mouseExit();
                        return false;

                    default:
                        return false;
                }
            }

            void paint()
            {
                editor.render (pixels.data());
                x.putImage (display, window, gc, image, 0, 0, 0, 0, (unsigned int) editor.getWidth(), (unsigned int) editor.getHeight());
                x.flush (display);
            }

            X11& x;
            Display* display;
            Editor editor;
            std::vector<std::uint32_t> pixels;
            XWindow window = 0;
            Colormap colormap = 0;
            GC gc = nullptr;
            XImage* image = nullptr;
            std::chrono::steady_clock::time_point lastPaint {};
            XTime lastClick = 0;
            int clickX = 0, clickY = 0;

            // The keys and the clipboard.
            XWindow parentWindow = 0, focusBefore = 0;
            XTime lastTime = 0;   // the newest event's (selections and the focus want a real time)
            bool focused = false, ownsClipboard = false;
            Atom clipboardAtom = 0, utf8Atom = 0, targetsAtom = 0, pasteAtom = 0, pasteTarget = 0;
            std::string copied;
        };
    }

    std::unique_ptr<Window> Window::create (Host& host)
    {
        auto& x = X11::get();

        if (! x.loaded)
            return nullptr;

        auto* display = x.openDisplay (nullptr);

        if (display == nullptr)
            return nullptr;

        return std::make_unique<X11Window> (host, display);
    }
}

#endif
