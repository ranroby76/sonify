// D:\workspace\Fanan_web_hub\src\KnobStudio.cpp

#include "KnobStudio.h"
#include "KnobFile.h"
#include "Preferences.h"
#include "Settings.h"
#include "StellaLookAndFeel.h"

namespace
{
    constexpr int listWidth = 240, headerHeight = 54, statusHeight = 26;

    // The New look menu's own entries, after the looks to start from.
    constexpr int emptyId = 10000, galleryId = 10001, knobFileId = 10002, lookFileId = 10003, knobFolderId = 10004;

    /** A look's starting point when the user asks for an empty one. */
    LayerDoc emptyLook (Looks::Kind kind)
    {
        auto doc = LayerDoc::emptyDocument();

        switch (kind)
        {
            case Looks::Kind::knob:          doc.canvasWidth = doc.canvasHeight = 128; doc.frames = 101; break;
            case Looks::Kind::slider:        doc.canvasWidth = 40;  doc.canvasHeight = 160; doc.frames = 101; break;
            case Looks::Kind::sliderAcross:  doc.canvasWidth = 160; doc.canvasHeight = 40;  doc.frames = 101; break;
            case Looks::Kind::button:        doc.canvasWidth = doc.canvasHeight = 64;  doc.frames = 2; break;
        }

        return doc;
    }

    juce::String withoutExtension (const juce::String& name)
    {
        const auto trimmed = name.trim();
        return trimmed.containsChar ('.') ? trimmed.upToLastOccurrenceOf (".", false, false) : trimmed;
    }

    /** KnobMaker's knob, kept in its library while it's worked on (never shown by this name). */
    const juce::String workbenchLookName { "KnobMaker knob" };

    /** KnobMaker's own settings: in Sonify's main settings folder (in AppData), with everything else. */
    juce::File knobMakerSettings (const juce::String& name)
    {
        const auto folder = SettingsFolder::get().getChildFile ("KnobMaker");
        folder.createDirectory();
        const auto file = folder.getChildFile (name);

        // Kept in Documents\Sonify\Plugins Creator\KnobMaker before: brought over once.
        if (const auto old = Settings::getDataFolder().getChildFile ("KnobMaker").getChildFile (name); ! file.existsAsFile() && old.existsAsFile())
            old.copyFileTo (file);

        return file;
    }

    /** Its name (its file's), kept for next time. */
    juce::File knobTitleFile()   { return knobMakerSettings ("knob name.txt"); }

    /** What a KnobMan knob is, by its shape and frames. */
    Looks::Kind kindOfDoc (const LayerDoc& doc)
    {
        return doc.frames == 2                               ? Looks::Kind::button
             : doc.canvasHeight >= doc.canvasWidth * 2       ? Looks::Kind::slider
             : doc.canvasWidth >= doc.canvasHeight * 2       ? Looks::Kind::sliderAcross
                                                             : Looks::Kind::knob;
    }

    constexpr const char* galleryDragPrefix = "sonify-gallery-item:";

    /** A look dragged out of the Knob Studio's list: saved first as a Sonify knob file (in the
        temp folder), so the Gallery takes it like a file dropped from the computer. */
    constexpr const char* knobFileDragPrefix = "sonify-knob-file:";

    juce::File knobFileOf (const juce::var& description)
    {
        const auto text = description.toString();

        if (! text.startsWith (knobFileDragPrefix))
            return {};

        const juce::File file (text.fromFirstOccurrenceOf (knobFileDragPrefix, false, false));
        return file.existsAsFile() ? file : juce::File();
    }

    //==========================================================================
    /** KnobMaker's Gallery: every knob in a folder (KnobMan's .knob and Sonify's .fklayers, in
        its folders too) as a picture with its name; double-click one (or Open) to open it. The
        folder is remembered; at first it's Sonify's gallery (in AppData, the knobs installed with
        Sonify copied into it, so knobs can be added to it). Knob files dropped on it from the
        computer are copied into the folder shown, and so is a look dragged in from the Knob
        Studio's list. Its items can also be dragged out (dragOut): onto Edit UI's window, or onto
        the Knob Studio to replace the look being designed. */
    class KnobGallery final : public juce::Component,
                              public juce::DragAndDropContainer,
                              public juce::DragAndDropTarget,
                              public juce::FileDragAndDropTarget,
                              private juce::Timer
    {
    public:
        explicit KnobGallery (std::function<void (const juce::File&)> open, bool closeWhenOpened = true, bool canDragOut = false,
                              const juce::String& pickText = {})
            : onOpen (std::move (open)), closeOnOpen (closeWhenOpened), dragOut (canDragOut)
        {
            if (pickText.isNotEmpty())
                openButton.setButtonText (pickText);
            else if (dragOut)
                openButton.setButtonText ("Add");

            folderButton.onClick = [this] { chooseFolder(); };
            sonifyButton.setTooltip ("Sonify's gallery: the knobs installed with Sonify, and the ones added to it");
            sonifyButton.onClick = [this]
            {
                memoryFile().deleteFile();   // back to the gallery from now on
                setFolder (sonifyGallery());
            };
            openButton.onClick = [this] { openSelected(); };
            exportButton.setTooltip ("Every KnobMan knob (.knob) in this folder saved as a Sonify knob (.fklayers) into a folder you choose");
            exportButton.onClick = [this] { chooseExportFolder(); };
            search.setTextToShowWhenEmpty ("Search by name", Theme::muted);
            search.onTextChange = [this] { filter(); };

            for (auto* c : std::initializer_list<juce::Component*> { &sonifyButton, &folderButton, &exportButton, &openButton, &search, &folderLabel, &viewport })
                addAndMakeVisible (c);

            folderLabel.setColour (juce::Label::textColourId, Theme::muted);
            folderLabel.setFont (Theme::font (12.5f));
            viewport.setViewedComponent (&grid, false);
            viewport.setScrollBarsShown (true, false);
            grid.owner = this;
            exporter.owner = this;

            setFolder (rememberedFolder());
            setSize (1060, 680);
        }

        ~KnobGallery() override   { stopTimer(); exporter.stopTimer(); }

        /** Sonify's gallery folder (made, and the installed knobs copied in, when asked for). */
        static juce::File galleryFolder()   { return sonifyGallery(); }

        /** The folder shown, read again (a knob saved into it from elsewhere shows). */
        void refresh()   { setFolder (folder); }

        void resized() override
        {
            auto area = getLocalBounds().reduced (12);
            auto bar = area.removeFromTop (30);
            sonifyButton.setBounds (bar.removeFromLeft (130));
            bar.removeFromLeft (8);
            folderButton.setBounds (bar.removeFromLeft (150));
            bar.removeFromLeft (8);
            exportButton.setBounds (bar.removeFromLeft (300));
            bar.removeFromLeft (8);
            openButton.setBounds (bar.removeFromRight (90));
            bar.removeFromRight (8);
            search.setBounds (bar.removeFromRight (200));
            bar.removeFromRight (8);
            folderLabel.setBounds (bar);
            area.removeFromTop (8);
            viewport.setBounds (area);
            layoutGrid();
        }

        void paint (juce::Graphics& g) override   { g.fillAll (Theme::panel); }

        void paintOverChildren (juce::Graphics& g) override
        {
            if (! dropHover)
                return;

            g.setColour (Theme::accent.withAlpha (0.12f));
            g.fillRect (viewport.getBounds());
            g.setColour (Theme::accent);
            g.drawRect (viewport.getBounds(), 2);
            g.setFont (Theme::font (16.0f, true));
            g.drawText ("Drop to add to this gallery", viewport.getBounds(), juce::Justification::centred);
        }

        //======================================================================
        // Knob files dropped from the computer: copied into the folder shown (Sonify's gallery
        // when that one can't take them), a name already there kept by numbering the new one.
        static bool isKnobFile (const juce::String& path)
        {
            const juce::File f (path);
            return f.existsAsFile() && (f.hasFileExtension ("knob") || f.hasFileExtension (Looks::fileExtension));
        }

        bool isInterestedInFileDrag (const juce::StringArray& files) override
        {
            return std::any_of (files.begin(), files.end(), [] (const juce::String& f) { return isKnobFile (f); });
        }

        void fileDragEnter (const juce::StringArray&, int, int) override   { dropHover = true;  repaint(); }
        void fileDragExit (const juce::StringArray&) override              { dropHover = false; repaint(); }

        void filesDropped (const juce::StringArray& files, int, int) override
        {
            dropHover = false;
            addFiles (files);
        }

        // A look dragged in from the Knob Studio's list (saved as a knob file as it was picked up).
        bool isInterestedInDragSource (const SourceDetails& details) override   { return knobFileOf (details.description) != juce::File(); }
        void itemDragEnter (const SourceDetails&) override                      { dropHover = true;  repaint(); }
        void itemDragExit (const SourceDetails&) override                       { dropHover = false; repaint(); }

        void itemDropped (const SourceDetails& details) override
        {
            dropHover = false;

            if (const auto file = knobFileOf (details.description); file != juce::File())
                addFiles ({ file.getFullPathName() });
        }

        /** Knob files copied into the folder shown (Sonify's gallery when that one can't take
            them), a name already there kept by numbering the new one. */
        void addFiles (const juce::StringArray& files)
        {
            auto into = folder;

            if (! into.isDirectory() || ! into.hasWriteAccess())
                into = sonifyGallery();

            int added = 0, failed = 0;

            for (const auto& path : files)
            {
                if (! isKnobFile (path))
                    continue;

                const juce::File source (path);

                if (source.isAChildOf (into))
                    continue;   // already in it

                const auto target = into.getNonexistentChildFile (source.getFileNameWithoutExtension(), source.getFileExtension(), false);
                (source.copyFileTo (target) ? added : failed)++;
            }

            setFolder (into);

            if (failed > 0)
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Gallery",
                                                        juce::String (failed) + (failed == 1 ? " knob" : " knobs") + " couldn't be copied into "
                                                            + into.getFullPathName() + ".");
            else if (added == 0)
                repaint();
        }

    private:
        struct Item
        {
            juce::File file;
            juce::String name;
            juce::String about;   // what its file says about it
            bool commercialSafe = true;   // ...whether that licence allows selling
            juce::Image picture;
            bool tried = false;
        };

        struct Grid final : public juce::Component
        {
            KnobGallery* owner = nullptr;

            void paint (juce::Graphics& g) override
            {
                g.fillAll (Theme::inset);

                if (owner->shown.empty())
                {
                    g.setColour (Theme::muted);
                    g.setFont (Theme::font (14.0f));
                    g.drawFittedText (owner->items.empty() ? "No knobs here (.knob or .fklayers). Drop knob files here, or choose the folder your knobs are in."
                                                           : "No knob has that name.",
                                      getLocalBounds().reduced (20).withHeight (60), juce::Justification::centred, 2);
                    return;
                }

                const auto clip = g.getClipBounds();

                for (size_t i = 0; i < owner->shown.size(); ++i)
                {
                    const auto r = owner->tileBounds ((int) i);

                    if (! r.intersects (clip))
                        continue;

                    const auto& item = owner->items[(size_t) owner->shown[i]];
                    const auto chosen = (int) i == owner->selected;
                    g.setColour (chosen ? Theme::accent.withAlpha (0.25f) : Theme::raised);
                    g.fillRoundedRectangle (r.toFloat(), 6.0f);
                    g.setColour (chosen ? Theme::accent : Theme::outline);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, chosen ? 2.0f : 1.0f);

                    const auto pictureArea = r.reduced (10).withTrimmedBottom (22);

                    if (item.picture.isValid())
                    {
                        // Fitted, and a small picture (KnobMan's own are) up to twice its size.
                        const auto w = (float) item.picture.getWidth(), h = (float) item.picture.getHeight();
                        const auto scale = juce::jmin (2.0f, (float) pictureArea.getWidth() / w, (float) pictureArea.getHeight() / h);
                        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                        g.drawImage (item.picture, juce::Rectangle<float> (w * scale, h * scale).withCentre (pictureArea.toFloat().getCentre()));
                    }
                    else
                    {
                        g.setColour (Theme::muted);
                        g.setFont (Theme::font (11.0f));
                        g.drawText (item.tried ? "no picture" : juce::String::fromUTF8 ("\xe2\x80\xa6"), pictureArea, juce::Justification::centred);
                    }

                    g.setColour (Theme::text);
                    g.setFont (Theme::font (12.0f));
                    g.drawText (item.name, r.withTop (r.getBottom() - 24).reduced (6, 0), juce::Justification::centred, true);

                    // A licence that doesn't allow selling: marked, so it's seen before it's used.
                    if (! item.commercialSafe)
                    {
                        const auto badge = r.reduced (6).removeFromTop (16).removeFromRight (30);
                        g.setColour (juce::Colour (0xffb3362c));
                        g.fillRoundedRectangle (badge.toFloat(), 3.0f);
                        g.setColour (juce::Colours::white);
                        g.setFont (Theme::font (10.0f, true));
                        g.drawText ("NC", badge, juce::Justification::centred);
                    }
                }
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                owner->selected = owner->tileAt (e.getPosition());
                owner->openButton.setEnabled (owner->selected >= 0);
                dragging = false;
                repaint();
            }

            // In Edit UI: an item dragged out, onto the plugin's window (into another window too).
            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (! owner->dragOut || dragging || e.getDistanceFromDragStart() < 6
                    || ! juce::isPositiveAndBelow (owner->selected, (int) owner->shown.size()))
                    return;

                dragging = true;
                const auto& item = owner->items[(size_t) owner->shown[(size_t) owner->selected]];
                auto image = item.picture.isValid() ? item.picture : juce::Image (juce::Image::ARGB, 64, 64, true);
                const auto side = (float) juce::jmax (image.getWidth(), image.getHeight());
                image = image.rescaled (juce::jmax (1, juce::roundToInt ((float) image.getWidth() * 72.0f / side)),
                                        juce::jmax (1, juce::roundToInt ((float) image.getHeight() * 72.0f / side)),
                                        juce::Graphics::highResamplingQuality);
                const juce::Point<int> offset (-image.getWidth() / 2, -image.getHeight() / 2);
                owner->startDragging (KnobStudio::galleryDrag (item.file), this, juce::ScaledImage (image), true, &offset);
            }

            bool dragging = false;

            void mouseDoubleClick (const juce::MouseEvent& e) override
            {
                owner->selected = owner->tileAt (e.getPosition());
                owner->openSelected();
            }
        };

        static juce::File memoryFile()
        {
            return knobMakerSettings ("gallery folder.txt");
        }

        /** The knobs installed with Sonify: "knob_gallery" next to Sonify (on a Mac, in the app's
            Resources). Read only (Program Files): they're copied into Sonify's gallery. */
        static juce::File installedKnobs()
        {
            const auto exeFolder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
           #if JUCE_MAC
            return exeFolder.getSiblingFile ("Resources").getChildFile ("knob_gallery");
           #else
            return exeFolder.getChildFile ("knob_gallery");
           #endif
        }

        /** Sonify's gallery, in AppData (Fanan\Sonify\Knob Gallery), where knobs can be added.
            Each knob installed with Sonify is copied into it once: a new Sonify's new knobs come
            in, and one deleted from the gallery or changed there stays so (the ones copied before
            are listed in KnobMaker's "gallery installed.txt"). */
        static juce::File sonifyGallery()
        {
            const auto gallery = SettingsFolder::folder ("Knob Gallery");
            const auto listFile = knobMakerSettings ("gallery installed.txt");
            juce::StringArray copied;
            copied.addLines (listFile.loadFileAsString());
            copied.removeEmptyStrings();
            const auto before = copied.size();

            if (const auto from = installedKnobs(); from.isDirectory())
            {
                for (const auto& file : from.findChildFiles (juce::File::findFiles, true, "*.knob;*" + juce::String (Looks::fileExtension)))
                {
                    const auto name = file.getRelativePathFrom (from).replaceCharacter ('\\', '/');

                    if (copied.contains (name))
                        continue;

                    const auto target = gallery.getChildFile (name);

                    if (target.existsAsFile() || (target.getParentDirectory().createDirectory().wasOk() && file.copyFileTo (target)))
                        copied.add (name);
                }
            }

            if (copied.size() != before)
                listFile.replaceWithText (copied.joinIntoString ("\n"));

            return gallery;
        }

        /** The folder chosen last; before one is chosen (or when it's gone), Sonify's gallery. */
        static juce::File rememberedFolder()
        {
            if (const juce::File saved (memoryFile().loadFileAsString().trim()); saved.isDirectory())
                return saved;

            return sonifyGallery();
        }

        void chooseFolder()
        {
            chooser = std::make_unique<juce::FileChooser> ("The folder your knobs are in", folder);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [safeThis = juce::Component::SafePointer<KnobGallery> (this)] (const juce::FileChooser& fc)
                                  {
                                      if (safeThis == nullptr || ! fc.getResult().isDirectory())
                                          return;

                                      memoryFile().replaceWithText (fc.getResult().getFullPathName());
                                      safeThis->setFolder (fc.getResult());
                                  });
        }

        void setFolder (const juce::File& newFolder)
        {
            folder = newFolder;
            items.clear();
            selected = -1;

            if (folder.isDirectory())
            {
                for (const auto& file : folder.findChildFiles (juce::File::findFiles, true, "*.knob;*" + juce::String (Looks::fileExtension)))
                {
                    if (items.size() >= 5000)
                        break;

                    Item item;
                    item.file = file;
                    item.name = file.getFileNameWithoutExtension();

                    items.push_back (std::move (item));
                }
            }

            std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.name.compareNatural (b.name) < 0; });
            folderLabel.setText (folder.isDirectory() ? folder.getFullPathName() + "  (" + juce::String ((int) items.size()) + " knobs)"
                                                      : juce::String ("No folder yet: choose the one your knobs are in."),
                                 juce::dontSendNotification);
            next = 0;
            filter();
            exportButton.setEnabled (std::any_of (items.begin(), items.end(), [] (const Item& i) { return i.file.hasFileExtension ("knob"); }));
            startTimer (30);   // the pictures, a few at a time
        }

        //======================================================================
        // Export all KnobMan knobs as Sonify knobs: each .knob here into a .fklayers there, named
        // as the gallery shows it (its gallery author and licence kept in its description).
        void chooseExportFolder()
        {
            chooser = std::make_unique<juce::FileChooser> ("The folder to save the Sonify knobs in", juce::File());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [safeThis = juce::Component::SafePointer<KnobGallery> (this)] (const juce::FileChooser& fc)
                                  {
                                      if (safeThis != nullptr && fc.getResult().isDirectory())
                                          safeThis->startExport (fc.getResult());
                                  });
        }

        void startExport (const juce::File& destination)
        {
            exporter.destination = destination;
            exporter.todo.clear();
            exporter.done = exporter.failed = 0;
            exporter.failures.clear();
            exporter.usedNames.clear();

            for (size_t i = 0; i < items.size(); ++i)
                if (items[i].file.hasFileExtension ("knob"))
                    exporter.todo.push_back (i);

            exporter.next = 0;
            exportButton.setEnabled (false);
            folderButton.setEnabled (false);
            exporter.startTimer (10);
        }

        /** A few knobs each time, so the window stays alive and shows how far it got. */
        void exportSome()
        {
            for (int n = 0; n < 8 && exporter.next < exporter.todo.size(); ++n, ++exporter.next)
            {
                const auto& item = items[exporter.todo[exporter.next]];
                const auto result = KnobFile::parseFile (item.file);

                if (! result.ok)
                {
                    ++exporter.failed;
                    exporter.failures.add (item.file.getFileName() + ": " + result.error);
                    continue;
                }

                // Its own file name: the name, or with (2), (3)... when two share it.
                const auto base = juce::File::createLegalFileName (item.name).trim();
                auto name = base.isNotEmpty() ? base : item.file.getFileNameWithoutExtension();

                for (int copy = 2; exporter.usedNames.contains (name.toLowerCase())
                                   || exporter.destination.getChildFile (name + Looks::fileExtension).exists(); ++copy)
                    name = (base.isNotEmpty() ? base : item.file.getFileNameWithoutExtension()) + " (" + juce::String (copy) + ")";

                exporter.usedNames.add (name.toLowerCase());

                Looks::Look look;
                look.name = name;
                look.description = item.about;
                look.kind = kindOfDoc (result.document);
                look.origin = Looks::Origin::mine;
                look.doc = result.document;

                if (Looks::writeFile (exporter.destination.getChildFile (name + Looks::fileExtension), look))
                    ++exporter.done;
                else
                {
                    ++exporter.failed;
                    exporter.failures.add (item.file.getFileName() + ": couldn't write it");
                }
            }

            folderLabel.setText ("Exporting: " + juce::String ((int) exporter.next) + " of " + juce::String ((int) exporter.todo.size()),
                                 juce::dontSendNotification);

            if (exporter.next < exporter.todo.size())
                return;

            exporter.stopTimer();
            exportButton.setEnabled (true);
            folderButton.setEnabled (true);
            setFolder (folder);   // its label back

            auto message = juce::String (exporter.done) + " Sonify knobs saved in " + exporter.destination.getFullPathName() + ".";

            if (exporter.failed > 0)
            {
                message << "\n\n" << exporter.failed << " couldn't be:\n" << exporter.failures.joinIntoString ("\n", 0, 20);

                if (exporter.failures.size() > 20)
                    message << "\n...";
            }

            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export all KnobMan knobs", message);
        }

        void filter()
        {
            const auto text = search.getText().trim();
            shown.clear();

            for (size_t i = 0; i < items.size(); ++i)
                if (text.isEmpty() || items[i].name.containsIgnoreCase (text))
                    shown.push_back ((int) i);

            selected = -1;
            openButton.setEnabled (false);
            layoutGrid();
            grid.repaint();
        }

        static constexpr int tileW = 140, tileH = 158, gap = 10;

        int columns() const   { return juce::jmax (1, (viewport.getMaximumVisibleWidth() - gap) / (tileW + gap)); }

        juce::Rectangle<int> tileBounds (int index) const
        {
            const auto c = columns();
            return { gap + (index % c) * (tileW + gap), gap + (index / c) * (tileH + gap), tileW, tileH };
        }

        int tileAt (juce::Point<int> p) const
        {
            for (size_t i = 0; i < shown.size(); ++i)
                if (tileBounds ((int) i).contains (p))
                    return (int) i;

            return -1;
        }

        void layoutGrid()
        {
            const auto rows = ((int) shown.size() + columns() - 1) / columns();
            grid.setSize (juce::jmax (1, viewport.getMaximumVisibleWidth()), juce::jmax (viewport.getMaximumVisibleHeight(), gap + rows * (tileH + gap)));
        }

        /** Its picture: a KnobMan file's own preview (or its first frame drawn), a Sonify knob's first frame. */
        static juce::Image pictureOf (const juce::File& file)
        {
            LayerDoc doc;

            if (file.hasFileExtension ("knob"))
            {
                juce::MemoryBlock raw;

                if (file.loadFileAsData (raw))
                    if (auto preview = KnobFile::extractPreview (raw); preview.isValid())
                        return preview;

                const auto result = KnobFile::parseFile (file);

                if (! result.ok)
                    return {};

                doc = result.document;
            }
            else
            {
                Looks::Look look;

                if (! Looks::readFile (file, look))
                    return {};

                doc = look.doc;
            }

            const auto side = (float) juce::jmax (1, doc.canvasWidth, doc.canvasHeight);
            return LayerRender::renderFrame (doc, 0.0f, juce::jlimit (0.25f, 4.0f, 120.0f / side));
        }

        void timerCallback() override
        {
            // Those in view first, a few each time, so the window stays quick.
            int done = 0;
            const auto view = viewport.getViewArea();

            for (size_t i = 0; i < shown.size() && done < 4; ++i)
            {
                auto& item = items[(size_t) shown[i]];

                if (! item.tried && tileBounds ((int) i).intersects (view))
                {
                    item.tried = true;
                    item.picture = pictureOf (item.file);
                    ++done;
                }
            }

            for (; done < 4 && next < items.size(); ++next)
            {
                if (! items[next].tried)
                {
                    items[next].tried = true;
                    items[next].picture = pictureOf (items[next].file);
                    ++done;
                }
            }

            grid.repaint();

            if (done == 0)
                stopTimer();
        }

        void openSelected()
        {
            if (! juce::isPositiveAndBelow (selected, (int) shown.size()))
                return;

            const auto file = items[(size_t) shown[(size_t) selected]].file;

            if (onOpen != nullptr)
                onOpen (file);

            if (closeOnOpen)
                if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                    window->exitModalState (0);
        }

        std::function<void (const juce::File&)> onOpen;
        bool closeOnOpen = true, dragOut = false;
        juce::File folder;
        std::vector<Item> items;
        std::vector<int> shown;
        size_t next = 0;
        int selected = -1;
        juce::TextButton folderButton { juce::String::fromUTF8 ("Choose Folder\xe2\x80\xa6") }, openButton { "Open" };
        juce::TextButton sonifyButton { "Sonify Gallery" };
        bool dropHover = false;
        juce::TextButton exportButton { juce::String::fromUTF8 ("Export all KnobMan knobs as Sonify knobs\xe2\x80\xa6") };

        struct Exporter final : public juce::Timer
        {
            KnobGallery* owner = nullptr;
            juce::File destination;
            std::vector<size_t> todo;
            size_t next = 0;
            int done = 0, failed = 0;
            juce::StringArray failures, usedNames;

            void timerCallback() override   { owner->exportSome(); }
        };

        Exporter exporter;
        juce::TextEditor search;
        juce::Label folderLabel;
        juce::Viewport viewport;
        Grid grid;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    //==========================================================================
    /** The Gallery beside the Knob Studio: not modal, so the studio (its list too) stays in use
        while it's open, for dragging both ways. It goes when it's closed (or with Escape). */
    class GalleryWindow final : public juce::DocumentWindow
    {
    public:
        GalleryWindow (const juce::String& title, std::unique_ptr<juce::Component> content, juce::Component* near)
            : juce::DocumentWindow (title, Theme::panel, juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content.release(), true);
            setResizable (true, false);

            if (near != nullptr)
                centreAroundComponent (near, getWidth(), getHeight());
            else
                centreWithSize (getWidth(), getHeight());

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::Component> (this)] { delete safe.getComponent(); });
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)
            {
                closeButtonPressed();
                return true;
            }

            return juce::DocumentWindow::keyPressed (key);
        }
    };
}

//==============================================================================
KnobStudio::KnobStudio (Looks& looksToEdit)
    : looks (looksToEdit)
{
    list.setRowHeight (34);
    list.setColour (juce::ListBox::backgroundColourId, Theme::inset);

    newButton.setTooltip ("A new look for this plugin: from a built-in look or one of yours, an empty one, "
                          "one from the Gallery, or a .knob or .fklayers file");

    // KnobMaker's buttons, on the look being designed.
    saveKnobButton.setTooltip ("Save this look as a Sonify knob file (.fklayers): into Sonify's gallery, or wherever you choose");
    saveKnobButton.onClick = [this] { saveKnobFile(); };
    exportPngButton.setTooltip ("Its frames, top to bottom, as a PNG filmstrip");
    exportPngButton.onClick = [this] { exportFilmstrip(); };
    loadKnobButton.setTooltip ("A Sonify knob file (.fklayers) replaces the look being designed (with none open: a new look)");
    loadKnobButton.onClick = [this] { chooseKnobFile (false); };
    loadKnobManButton.setTooltip ("A KnobMan knob file (.knob) replaces the look being designed, drawn as KnobMan draws it");
    loadKnobManButton.onClick = [this] { chooseKnobFile (true); };
    galleryButton.setTooltip ("Sonify's gallery, as pictures, in a window that stays open: double-click a knob (or drag it here) "
                              "to replace the look being designed; drag a look from the list into the gallery to keep it there");
    galleryButton.onClick = [this] { openGallery(); };
    newButton.onClick = [this] { showNewMenu(); };

    duplicateButton.setTooltip ("A copy of this look, to change on its own");
    duplicateButton.onClick = [this]
    {
        if (const auto* look = looks.find (current))
        {
            const auto doc = look->doc;
            addLook (look->name, look->kind, doc, look->description);
        }
    };

    mineButton.setTooltip ("Keep this look in My looks, so every project can use it");
    mineButton.onClick = [this]
    {
        if (current.isNotEmpty())
            status (looks.saveToMine (current) ? "\"" + current + "\" is in My looks now: every project can use it."
                                               : "Couldn't save \"" + current + "\" to My looks.");
    };

    deleteButton.setTooltip ("Delete this look from the plugin (only when no control wears it)");
    deleteButton.onClick = [this]
    {
        const auto name = current;

        if (looks.removeProjectLook (name))
            status ("Deleted \"" + name + "\".");
    };

    useButton.onClick = [this]
    {
        if (editingWidget >= 0 && current.isNotEmpty() && onUseLook != nullptr)
        {
            const auto who = controlsName();
            useOnControls (current);
            status (who.substring (0, 1).toUpperCase() + who.substring (1) + (similars && similarWidgets.size() > 1 ? " wear \"" : " wears \"")
                    + current + "\" now.");
        }
    };

    uniqueButton.onClick = [this]
    {
        if (editingWidget >= 0 && onGiveOwnLook != nullptr)
        {
            if (const auto name = onGiveOwnLook (editingWidget); name.isNotEmpty())
            {
                select (name);
                status ("The " + controlName() + " has a look of its own now: \"" + name + "\". Changes here change only it.");
            }
        }
    };

    backButton.onClick = [this] { if (onBack != nullptr) onBack(); };

    // The builder: every change goes into the plugin's own looks at once.
    layers.onDocumentChanged = [this] { builderChanged(); };

    layers.onImported = [this] (const LayerDoc& doc, const juce::String& name)
    {
        if (workbench)
            workbenchLoad (doc, kindOfDoc (doc), withoutExtension (name));
        else
            addLook (withoutExtension (name), kindForNew(), doc);
    };

    layers.onStatus = [this] (const juce::String& text) { status (text == "Ready" ? juce::String() : text); };
    shapes.onStatus = [this] (const juce::String& text)
    {
        status (text);

        // A shape saved or deleted (the Shapes tab says so after each): the looks whose Shape
        // layers draw from the inventory are drawn again, on the plugin and in the builder.
        if (ShapeLibrary::revision() != shapeRevision)
        {
            shapeRevision = ShapeLibrary::revision();
            looks.shapesChanged();
        }
    };

    tabs.setOutline (0);
    tabs.setTabBarDepth (32);
    tabs.addTab ("Looks", Theme::panel, &layers, false);
    tabs.addTab ("Shapes", Theme::panel, &shapes, false);
    tabs.onChange = [this] (int index)
    {
        if (index == 1)
            shapes.refreshInventory();
    };

    for (auto* c : std::initializer_list<juce::Component*> { &saveKnobButton, &exportPngButton, &loadKnobButton, &loadKnobManButton, &galleryButton,
                                                             &list, &newButton, &duplicateButton, &mineButton, &deleteButton, &backButton })
        addAndMakeVisible (c);

    for (auto* c : std::initializer_list<juce::Component*> { &useButton, &uniqueButton, &tabs })
        addChildComponent (c);

    refreshList();
}

KnobStudio::~KnobStudio()
{
    if (galleryWindow != nullptr)
        delete galleryWindow.getComponent();   // the Gallery it opened goes with it

    tabs.clearTabs();
}

//==============================================================================
void KnobStudio::builderChanged()
{
    const auto* look = looks.find (current);

    if (look == nullptr)
        return;

    const auto kind = look->kind;
    auto target = current;

    // A control opened alone, wearing a look others wear too: its first change gives it a
    // look of its own (a copy of the one it wore), so only it changes.
    if (needsOwnLook() && onGiveOwnLook != nullptr)
    {
        const juce::ScopedValueSetter<bool> quiet (saving, true);

        if (const auto own = onGiveOwnLook (editingWidget); own.isNotEmpty())
            target = own;
    }

    {
        const juce::ScopedValueSetter<bool> quiet (saving, true);
        looks.setProjectLook (target, kind, layers.getDocument());
    }

    if (target != current)
    {
        // The builder already shows the change: the list follows, nothing is loaded again
        // (the builder holds this revision already, so select() leaves it be).
        current = target;

        if (const auto* saved = looks.find (current))
            loadedRevision = saved->revision;

        refreshList();
        status ("The " + controlName() + " has a look of its own now: \"" + current + "\". Changes here change only it "
                "(\"Apply this " + controlName() + " on similars\", in its menu in Edit UI, puts it on the controls like it).");
    }

    if (const auto* saved = looks.find (current))
        loadedRevision = saved->revision;

    list.repaint();
    updateButtons();
    repaint (headerText);
}

//==============================================================================
void KnobStudio::setLayout (const GuiLayout& newLayout)
{
    layout = newLayout;

    if (editedControl() == nullptr)
    {
        editingWidget = -1;
        similars = false;
        similarWidgets.clear();
    }

    refreshList();
}

void KnobStudio::editControl (int widgetIndex, bool includeSimilars)
{
    editingWidget = widgetIndex;
    similars = false;
    similarWidgets.clear();

    if (const auto* control = editedControl())
    {
        // The controls like it: the same kind, wearing the same look.
        const auto worn = looks.lookFor (*control).name;
        const auto kind = Looks::kindOf (*control);

        for (int i = 0; i < (int) layout.widgets.size(); ++i)
        {
            const auto& w = layout.widgets[(size_t) i];

            if (Looks::takesLook (w) && Looks::kindOf (w) == kind && looks.lookFor (w).name == worn)
                similarWidgets.push_back (i);
        }

        similars = includeSimilars;
        tabs.setCurrentTabIndex (0);
        select (worn);

        if (similars && similarWidgets.size() > 1)
            status ("Designing " + controlsName() + " at once.");
        else
            status ({});
    }
    else
    {
        editingWidget = -1;
        updateButtons();
    }

    resized();
    repaint();
}

void KnobStudio::clear()
{
    editingWidget = -1;
    similars = false;
    similarWidgets.clear();
    current.clear();
    list.deselectAllRows();
    tabs.setVisible (false);
    status ({});
    updateButtons();
    resized();
    repaint();
}

bool KnobStudio::needsOwnLook() const
{
    const auto* control = editedControl();
    return control != nullptr && ! similars && looks.lookFor (*control).name == current && usersOf (current) > 1;
}

void KnobStudio::useOnControls (const juce::String& lookName)
{
    if (onUseLook == nullptr || editingWidget < 0)
        return;

    if (! similars)
    {
        onUseLook (editingWidget, lookName);
        return;
    }

    // Every control like it (as they were when it was opened).
    const auto widgets = similarWidgets;

    for (const auto index : widgets)
        if (juce::isPositiveAndBelow (index, (int) layout.widgets.size()) && Looks::takesLook (layout.widgets[(size_t) index]))
            onUseLook (index, lookName);
}

juce::String KnobStudio::controlsName() const
{
    const auto count = (int) similarWidgets.size();

    if (similars && count > 1)
        return "these " + juce::String (count) + " " + controlName() + (controlName() == "switch" ? "es" : "s");

    return "this " + controlName();
}

void KnobStudio::setWorkbench (bool isWorkbench)
{
    workbench = isWorkbench;
    backButton.setVisible (! workbench);

    // KnobMaker: its menu in place of the list of looks.
    for (auto* c : std::initializer_list<juce::Component*> { &list, &newButton, &duplicateButton, &mineButton, &deleteButton })
        c->setVisible (! workbench);

    if (workbench)
    {
        saveKnobButton.setTooltip ("Save this knob as a Sonify knob file (.fklayers)");
        loadKnobButton.setTooltip ("Open a Sonify knob file (.fklayers)");
        loadKnobManButton.setTooltip ("Open a KnobMan knob file (.knob): it's drawn as KnobMan draws it");
        galleryButton.setTooltip ("Sonify's gallery, as pictures: open one straight from there");

        // The knob worked on last time (or an empty one).
        if (const auto saved = knobTitleFile().loadFileAsString().trim(); saved.isNotEmpty())
            knobTitle = saved;

        if (looks.find (workbenchLookName, Looks::Origin::project) == nullptr)
        {
            const juce::ScopedValueSetter<bool> quiet (saving, true);
            looks.setProjectLook (workbenchLookName, Looks::Kind::knob, emptyLook (Looks::Kind::knob));
            knobTitle = "New knob";
        }
    }

    newButton.setTooltip (workbench ? "A new look for your library: from a built-in look or one of yours, an empty one, "
                                      "one from the Gallery, a .knob file, a folder of them, or a .fklayers file"
                                    : "A new look for this plugin: from a built-in look or one of yours, an empty one, "
                                      "one from the Gallery, a .knob file, a folder of them, or a .fklayers file");
    deleteButton.setTooltip (workbench ? "Delete this look from your library" : "Delete this look from the plugin (only when no control wears it)");
    mineButton.setTooltip (workbench ? "Put this look in My looks: Sonify (and every Sonify project) can use it"
                                     : "Keep this look in My looks, so every project can use it");
    refreshList();

    if (workbench)
        select (workbenchLookName);

    resized();
    repaint();
}

//==============================================================================
// KnobMaker's menu
void KnobStudio::workbenchLoad (const LayerDoc& doc, Looks::Kind kind, const juce::String& title)
{
    {
        const juce::ScopedValueSetter<bool> quiet (saving, true);
        looks.setProjectLook (workbenchLookName, kind, doc);
    }

    knobTitle = title;
    knobTitleFile().replaceWithText (title);
    loadedRevision = -1;   // into the builder, even if it's the same look
    refreshList();
    select (workbenchLookName);
    tabs.setCurrentTabIndex (0);
    repaint();
}

void KnobStudio::openKnobFile (const juce::File& file)
{
    if (! workbench)
    {
        knobFolder = file.getParentDirectory();
        replaceCurrentFrom (file);
        return;
    }

    if (file.hasFileExtension ("knob"))
    {
        const auto result = KnobFile::parseFile (file);

        if (! result.ok)
        {
            status ("Couldn't open " + file.getFileName() + ": " + result.error);
            return;
        }

        workbenchLoad (result.document, kindOfDoc (result.document), file.getFileNameWithoutExtension());
        knobFolder = file.getParentDirectory();
        status ("Opened the KnobMan knob " + file.getFileName()
                + (result.warnings.isEmpty() ? juce::String (".") : ". " + result.warnings.joinIntoString (" ")));
        return;
    }

    Looks::Look look;

    if (! Looks::readFile (file, look))
    {
        status ("Couldn't open " + file.getFileName() + ": it isn't a Sonify knob.");
        return;
    }

    workbenchLoad (look.doc, look.kind, file.getFileNameWithoutExtension());
    knobFolder = file.getParentDirectory();
    status ("Opened " + file.getFileName() + ".");
}

void KnobStudio::chooseKnobFile (bool knobMan)
{
    chooser = std::make_unique<juce::FileChooser> (knobMan ? "Load a KnobMan knob (.knob)" : "Load a Sonify knob (.fklayers)",
                                                   knobFolder.isDirectory() ? knobFolder : Looks::myLooksFolder(),
                                                   knobMan ? juce::String ("*.knob") : juce::String ("*") + Looks::fileExtension);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer<KnobStudio> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis != nullptr && fc.getResult().existsAsFile())
                                  safeThis->openKnobFile (fc.getResult());
                          });
}

void KnobStudio::saveKnobFile()
{
    if (looks.find (current) == nullptr)
        return;

    // Into Sonify's gallery at first, so it's there to pick; then where the last one went.
    const auto start = (knobFolder.isDirectory() ? knobFolder : galleryFolder())
                           .getChildFile (juce::File::createLegalFileName (workbench ? knobTitle : current) + Looks::fileExtension);
    chooser = std::make_unique<juce::FileChooser> ("Save as a Sonify knob", start, juce::String ("*") + Looks::fileExtension);

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis = juce::Component::SafePointer<KnobStudio> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || fc.getResult() == juce::File())
                                  return;

                              const auto* look = safeThis->looks.find (safeThis->current);

                              if (look == nullptr)
                                  return;

                              // Named after its file.
                              const auto file = fc.getResult().withFileExtension (Looks::fileExtension);
                              auto saved = *look;
                              saved.name = file.getFileNameWithoutExtension();

                              if (Looks::writeFile (file, saved))
                              {
                                  if (safeThis->workbench)
                                  {
                                      safeThis->knobTitle = saved.name;
                                      knobTitleFile().replaceWithText (saved.name);
                                  }

                                  safeThis->knobFolder = file.getParentDirectory();
                                  safeThis->refreshGallery();
                                  safeThis->status ("Saved " + file.getFullPathName() + ".");
                                  safeThis->repaint();
                              }
                              else
                              {
                                  safeThis->status ("Couldn't write " + file.getFullPathName() + ".");
                              }
                          });
}

void KnobStudio::openGallery()
{
    // One Gallery at a time: open already, it comes to the front.
    if (galleryWindow != nullptr)
    {
        galleryWindow->toFront (true);
        return;
    }

    // It stays open beside the studio, both ways: a knob picked there (double-click, Use, or
    // dragged onto the studio) replaces the look being designed; a look dragged from the list
    // into it is kept there.
    galleryWindow = showGallery ("Gallery", [safeThis = juce::Component::SafePointer<KnobStudio> (this)] (const juce::File& file)
    {
        if (safeThis != nullptr)
            safeThis->openKnobFile (file);
    }, workbench, ! workbench, this, workbench ? juce::String() : juce::String ("Use"), workbench);
}

void KnobStudio::refreshGallery()
{
    if (galleryWindow != nullptr)
        if (auto* gallery = dynamic_cast<KnobGallery*> (galleryWindow->getContentComponent()))
            gallery->refresh();
}

juce::File KnobStudio::galleryFolder()
{
    return KnobGallery::galleryFolder();
}

juce::ResizableWindow* KnobStudio::showGallery (const juce::String& title, std::function<void (const juce::File&)> onPick,
                                                bool closeOnPick, bool dragOut, juce::Component* near, const juce::String& pickText,
                                                bool modal)
{
    auto gallery = std::make_unique<KnobGallery> (std::move (onPick), closeOnPick, dragOut, pickText);

    if (! modal)
        return new GalleryWindow (title, std::move (gallery), near);   // it deletes itself when closed

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (gallery.release());
    options.dialogTitle = title;
    options.dialogBackgroundColour = Theme::panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.componentToCentreAround = near;
    return options.launchAsync();
}

juce::var KnobStudio::galleryDrag (const juce::File& file)
{
    return juce::String (galleryDragPrefix) + file.getFullPathName();
}

juce::File KnobStudio::galleryDragFile (const juce::var& description)
{
    const auto text = description.toString();

    if (! text.startsWith (galleryDragPrefix))
        return {};

    const juce::File file (text.fromFirstOccurrenceOf (galleryDragPrefix, false, false));
    return file.existsAsFile() ? file : juce::File();
}

bool KnobStudio::readGalleryItem (const juce::File& file, LayerDoc& doc, Looks::Kind& kind,
                                  juce::String& credits, bool& commercialSafe, juce::String& error)
{
    commercialSafe = true;

    if (file.hasFileExtension ("knob"))
    {
        const auto result = KnobFile::parseFile (file);

        if (! result.ok)
        {
            error = result.error;
            return false;
        }

        doc = result.document;
        kind = kindOfDoc (doc);
        credits = {};
        return true;
    }

    Looks::Look look;

    if (! Looks::readFile (file, look))
    {
        error = "it isn't a KnobMan or Sonify knob";
        return false;
    }

    doc = look.doc;
    kind = look.kind;
    credits = look.description;
    commercialSafe = ! credits.containsIgnoreCase ("Licence: CC BY-NC") && ! credits.containsIgnoreCase ("-NC")
                     && ! credits.containsIgnoreCase ("-ND");
    return true;
}

juce::String KnobStudio::ownerName() const
{
    return workbench ? "your library" : "this plugin";
}

void KnobStudio::looksChanged()
{
    if (! saving)
        refreshList();
}

void KnobStudio::refreshList()
{
    // What the plugin's controls wear, in the order they come, then its own looks.
    names.clear();

    for (const auto& w : layout.widgets)
        if (Looks::takesLook (w))
            names.addIfNotAlreadyThere (looks.lookFor (w).name);

    for (const auto* look : looks.all())
        if (look->origin == Looks::Origin::project)
            names.addIfNotAlreadyThere (look->name);

    list.updateContent();
    list.repaint();

    // Nothing is loaded by itself: a look opens from a control, or from the list.
    select (names.contains (current) ? current : juce::String());
}

void KnobStudio::select (const juce::String& name)
{
    const bool sameLook = name == current;
    current = name;

    // Not in the list (none, or it's gone): no row stays lit (the list ignores row -1).
    if (const auto row = names.indexOf (current); row >= 0)
        list.selectRow (row, false, true);
    else
        list.deselectAllRows();

    // Into the builder: a different look, or this one changed elsewhere (an undo, say).
    if (const auto* look = looks.find (current); look != nullptr && (! sameLook || look->revision != loadedRevision))
    {
        loadedRevision = look->revision;
        layers.setDocument (look->doc);
    }

    tabs.setVisible (looks.find (current) != nullptr);
    updateButtons();
    resized();
    repaint();
}

void KnobStudio::updateButtons()
{
    const auto* look = looks.find (current);
    const auto* control = editedControl();
    const auto wearing = control != nullptr ? looks.lookFor (*control).name : juce::String();

    newButton.setEnabled (looks.isProjectOpen());
    duplicateButton.setEnabled (look != nullptr && looks.isProjectOpen());
    mineButton.setEnabled (look != nullptr);
    saveKnobButton.setEnabled (look != nullptr);
    exportPngButton.setEnabled (look != nullptr);
    loadKnobButton.setEnabled (workbench || looks.isProjectOpen());
    loadKnobManButton.setEnabled (workbench || looks.isProjectOpen());
    galleryButton.setEnabled (workbench || looks.isProjectOpen());
    deleteButton.setEnabled (look != nullptr && look->origin == Looks::Origin::project && usersOf (current) == 0);

    useButton.setButtonText ("Use on " + controlsName());
    useButton.setTooltip (similars ? "The controls like the one you opened wear this look" : "The " + controlName() + " you opened wears this look");
    useButton.setVisible (control != nullptr && look != nullptr && wearing != current && look->kind == Looks::kindOf (*control));

    uniqueButton.setButtonText ("Give this " + controlName() + " its own look");
    uniqueButton.setTooltip ("The " + controlName() + " you opened gets a copy of this look, so changes affect only it");
    uniqueButton.setVisible (control != nullptr && ! similars && wearing == current && usersOf (current) > 1);
}

int KnobStudio::usersOf (const juce::String& name) const
{
    int count = 0;

    for (const auto& w : layout.widgets)
        if (Looks::takesLook (w) && looks.lookFor (w).name == name)
            ++count;

    return count;
}

const GuiWidget* KnobStudio::editedControl() const
{
    if (! juce::isPositiveAndBelow (editingWidget, (int) layout.widgets.size()))
        return nullptr;

    const auto& w = layout.widgets[(size_t) editingWidget];
    return Looks::takesLook (w) ? &w : nullptr;
}

Looks::Kind KnobStudio::kindForNew() const
{
    if (const auto* control = editedControl())
        return Looks::kindOf (*control);

    if (const auto* look = looks.find (current))
        return look->kind;

    return Looks::Kind::knob;
}

juce::String KnobStudio::controlName() const
{
    switch (kindForNew())
    {
        case Looks::Kind::knob:          return "knob";
        case Looks::Kind::slider:
        case Looks::Kind::sliderAcross:  return "slider";
        case Looks::Kind::button:        return "switch";
    }

    return "knob";
}

//==============================================================================
void KnobStudio::addLook (const juce::String& baseName, Looks::Kind kind, const LayerDoc& doc, const juce::String& description)
{
    if (! looks.isProjectOpen())
        return;

    // Never the name of a look a control wears: the new look would take its place on them.
    const auto name = looks.freeName (baseName, names);
    looks.setProjectLook (name, kind, doc, description);

    // Opened from a control: the new look goes onto it (and the controls like it, with similars).
    bool used = false;
    const auto who = controlsName();

    if (const auto* control = editedControl(); control != nullptr && Looks::kindOf (*control) == kind && onUseLook != nullptr)
    {
        useOnControls (name);
        used = true;
    }

    refreshList();
    select (name);
    tabs.setCurrentTabIndex (0);
    status ("New look \"" + name + "\"" + (used ? ", on " + who : juce::String()) + ".");
}

void KnobStudio::showNewMenu()
{
    if (! looks.isProjectOpen())
    {
        status ("Open a plugin first: its looks are kept in its project.");
        return;
    }

    juce::PopupMenu menu;
    std::vector<std::pair<juce::String, Looks::Origin>> sources;   // by item id - 1

    // The built-in looks and mine, as they come (even one the plugin's own look hides).
    auto addKind = [&] (Looks::Kind kind, const juce::String& title)
    {
        juce::PopupMenu sub;

        for (const auto origin : { Looks::Origin::builtIn, Looks::Origin::mine })
        {
            for (const auto* look : looks.listFrom (origin, kind))
            {
                sources.push_back ({ look->name, origin });
                sub.addItem ((int) sources.size(), look->name + (origin == Looks::Origin::mine ? juce::String ("  (mine)") : juce::String()));
            }
        }

        menu.addSubMenu (title, sub, sub.getNumItems() > 0);
    };

    menu.addSectionHeader ("Start from");
    addKind (Looks::Kind::knob, "Knobs");
    addKind (Looks::Kind::slider, "Sliders");
    addKind (Looks::Kind::sliderAcross, "Sliders across");
    addKind (Looks::Kind::button, "Switches");
    menu.addSeparator();
    menu.addItem (emptyId, "An empty " + Looks::kindDisplayName (kindForNew()));
    menu.addItem (galleryId, juce::String::fromUTF8 ("From the Gallery\xe2\x80\xa6"));
    menu.addItem (knobFileId, juce::String::fromUTF8 ("Import a KnobMan .knob file\xe2\x80\xa6"));
    menu.addItem (knobFolderId, juce::String::fromUTF8 ("Import a folder of KnobMan files\xe2\x80\xa6"));
    menu.addItem (lookFileId, juce::String::fromUTF8 ("Open a .fklayers file\xe2\x80\xa6"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&newButton),
                        [safeThis = juce::Component::SafePointer<KnobStudio> (this), sources] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            auto& self = *safeThis;

                            if (result == emptyId)
                            {
                                const auto kind = self.kindForNew();
                                self.addLook ("New " + Looks::kindDisplayName (kind), kind, emptyLook (kind));
                            }
                            else if (result == galleryId)
                            {
                                // Sonify's gallery (the one Gallery opens everywhere): a pick becomes a new look.
                                KnobStudio::showGallery ("Gallery", [safe = juce::Component::SafePointer<KnobStudio> (&self)] (const juce::File& file)
                                {
                                    if (safe != nullptr)
                                        safe->addLookFromFile (file);
                                }, true, false, &self);
                            }
                            else if (result == knobFileId)  self.layers.importKnobFile();
                            else if (result == knobFolderId) self.importKnobFolder();
                            else if (result == lookFileId)  self.openLookFile();
                            else if (juce::isPositiveAndBelow (result - 1, (int) sources.size()))
                            {
                                const auto& [name, origin] = sources[(size_t) (result - 1)];

                                if (const auto* look = self.looks.find (name, origin))
                                {
                                    const auto doc = look->doc;
                                    self.addLook (look->name, look->kind, doc, look->description);
                                }
                            }
                        });
}

void KnobStudio::openLookFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a look (.fklayers)", Looks::myLooksFolder(), juce::String ("*") + Looks::fileExtension);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer<KnobStudio> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || fc.getResult() == juce::File())
                                  return;

                              Looks::Look look;

                              if (Looks::readFile (fc.getResult(), look))
                                  safeThis->addLook (look.name, look.kind, look.doc, look.description);
                              else
                                  safeThis->status ("Couldn't read " + fc.getResult().getFileName() + ": it isn't a KnobMaker layer document.");
                          });
}

void KnobStudio::addLookFromFile (const juce::File& file)
{
    if (file.hasFileExtension ("knob"))
    {
        const auto result = KnobFile::parseFile (file);

        if (! result.ok)
        {
            status ("Couldn't read " + file.getFileName() + ": " + result.error);
            return;
        }

        addLook (file.getFileNameWithoutExtension(), kindOfDoc (result.document), result.document);
        return;
    }

    Looks::Look look;

    if (! Looks::readFile (file, look))
    {
        status ("Couldn't read " + file.getFileName() + ": it isn't a KnobMaker layer document.");
        return;
    }

    addLook (look.name.isNotEmpty() ? look.name : file.getFileNameWithoutExtension(), look.kind, look.doc, look.description);
}

void KnobStudio::replaceCurrentFrom (const juce::File& file)
{
    LayerDoc doc;
    Looks::Kind kind = Looks::Kind::knob;
    juce::String credits, error;
    bool commercialSafe = true;

    if (! readGalleryItem (file, doc, kind, credits, commercialSafe, error))
    {
        status ("Couldn't read " + file.getFileName() + ": " + error + ".");
        return;
    }

    const auto* look = looks.find (current);

    // None open: it comes in as a new look.
    if (look == nullptr)
    {
        addLook (file.getFileNameWithoutExtension(), kind, doc, credits);
        return;
    }

    // Only the same kind: the controls wearing it stay what they are.
    if (look->kind != kind)
    {
        status (file.getFileNameWithoutExtension() + " is a " + Looks::kindDisplayName (kind) + ", and \"" + current + "\" is a "
                + Looks::kindDisplayName (look->kind) + ": pick a " + Looks::kindDisplayName (look->kind) + " to replace it with.");
        return;
    }

    // Into the builder, then saved as any change there is (a control opened alone gets a look
    // of its own first), so it shows on the plugin at once.
    const auto name = current;
    layers.setDocument (doc);
    builderChanged();
    status ("\"" + current + "\" now looks like " + file.getFileNameWithoutExtension()
            + (current != name ? juce::String (" (a look of its own for this ") + controlName() + ")." : juce::String (".")));
}

//==============================================================================
// Both ways with the Gallery: a knob dragged from it replaces the look being designed...
bool KnobStudio::isInterestedInDragSource (const SourceDetails& details)
{
    return galleryDragFile (details.description) != juce::File() && (workbench || looks.isProjectOpen());
}

void KnobStudio::itemDragEnter (const SourceDetails&)   { dropHover = true;  repaint(); }
void KnobStudio::itemDragExit (const SourceDetails&)    { dropHover = false; repaint(); }

void KnobStudio::itemDropped (const SourceDetails& details)
{
    dropHover = false;
    repaint();

    if (const auto file = galleryDragFile (details.description); file != juce::File())
        openKnobFile (file);
}

void KnobStudio::paintOverChildren (juce::Graphics& g)
{
    if (! dropHover)
        return;

    const auto area = getLocalBounds().withTrimmedTop (headerHeight + 1).withTrimmedBottom (statusHeight).withTrimmedLeft (listWidth + 1);
    g.setColour (Theme::accent.withAlpha (0.10f));
    g.fillRect (area);
    g.setColour (Theme::accent);
    g.drawRect (area, 2);
    g.setFont (Theme::font (16.0f, true));
    g.drawText (looks.find (current) != nullptr ? "Drop to replace \"" + current + "\"" : juce::String ("Drop to add it as a new look"),
                area, juce::Justification::centred);
}

// ...and a look dragged from the list into the Gallery is kept there (saved as a Sonify knob
// file on the way, in the temp folder, named after the look).
juce::var KnobStudio::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    if (rows.isEmpty() || ! juce::isPositiveAndBelow (rows[0], names.size()))
        return {};

    const auto* look = looks.find (names[rows[0]]);

    if (look == nullptr)
        return {};

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Sonify Knob Drag");
    folder.deleteRecursively();
    folder.createDirectory();

    const auto file = folder.getChildFile (juce::File::createLegalFileName (look->name) + Looks::fileExtension);

    if (! Looks::writeFile (file, *look))
        return {};

    return juce::String (knobFileDragPrefix) + file.getFullPathName();
}

void KnobStudio::importKnobFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a folder of KnobMan files (.knob)", juce::File());

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safeThis = juce::Component::SafePointer<KnobStudio> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || ! fc.getResult().isDirectory())
                                  return;

                              auto& self = *safeThis;
                              const auto files = fc.getResult().findChildFiles (juce::File::findFiles, true, "*.knob");
                              int added = 0;
                              juce::StringArray failed;

                              // Each one a look of its own (its file's name), then the list once.
                              {
                                  const juce::ScopedValueSetter<bool> quiet (self.saving, true);

                                  for (const auto& file : files)
                                  {
                                      const auto result = KnobFile::parseFile (file);

                                      if (! result.ok)
                                      {
                                          failed.add (file.getFileName());
                                          continue;
                                      }

                                      const auto& doc = result.document;
                                      const auto kind = doc.frames == 2                               ? Looks::Kind::button
                                                      : doc.canvasHeight >= doc.canvasWidth * 2        ? Looks::Kind::slider
                                                      : doc.canvasWidth >= doc.canvasHeight * 2        ? Looks::Kind::sliderAcross
                                                                                                       : Looks::Kind::knob;
                                      self.looks.setProjectLook (self.looks.freeName (file.getFileNameWithoutExtension(), self.names), kind, doc);
                                      ++added;
                                  }
                              }

                              self.refreshList();
                              self.status ("Imported " + juce::String (added) + " of " + juce::String (files.size()) + " KnobMan files"
                                           + (failed.isEmpty() ? juce::String (".")
                                                               : ". Couldn't read: " + failed.joinIntoString (", ") + "."));
                          });
}

void KnobStudio::exportFilmstrip()
{
    const auto* look = looks.find (current);

    if (look == nullptr)
        return;

    const auto start = (workbench && knobFolder.isDirectory() ? knobFolder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory))
                           .getChildFile (juce::File::createLegalFileName (workbench ? knobTitle : current) + ".png");
    chooser = std::make_unique<juce::FileChooser> ("Export the frames as a PNG filmstrip", start, "*.png");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis = juce::Component::SafePointer<KnobStudio> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || fc.getResult() == juce::File())
                                  return;

                              const auto* look = safeThis->looks.find (safeThis->current);

                              if (look == nullptr)
                                  return;

                              // Top to bottom, every frame at the look's own size.
                              const auto strip = LayerRender::renderFilmstrip (look->doc, true);
                              const auto file = fc.getResult().withFileExtension (".png");
                              file.deleteFile();
                              juce::FileOutputStream out (file);
                              juce::PNGImageFormat png;

                              safeThis->status (out.openedOk() && png.writeImageToStream (strip, out)
                                                    ? "Exported " + juce::String (look->doc.frames) + " frames to " + file.getFullPathName() + "."
                                                    : "Couldn't write " + file.getFullPathName() + ".");
                          });
}

void KnobStudio::status (const juce::String& text)
{
    statusText = text;
    repaint (statusArea);
}

//==============================================================================
int KnobStudio::getNumRows()
{
    return names.size();
}

void KnobStudio::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, names.size()))
        return;

    if (selected)
    {
        g.setColour (Theme::accent.withAlpha (0.25f));
        g.fillRect (0, 0, width, height);
    }

    const auto name = names[row];
    const auto users = usersOf (name);

    g.setColour (Theme::text);
    g.setFont (Theme::font (14.5f, selected));
    g.drawText (name, 12, 0, width - 96, height, juce::Justification::centredLeft, true);

    g.setColour (Theme::muted);
    g.setFont (Theme::font (12.5f));
    if (! workbench)
        g.drawText (users == 0 ? juce::String ("unused") : users == 1 ? juce::String ("1 control") : juce::String (users) + " controls",
                    width - 92, 0, 82, height, juce::Justification::centredRight, false);
}

void KnobStudio::selectedRowsChanged (int lastRowSelected)
{
    if (juce::isPositiveAndBelow (lastRowSelected, names.size()) && names[lastRowSelected] != current)
        select (names[lastRowSelected]);
}

//==============================================================================
void KnobStudio::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    const auto* look = looks.find (current);
    auto header = headerText;

    g.setColour (Theme::text);
    g.setFont (Theme::font (16.0f, true));
    g.drawText (workbench ? "Knob: " + knobTitle : look != nullptr ? "Look: " + current : juce::String ("Knob Studio"),
                header.removeFromTop (20), juce::Justification::centredLeft, true);

    juce::String detail;

    if (workbench)
    {
        detail = "Save as Sonify knob keeps it as a .fklayers file; Export to PNG writes its frames as a filmstrip. "
                 "Load a Sonify or a KnobMan knob, or pick one in the Gallery.";
    }
    else if (look == nullptr)
    {
        detail = ! looks.isProjectOpen() ? "Open a plugin: the Knob Studio works on its knobs, sliders and switches."
               : names.isEmpty()         ? "Build the plugin first: its knobs, sliders and switches bring their looks."
                                         : "Nothing open: select a control in Edit UI and press \"Design it in the Knob Studio\" in its menu.";
    }
    else
    {
        const auto users = usersOf (current);
        const auto* control = editedControl();
        detail = (control != nullptr && looks.lookFor (*control).name == current
                      ? (similars && similarWidgets.size() > 1 ? "Designing " + controlsName() : juce::String ("Designing the ") + controlName()
                                                                                                 + (control->label.isNotEmpty() ? " \"" + control->label + "\"" : juce::String()))
                          + juce::String::fromUTF8 (" \xc2\xb7 ")
                      : juce::String())
               + (users == 0 ? juce::String ("No control wears it")
                             : "Worn by " + (users == 1 ? juce::String ("1 control") : juce::String (users) + " controls"))
               + juce::String::fromUTF8 (" \xc2\xb7 ")
               + (look->origin == Looks::Origin::project ? juce::String ("this plugin's own look")
                  : look->origin == Looks::Origin::mine  ? juce::String ("from My looks: it becomes this plugin's own when you change it")
                                                         : juce::String ("built in: it becomes this plugin's own when you change it"))
               + ". Every change shows on the plugin at once.";
    }

    g.setColour (Theme::muted);
    g.setFont (Theme::font (13.0f));
    g.drawText (detail, header, juce::Justification::centredLeft, true);

    g.setColour (Theme::outline);
    g.fillRect (0, headerHeight, getWidth(), 1);
    g.fillRect (listWidth, headerHeight, 1, getHeight() - headerHeight - statusHeight);
    g.fillRect (0, statusArea.getY(), getWidth(), 1);

    // The builder's notes (a shadow out of room), and what the buttons did.
    g.setColour (Theme::muted);
    g.setFont (Theme::font (12.5f));
    g.drawText (statusText, statusArea.reduced (16, 0), juce::Justification::centredLeft, true);

    if (look == nullptr && ! workbench)
    {
        auto hint = getLocalBounds().withTrimmedTop (headerHeight).withTrimmedBottom (statusHeight).withTrimmedLeft (listWidth);
        g.setColour (Theme::muted);
        g.setFont (Theme::font (15.0f));
        g.drawFittedText ("Select a control in Edit UI and press \"Design it in the Knob Studio\" in its menu: its look opens here "
                          "(\"Apply this knob on similars\", under it, puts its look on every control of its kind). Looks are made of layers, the "
                          "KnobMan way: a shape on each, with its own light, bevel, texture and movement.",
                          hint.withSizeKeepingCentre (juce::jmin (520, hint.getWidth() - 40), 80), juce::Justification::centred, 3, 1.0f);
    }
}

void KnobStudio::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (16, 8);
    auto buttons = header.withTrimmedTop (4).withTrimmedBottom (4);

    if (backButton.isVisible())
    {
        backButton.setBounds (buttons.removeFromRight (150));
        buttons.removeFromRight (10);
    }

    if (uniqueButton.isVisible())
    {
        uniqueButton.setBounds (buttons.removeFromRight (240));
        buttons.removeFromRight (10);
    }

    if (useButton.isVisible())
    {
        useButton.setBounds (buttons.removeFromRight (170));
        buttons.removeFromRight (10);
    }

    headerText = header.withRight (buttons.getRight());

    area.removeFromTop (1);
    statusArea = area.removeFromBottom (statusHeight);

    auto left = area.removeFromLeft (listWidth).reduced (10);

    // KnobMaker's buttons: saving and exporting, then loading, then the Gallery.
    for (auto* button : { &saveKnobButton, &exportPngButton, &loadKnobButton, &loadKnobManButton, &galleryButton })
    {
        if (button == &loadKnobButton || button == &galleryButton)
            left.removeFromTop (14);

        button->setBounds (left.removeFromTop (32).reduced (2, 0));
        left.removeFromTop (6);
    }

    if (workbench)
    {
        tabs.setBounds (area.withTrimmedLeft (1));
        return;
    }

    // Then the plugin's looks.
    left.removeFromTop (12);
    auto lower = left.removeFromBottom (30);
    left.removeFromBottom (6);
    auto upper = left.removeFromBottom (30);
    left.removeFromBottom (8);
    list.setBounds (left);

    newButton.setBounds (upper.removeFromLeft (upper.getWidth() / 2).reduced (2, 0));
    duplicateButton.setBounds (upper.reduced (2, 0));
    mineButton.setBounds (lower.removeFromLeft (lower.getWidth() * 3 / 5).reduced (2, 0));
    deleteButton.setBounds (lower.reduced (2, 0));

    tabs.setBounds (area.withTrimmedLeft (1));
}
