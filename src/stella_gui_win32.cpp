// D:\workspace\Fanan_web_hub\src\stella_gui_win32.cpp
//
// An exported plugin's window on Windows: a child window inside the host's, showing the
// Editor's pixels and passing the mouse to it. Redrawn about 30 times a second so meters,
// scopes and automation stay live. While Registration is open it takes the keyboard focus (a
// click on it) and the keys, and gives the focus back when it closes; the clipboard is the
// system's (CF_UNICODETEXT).

#include "stella_gui.h"

#if defined (_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
 #define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>
#include <string>

namespace stella::gui
{
    namespace
    {
        int liveWindows = 0;

        HMODULE thisModule()
        {
            HMODULE module = nullptr;
            GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR> (&thisModule), &module);
            return module;
        }

        /** One window class per plugin file, so two exported plugins in one host never mix. */
        std::wstring className()
        {
            return L"SonifyPluginWindow" + std::to_wstring (reinterpret_cast<std::uintptr_t> (thisModule()));
        }

        int pointerX (LPARAM l)   { return (int) (short) LOWORD (l); }
        int pointerY (LPARAM l)   { return (int) (short) HIWORD (l); }
        bool fineKeys (WPARAM w)  { return (w & (MK_SHIFT | MK_CONTROL)) != 0; }

        /** Shift, and Ctrl as the shortcut key (not with Alt: Ctrl+Alt is AltGr, which types). */
        int keyModifiers()
        {
            const bool control = GetKeyState (VK_CONTROL) < 0 && GetKeyState (VK_MENU) >= 0;
            return (GetKeyState (VK_SHIFT) < 0 ? shiftKey : 0) | (control ? commandKey : 0);
        }

        class Win32Window final : public Window,
                                  private Clipboard
        {
        public:
            explicit Win32Window (Host& host)
                : editor (host)
            {
                pixels.resize ((size_t) editor.getWidth() * (size_t) editor.getHeight());
                editor.setClipboard (this);

                if (liveWindows++ == 0)
                {
                    WNDCLASSEXW wc {};
                    wc.cbSize = sizeof (wc);
                    wc.style = CS_DBLCLKS;
                    wc.lpfnWndProc = proc;
                    wc.hInstance = thisModule();
                    wc.hCursor = LoadCursorW (nullptr, MAKEINTRESOURCEW (32512));   // IDC_ARROW, as a wide resource id
                    static std::wstring name;
                    name = className();
                    wc.lpszClassName = name.c_str();
                    RegisterClassExW (&wc);
                }
            }

            ~Win32Window() override
            {
                editor.setClipboard (nullptr);

                if (hwnd != nullptr)
                {
                    // Closed with the keys: the host has them again.
                    if (GetFocus() == hwnd)
                        if (auto* back = previousFocus != nullptr && IsWindow (previousFocus) ? previousFocus : GetParent (hwnd))
                            SetFocus (back);

                    KillTimer (hwnd, 1);
                    SetWindowLongPtrW (hwnd, GWLP_USERDATA, 0);
                    DestroyWindow (hwnd);
                }

                if (--liveWindows == 0)
                    UnregisterClassW (className().c_str(), thisModule());
            }

            bool attach (void* parent) override
            {
                if (hwnd != nullptr)
                    return false;

                hwnd = CreateWindowExW (0, className().c_str(), L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                                        0, 0, editor.getWidth(), editor.getHeight(), static_cast<HWND> (parent), nullptr,
                                        thisModule(), this);

                if (hwnd == nullptr)
                    return false;

                SetTimer (hwnd, 1, 33, nullptr);
                return true;
            }

            void setVisible (bool visible) override
            {
                if (hwnd != nullptr)
                    ShowWindow (hwnd, visible ? SW_SHOW : SW_HIDE);
            }

            // Keys the host hands the plugin's view.
            bool wantsKeys() override    { return editor.wantsKeys(); }

            bool keyDown (Key key, std::uint32_t character, int modifiers) override
            {
                const bool taken = editor.keyDown (key, character, modifiers, true);
                afterKeys();
                return taken;
            }

        private:
            //==================================================================
            // The clipboard (Editor's Clipboard).
            bool getText (std::string& text) override
            {
                text.clear();

                if (! OpenClipboard (hwnd))
                    return true;   // busy: nothing to paste now

                if (auto* data = GetClipboardData (CF_UNICODETEXT))
                {
                    if (const auto* wide = static_cast<const wchar_t*> (GlobalLock (data)))
                    {
                        // A serial is short: what's past the first few thousand characters isn't one.
                        const auto limit = (int) (GlobalSize (data) / sizeof (wchar_t));
                        int length = 0;

                        while (length < limit && length < 4096 && wide[length] != 0)
                            ++length;

                        const auto size = WideCharToMultiByte (CP_UTF8, 0, wide, length, nullptr, 0, nullptr, nullptr);

                        if (size > 0)
                        {
                            text.resize ((size_t) size);
                            WideCharToMultiByte (CP_UTF8, 0, wide, length, &text[0], size, nullptr, nullptr);
                        }

                        GlobalUnlock (data);
                    }
                }

                CloseClipboard();
                return true;
            }

            void setText (const std::string& text) override
            {
                const auto length = MultiByteToWideChar (CP_UTF8, 0, text.data(), (int) text.size(), nullptr, 0);
                auto* memory = GlobalAlloc (GMEM_MOVEABLE, ((size_t) std::max (0, length) + 1) * sizeof (wchar_t));

                if (memory == nullptr)
                    return;

                auto* wide = static_cast<wchar_t*> (GlobalLock (memory));

                if (wide == nullptr)
                {
                    GlobalFree (memory);
                    return;
                }

                if (length > 0)
                    MultiByteToWideChar (CP_UTF8, 0, text.data(), (int) text.size(), wide, length);

                wide[std::max (0, length)] = 0;
                GlobalUnlock (memory);

                if (hwnd != nullptr && OpenClipboard (hwnd))
                {
                    EmptyClipboard();

                    if (SetClipboardData (CF_UNICODETEXT, memory) == nullptr)
                        GlobalFree (memory);   // still ours

                    CloseClipboard();
                }
                else
                {
                    GlobalFree (memory);
                }
            }

            //==================================================================
            // The keyboard focus: taken while Registration is open (from a click on it), given
            // back to whoever had it when it closes.
            void followKeys (bool mayTake)
            {
                if (hwnd == nullptr)
                    return;

                const bool wants = editor.wantsKeys();

                if (wants && mayTake && GetFocus() != hwnd)
                {
                    const auto before = SetFocus (hwnd);

                    if (before != hwnd)
                        previousFocus = before;
                }
                else if (! wants && GetFocus() == hwnd)
                {
                    const auto back = previousFocus != nullptr && IsWindow (previousFocus) ? previousFocus : GetParent (hwnd);
                    previousFocus = nullptr;

                    if (back != nullptr)
                        SetFocus (back);
                }
            }

            void afterKeys()
            {
                if (hwnd != nullptr)
                    InvalidateRect (hwnd, nullptr, FALSE);

                followKeys (false);
            }

            /** Keys the window doesn't take go on to the host's window, as if it never had them. */
            static LRESULT passOn (HWND window, UINT message, WPARAM w, LPARAM l)
            {
                if (auto* parent = GetParent (window))
                    return SendMessageW (parent, message, w, l);

                return DefWindowProcW (window, message, w, l);
            }

            /** A key going down while Registration is open: the ones that don't type. */
            bool specialKey (WPARAM virtualKey)
            {
                const auto modifiers = keyModifiers();
                const bool command = (modifiers & commandKey) != 0, shift = (modifiers & shiftKey) != 0;

                switch (virtualKey)
                {
                    case VK_BACK:    return editor.keyDown (Key::backspace, 0, modifiers);
                    case VK_DELETE:  return shift ? editor.keyDown (Key::character, 'x', commandKey)          // Shift+Del: cut
                                                  : editor.keyDown (Key::forwardDelete, 0, modifiers);
                    case VK_LEFT:    return editor.keyDown (Key::left, 0, modifiers);
                    case VK_RIGHT:   return editor.keyDown (Key::right, 0, modifiers);
                    case VK_HOME:    return editor.keyDown (Key::home, 0, modifiers);
                    case VK_END:     return editor.keyDown (Key::end, 0, modifiers);
                    case VK_RETURN:  return editor.keyDown (Key::enter, 0, modifiers);
                    case VK_ESCAPE:  return editor.keyDown (Key::escape, 0, modifiers);
                    case VK_TAB:     return editor.keyDown (Key::tab, 0, modifiers);

                    case VK_INSERT:  // Shift+Insert pastes, Ctrl+Insert copies
                        if (shift)   return editor.keyDown (Key::character, 'v', commandKey);
                        if (command) return editor.keyDown (Key::character, 'c', commandKey);
                        return false;

                    default:
                        // Ctrl and a letter: a shortcut (its WM_CHAR is a control character, left alone).
                        if (command && virtualKey >= 'A' && virtualKey <= 'Z')
                            return editor.keyDown (Key::character, (std::uint32_t) ('a' + (virtualKey - 'A')), modifiers);

                        return false;
                }
            }

            static LRESULT CALLBACK proc (HWND window, UINT message, WPARAM w, LPARAM l)
            {
                if (message == WM_NCCREATE)
                {
                    auto* created = reinterpret_cast<CREATESTRUCTW*> (l);
                    SetWindowLongPtrW (window, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (created->lpCreateParams));
                }

                auto* self = reinterpret_cast<Win32Window*> (GetWindowLongPtrW (window, GWLP_USERDATA));

                if (self == nullptr)
                    return DefWindowProcW (window, message, w, l);

                switch (message)
                {
                    case WM_ERASEBKGND:
                        return 1;

                    case WM_PAINT:
                    {
                        PAINTSTRUCT paint;
                        auto* dc = BeginPaint (window, &paint);
                        self->editor.render (self->pixels.data());

                        BITMAPINFO info {};
                        info.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
                        info.bmiHeader.biWidth = self->editor.getWidth();
                        info.bmiHeader.biHeight = -self->editor.getHeight();   // top row first
                        info.bmiHeader.biPlanes = 1;
                        info.bmiHeader.biBitCount = 32;
                        info.bmiHeader.biCompression = BI_RGB;

                        StretchDIBits (dc, 0, 0, self->editor.getWidth(), self->editor.getHeight(),
                                       0, 0, self->editor.getWidth(), self->editor.getHeight(),
                                       self->pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
                        EndPaint (window, &paint);
                        return 0;
                    }

                    case WM_TIMER:
                        InvalidateRect (window, nullptr, FALSE);
                        self->followKeys (false);
                        return 0;

                    case WM_LBUTTONDOWN:
                        SetCapture (window);
                        self->editor.mouseDown (pointerX (l), pointerY (l), fineKeys (w), false);
                        self->followKeys (true);
                        return 0;

                    case WM_LBUTTONDBLCLK:
                        self->editor.mouseDown (pointerX (l), pointerY (l), fineKeys (w), true);
                        self->followKeys (true);
                        return 0;

                    //==========================================================
                    // The keys, while Registration is open (otherwise the host's).
                    case WM_GETDLGCODE:
                        if (self->editor.wantsKeys())
                            return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS | DLGC_WANTTAB;
                        break;

                    case WM_KEYDOWN:
                        if (! self->editor.wantsKeys())
                            return passOn (window, message, w, l);

                        if (self->specialKey (w))
                        {
                            self->afterKeys();
                            return 0;
                        }

                        // A shortcut it doesn't have stays the host's; what types comes as WM_CHAR.
                        if ((keyModifiers() & commandKey) != 0)
                            return passOn (window, message, w, l);

                        return 0;

                    case WM_KEYUP:
                        if (! self->editor.wantsKeys())
                            return passOn (window, message, w, l);

                        return 0;

                    case WM_CHAR:
                    {
                        if (! self->editor.wantsKeys())
                            return passOn (window, message, w, l);

                        // UTF-16: a character beyond the first 65536 comes in two halves.
                        auto unit = (std::uint32_t) (w & 0xffff);

                        if (unit >= 0xd800 && unit < 0xdc00)
                        {
                            self->highSurrogate = unit;
                            return 0;
                        }

                        if (unit >= 0xdc00 && unit < 0xe000)
                        {
                            if (self->highSurrogate == 0)
                                return 0;

                            unit = 0x10000 + ((self->highSurrogate - 0xd800) << 10) + (unit - 0xdc00);
                        }

                        self->highSurrogate = 0;

                        if (unit >= 0x20 && unit != 0x7f)   // control characters came as WM_KEYDOWN
                        {
                            self->editor.keyDown (Key::character, unit, 0);
                            self->afterKeys();
                        }

                        return 0;
                    }

                    case WM_MOUSEMOVE:
                        if ((w & MK_LBUTTON) != 0)
                        {
                            self->editor.mouseDrag (pointerX (l), pointerY (l), fineKeys (w));
                        }
                        else
                        {
                            // Hovering: programmed elements hear it, and when it leaves.
                            if (! self->tracking)
                            {
                                TRACKMOUSEEVENT track {};
                                track.cbSize = sizeof (track);
                                track.dwFlags = TME_LEAVE;
                                track.hwndTrack = window;
                                self->tracking = TrackMouseEvent (&track) != 0;
                            }

                            self->editor.mouseMove (pointerX (l), pointerY (l));
                        }
                        return 0;

                    case WM_MOUSELEAVE:
                        self->tracking = false;
                        self->editor.mouseExit();
                        return 0;

                    case WM_LBUTTONUP:
                        ReleaseCapture();
                        self->editor.mouseUp();
                        return 0;

                    case WM_CAPTURECHANGED:
                        self->editor.mouseUp();
                        return 0;

                    case WM_MOUSEWHEEL:
                    {
                        POINT point { pointerX (l), pointerY (l) };
                        ScreenToClient (window, &point);
                        self->editor.mouseWheel (point.x, point.y, (float) GET_WHEEL_DELTA_WPARAM (w) / (float) WHEEL_DELTA);
                        return 0;
                    }

                    default:
                        break;
                }

                return DefWindowProcW (window, message, w, l);
            }

            Editor editor;
            std::vector<std::uint32_t> pixels;
            HWND hwnd = nullptr;
            HWND previousFocus = nullptr;   // who had the keys before Registration took them
            std::uint32_t highSurrogate = 0;
            bool tracking = false;          // asked to hear when the mouse leaves
        };
    }

    std::unique_ptr<Window> Window::create (Host& host)
    {
        return std::make_unique<Win32Window> (host);
    }
}

#endif
