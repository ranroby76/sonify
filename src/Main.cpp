// D:\workspace\Fanan_web_hub\src\Main.cpp

#include <juce_gui_extra/juce_gui_extra.h>

#include "BinaryData.h"
#include "MainComponent.h"
#include "GalleryBrowser.h"   // GallerySync: the KnobMan gallery's download, stopped here
#include "PluginScanner.h"
#include "StellaLookAndFeel.h"

//==============================================================================
class FananWebHubApplication final : public juce::JUCEApplication
{
public:
    FananWebHubApplication() = default;

    const juce::String getApplicationName() override       { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }

    // One instance only: most ASIO drivers can be opened by a single process at a time.
    // The copies that read plugins for the scanner are the exception: they open no audio.
    bool moreThanOneInstanceAllowed() override
    {
        return PluginScanner::isWorkerCommandLine (getCommandLineParameters());
    }

    void initialise (const juce::String& commandLine) override
    {
        // A copy started only to read one plugin for the scanner: it reads it, answers and goes,
        // with no window and no audio. If the plugin crashes, only this copy goes down.
        if (PluginScanner::isWorkerCommandLine (commandLine))
        {
            setApplicationReturnValue (PluginScanner::runWorker (getCommandLineParameterArray()));
            quit();
            return;
        }

        // Sonify's own windows, menus and dialogs wear the Plugins Creator Studio's look (Stella
        // AI Studio's, in Sonify's green): the studio needs it, and the rest matches it.
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;

        // The KnobMan gallery's download (when one was ever started; none is made here) is
        // stopped and joined while JUCE still stands, not in the static teardown after it.
        GallerySync::shutdownInstance();

        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        // Unsaved changes get the Save / Don't Save / Cancel question first.
        if (mainWindow != nullptr)
        {
            if (auto* content = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
            {
                content->whenSongIsSafe ([] { juce::JUCEApplicationBase::quit(); });
                return;
            }
        }

        quit();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront (true);
    }

    //==============================================================================
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff1b1d21), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (820, 560, 10000, 10000);
            centreWithSize (getWidth(), getHeight());   // the size Restore brings it back to

            // Opens maximised: with the native title bar this is Windows' own maximise (title
            // bar and taskbar stay), not a kiosk-style full screen.
            setFullScreen (true);
            setVisible (true);

            // Sonify's icon in the native title bar and the taskbar, straight from its PNG (Sonify.exe
            // carries it too, from CMakeLists.txt's ICON_BIG, which Windows shows as well).
            if (auto* peer = getPeer())
                peer->setIcon (juce::ImageCache::getFromMemory (BinaryData::sonify_icon_png, BinaryData::sonify_icon_pngSize));
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    StellaLookAndFeel lookAndFeel;   // outlives every window
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (FananWebHubApplication)
