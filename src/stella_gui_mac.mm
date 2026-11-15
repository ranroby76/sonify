// D:\workspace\Fanan_web_hub\src\stella_gui_mac.mm
//
// An exported plugin's window on macOS: an NSView inside the host's, showing the Editor's
// pixels and passing the mouse to it. Redrawn about 30 times a second while it's on screen, so
// meters, scopes and automation stay live. While Registration is open it becomes the first
// responder (a click on it) and takes the keys (Cmd+A/C/V/X too); the clipboard is the general
// pasteboard. An Audio Unit's host asks for it through a view factory (STELLA_AU). Its classes carry the plugin's own name (STELLA_OBJC_SUFFIX, set by
// Export), so two exported plugins loaded together never mix. Manual retain and release (no ARC).

#include "stella_gui.h"

#if defined (__APPLE__)

#import <Cocoa/Cocoa.h>

#if STELLA_AU
 #import <AudioToolbox/AudioToolbox.h>
 #import <AudioToolbox/AUCocoaUIView.h>
 #include "stella_plugin_info.h"
#endif

#include <string>
#include <vector>

#ifndef STELLA_OBJC_SUFFIX
 #define STELLA_OBJC_SUFFIX Default
#endif

#define STELLA_JOIN2(a, b) a##b
#define STELLA_JOIN(a, b) STELLA_JOIN2 (a, b)
#define STELLA_VIEW_CLASS STELLA_JOIN (StellaView_, STELLA_OBJC_SUFFIX)
#define STELLA_FACTORY_CLASS STELLA_JOIN (StellaViewFactory_, STELLA_OBJC_SUFFIX)

typedef void (*StellaViewGone) (void* context, void* view);

namespace
{
    /** The general pasteboard's text. */
    class MacClipboard final : public stella::gui::Clipboard
    {
    public:
        bool getText (std::string& text) override
        {
            text.clear();
            NSString* string = [[NSPasteboard generalPasteboard] stringForType: NSPasteboardTypeString];

            if (string != nil)
            {
                // A serial is short: what's past the first few thousand characters isn't one.
                if ([string length] > 4096)
                    string = [string substringToIndex: 4096];

                if (const char* utf8 = [string UTF8String])
                    text = utf8;
            }

            return true;
        }

        void setText (const std::string& text) override
        {
            NSString* string = [NSString stringWithUTF8String: text.c_str()];

            if (string == nil)
                return;

            NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
            [pasteboard clearContents];
            [pasteboard setString: string forType: NSPasteboardTypeString];
        }
    };

    NSString* nsString (const std::string& s)
    {
        NSString* string = [NSString stringWithUTF8String: s.c_str()];
        return string != nil ? string : @"";
    }

    /** The preset commands' folder and dialogs: Documents, the save and open panels, an alert. */
    class MacDesktop final : public stella::gui::Desktop
    {
    public:
        std::string documentsFolder() override
        {
            NSArray* paths = NSSearchPathForDirectoriesInDomains (NSDocumentDirectory, NSUserDomainMask, YES);

            if ([paths count] > 0)
                if (const char* path = [[paths objectAtIndex: 0] fileSystemRepresentation])
                    return path;

            return {};
        }

        bool chooseFile (bool saving, const std::string& title, const std::string& folder, const std::string& name,
                         const std::string& extension, std::string& chosen) override
        {
            NSURL* start = folder.empty() ? nil : [NSURL fileURLWithPath: nsString (folder) isDirectory: YES];
            NSArray* types = [NSArray arrayWithObject: nsString (extension)];
            NSURL* url = nil;

            if (saving)
            {
                NSSavePanel* panel = [NSSavePanel savePanel];
                [panel setTitle: nsString (title)];
                [panel setCanCreateDirectories: YES];
                [panel setAllowedFileTypes: types];
                [panel setNameFieldStringValue: nsString (name)];

                if (start != nil)
                    [panel setDirectoryURL: start];

                if ([panel runModal] == NSModalResponseOK)
                    url = [panel URL];
            }
            else
            {
                NSOpenPanel* panel = [NSOpenPanel openPanel];
                [panel setTitle: nsString (title)];
                [panel setCanChooseFiles: YES];
                [panel setCanChooseDirectories: NO];
                [panel setAllowsMultipleSelection: NO];
                [panel setAllowedFileTypes: types];

                if (start != nil)
                    [panel setDirectoryURL: start];

                if ([panel runModal] == NSModalResponseOK && [[panel URLs] count] > 0)
                    url = [[panel URLs] objectAtIndex: 0];
            }

            if (url == nil)
                return false;

            const char* path = [[url path] fileSystemRepresentation];
            chosen = path != nullptr ? path : "";
            return ! chosen.empty();
        }

        bool confirm (const std::string& question) override
        {
            const bool asking = question.find ('?') != std::string::npos;
            NSAlert* alert = [[[NSAlert alloc] init] autorelease];
            [alert setMessageText: nsString (question)];
            [alert addButtonWithTitle: asking ? @"Yes" : @"OK"];

            if (asking)
                [alert addButtonWithTitle: @"No"];

            return [alert runModal] == NSAlertFirstButtonReturn;
        }

        void showFolder (const std::string& path) override
        {
            [[NSWorkspace sharedWorkspace] openURL: [NSURL fileURLWithPath: nsString (path) isDirectory: YES]];
        }
    };
}

__attribute__ ((visibility ("default")))
@interface STELLA_VIEW_CLASS : NSView
{
    stella::gui::Editor* editor;
    stella::gui::Clipboard* clipboard;
    stella::gui::Desktop* desktop;
    BOOL inPresetCommand;   // a preset command's panel is up: the editor is deleted after it
    BOOL releaseAfter;
    std::vector<std::uint32_t>* pixels;
    NSTimer* timer;
    NSTrackingArea* tracking;
    StellaViewGone gone;
    void* goneContext;
    std::function<void (int, int)> resized;   // its size menu chose a size: the host's told (a VST3's, a CLAP's)
}

- (instancetype) initWithHost: (stella::gui::Host*) host gone: (StellaViewGone) whenGone context: (void*) context;
- (void) releaseHost;
- (BOOL) wantsKeys;
- (BOOL) hostKey: (int) key character: (std::uint32_t) character modifiers: (int) modifiers;
- (void) fitToEditor;
- (void) sizeChosen;
- (void) setResized: (std::function<void (int, int)>) callback;
- (void) takeSize: (int) w height: (int) h;
- (NSSize) editorSize;
@end

@implementation STELLA_VIEW_CLASS

- (instancetype) initWithHost: (stella::gui::Host*) host gone: (StellaViewGone) whenGone context: (void*) context
{
    const auto scale = stella::gui::snapScale (host->windowScale());
    self = [super initWithFrame: NSMakeRect (0, 0, stella::gui::scaledWidth (scale), stella::gui::scaledHeight (scale))];

    if (self != nil)
    {
        editor = new stella::gui::Editor (*host);
        clipboard = new MacClipboard();
        editor->setClipboard (clipboard);
        desktop = new MacDesktop();
        editor->setDesktop (desktop);
        inPresetCommand = NO;
        releaseAfter = NO;
        pixels = new std::vector<std::uint32_t> ((size_t) editor->getWidth() * (size_t) editor->getHeight(), 0u);

        // Its size menu chose another size: its pixels and itself that size (an Audio Unit's
        // host sees the view's frame change), and the host told.
        STELLA_VIEW_CLASS* me = self;
        editor->onScaleChosen = [me] { [me sizeChosen]; };
        timer = nil;
        tracking = nil;
        gone = whenGone;
        goneContext = context;
    }

    return self;
}

- (void) stopTimer
{
    if (timer != nil)
    {
        [timer invalidate];
        timer = nil;   // the run loop held it
    }
}

- (void) releaseHost
{
    [self stopTimer];

    if (inPresetCommand)
    {
        releaseAfter = YES;   // in the middle of a preset command: deleted once it's done
        gone = nullptr;
        return;
    }

    delete editor;
    editor = nullptr;
    gone = nullptr;
}

- (void) dealloc
{
    [self stopTimer];

    if (tracking != nil)
        [tracking release];

    delete editor;
    delete clipboard;
    delete desktop;
    delete pixels;

    if (gone != nullptr)
        gone (goneContext, (void*) self);

    [super dealloc];
}

- (NSSize) editorSize
{
    return editor != nullptr ? NSMakeSize ((CGFloat) editor->getWidth(), (CGFloat) editor->getHeight())
                             : NSMakeSize ((CGFloat) stella::gui::width, (CGFloat) stella::gui::height);
}

/** Its pixels, and itself, the Editor's size. */
- (void) fitToEditor
{
    if (editor == nullptr)
        return;

    pixels->assign ((size_t) editor->getWidth() * (size_t) editor->getHeight(), 0u);
    [self setFrameSize: [self editorSize]];
    [self setNeedsDisplay: YES];
}

/** Its size menu chose a size: itself that size, and the host told. */
- (void) sizeChosen
{
    [self fitToEditor];
    const auto size = [self editorSize];

    if (resized)
        resized ((int) size.width, (int) size.height);
}

- (void) setResized: (std::function<void (int, int)>) callback
{
    resized = std::move (callback);
}

/** The host gives it a size: the step nearest it. */
- (void) takeSize: (int) w height: (int) h
{
    if (editor == nullptr)
        return;

    const auto scale = stella::gui::scaleForSize (w, h);

    if (scale == editor->getScale())
        return;

    editor->setScale (scale);
    editor->getHost().setWindowScale (scale);
    [self fitToEditor];
}

- (BOOL) isFlipped                              { return YES; }
- (BOOL) isOpaque                               { return YES; }
- (BOOL) acceptsFirstMouse: (NSEvent*) event    { (void) event; return YES; }

// The keys: only while Registration is open (the rest of the time they're the host's).
- (BOOL) acceptsFirstResponder                  { return editor != nullptr && editor->wantsKeys(); }
- (BOOL) becomeFirstResponder                   { return YES; }
- (BOOL) resignFirstResponder                   { return YES; }

- (BOOL) wantsKeys
{
    return editor != nullptr && editor->wantsKeys();
}

/** Takes the keys while Registration is open (mayTake: from a click), and gives them back
    when it closes. */
- (void) followKeys: (BOOL) mayTake
{
    NSWindow* window = [self window];

    if (window == nil)
        return;

    const bool wants = editor != nullptr && editor->wantsKeys();

    if (wants && mayTake && [window firstResponder] != self)
        [window makeFirstResponder: self];
    else if (! wants && [window firstResponder] == self)
        [window makeFirstResponder: nil];
}

- (void) tick: (NSTimer*) sender
{
    (void) sender;

    // A preset command a click asked for (its panel or alert runs its own loop): kept alive
    // till it's done, should the host close the view meanwhile.
    if (editor != nullptr && ! inPresetCommand)
    {
        [self retain];
        inPresetCommand = YES;
        editor->runQueuedPresetCommand();
        inPresetCommand = NO;

        if (releaseAfter)
        {
            delete editor;
            editor = nullptr;
            releaseAfter = NO;
        }

        [self autorelease];

        if (editor == nullptr)
            return;
    }

    [self setNeedsDisplay: YES];
    [self followKeys: NO];
}

- (void) viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self stopTimer];

    // While it's in a window: redrawn about 30 times a second (in every run loop mode, so it
    // stays live while a menu is open or the host drags something).
    if ([self window] != nil && editor != nullptr)
    {
        timer = [NSTimer timerWithTimeInterval: 1.0 / 30.0 target: self selector: @selector (tick:) userInfo: nil repeats: YES];
        [[NSRunLoop currentRunLoop] addTimer: timer forMode: NSRunLoopCommonModes];
    }
}

- (void) updateTrackingAreas
{
    if (tracking != nil)
    {
        [self removeTrackingArea: tracking];
        [tracking release];
    }

    tracking = [[NSTrackingArea alloc] initWithRect: NSZeroRect
                                            options: NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited
                                                   | NSTrackingActiveAlways | NSTrackingInVisibleRect
                                              owner: self
                                           userInfo: nil];
    [self addTrackingArea: tracking];
    [super updateTrackingAreas];
}

- (void) drawRect: (NSRect) dirty
{
    (void) dirty;

    if (editor == nullptr)
    {
        [[NSColor blackColor] setFill];
        NSRectFill ([self bounds]);
        return;
    }

    const auto w = (size_t) editor->getWidth(), h = (size_t) editor->getHeight();

    if (pixels->size() != w * h)
        pixels->assign (w * h, 0u);

    editor->render (pixels->data());
    auto* space = CGColorSpaceCreateWithName (kCGColorSpaceSRGB);
    auto* provider = CGDataProviderCreateWithData (nullptr, pixels->data(), w * h * 4, nullptr);
    auto* image = CGImageCreate (w, h, 8, 32, w * 4, space, (CGBitmapInfo) kCGBitmapByteOrder32Little | (CGBitmapInfo) kCGImageAlphaPremultipliedFirst,
                                 provider, nullptr, false, kCGRenderingIntentDefault);

    if (image != nullptr)
    {
        auto* context = [[NSGraphicsContext currentContext] CGContext];

        // The view is flipped (top-left first); images are drawn the right way up by flipping back.
        CGContextSaveGState (context);
        CGContextTranslateCTM (context, 0, (CGFloat) h);
        CGContextScaleCTM (context, 1, -1);
        CGContextSetInterpolationQuality (context, kCGInterpolationNone);
        CGContextDrawImage (context, CGRectMake (0, 0, (CGFloat) w, (CGFloat) h), image);
        CGContextRestoreGState (context);
        CGImageRelease (image);
    }

    CGDataProviderRelease (provider);
    CGColorSpaceRelease (space);
}

//==============================================================================
- (NSPoint) pointOf: (NSEvent*) event
{
    return [self convertPoint: [event locationInWindow] fromView: nil];
}

static bool isFine (NSEvent* event)
{
    return ([event modifierFlags] & (NSEventModifierFlagShift | NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0;
}

- (void) mouseDown: (NSEvent*) event
{
    if (editor == nullptr)
        return;

    const auto p = [self pointOf: event];
    editor->mouseDown ((int) p.x, (int) p.y, isFine (event), [event clickCount] == 2);
    [self setNeedsDisplay: YES];
    [self followKeys: YES];
}

/** A right-click: the size menu. */
- (void) rightMouseUp: (NSEvent*) event
{
    if (editor == nullptr)
        return;

    const auto p = [self pointOf: event];
    editor->contextClick ((int) p.x, (int) p.y);
    [self setNeedsDisplay: YES];
}

- (void) rightMouseDown: (NSEvent*) event
{
    (void) event;   // (its release opens the menu; the host doesn't get it)
}

- (void) mouseDragged: (NSEvent*) event
{
    if (editor == nullptr)
        return;

    const auto p = [self pointOf: event];
    editor->mouseDrag ((int) p.x, (int) p.y, isFine (event));
    [self setNeedsDisplay: YES];
}

- (void) mouseUp: (NSEvent*) event
{
    (void) event;

    if (editor == nullptr)
        return;

    editor->mouseUp();
    [self setNeedsDisplay: YES];
}

- (void) mouseMoved: (NSEvent*) event
{
    if (editor == nullptr)
        return;

    const auto p = [self pointOf: event];
    editor->mouseMove ((int) p.x, (int) p.y);
}

- (void) mouseExited: (NSEvent*) event
{
    (void) event;

    if (editor != nullptr)
        editor->mouseExit();
}

- (void) scrollWheel: (NSEvent*) event
{
    if (editor == nullptr)
        return;

    const auto p = [self pointOf: event];
    const auto delta = [event hasPreciseScrollingDeltas] ? [event scrollingDeltaY] / 40.0 : [event scrollingDeltaY];
    editor->mouseWheel ((int) p.x, (int) p.y, (float) delta);
    [self setNeedsDisplay: YES];
}

//==============================================================================
/** A key for Registration: true if it took it. */
- (BOOL) takeKey: (NSEvent*) event
{
    using stella::gui::Key;

    if (editor == nullptr || ! editor->wantsKeys())
        return NO;

    const auto flags = [event modifierFlags];
    const int modifiers = ((flags & NSEventModifierFlagShift) != 0 ? stella::gui::shiftKey : 0)
                        | ((flags & NSEventModifierFlagCommand) != 0 ? stella::gui::commandKey : 0);
    bool taken = false;

    switch ([event keyCode])
    {
        case 51:            taken = editor->keyDown (Key::backspace, 0, modifiers); break;
        case 117:           taken = editor->keyDown (Key::forwardDelete, 0, modifiers); break;
        case 123:           taken = editor->keyDown (Key::left, 0, modifiers); break;
        case 124:           taken = editor->keyDown (Key::right, 0, modifiers); break;
        case 115:           taken = editor->keyDown (Key::home, 0, modifiers); break;
        case 119:           taken = editor->keyDown (Key::end, 0, modifiers); break;
        case 36: case 76:   taken = editor->keyDown (Key::enter, 0, modifiers); break;
        case 53:            taken = editor->keyDown (Key::escape, 0, modifiers); break;
        case 48:            taken = editor->keyDown (Key::tab, 0, modifiers); break;

        default:
            if ((modifiers & stella::gui::commandKey) != 0)
            {
                // Cmd and a letter: a shortcut, by the letter on the key.
                NSString* plain = [event charactersIgnoringModifiers];

                if ([plain length] == 1)
                    taken = editor->keyDown (Key::character, (std::uint32_t) [plain characterAtIndex: 0], modifiers);
            }
            else if ((flags & NSEventModifierFlagControl) == 0)
            {
                NSString* typed = [event characters];

                for (NSUInteger i = 0; i < [typed length]; ++i)
                {
                    const auto c = (std::uint32_t) [typed characterAtIndex: i];

                    // Not control characters, nor the function keys' private codes, nor half characters.
                    if (c >= 0x20 && c != 0x7f && ! (c >= 0xf700 && c <= 0xf8ff) && ! (c >= 0xd800 && c <= 0xdfff))
                        taken = editor->keyDown (Key::character, c, 0) || taken;
                }
            }

            break;
    }

    if (taken)
    {
        [self setNeedsDisplay: YES];
        [self followKeys: NO];
    }

    return taken ? YES : NO;
}

- (void) keyDown: (NSEvent*) event
{
    // What Registration doesn't take goes on to the host.
    if (! [self takeKey: event])
        [super keyDown: event];
}

- (void) keyUp: (NSEvent*) event
{
    if (! [self wantsKeys])
        [super keyUp: event];
}

/** Cmd shortcuts come here before the host's menus do. */
- (BOOL) performKeyEquivalent: (NSEvent*) event
{
    if ([[self window] firstResponder] == self && ([event modifierFlags] & NSEventModifierFlagCommand) != 0 && [self takeKey: event])
        return YES;

    return [super performKeyEquivalent: event];
}

/** A key the host handed the plugin's view (some hosts keep the keyboard). */
- (BOOL) hostKey: (int) key character: (std::uint32_t) character modifiers: (int) modifiers
{
    if (editor == nullptr || ! editor->keyDown ((stella::gui::Key) key, character, modifiers, true))
        return NO;

    [self setNeedsDisplay: YES];
    [self followKeys: NO];
    return YES;
}

@end

//==============================================================================
#if STELLA_AU
/** The Audio Unit's view factory: the host finds it by name in the bundle (kAudioUnitProperty_CocoaUI). */
__attribute__ ((visibility ("default")))
@interface STELLA_FACTORY_CLASS : NSObject <AUCocoaUIBase>
@end

@implementation STELLA_FACTORY_CLASS

- (unsigned) interfaceVersion
{
    return 0;
}

- (NSString*) description
{
    return [NSString stringWithUTF8String: stella::info::name];
}

- (NSView*) uiViewForAudioUnit: (AudioUnit) unit withSize: (NSSize) size
{
    (void) size;
    stella::gui::MacViewLink* link = nullptr;
    UInt32 linkSize = sizeof (link);

    if (AudioUnitGetProperty (unit, 64917, kAudioUnitScope_Global, 0, &link, &linkSize) != 0 || link == nullptr || link->host == nullptr)
        return nil;

    auto* view = stella::gui::createMacView (*link->host, link->gone, link->context);

    if (link->opened != nullptr)
        link->opened (link->context, view);

    return [(NSView*) view autorelease];
}

@end
#endif

//==============================================================================
namespace stella::gui
{
    void* createMacView (Host& host, void (*whenGone) (void* context, void* view), void* context)
    {
        return (void*) [[STELLA_VIEW_CLASS alloc] initWithHost: &host gone: whenGone context: context];
    }

    void releaseMacViewHost (void* view)
    {
        [(STELLA_VIEW_CLASS*) view releaseHost];
    }

    namespace
    {
        class MacWindow final : public Window
        {
        public:
            explicit MacWindow (Host& host)
                : view ((NSView*) createMacView (host, nullptr, nullptr))
            {
                [(STELLA_VIEW_CLASS*) view setResized: [this] (int w, int h)
                                                       {
                                                           if (onResizeRequest)
                                                               onResizeRequest (w, h);
                                                       }];
            }

            ~MacWindow() override
            {
                [(STELLA_VIEW_CLASS*) view setResized: nullptr];
                [view removeFromSuperview];
                [view release];
            }

            bool attach (void* parent) override
            {
                if (parent == nullptr)
                    return false;

                [(NSView*) parent addSubview: view];
                return true;
            }

            void setVisible (bool visible) override
            {
                [view setHidden: ! visible];
            }

            bool wantsKeys() override
            {
                return [(STELLA_VIEW_CLASS*) view wantsKeys] == YES;
            }

            void getSize (int& w, int& h) override
            {
                const auto size = [(STELLA_VIEW_CLASS*) view editorSize];
                w = (int) size.width;
                h = (int) size.height;
            }

            void setSize (int w, int h) override
            {
                [(STELLA_VIEW_CLASS*) view takeSize: w height: h];
            }

            bool keyDown (Key key, std::uint32_t character, int modifiers) override
            {
                return [(STELLA_VIEW_CLASS*) view hostKey: (int) key character: character modifiers: modifiers] == YES;
            }

        private:
            NSView* view;
        };
    }

    std::unique_ptr<Window> Window::create (Host& host)
    {
        return std::make_unique<MacWindow> (host);
    }
}

#endif
