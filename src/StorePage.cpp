// D:\workspace\Fanan_web_hub\src\StorePage.cpp

#include "StorePage.h"
#include "StellaLookAndFeel.h"
#include "Preferences.h"   // SettingsFolder: Sonify's folder in AppData

#include <juce_gui_extra/juce_gui_extra.h>

#include <cstring>

namespace
{
    constexpr int headerHeight = 64, footerHeight = 44, detailWidth = 420, narrowWidth = 900;
    constexpr int formMargin = 24, formGap = 18, rowHeight = 30, labelHeight = 20, maxFormWidth = 1120;
    const juce::Colour problemColour { 0xffff8a80 };
    const juce::Colour goodColour { 0xff7fd6a3 };

    juce::String sizeText (juce::int64 bytes)
    {
        if (bytes <= 0)
            return {};

        return bytes < 1024 * 1024 ? juce::String (juce::jmax ((juce::int64) 1, bytes / 1024)) + " KB"
                                   : juce::String ((double) bytes / (1024.0 * 1024.0), 1) + " MB";
    }

    juce::String dot()
    {
        return juce::String::fromUTF8 ("  \xc2\xb7  ");
    }

   #if JUCE_WEB_BROWSER
    /** A YouTube video, playing: its embedded player, in a page of its own (so YouTube knows where
        it's shown). Links out of it open in the browser. */
    class VideoView final : public juce::WebBrowserComponent
    {
    public:
        explicit VideoView (const juce::String& videoId)
            : WebBrowserComponent (optionsFor (videoId))
        {
            goToURL (getResourceProviderRoot() + "video.html");
        }

        bool pageAboutToLoad (const juce::String& url) override
        {
            if (url.startsWith (getResourceProviderRoot()) || url.startsWith ("about:") || url.containsIgnoreCase ("/embed/"))
                return true;

            juce::URL (url).launchInDefaultBrowser();
            return false;
        }

        void newWindowAttemptingToLoad (const juce::String& url) override    { juce::URL (url).launchInDefaultBrowser(); }

    private:
        static Options optionsFor (const juce::String& videoId)
        {
            const juce::String html = R"(<!doctype html><html><head><meta charset="utf-8"><meta name="referrer" content="strict-origin-when-cross-origin">
<style>html,body{margin:0;height:100%;background:#000;overflow:hidden}iframe{position:absolute;inset:0;width:100%;height:100%;border:0}</style></head>
<body><iframe src="https://www.youtube-nocookie.com/embed/)" + videoId + R"(?autoplay=1&rel=0&playsinline=1&modestbranding=1" title="Demo video"
allow="autoplay; encrypted-media; picture-in-picture; fullscreen" allowfullscreen referrerpolicy="strict-origin-when-cross-origin"></iframe></body></html>)";

            const auto folder = SettingsFolder::folder ("Store video", "Store video");

            return Options{}
                .withBackend (Options::Backend::webview2)
                .withWinWebView2Options (Options::WinWebView2{}.withUserDataFolder (folder).withBackgroundColour (juce::Colours::black))
                .withResourceProvider ([html] (const juce::String&) -> std::optional<Resource>
                {
                    const auto utf8 = html.toUTF8();
                    std::vector<std::byte> data (utf8.sizeInBytes() - 1);
                    std::memcpy (data.data(), utf8.getAddress(), data.size());
                    return Resource { std::move (data), "text/html" };
                });
        }
    };
   #endif

    juce::String watchUrl (const juce::String& videoId)
    {
        return "https://www.youtube.com/watch?v=" + videoId;
    }

    void styleNote (juce::Label& label, juce::Colour colour = Theme::muted)
    {
        label.setFont (Theme::font (13.0f));
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (juce::Justification::topLeft);
        label.setMinimumHorizontalScale (1.0f);
    }

    void styleNote (juce::Label& label, const juce::String& text)
    {
        styleNote (label);
        label.setText (text, juce::dontSendNotification);
    }

    void styleHeading (juce::Label& label, const juce::String& text, float size)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Theme::font (size, true));
        label.setColour (juce::Label::textColourId, Theme::text);
        label.setJustificationType (juce::Justification::centredLeft);
    }

    void styleEditor (juce::TextEditor& editor, int maxLength, bool multiLine)
    {
        editor.setMultiLine (multiLine, true);
        editor.setReturnKeyStartsNewLine (multiLine);
        editor.setScrollbarsShown (multiLine);
        editor.setFont (Theme::font (14.5f));
        editor.setInputRestrictions (maxLength);
        editor.setIndents (8, multiLine ? 6 : 7);
    }

    void accentButton (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, Theme::accent);
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    }

    juce::String formatDate (const juce::String& iso)
    {
        const auto time = juce::Time::fromISO8601 (iso);
        return time.toMilliseconds() > 0 ? time.formatted ("%d %b %Y") : juce::String();
    }
}

//==============================================================================
StorePage::Card::Card (StorePage& owner, const StoreClient::Ad& storeAd)
    : ad (storeAd), page (owner)
{
    setSize (width, height);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (ad.title);
}

void StorePage::Card::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked())
        page.select (ad);
}

void StorePage::Card::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (selected ? Theme::accentDeep : hover ? Theme::raised.brighter (0.06f) : Theme::raised);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (selected ? Theme::accent : Theme::outline);
    g.drawRoundedRectangle (bounds, 10.0f, selected ? 2.0f : 1.0f);

    // The picture, filling its box (16:10).
    const auto pictureBox = getLocalBounds().reduced (8).removeFromTop ((width - 16) * 10 / 16).toFloat();

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (pictureBox, 7.0f);
        g.reduceClipRegion (clip);
        g.setColour (Theme::inset);
        g.fillRect (pictureBox);

        if (picture.isValid())
            g.drawImage (picture, pictureBox, juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        else
        {
            g.setColour (Theme::muted.withAlpha (0.5f));
            g.setFont (Theme::font (13.0f));
            g.drawText (ad.kindName(), pictureBox, juce::Justification::centred);
        }
    }

    auto text = getLocalBounds().reduced (12, 0).withTrimmedTop ((int) pictureBox.getBottom() + 8);

    g.setColour (Theme::text);
    g.setFont (Theme::font (15.0f, true));
    g.drawText (ad.title, text.removeFromTop (22), juce::Justification::centredLeft, true);

    auto makerRow = text.removeFromTop (18);

    if (logo.isValid())
    {
        // The maker's logo, before their name: 14 pixels high, at most 60 wide.
        const auto logoWidth = juce::jmin (60, juce::roundToInt (14.0f * (float) logo.getWidth() / (float) juce::jmax (1, logo.getHeight())));
        g.drawImage (logo, makerRow.removeFromLeft (logoWidth).withSizeKeepingCentre (logoWidth, 14).toFloat(), juce::RectanglePlacement::centred);
        makerRow.removeFromLeft (6);
    }

    g.setColour (Theme::muted);
    g.setFont (Theme::font (13.0f));
    g.drawText (ad.maker.isNotEmpty() ? ad.maker : juce::String ("Made in Sonify"), makerRow, juce::Justification::centredLeft, true);

    g.setColour (Theme::text.withAlpha (0.8f));
    g.setFont (Theme::font (13.0f));
    g.drawText (ad.tagline, text.removeFromTop (20), juce::Justification::centredLeft, true);

    auto row = text.removeFromTop (24);

    // On the right: the poster's own (hidden or not), or that the Sonify version can be added.
    juce::String mark;
    auto markColour = Theme::accent;

    if (ad.mine)
    {
        mark = ad.hidden ? juce::String ("Hidden") : juce::String ("Yours");
        markColour = ad.hidden ? Theme::muted : goodColour;
    }
    else if (ad.offersDownload)
    {
        mark = page.client.installedVersion (ad) > 0 ? juce::String::fromUTF8 ("Added \xe2\x9c\x93") : juce::String ("Add to Sonify");
        markColour = goodColour;
    }

    g.setColour (markColour);
    g.setFont (Theme::font (12.5f, true));
    g.drawText (mark, row, juce::Justification::centredRight, false);

    const auto markWidth = mark.isNotEmpty() ? juce::roundToInt (juce::GlyphArrangement::getStringWidth (Theme::font (12.5f, true), mark)) + 8 : 0;

    g.setColour (Theme::muted);
    g.setFont (Theme::font (12.5f));
    g.drawText (ad.section.isNotEmpty() ? ad.section : ad.kindName(), row.withTrimmedRight (markWidth), juce::Justification::centredLeft, true);
}

//==============================================================================
void StorePage::Grid::layOut (int width)
{
    const auto columns = juce::jmax (1, (width - 2 * margin + gap) / (Card::width + gap));
    const auto used = columns * Card::width + (columns - 1) * gap;
    const auto left = juce::jmax (margin, (width - used) / 2);
    int rows = 0;

    for (int i = 0; i < cards.size(); ++i)
    {
        const auto column = i % columns, row = i / columns;
        cards[i]->setTopLeftPosition (left + column * (Card::width + gap), margin + row * (Card::height + gap));
        rows = row + 1;
    }

    setSize (width, juce::jmax (1, margin * 2 + rows * Card::height + juce::jmax (0, rows - 1) * gap));
}

//==============================================================================
StorePage::Detail::Detail (StorePage& owner)
    : page (owner)
{
    backButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80  Back"));
    backButton.onClick = [this] { page.closeDetail(); };

    accentButton (websiteButton);
    websiteButton.setButtonText (juce::String::fromUTF8 ("Visit website \xe2\x86\x97"));
    websiteButton.onClick = [this] { page.visitWebsite(); };

    addButton.onClick = [this] { page.addToSonify(); };

    watchButton.setTooltip ("Its demo video, played here");
    watchButton.onClick = [this] { if (video != nullptr) stopVideo(); else playVideo(); };
    youtubeButton.setButtonText (juce::String::fromUTF8 ("YouTube \xe2\x86\x97"));
    youtubeButton.setTooltip ("Watch it on YouTube, in your browser");
    youtubeButton.onClick = [this] { juce::URL (watchUrl (ad.videoId())).launchInDefaultBrowser(); };

    moreButton.setButtonText (juce::String::fromUTF8 ("\xe2\x8b\xaf"));
    moreButton.setTooltip ("More");
    moreButton.onClick = [this] { page.showMoreMenu(); };

    editButton.setTooltip ("Change the ad: its words, pictures, links and the Sonify version");
    editButton.onClick = [this] { page.editAd (ad); };
    hideButton.onClick = [this] { page.hideOrShow(); };
    deleteButton.setTooltip ("Take the ad off the board for good");
    deleteButton.onClick = [this] { page.deleteAd(); };

    styleNote (warningLabel, problemColour);
    styleNote (statusLabel);

    storyText.setMultiLine (true, true);
    storyText.setReadOnly (true);
    storyText.setScrollbarsShown (true);
    storyText.setCaretVisible (false);
    storyText.setFont (Theme::font (14.5f));
    storyText.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    storyText.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    storyText.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    storyText.setColour (juce::TextEditor::textColourId, Theme::text.withAlpha (0.9f));

    for (auto* c : std::initializer_list<juce::Component*> { &backButton, &websiteButton, &moreButton, &warningLabel, &statusLabel, &storyText })
        addAndMakeVisible (c);

    for (auto* c : std::initializer_list<juce::Component*> { &addButton, &editButton, &hideButton, &deleteButton, &watchButton, &youtubeButton })
        addChildComponent (c);
}

void StorePage::Detail::setAd (const StoreClient::Ad& newAd)
{
    const bool other = newAd.id != ad.id;
    ad = newAd;

    if (other)
    {
        stopVideo();
        picture = {};
        logo = {};
        setStatus ({}, false);
    }

    watchButton.setVisible (ad.videoId().isNotEmpty());

    juce::String text = ad.story.trim();

    if (! ad.features.isEmpty())
    {
        text << "\n\n";

        for (const auto& feature : ad.features)
            text << juce::String::fromUTF8 ("\xe2\x80\xa2  ") << feature << "\n";
    }

    storyText.setText (text.trimEnd(), false);
    storyText.moveCaretToTop (false);
    refreshState();
}

void StorePage::Detail::setStatus (const juce::String& text, bool problem)
{
    statusLabel.setText (text, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, problem ? problemColour : Theme::muted);
}

void StorePage::Detail::playVideo()
{
    const auto id = ad.videoId();

    if (id.isEmpty())
        return;

   #if JUCE_WEB_BROWSER
    video = std::make_unique<VideoView> (id);
    addAndMakeVisible (*video);
   #else
    juce::URL (watchUrl (id)).launchInDefaultBrowser();   // no web pages in this Sonify: the browser shows it
   #endif

    refreshState();
}

void StorePage::Detail::stopVideo()
{
    if (video == nullptr)
        return;

    video = nullptr;   // the player goes, and its sound with it
    refreshState();
}

void StorePage::Detail::refreshState()
{
    auto& store = page.client;
    const auto installed = store.installedVersion (ad);
    const bool installing = store.isInstalling (ad.id);
    const auto offered = juce::jmax (1, ad.package.version);

    websiteButton.setEnabled (ad.website.isNotEmpty());
    websiteButton.setTooltip (ad.website.isNotEmpty() ? "Opens " + ad.website + " in your browser: buy it there, and get your serial"
                                                      : juce::String ("This ad has no website"));

    // Add to Sonify, Update, or Added: only when the ad offers the Sonify version.
    addButton.setVisible (ad.offersDownload);

    if (installing)
        addButton.setButtonText (juce::String::fromUTF8 ("Adding\xe2\x80\xa6"));
    else if (installed == 0)
        addButton.setButtonText ("Add to Sonify");
    else if (installed < offered)
        addButton.setButtonText ("Update to version " + juce::String (offered));
    else
        addButton.setButtonText (juce::String::fromUTF8 ("Added \xe2\x9c\x93"));

    addButton.setEnabled (! installing && (installed == 0 || installed < offered));
    addButton.setTooltip ((ad.isMidiFx() ? juce::String ("Add it to My Devices: it's in the tracks' MIDI FX menu")
                                         : juce::String ("Add it to Sonify's Browser, under Made in Sonify"))
                          + ". If it's protected, it plays its demo until you register it with the serial from the maker's website");

    editButton.setVisible (ad.mine);
    hideButton.setVisible (ad.mine);
    deleteButton.setVisible (ad.mine);
    hideButton.setButtonText (ad.hidden ? "Show it again" : "Hide");
    hideButton.setTooltip (ad.hidden ? "Put the ad back on the board" : "Take the ad off the board for now (you can show it again)");

    warningLabel.setText (ad.mine && ad.hidden ? "Hidden: it isn't on the board. Show it again any time." : juce::String(), juce::dontSendNotification);
    warningLabel.setVisible (ad.mine && ad.hidden);

    watchButton.setButtonText (video != nullptr ? juce::String::fromUTF8 ("\xe2\x96\xa0  Stop the video")
                                                : juce::String::fromUTF8 ("\xe2\x96\xb6  Watch the demo video"));
    youtubeButton.setVisible (video != nullptr);

    resized();
    repaint();
}

void StorePage::Detail::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());

    if (! ad.isValid())
        return;

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (pictureArea.toFloat(), 9.0f);
        g.reduceClipRegion (clip);
        g.setColour (Theme::inset);
        g.fillRect (pictureArea);

        if (picture.isValid())
            g.drawImage (picture, pictureArea.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }

    auto text = textArea;
    const auto besideLogo = logo.isValid() ? logoArea.getWidth() + 10 : 0;

    // The maker's logo, at the right of the title.
    if (logo.isValid() && ! logoArea.isEmpty())
        g.drawImage (logo, logoArea.toFloat(), juce::RectanglePlacement::xRight | juce::RectanglePlacement::yTop | juce::RectanglePlacement::onlyReduceInSize);

    g.setColour (Theme::text);
    g.setFont (Theme::font (21.0f, true));
    g.drawFittedText (ad.title, text.removeFromTop (30).withTrimmedRight (besideLogo), juce::Justification::centredLeft, 1, 0.9f);

    juce::StringArray facts;
    facts.add (ad.maker.isNotEmpty() ? ad.maker : juce::String ("Made in Sonify"));
    facts.add (ad.kindName());

    if (ad.section.isNotEmpty())
        facts.add (ad.section);

    if (ad.offersDownload && ad.package.size > 0)
        facts.add (sizeText (ad.package.size));

    g.setColour (Theme::muted);
    g.setFont (Theme::font (13.0f));
    g.drawFittedText (facts.joinIntoString (dot()), text.removeFromTop (20).withTrimmedRight (besideLogo), juce::Justification::centredLeft, 1, 0.8f);

    if (! ad.formats.isEmpty())
    {
        g.setColour (Theme::text.withAlpha (0.85f));
        g.setFont (Theme::font (13.0f));
        g.drawFittedText ("Comes as " + ad.formats.joinIntoString (dot()), text.removeFromTop (22), juce::Justification::centredLeft, 1, 0.8f);
    }

    if (ad.priceText.isNotEmpty())
    {
        g.setColour (Theme::text);
        g.setFont (Theme::font (16.0f, true));
        g.drawFittedText (ad.priceText, text.removeFromTop (26), juce::Justification::centredLeft, 1, 0.8f);
    }

    if (ad.tagline.isNotEmpty())
    {
        g.setColour (Theme::text.withAlpha (0.85f));
        g.setFont (Theme::font (14.5f, true));
        g.drawFittedText (ad.tagline, text.removeFromTop (40), juce::Justification::topLeft, 2, 1.0f);
    }
}

void StorePage::Detail::resized()
{
    auto area = getLocalBounds().reduced (20, 16);
    area.removeFromLeft (1);

    if (backButton.isVisible())
    {
        backButton.setBounds (area.removeFromTop (30).removeFromLeft (100));
        area.removeFromTop (10);
    }

    pictureArea = area.removeFromTop (juce::jmin (area.getWidth() * 10 / 16, 300));

    // The video plays where the picture is (a web page: nothing can lie over it), its buttons
    // under it.
    if (video != nullptr)
        video->setBounds (pictureArea);

    if (watchButton.isVisible())
    {
        area.removeFromTop (8);
        auto row = area.removeFromTop (30);
        watchButton.setBounds (row.removeFromLeft (210));
        row.removeFromLeft (8);
        youtubeButton.setBounds (row.removeFromLeft (100));
    }

    area.removeFromTop (12);
    textArea = area.removeFromTop (54 + (ad.formats.isEmpty() ? 0 : 22) + (ad.priceText.isNotEmpty() ? 26 : 0) + (ad.tagline.isNotEmpty() ? 42 : 0));
    logoArea = textArea.withHeight (46).removeFromRight (juce::jmin (130, textArea.getWidth() / 3));
    area.removeFromTop (6);

    auto row = area.removeFromTop (36);
    moreButton.setBounds (row.removeFromRight (40));
    row.removeFromRight (8);

    if (addButton.isVisible())
    {
        const auto half = (row.getWidth() - 8) / 2;
        websiteButton.setBounds (row.removeFromLeft (half));
        row.removeFromLeft (8);
        addButton.setBounds (row);
    }
    else
    {
        websiteButton.setBounds (row);
    }

    area.removeFromTop (8);

    if (editButton.isVisible())
    {
        // The poster's own: Edit, Hide (or Show), Delete.
        auto mineRow = area.removeFromTop (32);
        const auto third = (mineRow.getWidth() - 16) / 3;
        editButton.setBounds (mineRow.removeFromLeft (third));
        mineRow.removeFromLeft (8);
        hideButton.setBounds (mineRow.removeFromLeft (third));
        mineRow.removeFromLeft (8);
        deleteButton.setBounds (mineRow);
        area.removeFromTop (6);
    }

    if (warningLabel.isVisible())
        warningLabel.setBounds (area.removeFromTop (22));

    statusLabel.setBounds (area.removeFromBottom (40));
    area.removeFromBottom (4);
    area.removeFromTop (6);
    storyText.setBounds (area.expanded (4, 0));
}

//==============================================================================
void StorePage::PictureView::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    g.setColour (Theme::inset);
    g.fillRoundedRectangle (area, 8.0f);

    if (image.isValid())
    {
        const auto fitted = juce::RectanglePlacement (juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize)
                                .appliedTo (image.getBounds().toFloat(), area.reduced (10.0f));
        g.drawImage (image, fitted, juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        g.setColour (Theme::muted);
        g.setFont (Theme::font (14.0f));
        g.drawFittedText (emptyText, getLocalBounds().reduced (16), juce::Justification::centred, 3, 1.0f);
    }

    g.setColour (Theme::outline);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);
}

//==============================================================================
StorePage::AdForm::AdForm (StorePage& owner)
    : page (owner)
{
    styleHeading (heading, "Your ad", 22.0f);
    styleNote (idLabel);
    idLabel.setJustificationType (juce::Justification::centredRight);

    styleHeading (listingHeading, "The ad", 16.0f);
    styleHeading (linksHeading, "Where to get it", 16.0f);
    styleHeading (pictureHeading, "Picture", 16.0f);
    styleHeading (logoHeading, "Your logo", 16.0f);
    styleHeading (downloadHeading, "The Sonify version", 16.0f);

    writeButton.setButtonText ("Write the story with Fanan AI");
    writeButton.setTooltip ("Fanan AI writes the tagline, story and features from the plugin's code and its picture. "
                            "Uses credits. Change them after as you like");
    writeButton.onClick = [this] { page.writeFormStory(); };
    styleNote (writeNote, "Uses Fanan AI credits");
    writeNote.setJustificationType (juce::Justification::centredRight);

    styleNote (titleLabel, "Title");
    styleNote (sectionLabel, "Section");
    sectionBox.setTextWhenNothingSelected ("Choose its section");
    sectionBox.setTooltip ("Where people look for it on the board (Fanan AI suggests one when it writes the story)");
    sectionBox.onChange = [this] { page.formEdited(); };
    styleNote (taglineLabel, "Tagline: one line under the title");
    styleNote (storyLabel, "Story: what people read");
    styleNote (featuresLabel, "Features: one per line");

    styleEditor (titleEditor, StorePublisher::maxTitle, false);
    styleEditor (taglineEditor, StorePublisher::maxTagline, false);
    styleEditor (storyEditor, StorePublisher::maxStory, true);
    styleEditor (featuresEditor, StorePublisher::maxFeatures * (StorePublisher::maxFeature + 1), true);

    for (auto* editor : { &titleEditor, &taglineEditor, &storyEditor, &featuresEditor })
        editor->onTextChange = [this] { page.formEdited(); };

    styleNote (websiteLabel, "Your website (required): where people buy it and get their serial");
    styleEditor (websiteEditor, StorePublisher::maxWebsite, false);
    websiteEditor.setTextToShowWhenEmpty ("https://www.example.com/my-plugin", Theme::muted);
    websiteEditor.setTooltip ("The page where people buy it. Visit website opens it in their browser");
    websiteEditor.onTextChange = [this] { page.formEdited(); page.showFormNotes(); };
    styleNote (websiteNote);

    styleNote (videoLabel, "Demo video: a YouTube link (optional)");
    styleEditor (videoEditor, 300, false);
    videoEditor.setTextToShowWhenEmpty ("https://www.youtube.com/watch?v=...", Theme::muted);
    videoEditor.setTooltip ("A video of it playing: it plays right on its ad");
    videoEditor.onTextChange = [this] { page.formEdited(); page.showFormNotes(); };
    styleNote (videoNote);

    styleNote (priceLabel, "Price or terms (optional)");
    styleEditor (priceEditor, StorePublisher::maxPriceText, false);
    priceEditor.setTextToShowWhenEmpty ("$29, free 30-day demo", Theme::muted);
    priceEditor.setTooltip ("Shown on the ad exactly as you write it. Nothing is paid here: people buy on your website");
    priceEditor.onTextChange = [this] { page.formEdited(); };
    styleNote (priceNote, "Shown as you write it. People pay on your website, never in Sonify.");

    styleNote (formatsLabel, "Comes as");

    for (const auto& format : StoreClient::allFormats())
    {
        auto* button = formatButtons.add (new juce::TextButton (format));
        button->setClickingTogglesState (true);
        button->setColour (juce::TextButton::buttonOnColourId, Theme::accent);
        button->setTooltip (format == "Sonify" ? juce::String ("A version that plays inside Sonify")
                                               : "Sold on your website as " + format + (format == "AU" ? " (Mac)" : ""));
        button->onClick = [this] { page.formEdited(); };
    }

    takeButton.setTooltip ("The plugin's panel as it plays now, as the ad's picture");
    takeButton.onClick = [this] { page.takeFormPicture(); };

    chooseButton.setButtonText (juce::String::fromUTF8 ("Choose a picture\xe2\x80\xa6"));
    chooseButton.setTooltip ("A PNG or JPEG of your own as the ad's picture");
    chooseButton.onClick = [this] { page.chooseFormPicture(); };

    styleNote (logoNote);
    chooseLogoButton.setButtonText (juce::String::fromUTF8 ("Choose a logo\xe2\x80\xa6"));
    chooseLogoButton.setTooltip ("Your logo (PNG, JPEG, GIF or SVG), beside your name on every ad you post. "
                                 "It's measured, trimmed and fitted by itself");
    chooseLogoButton.onClick = [this] { page.chooseFormLogo(); };
    removeLogoButton.setTooltip ("No logo on your ads from your next post on");
    removeLogoButton.onClick = [this]
    {
        StorePublisher::removeLogo();
        page.showFormLogo ({}, false);
        page.updateFormState();
    };

    downloadToggle.setColour (juce::ToggleButton::textColourId, Theme::text);
    downloadToggle.setColour (juce::ToggleButton::tickColourId, Theme::accent);
    downloadToggle.setTooltip ("People can add it to Sonify straight from the ad, in one click");
    downloadToggle.onClick = [this] { page.formEdited(); page.updateFormState(); };
    styleNote (downloadNote);

    accentButton (postButton);
    postButton.setTooltip ("Put the ad on Sonify's Store, open to every Sonify user, under your Fanan account. "
                           "Post again later to update it");
    postButton.onClick = [this] { page.postForm(); };

    styleNote (statusLabel);

    for (auto* c : std::initializer_list<juce::Component*> {
             &heading, &idLabel, &listingHeading, &linksHeading, &pictureHeading, &logoHeading, &downloadHeading, &writeButton, &writeNote,
             &titleLabel, &sectionLabel, &taglineLabel, &storyLabel, &featuresLabel, &titleEditor, &taglineEditor, &storyEditor,
             &featuresEditor, &sectionBox, &websiteLabel, &websiteNote, &videoLabel, &videoNote, &priceLabel, &priceNote, &formatsLabel,
             &websiteEditor, &videoEditor, &priceEditor, &pictureView, &takeButton, &chooseButton, &logoView, &chooseLogoButton,
             &removeLogoButton, &logoNote, &downloadToggle, &downloadNote, &postButton, &statusLabel })
        addAndMakeVisible (c);

    for (auto* button : formatButtons)
        addAndMakeVisible (button);
}

void StorePage::AdForm::layOut (int width)
{
    const auto formWidth = juce::jmin (maxFormWidth, width);
    auto area = juce::Rectangle<int> ((width - formWidth) / 2, 0, formWidth, 10000).reduced (formMargin, formMargin);

    auto top = area.removeFromTop (34);
    heading.setBounds (top.removeFromLeft (220));
    idLabel.setBounds (top);
    area.removeFromTop (formGap);

    // Two cards side by side (one under the other when it's narrow).
    const bool wide = area.getWidth() >= 760;
    auto left = wide ? area.removeFromLeft (area.getWidth() * 58 / 100) : area;
    auto right = wide ? area.withTrimmedLeft (formGap) : juce::Rectangle<int>();

    const auto leftTop = left.getY();
    auto inner = left.reduced (formGap, formGap);

    // The ad's heading, with Fanan AI's button on its right.
    auto row = inner.removeFromTop (rowHeight + 2);
    writeButton.setBounds (row.removeFromRight (240));
    row.removeFromRight (10);
    writeNote.setBounds (row.removeFromRight (juce::jmin (150, juce::jmax (0, row.getWidth() - 100))));
    listingHeading.setBounds (row);
    inner.removeFromTop (6);

    // The title, and its section beside it.
    {
        auto labels = inner.removeFromTop (labelHeight);
        auto fields = inner.removeFromTop (rowHeight);
        const auto sectionWidth = juce::jlimit (150, 240, fields.getWidth() * 2 / 5);
        sectionLabel.setBounds (labels.removeFromRight (sectionWidth));
        sectionBox.setBounds (fields.removeFromRight (sectionWidth));
        labels.removeFromRight (10);
        fields.removeFromRight (10);
        titleLabel.setBounds (labels);
        titleEditor.setBounds (fields);
    }

    inner.removeFromTop (10);
    taglineLabel.setBounds (inner.removeFromTop (labelHeight));
    taglineEditor.setBounds (inner.removeFromTop (rowHeight));
    inner.removeFromTop (10);
    storyLabel.setBounds (inner.removeFromTop (labelHeight));
    storyEditor.setBounds (inner.removeFromTop (190));
    inner.removeFromTop (10);
    featuresLabel.setBounds (inner.removeFromTop (labelHeight));
    featuresEditor.setBounds (inner.removeFromTop (120));
    inner.removeFromTop (10);
    videoLabel.setBounds (inner.removeFromTop (labelHeight));
    videoEditor.setBounds (inner.removeFromTop (rowHeight));
    inner.removeFromTop (4);
    videoNote.setBounds (inner.removeFromTop (labelHeight));

    // Where to get it: the website, the formats, the price or terms.
    inner.removeFromTop (formGap);
    linksHeading.setBounds (inner.removeFromTop (labelHeight + 4));
    inner.removeFromTop (6);
    websiteLabel.setBounds (inner.removeFromTop (labelHeight));
    websiteEditor.setBounds (inner.removeFromTop (rowHeight));
    inner.removeFromTop (4);
    websiteNote.setBounds (inner.removeFromTop (labelHeight));
    inner.removeFromTop (10);

    row = inner.removeFromTop (rowHeight);
    formatsLabel.setBounds (row.removeFromLeft (80));

    for (auto* button : formatButtons)
    {
        button->setBounds (row.removeFromLeft (80));
        row.removeFromLeft (6);
    }

    inner.removeFromTop (12);
    priceLabel.setBounds (inner.removeFromTop (labelHeight));
    priceEditor.setBounds (inner.removeFromTop (rowHeight));
    inner.removeFromTop (4);
    priceNote.setBounds (inner.removeFromTop (labelHeight));

    leftCard = left.withY (leftTop).withHeight (priceNote.getBottom() + formGap - leftTop);

    // The pictures, the Sonify version and the post: beside the form, or under it.
    if (! wide)
        right = juce::Rectangle<int> (left.getX(), leftCard.getBottom() + formGap, left.getWidth(), 10000);

    const auto rightTop = right.getY();
    inner = right.reduced (formGap, formGap);

    pictureHeading.setBounds (inner.removeFromTop (labelHeight + 4));
    inner.removeFromTop (8);
    pictureView.setBounds (inner.removeFromTop (juce::jlimit (160, 360, inner.getWidth() * 10 / 16)));
    inner.removeFromTop (10);

    row = inner.removeFromTop (rowHeight);
    const auto half = (row.getWidth() - 8) / 2;
    takeButton.setBounds (row.removeFromLeft (half));
    row.removeFromLeft (8);
    chooseButton.setBounds (row);

    inner.removeFromTop (formGap);
    logoHeading.setBounds (inner.removeFromTop (labelHeight + 4));
    inner.removeFromTop (8);
    row = inner.removeFromTop (72);
    logoView.setBounds (row.removeFromLeft (juce::jmin (220, row.getWidth() / 2)));
    row.removeFromLeft (10);
    chooseLogoButton.setBounds (row.removeFromTop (rowHeight));
    row.removeFromTop (8);
    removeLogoButton.setBounds (row.removeFromTop (rowHeight).withWidth (juce::jmin (row.getWidth(), 110)));
    inner.removeFromTop (6);
    logoNote.setBounds (inner.removeFromTop (48));

    inner.removeFromTop (formGap);
    downloadHeading.setBounds (inner.removeFromTop (labelHeight + 4));
    inner.removeFromTop (6);
    downloadToggle.setBounds (inner.removeFromTop (rowHeight));
    inner.removeFromTop (4);
    downloadNote.setBounds (inner.removeFromTop (64));

    inner.removeFromTop (16);
    postButton.setBounds (inner.removeFromTop (44));
    inner.removeFromTop (10);
    statusLabel.setBounds (inner.removeFromTop (96));

    rightCard = right.withY (rightTop).withHeight (statusLabel.getBottom() + formGap - rightTop);

    setSize (width, juce::jmax (leftCard.getBottom(), rightCard.getBottom()) + formMargin);
}

void StorePage::AdForm::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    for (const auto& card : { leftCard, rightCard })
    {
        if (card.isEmpty())
            continue;

        g.setColour (Theme::raised.withAlpha (0.55f));
        g.fillRoundedRectangle (card.toFloat(), 10.0f);
        g.setColour (Theme::outline);
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }
}

//==============================================================================
StorePage::StorePage (StoreClient& storeClient, StorePublisher& storePublisher)
    : client (storeClient), publisher (storePublisher)
{
    searchBox.setTextToShowWhenEmpty ("Search the Store", Theme::muted);
    searchBox.setFont (Theme::font (14.5f));
    searchBox.setIndents (10, 8);
    searchBox.onTextChange = [this] { startTimer (450); };
    searchBox.onReturnKey = [this] { stopTimer(); page = 1; load(); };

    kindBox.addItemList ({ "Every kind", "Instruments", "Effects", "MIDI FX", "Widgets" }, 1);
    kindBox.setSelectedId (1, juce::dontSendNotification);
    kindBox.onChange = [this] { fillSections(); page = 1; load(); };

    sectionBox.setTooltip ("Its section on the board");
    sectionBox.onChange = [this] { page = 1; load(); };
    fillSections();

    showBox.addItemList ({ "Every ad", "My ads" }, 1);
    showBox.setSelectedId (1, juce::dontSendNotification);
    showBox.setTooltip ("Every ad on the board, or the ones you posted (hidden ones too)");
    showBox.onChange = [this]
    {
        show = showBox.getSelectedId() == 2 ? Show::mine : Show::everything;
        page = 1;
        load();
    };

    refreshButton.setTooltip ("Ask the Store again");
    refreshButton.onClick = [this] { refresh(); };

    accentButton (postAdButton);
    postAdButton.setTooltip ("Post an ad for the plugin or MIDI FX open in the studio, with a link to your website");
    postAdButton.onClick = [this] { openForm(); };

    formBackButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80  Back to the board"));
    formBackButton.onClick = [this] { closeForm(); };

    previousButton.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));
    nextButton.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xb6"));
    previousButton.onClick = [this] { if (page > 1) { --page; load(); } };
    nextButton.onClick = [this] { if (page < pages) { ++page; load(); } };
    styleNote (pageLabel);
    pageLabel.setJustificationType (juce::Justification::centred);

    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, false);
    formViewport.setViewedComponent (&form, false);
    formViewport.setScrollBarsShown (true, false);

    for (auto* c : std::initializer_list<juce::Component*> { &searchBox, &kindBox, &sectionBox, &showBox, &refreshButton, &postAdButton,
                                                            &previousButton, &nextButton, &pageLabel, &viewport })
        addAndMakeVisible (c);

    addChildComponent (detail);
    addChildComponent (formViewport);
    addChildComponent (formBackButton);
    signedIn = client.isSignedIn();
    showFormLogo ({}, false);
}

StorePage::~StorePage()
{
    stopTimer();
    flushForm();
}

void StorePage::timerCallback()
{
    stopTimer();
    page = 1;
    load();
}

juce::String StorePage::kindFilter() const
{
    switch (kindBox.getSelectedId())
    {
        case 2:  return "instrument";
        case 3:  return "effect";
        case 4:  return "midifx";
        case 5:  return "widget";
        default: return {};
    }
}

juce::String StorePage::sectionFilter() const
{
    const auto id = sectionBox.getSelectedId();
    return id >= 2 ? sectionNames[id - 2] : juce::String();
}

void StorePage::fillSections()
{
    const auto kept = sectionFilter();
    const auto kind = kindFilter();

    sectionBox.clear (juce::dontSendNotification);
    sectionNames.clear();
    sectionBox.addItem ("Every section", 1);

    for (const auto* each : { "instrument", "effect", "midifx", "widget" })
    {
        if (kind.isNotEmpty() && kind != each)
            continue;

        StoreClient::Ad kindOf;
        kindOf.kind = each;

        if (kind.isEmpty())
            sectionBox.addSectionHeading (kindOf.kindName());

        const juce::String eachKind (each);   // the lists are the store's own, kept for good

        for (const auto& section : StoreClient::sectionsFor (eachKind))
        {
            sectionNames.add (section);
            sectionBox.addItem (section, sectionNames.size() + 1);
        }
    }

    const auto index = sectionNames.indexOf (kept);
    sectionBox.setSelectedId (index >= 0 ? index + 2 : 1, juce::dontSendNotification);
}

void StorePage::refresh()
{
    loadedOnce = true;
    load();
}

void StorePage::load()
{
    const auto number = ++loadNumber;
    loading = true;
    message = juce::String::fromUTF8 ("Asking the Store\xe2\x80\xa6");
    repaint();

    auto done = [safeThis = juce::Component::SafePointer<StorePage> (this), number] (const juce::Result& result, const StoreClient::Page& found)
    {
        if (safeThis != nullptr && number == safeThis->loadNumber)
            safeThis->showPage (result, found);
    };

    if (show == Show::mine)
        client.mine (done);
    else
        client.list (kindFilter(), sectionFilter(), searchBox.getText(), page, done);
}

void StorePage::showPage (const juce::Result& result, const StoreClient::Page& found)
{
    loading = false;
    ads.clear();

    if (result.wasOk())
    {
        // My ads is a short list: the kind, the section and the words filter it here.
        const auto kind = kindFilter();
        const auto section = sectionFilter();
        const auto words = searchBox.getText().trim();

        for (const auto& ad : found.ads)
            if (show == Show::everything
                || ((kind.isEmpty() || ad.kind == kind) && (section.isEmpty() || ad.section == section)
                    && (words.isEmpty() || ad.title.containsIgnoreCase (words) || ad.maker.containsIgnoreCase (words)
                        || ad.tagline.containsIgnoreCase (words))))
                ads.push_back (ad);

        page = found.page;
        pages = show == Show::everything ? found.pages : 1;
        total = show == Show::everything ? found.total : (int) ads.size();
    }

    if (result.failed())
        message = result.getErrorMessage();
    else if (! ads.empty())
        message.clear();
    else if (show == Show::mine)
        message = "You haven't posted an ad yet. Open your plugin or MIDI FX in the studio, then press Post an ad.";
    else if (sectionFilter().isNotEmpty())
        message = "Nothing in " + sectionFilter() + " yet.";
    else if (searchBox.getText().trim().isNotEmpty() || kindFilter().isNotEmpty())
        message = "Nothing matches.";
    else
        message = "The board is empty so far. Post the first ad: press Post an ad!";

    grid.cards.clear();

    for (const auto& ad : ads)
    {
        auto* card = grid.cards.add (new Card (*this, ad));
        card->setSelected (ad.id == selected);
        grid.addAndMakeVisible (card);

        client.picture (ad, [safeCard = juce::Component::SafePointer<Card> (card)] (const juce::Image& image)
        {
            if (safeCard != nullptr)
                safeCard->setPicture (image);
        });

        client.logo (ad, [safeCard = juce::Component::SafePointer<Card> (card)] (const juce::Image& image)
        {
            if (safeCard != nullptr)
                safeCard->setLogo (image);
        });
    }

    // The open one, as the Store has it now.
    for (const auto& ad : ads)
        if (ad.id == selected)
            detail.setAd (ad);

    pageLabel.setText (show == Show::everything && total > 0
                           ? "Page " + juce::String (page) + " of " + juce::String (pages) + dot() + juce::String (total) + (total == 1 ? " ad" : " ads")
                           : juce::String(),
                       juce::dontSendNotification);
    previousButton.setEnabled (page > 1);
    nextButton.setEnabled (page < pages);

    resized();
    repaint();
}

void StorePage::select (const StoreClient::Ad& ad)
{
    selected = ad.id;

    for (auto* card : grid.cards)
        card->setSelected (card->ad.id == selected);

    detail.setAd (ad);
    detail.setVisible (true);

    client.picture (ad, [safeThis = juce::Component::SafePointer<StorePage> (this), id = ad.id] (const juce::Image& image)
    {
        if (safeThis != nullptr && safeThis->detail.ad.id == id)
            safeThis->detail.setPicture (image);
    });

    client.logo (ad, [safeThis = juce::Component::SafePointer<StorePage> (this), id = ad.id] (const juce::Image& image)
    {
        if (safeThis != nullptr && safeThis->detail.ad.id == id)
            safeThis->detail.setLogo (image);
    });

    resized();
}

void StorePage::closeDetail()
{
    detail.stopVideo();
    selected.clear();
    detail.setVisible (false);

    for (auto* card : grid.cards)
        card->setSelected (false);

    resized();
}

void StorePage::showAd (const juce::String& adId)
{
    if (formOpen)
        closeForm();

    for (const auto& ad : ads)
    {
        if (ad.id == adId)
        {
            select (ad);
            return;
        }
    }

    client.get (adId, [safeThis = juce::Component::SafePointer<StorePage> (this)] (const juce::Result& result, const StoreClient::Ad& ad)
    {
        if (safeThis == nullptr)
            return;

        if (result.wasOk() && ad.isValid())
            safeThis->select (ad);
        else
        {
            safeThis->message = result.failed() ? result.getErrorMessage() : juce::String ("That ad isn't on the board any more.");
            safeThis->repaint();
        }
    });
}

void StorePage::connectionChanged()
{
    const auto now = client.isSignedIn();

    if (now == signedIn)
        return;

    signedIn = now;

    if (loadedOnce)
        load();   // My ads changed
}

void StorePage::updateCards()
{
    for (auto* card : grid.cards)
        card->repaint();
}

void StorePage::itemChanged()
{
    updateCards();

    if (detail.isVisible())
        detail.refreshState();
}

//==============================================================================
void StorePage::visitWebsite()
{
    const auto website = detail.ad.website;

    if (! StoreClient::isWebsite (website) || ! juce::URL (website).launchInDefaultBrowser())
        detail.setStatus ("The website couldn't be opened.", true);
}

void StorePage::addToSonify()
{
    const auto ad = detail.ad;

    client.install (ad,
                    [safeThis = juce::Component::SafePointer<StorePage> (this), id = ad.id] (const juce::String& step)
                    {
                        if (safeThis != nullptr && safeThis->detail.ad.id == id)
                            safeThis->detail.setStatus (step, false);
                    },
                    [safeThis = juce::Component::SafePointer<StorePage> (this), ad] (const juce::Result& result)
                    {
                        if (safeThis == nullptr)
                            return;

                        if (safeThis->detail.ad.id == ad.id)
                        {
                            if (result.failed())
                                safeThis->detail.setStatus (result.getErrorMessage(), true);
                            else
                                safeThis->detail.setStatus ((ad.isMidiFx() ? juce::String ("Added to My Devices: it's in the tracks' MIDI FX menu.")
                                                                           : juce::String ("Added: it's in Sonify's Browser, under Made in Sonify."))
                                                            + " If it's protected, it plays its demo until you register it with the serial "
                                                              "from the maker's website.", false);
                        }

                        safeThis->itemChanged();
                    });

    itemChanged();
}

void StorePage::showMoreMenu()
{
    const auto ad = detail.ad;
    juce::PopupMenu menu;

    menu.addItem (1, "Copy the website's address", ad.website.isNotEmpty());
    menu.addItem (2, "Copy the ad's id (" + ad.id + ")");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&detail.moreButton), [ad] (int choice)
    {
        if (choice == 1)
            juce::SystemClipboard::copyTextToClipboard (ad.website);
        else if (choice == 2)
            juce::SystemClipboard::copyTextToClipboard (ad.id);
    });
}

void StorePage::hideOrShow()
{
    const auto ad = detail.ad;
    const bool hide = ! ad.hidden;
    detail.setStatus (juce::String::fromUTF8 ("Asking the Store\xe2\x80\xa6"), false);

    client.hide (ad, hide, [safeThis = juce::Component::SafePointer<StorePage> (this), id = ad.id, hide] (const juce::Result& result)
    {
        if (safeThis == nullptr)
            return;

        if (safeThis->detail.ad.id == id)
        {
            if (result.failed())
            {
                safeThis->detail.setStatus (result.getErrorMessage(), true);
                return;
            }

            safeThis->detail.ad.hidden = hide;
            safeThis->detail.setStatus (hide ? "Hidden: it isn't on the board now." : "It's on the board again.", false);
            safeThis->detail.refreshState();
        }

        safeThis->load();
    });
}

void StorePage::deleteAd()
{
    const auto ad = detail.ad;

    auto options = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "Delete this ad?",
                                                                 "\"" + ad.title + "\" comes off the board for good. People who added its Sonify "
                                                                 "version keep it. You can post a new ad for it any time.",
                                                                 "Delete", "Cancel", this);

    juce::AlertWindow::showAsync (options, [safeThis = juce::Component::SafePointer<StorePage> (this), ad] (int answer)
    {
        if (safeThis == nullptr || answer != 1)
            return;

        safeThis->client.remove (ad, [safeThis, ad] (const juce::Result& result)
        {
            if (safeThis == nullptr)
                return;

            if (result.failed())
            {
                if (safeThis->detail.ad.id == ad.id)
                    safeThis->detail.setStatus (result.getErrorMessage(), true);

                return;
            }

            // The project's ad here forgets it: its next post is a new ad.
            if (ad.key.isNotEmpty())
            {
                if (auto kept = StorePublisher::load (ad.key, ad.kind); kept.ad == ad.id)
                {
                    kept.ad.clear();
                    kept.postedVersion = 0;
                    kept.postedAt.clear();
                    StorePublisher::save (kept);

                    if (safeThis->formListing.key == ad.key)
                        safeThis->formListing = kept;
                }
            }

            if (safeThis->detail.ad.id == ad.id)
                safeThis->closeDetail();

            if (safeThis->onNotice != nullptr)
                safeThis->onNotice ("Ad deleted");

            safeThis->load();
        });
    });
}

//==============================================================================
StorePage::Subject StorePage::currentSubject() const
{
    return getSubject != nullptr ? getSubject() : Subject();
}

void StorePage::openForm()
{
    const auto subject = currentSubject();

    if (subject.key.isEmpty())
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::InfoIcon, "Open what you want to advertise",
                                                                              "Open your plugin or MIDI FX in the studio first (or ask Fanan AI for "
                                                                              "one in Build with AI), then press Post an ad. To change an ad you "
                                                                              "posted, open it in My ads and press Edit the ad.",
                                                                              "OK", this),
                                      nullptr);
        return;
    }

    flushForm();
    formListing = StorePublisher::load (subject.key, subject.kind);
    formPicture = StorePublisher::loadPicture (subject.key);
    formMaker = subject.maker;
    formHadPackage = formListing.wasPosted() && formListing.offersDownload;

    if (formListing.title.isEmpty())
        formListing.title = subject.name.substring (0, StorePublisher::maxTitle);

    showForm();
}

void StorePage::editAd (const StoreClient::Ad& ad)
{
    if (! ad.isValid() || ! ad.mine)
        return;

    flushForm();

    const auto key = ad.key.isNotEmpty() ? ad.key : "ad-" + ad.id;
    formListing = StorePublisher::load (key, ad.kind);

    // What's kept here is what was typed last on this computer; otherwise the ad as it's posted.
    if (formListing.ad != ad.id)
    {
        formListing.ad = ad.id;
        formListing.title = ad.title;
        formListing.tagline = ad.tagline;
        formListing.story = ad.story;
        formListing.section = StoreClient::sectionsFor (ad.kind).contains (ad.section) ? ad.section : juce::String();
        formListing.video = ad.video;
        formListing.website = ad.website;
        formListing.priceText = ad.priceText;
        formListing.features = ad.features;
        formListing.formats = ad.formats;
        formListing.offersDownload = ad.offersDownload;
        formListing.postedVersion = juce::jmax (1, ad.version);
    }

    formMaker = ad.maker;
    formHadPackage = ad.canDownload();
    formPicture = StorePublisher::loadPicture (key);

    if (! formPicture.isValid())
    {
        // The posted picture, from the Store.
        client.picture (ad, [safeThis = juce::Component::SafePointer<StorePage> (this), key] (const juce::Image& image)
        {
            if (safeThis != nullptr && image.isValid() && safeThis->formListing.key == key && ! safeThis->formPicture.isValid())
                safeThis->setFormPicture (image);
        });
    }

    showForm();
}

void StorePage::showForm()
{
    detail.stopVideo();
    formOpen = true;
    form.pictureView.setImage (formPicture);
    setFormStatus ({}, false);
    showFormListing();
    showFormLogo ({}, false);
    updateFormState();
    formViewport.setViewPosition (0, 0);
    resized();
    repaint();
}

void StorePage::closeForm()
{
    flushForm();
    formOpen = false;
    resized();
    repaint();

    if (loadedOnce)
        load();
}

void StorePage::studioChanged()
{
    if (formOpen)
        updateFormState();
}

void StorePage::showFormListing()
{
    const juce::ScopedValueSetter<bool> quiet (formShowing, true);

    form.titleEditor.setText (formListing.title, false);

    const auto& sections = StoreClient::sectionsFor (formListing.kind);
    form.sectionBox.clear (juce::dontSendNotification);
    form.sectionBox.addItemList (sections, 1);
    form.sectionBox.setSelectedItemIndex (sections.indexOf (formListing.section), juce::dontSendNotification);
    form.taglineEditor.setText (formListing.tagline, false);
    form.storyEditor.setText (formListing.story, false);
    form.featuresEditor.setText (formListing.features.joinIntoString ("\n"), false);
    form.videoEditor.setText (formListing.video, false);
    form.websiteEditor.setText (formListing.website, false);
    form.priceEditor.setText (formListing.priceText, false);

    for (int i = 0; i < form.formatButtons.size(); ++i)
        form.formatButtons[i]->setToggleState (formListing.formats.contains (StoreClient::allFormats()[i]), juce::dontSendNotification);

    form.downloadToggle.setToggleState (formListing.offersDownload, juce::dontSendNotification);
    showFormNotes();
}

void StorePage::readFormFields()
{
    if (formListing.key.isEmpty())
        return;

    formListing.title = form.titleEditor.getText().trim();
    formListing.section = form.sectionBox.getSelectedItemIndex() >= 0 ? form.sectionBox.getText() : juce::String();
    formListing.tagline = form.taglineEditor.getText().trim();
    formListing.story = form.storyEditor.getText().trim();
    formListing.video = form.videoEditor.getText().trim();
    formListing.website = form.websiteEditor.getText().trim();
    formListing.priceText = form.priceEditor.getText().trim();
    formListing.offersDownload = form.downloadToggle.getToggleState();
    formListing.features.clear();
    formListing.formats.clear();

    for (const auto& line : juce::StringArray::fromLines (form.featuresEditor.getText()))
        if (const auto feature = line.trim().trimCharactersAtStart (juce::String::fromUTF8 ("-*\xe2\x80\xa2 ")).trim().substring (0, StorePublisher::maxFeature);
            feature.isNotEmpty() && formListing.features.size() < StorePublisher::maxFeatures)
            formListing.features.add (feature);

    for (int i = 0; i < form.formatButtons.size(); ++i)
        if (form.formatButtons[i]->getToggleState() || (formListing.offersDownload && StoreClient::allFormats()[i] == "Sonify"))
            formListing.formats.add (StoreClient::allFormats()[i]);
}

void StorePage::formEdited()
{
    if (formShowing || formListing.key.isEmpty())
        return;

    formSaver.startTimer (500);
}

void StorePage::flushForm()
{
    formSaver.stopTimer();

    if (! formOpen || formListing.key.isEmpty())
        return;

    readFormFields();
    StorePublisher::save (formListing);
}

void StorePage::setFormStatus (const juce::String& text, bool problem)
{
    form.statusLabel.setText (text, juce::dontSendNotification);
    form.statusLabel.setColour (juce::Label::textColourId, problem ? problemColour : Theme::muted);
}

void StorePage::setFormPicture (const juce::Image& image)
{
    if (formListing.key.isEmpty())
        return;

    formPicture = image;
    form.pictureView.setImage (formPicture);

    if (const auto kept = StorePublisher::savePicture (formListing.key, formPicture); kept.failed())
        setFormStatus (kept.getErrorMessage(), true);
}

void StorePage::showFormNotes()
{
    // The website.
    const auto website = StorePublisher::completeWebsite (form.websiteEditor.getText());

    if (website.isEmpty())
        form.websiteNote.setText ("Visit website opens it: people buy it there, and get their serial from you.", juce::dontSendNotification);
    else if (StoreClient::isWebsite (website))
        form.websiteNote.setText (juce::String::fromUTF8 ("\xe2\x9c\x93  ") + website, juce::dontSendNotification);
    else
        form.websiteNote.setText ("That doesn't look like a website's address (like https://www.example.com).", juce::dontSendNotification);

    form.websiteNote.setColour (juce::Label::textColourId, website.isNotEmpty() && ! StoreClient::isWebsite (website) ? problemColour : Theme::muted);

    // The video.
    const auto link = form.videoEditor.getText().trim();
    const auto id = StoreClient::youtubeId (link);

    if (link.isEmpty())
        form.videoNote.setText ("A video of it playing: it plays right on its ad.", juce::dontSendNotification);
    else if (id.isNotEmpty())
        form.videoNote.setText (juce::String::fromUTF8 ("\xe2\x9c\x93  A YouTube video (") + id + "): it plays on its ad.", juce::dontSendNotification);
    else
        form.videoNote.setText ("That isn't a YouTube video's link.", juce::dontSendNotification);

    form.videoNote.setColour (juce::Label::textColourId, link.isNotEmpty() && id.isEmpty() ? problemColour : Theme::muted);
}

void StorePage::showFormLogo (const juce::String& note, bool problem)
{
    const auto logo = StorePublisher::loadLogo();
    form.logoView.setImage (logo);

    auto text = note;

    if (text.isEmpty())
        text = logo.isValid() ? "Beside your name on every ad you post, from your next post on."
                              : "Optional: beside your name on every ad you post. It's measured, trimmed and fitted by itself.";

    form.logoNote.setText (text, juce::dontSendNotification);
    form.logoNote.setColour (juce::Label::textColourId, problem ? problemColour : Theme::muted);
}

void StorePage::updateFormState()
{
    const auto subject = currentSubject();
    const bool attached = subject.key.isNotEmpty() && subject.key == formListing.key;
    const bool midi = formListing.kind == "midifx";
    const bool idle = ! publisher.isPosting() && ! publisher.isWriting();
    const bool offering = form.downloadToggle.getToggleState();

    formViewport.setVisible (formOpen);
    formBackButton.setVisible (formOpen);

    // The ad's id, for good once it's posted.
    juce::String idText;

    if (formListing.ad.isEmpty())
        idText = "Not on the board yet";
    else if (formListing.wasPosted())
        idText = "Ad " + formListing.ad + (formListing.postedAt.isNotEmpty() ? ", posted " + formatDate (formListing.postedAt) : juce::String());
    else
        idText = "Ad " + formListing.ad + " (not posted yet)";

    form.idLabel.setText (idText, juce::dontSendNotification);
    form.heading.setText (formListing.wasPosted() ? "Edit your ad" : "Your ad", juce::dontSendNotification);

    // The Sonify version: sent from the project open in the studio, or the one posted before.
    form.downloadToggle.setEnabled (idle && (attached || formHadPackage || offering));

    juce::String note;

    if (! offering)
        note = "People get it from your website. Tick this to also let them add the Sonify version here, in one click.";
    else if (! attached && formHadPackage)
        note = "The Sonify version you posted before stays. To send a new one, open this " + juce::String (midi ? "MIDI FX" : "plugin")
             + " in the studio, then post again.";
    else if (! attached)
        note = "Open this " + juce::String (midi ? "MIDI FX" : "plugin") + " in the studio to send its Sonify version (or untick this).";
    else if (midi && subject.isProtected)
        note = "Protected: its last build goes with the ad (never its code), and it holds notes back in its demo until the buyer "
               "registers it with the serial they get from your website.";
    else if (subject.isProtected)
        note = "Protected: it's built with its protection and plays its demo until the buyer registers it with the serial they get "
               "from your website.";
    else
        note = "Not protected: it plays fully for everyone who adds it. To sell it, turn its protection on in the Protection tab first.";

    form.downloadNote.setText (note, juce::dontSendNotification);
    form.downloadNote.setColour (juce::Label::textColourId, offering && ! attached && ! formHadPackage ? problemColour : Theme::muted);

    for (int i = 0; i < form.formatButtons.size(); ++i)
    {
        auto* button = form.formatButtons[i];

        if (StoreClient::allFormats()[i] == "Sonify" && offering)
            button->setToggleState (true, juce::dontSendNotification);

        button->setEnabled (idle && ! (StoreClient::allFormats()[i] == "Sonify" && offering));
    }

    form.takeButton.setEnabled (idle && attached && subject.hasPanel);
    form.takeButton.setTooltip (! attached ? juce::String ("Open its project in the studio to take its panel")
                                : subject.hasPanel ? juce::String ("The plugin's panel as it plays now, as the ad's picture")
                                : (midi ? juce::String ("A MIDI FX has no panel: choose a picture") : juce::String ("It has no panel yet, or it isn't playing")));
    form.chooseButton.setEnabled (idle);
    form.writeButton.setEnabled (idle && attached);
    form.writeButton.setTooltip (attached ? "Fanan AI writes the tagline, story and features from its code and its picture. "
                                            "Uses credits. Change them after as you like"
                                          : "Open its project in the studio: Fanan AI writes from its code");
    form.writeButton.setButtonText (publisher.isWriting() ? juce::String::fromUTF8 ("Fanan AI is writing\xe2\x80\xa6")
                                                          : juce::String ("Write the story with Fanan AI"));
    form.postButton.setEnabled (idle);
    form.postButton.setButtonText (publisher.isPosting() ? juce::String::fromUTF8 ("Posting\xe2\x80\xa6")
                                                         : formListing.wasPosted() ? juce::String ("Update the ad") : juce::String ("Post the ad"));

    for (auto* editor : { &form.titleEditor, &form.taglineEditor, &form.storyEditor, &form.featuresEditor, &form.websiteEditor,
                          &form.videoEditor, &form.priceEditor })
        editor->setReadOnly (! idle);

    form.sectionBox.setEnabled (idle);
    form.chooseLogoButton.setEnabled (idle);
    form.removeLogoButton.setEnabled (idle && StorePublisher::logoFile().existsAsFile());
    form.layOut (juce::jmax (320, formViewport.getMaximumVisibleWidth()));
}

void StorePage::chooseFormLogo()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose your logo", juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                                                   "*.png;*.jpg;*.jpeg;*.gif;*.svg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer<StorePage> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || fc.getResult() == juce::File())
                                  return;

                              juce::String report;
                              const auto imported = StorePublisher::importLogo (fc.getResult(), report);
                              safeThis->showFormLogo (report, imported.failed());
                              safeThis->updateFormState();
                          });
}

void StorePage::chooseFormPicture()
{
    if (formListing.key.isEmpty())
        return;

    chooser = std::make_unique<juce::FileChooser> ("Choose the ad's picture", juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                                                   "*.png;*.jpg;*.jpeg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer<StorePage> (this)] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr || fc.getResult() == juce::File())
                                  return;

                              const auto image = juce::ImageFileFormat::loadFrom (fc.getResult());

                              if (! image.isValid())
                              {
                                  safeThis->setFormStatus ("That file isn't a picture Sonify can read (PNG or JPEG).", true);
                                  return;
                              }

                              safeThis->setFormPicture (image);
                              safeThis->setFormStatus ({}, false);
                          });
}

void StorePage::takeFormPicture()
{
    const auto subject = currentSubject();

    if (takePicture == nullptr || subject.key != formListing.key)
        return;

    juce::String problem;
    const auto picture = takePicture (problem);

    if (! picture.isValid())
    {
        setFormStatus (problem.isNotEmpty() ? problem : juce::String ("The panel couldn't be taken."), true);
        return;
    }

    setFormPicture (picture);
    setFormStatus ("Taken: the panel as it plays now.", false);
}

void StorePage::writeFormStory()
{
    const auto subject = currentSubject();

    if (publisher.isWriting() || formListing.key.isEmpty() || subject.key != formListing.key)
        return;

    readFormFields();
    setFormStatus (juce::String::fromUTF8 ("Fanan AI is writing its story\xe2\x80\xa6"), false);

    publisher.writeStory (storyFacts != nullptr ? storyFacts() : juce::String(), formPicture, formListing,
                          [safeThis = juce::Component::SafePointer<StorePage> (this)] (const juce::Result& result, const StorePublisher::Listing& story)
                          {
                              if (safeThis == nullptr)
                                  return;

                              if (safeThis->formListing.key == story.key)
                              {
                                  if (result.failed())
                                  {
                                      safeThis->setFormStatus (result.getErrorMessage(), true);
                                  }
                                  else
                                  {
                                      // Only what Fanan AI wrote: what was typed meanwhile elsewhere stays.
                                      safeThis->readFormFields();
                                      safeThis->formListing.tagline = story.tagline;
                                      safeThis->formListing.story = story.story;
                                      safeThis->formListing.features = story.features;
                                      safeThis->formListing.section = story.section;
                                      StorePublisher::save (safeThis->formListing);
                                      safeThis->showFormListing();
                                      safeThis->setFormStatus ("Written. Read it over and change anything you like.", false);
                                  }
                              }

                              safeThis->updateFormState();
                          });

    updateFormState();
}

void StorePage::postForm()
{
    if (publisher.isPosting() || formListing.key.isEmpty())
        return;

    readFormFields();
    formListing.website = StorePublisher::completeWebsite (formListing.website);
    StorePublisher::save (formListing);
    showFormNotes();

    const auto subject = currentSubject();
    const bool attached = subject.key.isNotEmpty() && subject.key == formListing.key;

    if (const auto problem = StorePublisher::problemWith (formListing, formPicture); problem.isNotEmpty())
    {
        setFormStatus (problem, true);
        return;
    }

    StorePublisher::Item item;
    item.listing = formListing;
    item.picture = formPicture;
    item.maker = attached && subject.maker.isNotEmpty() ? subject.maker : formMaker;
    item.sendPackage = formListing.offersDownload && attached;

    if (formListing.offersDownload && ! attached && ! formHadPackage)
    {
        setFormStatus ("To offer the Sonify version, open this " + juce::String (formListing.kind == "midifx" ? "MIDI FX" : "plugin")
                       + " in the studio first (or untick it).", true);
        return;
    }

    if (item.sendPackage)
    {
        juce::String problem;

        if (makeItem == nullptr || ! makeItem (item, problem))
        {
            setFormStatus (problem.isNotEmpty() ? problem : juce::String ("Its Sonify version couldn't be made ready."), true);
            return;
        }
    }

    const bool sendsPackage = item.sendPackage;

    publisher.post (std::move (item),
                    [safeThis = juce::Component::SafePointer<StorePage> (this)] (const juce::String& step)
                    {
                        if (safeThis != nullptr)
                            safeThis->setFormStatus (step, false);
                    },
                    [safeThis = juce::Component::SafePointer<StorePage> (this), sendsPackage] (const juce::Result& result, const StorePublisher::Listing& listing)
                    {
                        if (safeThis == nullptr)
                            return;

                        if (safeThis->formListing.key == listing.key)
                        {
                            // Its ad id, kept even when the post failed after it came.
                            safeThis->formListing.ad = listing.ad;
                            safeThis->formListing.postedVersion = listing.postedVersion;
                            safeThis->formListing.postedAt = listing.postedAt;

                            if (result.wasOk())
                                safeThis->formHadPackage = listing.offersDownload && (sendsPackage || safeThis->formHadPackage);

                            safeThis->setFormStatus (result.failed() ? result.getErrorMessage()
                                                                     : juce::String ("Posted: your ad is on Sonify's Store, open to every Sonify user."),
                                                     result.failed());
                        }

                        safeThis->updateFormState();

                        if (result.wasOk())
                        {
                            if (safeThis->onNotice != nullptr)
                                safeThis->onNotice ("Ad posted");

                            safeThis->loadedOnce = true;
                        }
                    });

    updateFormState();
}

//==============================================================================
void StorePage::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    g.setColour (Theme::window.withAlpha (0.5f));
    g.fillRect (headerArea);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) getWidth());

    g.setColour (Theme::text);
    g.setFont (Theme::font (20.0f, true));
    g.drawText ("Store", headerArea.withTrimmedLeft (20).withWidth (90), juce::Justification::centredLeft, false);

    if (formOpen)
        return;

    if (message.isNotEmpty() && ads.empty())
    {
        auto area = viewport.getBounds().withSizeKeepingCentre (juce::jmin (560, viewport.getWidth() - 40), 120);
        g.setColour (loading ? Theme::muted : Theme::text.withAlpha (0.85f));
        g.setFont (Theme::font (15.5f));
        g.drawFittedText (message, area, juce::Justification::centred, 4, 1.0f);
    }
}

void StorePage::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (headerHeight);

    auto header = headerArea.reduced (16, 14);
    header.removeFromLeft (100);

    // The board's controls, or (while the ad form is open) the way back.
    for (auto* c : std::initializer_list<juce::Component*> { &searchBox, &kindBox, &sectionBox, &showBox, &refreshButton, &postAdButton })
        c->setVisible (! formOpen);

    formBackButton.setVisible (formOpen);
    formViewport.setVisible (formOpen);
    viewport.setVisible (! formOpen);

    if (formOpen)
    {
        formBackButton.setBounds (header.removeFromRight (190));
        detail.setVisible (false);
        previousButton.setVisible (false);
        nextButton.setVisible (false);
        pageLabel.setVisible (false);
        formViewport.setBounds (area);
        form.layOut (juce::jmax (320, formViewport.getMaximumVisibleWidth()));
        return;
    }

    detail.setVisible (selected.isNotEmpty() && detail.ad.isValid());

    postAdButton.setBounds (header.removeFromRight (120));
    header.removeFromRight (10);
    refreshButton.setBounds (header.removeFromRight (90));
    header.removeFromRight (10);
    showBox.setBounds (header.removeFromRight (120));
    header.removeFromRight (10);
    sectionBox.setBounds (header.removeFromRight (180));
    header.removeFromRight (10);
    kindBox.setBounds (header.removeFromRight (140));
    header.removeFromRight (10);
    searchBox.setBounds (header.withWidth (juce::jmin (header.getWidth(), 420)));

    footerArea = area.removeFromBottom (pages > 1 || total > 0 ? footerHeight : 0);
    bodyArea = area;
    layOutBody();

    auto footer = footerArea.withWidth (viewport.getWidth()).reduced (16, 8);
    const bool paging = pages > 1;
    previousButton.setVisible (paging);
    nextButton.setVisible (paging);
    pageLabel.setVisible (footerArea.getHeight() > 0);

    if (paging)
    {
        previousButton.setBounds (footer.removeFromLeft (40));
        nextButton.setBounds (footer.removeFromRight (40));
    }

    pageLabel.setBounds (footer);
}

void StorePage::layOutBody()
{
    auto body = bodyArea;
    const bool open = detail.isVisible() && detail.ad.isValid();
    const bool narrow = getWidth() < narrowWidth;

    detail.backButton.setVisible (narrow);

    if (open && narrow)
    {
        detail.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
        viewport.setBounds (body.withWidth (0));
        detail.toFront (false);
    }
    else
    {
        if (open)
            detail.setBounds (body.removeFromRight (detailWidth).withBottom (getHeight()));

        viewport.setBounds (body);
    }

    grid.layOut (juce::jmax (Card::width + 2 * Grid::margin, viewport.getMaximumVisibleWidth()));
    detail.resized();
}
