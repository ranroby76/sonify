// D:\workspace\Fanan_web_hub\src\StorePage.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "StoreClient.h"
#include "StorePublisher.h"

#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Plugins Creator Studio's Store tab: Sonify's Store, a notice board open to every user,
    where makers post ads for their plugins and MIDI FX with a link to their own website (where
    they sell them and register their buyers). Nothing is sold here.

        top     words to look for, the kind (instruments, effects, MIDI FX, widgets), its section
                (each kind's own: Synths, Reverb & Space, Arpeggiators...), what to show (every
                ad, or My ads), and Post an ad
        middle  the ads as cards (picture, title, maker, tagline, section), page by page
        right   the chosen one: its picture, story and features, its maker's logo and demo video
                (YouTube, played right there), the formats it comes in, its price or terms as its
                maker wrote them, Visit website (the default browser), and Add to Sonify when the
                ad offers the Sonify version; for the poster's own, Edit, Hide (or Show) and Delete

    The ad form (Post an ad, or Edit) is for the plugin or MIDI FX open in the studio, or an ad
    already posted: its title, section, tagline, story (Fanan AI can write it: credits),
    features, YouTube link, picture, logo, website (required), formats, price or terms, and
    "Offer the Sonify version for instant download". What's typed is kept as it's typed
    (StorePublisher's listing, per project).

    The work is the StoreClient's and the StorePublisher's; the studio gives what's open in it
    through the callbacks.
*/
class StorePage final : public juce::Component,
                        private juce::Timer
{
public:
    StorePage (StoreClient& client, StorePublisher& publisher);
    ~StorePage() override;

    /** What's open in the studio, as the ad form needs it. */
    struct Subject
    {
        juce::String key;          // the project's id or the MIDI FX's (empty: nothing open)
        juce::String kind;         // "instrument", "effect", "widget" or "midifx"
        juce::String name;         // its name: a new ad's title starts as it
        juce::String maker;        // the developer's name the ad shows
        bool hasPanel = false;     // a plugin playing, with a panel to take a picture of
        bool isProtected = false;  // its protection is on (its download plays a demo)
    };

    /** What's open in the studio now. */
    std::function<Subject()> getSubject;

    /** The panel as it plays now (invalid, and problem says why, when it can't be taken). */
    std::function<juce::Image (juce::String& problem)> takePicture;

    /** What Fanan AI writes the story from: the open plugin's or MIDI FX's facts and code. */
    std::function<juce::String()> storyFacts;

    /** The open plugin or MIDI FX packed into the item (its Sonify version): false, and problem
        says why, when it can't be. */
    std::function<bool (StorePublisher::Item& item, juce::String& problem)> makeItem;

    /** A short notice for the studio's top bar ("Ad posted"). */
    std::function<void (const juce::String& text)> onNotice;

    /** The list again (the tab opened, or Refresh). */
    void refresh();

    /** That ad, opened. */
    void showAd (const juce::String& adId);

    /** Signed in or out: My ads changes. */
    void connectionChanged();

    /** What's open in the studio may have changed (the tab shows): the ad form follows it. */
    void studioChanged();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class Show { everything, mine };

    //==========================================================================
    class Card final : public juce::Component
    {
    public:
        Card (StorePage& owner, const StoreClient::Ad& ad);

        void setPicture (const juce::Image& image)    { picture = image; repaint(); }
        void setLogo (const juce::Image& image)       { logo = image; repaint(); }
        void setSelected (bool shouldBeSelected)      { selected = shouldBeSelected; repaint(); }
        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override   { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { hover = false; repaint(); }
        void mouseUp (const juce::MouseEvent&) override;

        StoreClient::Ad ad;

        static constexpr int width = 232, height = 250;

    private:
        StorePage& page;
        juce::Image picture, logo;
        bool selected = false, hover = false;
    };

    //==========================================================================
    class Grid final : public juce::Component
    {
    public:
        void layOut (int width);
        juce::OwnedArray<Card> cards;
        static constexpr int gap = 16, margin = 20;
    };

    //==========================================================================
    class Detail final : public juce::Component
    {
    public:
        explicit Detail (StorePage& owner);

        void setAd (const StoreClient::Ad& ad);
        void setPicture (const juce::Image& image)    { picture = image; repaint(); }
        void setLogo (const juce::Image& image)       { logo = image; repaint(); }
        void refreshState();                          // the buttons, as things stand now

        /** Its demo video, playing in place of its picture (or in the browser, where Sonify
            can't show web pages); stopped when another ad opens. */
        void playVideo();
        void stopVideo();
        void setStatus (const juce::String& text, bool problem);

        void paint (juce::Graphics&) override;
        void resized() override;

        StoreClient::Ad ad;
        juce::TextButton backButton, websiteButton, addButton, moreButton;
        juce::TextButton editButton { "Edit the ad" }, hideButton, deleteButton { "Delete" };
        juce::TextButton watchButton, youtubeButton;
        juce::Label warningLabel, statusLabel;
        juce::TextEditor storyText;

    private:
        StorePage& page;
        juce::Image picture, logo;
        juce::Rectangle<int> pictureArea, textArea, logoArea;
        std::unique_ptr<juce::Component> video;     // the player, while it plays
    };

    //==========================================================================
    /** A picture fitted in its box (or a note when there's none). */
    class PictureView final : public juce::Component
    {
    public:
        explicit PictureView (const juce::String& whenEmpty) : emptyText (whenEmpty) {}

        void setImage (const juce::Image& newImage)    { image = newImage; repaint(); }
        void paint (juce::Graphics&) override;

    private:
        juce::Image image;
        juce::String emptyText;
    };

    //==========================================================================
    /** The ad form: Post an ad, or Edit the ad. */
    class AdForm final : public juce::Component
    {
    public:
        explicit AdForm (StorePage& owner);

        void layOut (int width);   // sets its own height
        void paint (juce::Graphics&) override;
        void resized() override    {}

        StorePage& page;

        juce::Label heading, idLabel;
        juce::Label listingHeading, linksHeading, pictureHeading, logoHeading, downloadHeading;
        juce::TextButton writeButton;
        juce::Label writeNote;

        juce::Label titleLabel, sectionLabel, taglineLabel, storyLabel, featuresLabel;
        juce::TextEditor titleEditor, taglineEditor, storyEditor, featuresEditor;
        juce::ComboBox sectionBox;

        juce::Label websiteLabel, websiteNote, videoLabel, videoNote, priceLabel, priceNote, formatsLabel;
        juce::TextEditor websiteEditor, videoEditor, priceEditor;
        juce::OwnedArray<juce::TextButton> formatButtons;   // in StoreClient::allFormats() order

        PictureView pictureView { "No picture yet.\nTake the panel as it plays, or choose a picture." };
        juce::TextButton takeButton { "Take the panel" }, chooseButton;

        PictureView logoView { "No logo yet." };
        juce::TextButton chooseLogoButton, removeLogoButton { "Remove" };
        juce::Label logoNote;

        juce::ToggleButton downloadToggle { "Offer the Sonify version for instant download" };
        juce::Label downloadNote;

        juce::TextButton postButton { "Post the ad" };
        juce::Label statusLabel;

        juce::Rectangle<int> leftCard, rightCard;
    };

    //==========================================================================
    void timerCallback() override;
    void load();                                           // the list for the filters and page now
    void showPage (const juce::Result& result, const StoreClient::Page& page);
    void select (const StoreClient::Ad& ad);
    void closeDetail();
    void layOutBody();
    void updateCards();                                    // installed marks
    juce::String kindFilter() const;
    juce::String sectionFilter() const;
    void fillSections();                                   // the kind's sections (every kind's, under their kinds)

    // What the detail's buttons do.
    void visitWebsite();
    void addToSonify();
    void showMoreMenu();
    void hideOrShow();
    void deleteAd();
    void itemChanged();                                    // after something was done: the marks and buttons again

    // The ad form.
    Subject currentSubject() const;
    void openForm();                                       // Post an ad: for what's open in the studio
    void editAd (const StoreClient::Ad& ad);               // one of My ads
    void showForm();
    void closeForm();
    void showFormListing();                                // the listing into the fields
    void readFormFields();                                 // the fields into the listing
    void formEdited();                                     // typed: kept in a moment
    void flushForm();                                      // kept now
    void updateFormState();                                // what shows and what's enabled
    void setFormStatus (const juce::String& text, bool problem);
    void setFormPicture (const juce::Image& image);        // shown and kept
    void showFormLogo (const juce::String& note, bool problem);
    void showFormNotes();                                  // the website's, the video's and the download's
    void chooseFormPicture();
    void chooseFormLogo();
    void takeFormPicture();
    void writeFormStory();
    void postForm();

    StoreClient& client;
    StorePublisher& publisher;

    juce::TextEditor searchBox;
    juce::ComboBox kindBox, sectionBox, showBox;
    juce::StringArray sectionNames;                        // in sectionBox's order (its ids from 2)
    juce::TextButton refreshButton { "Refresh" }, postAdButton { "Post an ad" }, formBackButton, previousButton, nextButton;
    juce::Label pageLabel;

    Grid grid;
    juce::Viewport viewport;
    Detail detail { *this };

    AdForm form { *this };
    juce::Viewport formViewport;
    bool formOpen = false;
    StorePublisher::Listing formListing;
    juce::Image formPicture;
    juce::String formMaker;                                // the maker's name, for an ad edited while its project isn't open
    bool formHadPackage = false;                           // the ad offers a package already (kept when none is sent)
    bool formShowing = false;                              // the fields are being filled (not typed)
    juce::TimedCallback formSaver { [this] { flushForm(); } };
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<StoreClient::Ad> ads;
    juce::String selected;                                 // the ad open on the right
    juce::String message;                                  // in place of the cards: empty, or why not
    Show show = Show::everything;
    int page = 1, pages = 1, total = 0;
    int loadNumber = 0;                                    // an older answer that comes late is ignored
    bool loading = false, loadedOnce = false, signedIn = false;
    juce::Rectangle<int> headerArea, footerArea, bodyArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StorePage)
};
