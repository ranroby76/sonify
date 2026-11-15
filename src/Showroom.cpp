// D:\workspace\Fanan_web_hub\src\Showroom.cpp

#include "Showroom.h"

#include "Body3D.h"
#include "PluginCanvas.h"
#include "StellaLookAndFeel.h"

namespace
{
    constexpr float pi = 3.14159265358979f;

    const juce::Colour backdropTop { 0xff26272c }, backdropBottom { 0xff0e0f11 };
    const juce::Colour axisColours[3] { juce::Colour (0xffe5484d), juce::Colour (0xff46c45a), juce::Colour (0xff3d8bff) };
    const juce::Colour hoverColour { 0xffffd23f };
    const juce::Colour chosenColour { 0xffff9a3c };

    /** A picture on the Showroom's backdrop: what's see-through over its dark. */
    juce::Image onBackdrop (const juce::Image& picture)
    {
        juce::Image out (juce::Image::ARGB, picture.getWidth(), picture.getHeight(), true);
        juce::Graphics g (out);
        g.setGradientFill (juce::ColourGradient (backdropTop, 0.0f, 0.0f, backdropBottom, 0.0f, (float) picture.getHeight(), false));
        g.fillAll();
        g.drawImageAt (picture, 0, 0);
        return out;
    }

    float numberOf (const juce::var& v, const char* key, float fallback)
    {
        const auto x = v.getProperty (key, {});
        return x.isVoid() ? fallback : (float) (double) x;
    }

    void setProperty (const juce::var& v, const juce::Identifier& key, const juce::var& value)
    {
        if (auto* o = v.getDynamicObject())
            o->setProperty (key, value);
    }

    double tidy (float v)    { return std::round ((double) v * 100.0) / 100.0; }

    //==========================================================================
    // Turns: row-major 3 x 3, world = R local (R = Rz Ry Rx, as Body3D reads "rotate").
    void fromEuler (float ax, float ay, float az, float* m)
    {
        ax *= pi / 180.0f; ay *= pi / 180.0f; az *= pi / 180.0f;
        const float cx = std::cos (ax), sx = std::sin (ax), cy = std::cos (ay), sy = std::sin (ay), cz = std::cos (az), sz = std::sin (az);
        m[0] = cz * cy; m[1] = cz * sy * sx - sz * cx; m[2] = cz * sy * cx + sz * sx;
        m[3] = sz * cy; m[4] = sz * sy * sx + cz * cx; m[5] = sz * sy * cx - cz * sx;
        m[6] = -sy;     m[7] = cy * sx;                m[8] = cy * cx;
    }

    void toEuler (const float* m, float& ax, float& ay, float& az)
    {
        ay = std::asin (juce::jlimit (-1.0f, 1.0f, -m[6]));

        if (std::cos (ay) > 1.0e-4f)
        {
            ax = std::atan2 (m[7], m[8]);
            az = std::atan2 (m[3], m[0]);
        }
        else
        {
            ax = 0.0f;
            az = std::atan2 (-m[1], m[4]);
        }

        ax *= 180.0f / pi; ay *= 180.0f / pi; az *= 180.0f / pi;
    }

    void aboutAxis (int k, float angle, float* m)
    {
        const float c = std::cos (angle), s = std::sin (angle);

        for (int i = 0; i < 9; ++i)
            m[i] = (i % 4 == 0) ? 1.0f : 0.0f;

        const int a = (k + 1) % 3, b = (k + 2) % 3;
        m[a * 3 + a] = c; m[a * 3 + b] = -s;
        m[b * 3 + a] = s; m[b * 3 + b] = c;
    }

    void multiply (const float* l, const float* r, float* out)
    {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                out[i * 3 + j] = l[i * 3] * r[j] + l[i * 3 + 1] * r[3 + j] + l[i * 3 + 2] * r[6 + j];
    }

    float dot3 (const float* a, const float* b)    { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

    float distanceToSegment (juce::Point<float> p, juce::Point<float> a, juce::Point<float> b)
    {
        juce::Point<float> nearest;
        return juce::Line<float> (a, b).getDistanceFromPoint (p, nearest);
    }

    /** The view cube's faces: their normal, and their own across and down (as their words read). */
    struct CubeFace { const char* name; float n[3], u[3], v[3]; };

    const CubeFace cubeFaces[6] {
        { "FRONT",  {  0,  0,  1 }, {  1, 0,  0 }, { 0, -1,  0 } },
        { "BACK",   {  0,  0, -1 }, { -1, 0,  0 }, { 0, -1,  0 } },
        { "RIGHT",  {  1,  0,  0 }, {  0, 0, -1 }, { 0, -1,  0 } },
        { "LEFT",   { -1,  0,  0 }, {  0, 0,  1 }, { 0, -1,  0 } },
        { "TOP",    {  0,  1,  0 }, {  1, 0,  0 }, { 0,  0,  1 } },
        { "BOTTOM", {  0, -1,  0 }, {  1, 0,  0 }, { 0,  0, -1 } } };
}

//==============================================================================
Showroom::Showroom (PluginCanvas& c) : canvas (c)
{
    setWantsKeyboardFocus (true);

    auto setUp = [this] (juce::Button& b, const juce::String& tip, std::function<void()> click)
    {
        b.setTooltip (tip);
        b.onClick = std::move (click);
        addAndMakeVisible (b);
    };

    // The tools.
    setUp (selectButton, "Choose parts by clicking them (Q)", [this] { setTool (Tool::select); });
    setUp (moveButton, "Move the chosen part: its arrows along an axis, its square across the picture (W)", [this] { setTool (Tool::move); });
    setUp (rotateButton, "Turn the chosen part: its rings round an axis; Shift: 15 degree steps (E)", [this] { setTool (Tool::rotate); });
    setUp (sizeButton, "Size the chosen part: its handles along its own axes, its middle box all of it (R)", [this] { setTool (Tool::scale); });

    for (auto* b : { &selectButton, &moveButton, &rotateButton, &sizeButton })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, Theme::accentDeep);
        b->setWantsKeyboardFocus (false);
    }

    // The views.
    setUp (frontButton, "From the front (1)", [this] { glideTo (0.0f, 0.0f, 0.0f); });
    setUp (backButton, "From the back (Ctrl 1)", [this] { glideTo (180.0f, 0.0f, 0.0f); });
    setUp (leftButton, "From its left (Ctrl 3)", [this] { glideTo (-90.0f, 0.0f, 0.0f); });
    setUp (rightButton, "From its right (3)", [this] { glideTo (90.0f, 0.0f, 0.0f); });
    setUp (topButton, "Straight down on it, as the panel shows it (7)", [this] { glideTo (0.0f, 90.0f, 0.0f); });
    setUp (bottomButton, "From underneath (Ctrl 7)", [this] { glideTo (0.0f, -90.0f, 0.0f); });
    setUp (threeQuarterButton, "Three-quarter: from the front, a little to the side and above, a product photograph's view",
           [this] { glideTo (32.0f, 28.0f, 0.0f); });
    setUp (frameButton, "The whole body framed in the view (F); double-click the view too", [this] { frameAll(); });
    setUp (saveButton, "The picture as a PNG file: twice the size, at full quality (See-through PNG: no backdrop, no floor)",
           [this] { savePicture(); });
    setUp (turnToggle, "It turns by itself, round and round", [] {});
    setUp (shadowToggle, "Its shadow on the floor", [] {});
    setUp (clearToggle, "Save picture: the body alone on a see-through background (its shadow too, if it has one)", [] {});
    shadowToggle.setToggleState (true, juce::dontSendNotification);

    for (auto* b : std::initializer_list<juce::Button*> { &frontButton, &backButton, &leftButton, &rightButton, &topButton, &bottomButton,
                                                          &threeQuarterButton, &frameButton, &saveButton, &turnToggle, &shadowToggle, &clearToggle })
        b->setWantsKeyboardFocus (false);

    printBox.addItem ("Its controls printed on it", 1);
    printBox.addItem ("The whole panel printed on it", 2);
    printBox.addItem ("The body alone", 3);
    printBox.setSelectedId (1, juce::dontSendNotification);
    printBox.setTooltip ("What shows on the body's upward faces: the panel's controls (knobs, switches, labels...), the panel's whole picture "
                         "(its background too), or nothing");
    printBox.onChange = [this] { decalStale = true; };
    printBox.setWantsKeyboardFocus (false);
    addAndMakeVisible (printBox);

    for (auto* s : { &perspectiveSlider, &lightSlider })
    {
        s->setSliderStyle (juce::Slider::LinearBar);
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 22);
        s->setColour (juce::Slider::trackColourId, Theme::accentDeep);
        s->setColour (juce::Slider::backgroundColourId, Theme::inset);
        s->setColour (juce::Slider::textBoxTextColourId, Theme::text);
        s->setColour (juce::Slider::textBoxOutlineColourId, Theme::outline);
        s->setWantsKeyboardFocus (false);
        addAndMakeVisible (*s);
    }

    perspectiveSlider.setRange (0, 100, 1);
    perspectiveSlider.setValue (45, juce::dontSendNotification);
    perspectiveSlider.setTooltip ("How strong the perspective is: 0 none (a drawing's), 100 a wide lens close up");
    lightSlider.setRange (0, 360, 1);
    lightSlider.setValue (canvas.getLayout().lightAngle, juce::dontSendNotification);
    lightSlider.setTextValueSuffix (juce::String::fromUTF8 ("\xc2\xb0"));
    lightSlider.setTooltip ("Where the light comes from, round it (the panel's light as it opens)");

    for (auto* l : { &perspectiveLabel, &lightLabel })
    {
        l->setColour (juce::Label::textColourId, Theme::muted);
        l->setFont (Theme::font (12.5f));
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*l);
    }

    perspectiveLabel.setText ("Perspective", juce::dontSendNotification);
    lightLabel.setText ("Light", juce::dontSendNotification);

    floorSwatch.setColourValue (juce::Colour (0xff2c2d32), juce::dontSendNotification);
    floorSwatch.setTooltip ("The floor it stands on (see-through: none)");
    floorSwatch.setWantsKeyboardFocus (false);
    addAndMakeVisible (floorSwatch);

    setTool (Tool::move);
    setSize (900, 640);
    startTimerHz (30);
}

Showroom::~Showroom()
{
    stopTimer();
    ++generation;
    painter.removeAllJobs (true, 20000);
}

void Showroom::setTool (Tool t)
{
    tool = t;
    selectButton.setToggleState (t == Tool::select, juce::dontSendNotification);
    moveButton.setToggleState (t == Tool::move, juce::dontSendNotification);
    rotateButton.setToggleState (t == Tool::rotate, juce::dontSendNotification);
    sizeButton.setToggleState (t == Tool::scale, juce::dontSendNotification);
    hovered = Handle::none;
    repaint();
}

void Showroom::resetView()
{
    zoom = 1.0f;
    panX = panY = 0.0f;
    glideTo (32.0f, 28.0f, 0.0f);
}

void Showroom::frameAll()
{
    zoom = 1.0f;
    panX = panY = 0.0f;
    repaint();
}

void Showroom::glideTo (float newYaw, float newPitch, float newRoll)
{
    // The shortest way round.
    toYaw = yaw + std::remainder (newYaw - yaw, 360.0f);
    toPitch = juce::jlimit (-90.0f, 90.0f, newPitch);
    toRoll = roll + std::remainder (newRoll - roll, 360.0f);
    gliding = true;
}

//==============================================================================
Render3D::Showroom Showroom::viewState (int w, int h) const
{
    juce::ignoreUnused (w, h);
    const auto& layout = canvas.getLayout();
    Render3D::Showroom view;
    view.yaw = yaw;
    view.pitch = pitch;
    view.roll = roll;
    view.zoom = zoom;
    view.panX = panX;
    view.panY = panY;
    view.perspective = (float) perspectiveSlider.getValue();
    view.floor = floorSwatch.getColourValue();
    view.shadow = shadowToggle.getToggleState();
    Render3D::frameOf (layout.body, layout.width, layout.height, view);
    return view;
}

Render3D::Camera Showroom::camera() const
{
    const auto area = viewArea();
    return Render3D::showroomCamera (area.getWidth(), area.getHeight(), viewState (area.getWidth(), area.getHeight()));
}

juce::String Showroom::stateKey() const
{
    const auto& layout = canvas.getLayout();
    return juce::JSON::toString (layout.body, true) + "|" + juce::String (layout.width) + "x" + juce::String (layout.height)
         + "|" + juce::String (yaw, 2) + "," + juce::String (pitch, 2) + "," + juce::String (roll, 2) + "," + juce::String (zoom, 3)
         + "," + juce::String (panX, 2) + "," + juce::String (panY, 2)
         + "|" + juce::String (perspectiveSlider.getValue()) + "," + juce::String (lightSlider.getValue()) + "," + juce::String (layout.lightAltitude)
         + "|" + juce::String (printBox.getSelectedId()) + "|" + floorSwatch.getColourValue().toString() + "|" + (shadowToggle.getToggleState() ? "s" : "-")
         + "|" + juce::String (canvas.editStamp) + "|" + viewArea().toString();
}

juce::Rectangle<int> Showroom::viewArea() const
{
    return getLocalBounds().withTrimmedBottom (barHeight);
}

void Showroom::refreshDecal()
{
    const int print = printBox.getSelectedId();
    decal = print == 3 ? juce::Image() : canvas.controlsPicture (1.5f, print == 2);
    decalPrint = print;
    decalStale = false;
}

void Showroom::request (int level)
{
    const auto area = viewArea();

    if (area.isEmpty() || ! canvas.hasPanel())
        return;

    if (decalStale || decal.isNull() != (printBox.getSelectedId() == 3) || decalPrint != printBox.getSelectedId())
        if (level == 2 || decalPrint != printBox.getSelectedId() || (decal.isNull() && printBox.getSelectedId() != 3))
            refreshDecal();

    const auto& layout = canvas.getLayout();
    const int gen = ++generation;
    painter.removeAllJobs (false, 0);

    // Small while it's moved, half size a moment later, sharp when it's been still.
    const int divide = level == 0 ? 4 : (level == 1 ? 2 : 1);
    const int w = juce::jmax (8, area.getWidth() / divide), h = juce::jmax (8, area.getHeight() / divide);

    auto view = viewState (w, h);
    view.decal = decal;
    view.decalScale = 1.5f;
    view.stop = &generation;
    view.stopUnless = gen;

    const float lightAngle = (float) lightSlider.getValue(), lightAltitude = layout.lightAltitude;
    const auto json = layout.body.clone();
    const int width = layout.width, height = layout.height;
    const auto images = canvas.imagesFolder();
    const auto key = stateKey();
    askedKey = key;
    askedLevel = level;

    painter.addJob ([safeThis = juce::Component::SafePointer<Showroom> (this), &gen = generation, gen0 = gen, json, width, height, images,
                     view, lightAngle, lightAltitude, w, h, level, key]
    {
        if (gen.load() != gen0)
            return;

        std::shared_ptr<const Body3D> body (Body3D::fromVar (json, width, height, images).release());
        juce::Image picture;

        if (body != nullptr && gen.load() == gen0)
            picture = Render3D::renderShowroom (*body, w, h, view, lightAngle, lightAltitude, level == 2 ? 2 : 1);

        if (gen.load() != gen0 || (body != nullptr && ! picture.isValid()))
            return;   // stopped: a newer one's on its way

        juce::MessageManager::callAsync ([safeThis, picture, body, gen0, level, key]
        {
            if (safeThis == nullptr || safeThis->generation.load() != gen0)
                return;

            safeThis->shown = picture;
            safeThis->shownKey = key;
            safeThis->shownLevel = level;
            safeThis->pickBody = body;
            safeThis->repaint();
        });
    });
}

void Showroom::timerCallback()
{
    if (! isShowing())
        return;

    bool moving = drag == Drag::orbit || drag == Drag::pan || drag == Drag::roll || drag == Drag::cube || drag == Drag::handle;

    // Gliding to a view.
    if (gliding)
    {
        yaw += (toYaw - yaw) * 0.32f;
        pitch += (toPitch - pitch) * 0.32f;
        roll += (toRoll - roll) * 0.32f;

        if (std::abs (toYaw - yaw) < 0.05f && std::abs (toPitch - pitch) < 0.05f && std::abs (toRoll - roll) < 0.05f)
        {
            yaw = std::fmod (toYaw + 720.0f, 360.0f);
            pitch = toPitch;
            roll = std::remainder (toRoll, 360.0f);
            gliding = false;
        }

        moving = true;
        repaint();
    }

    // Turning by itself.
    if (turnToggle.getToggleState() && drag == Drag::none)
    {
        yaw = std::fmod (yaw + 1.2f, 360.0f);
        moving = true;
        repaint();
    }

    const auto key = stateKey();

    if (key != askedKey)
    {
        quietTicks = 0;
        request (0);   // at once, small
        return;
    }

    if (moving)
    {
        quietTicks = 0;
        return;
    }

    ++quietTicks;

    if (quietTicks == 3 && askedLevel < 1)
        request (1);
    else if (quietTicks >= 9 && askedLevel < 2)
        request (2);
}

//==============================================================================
juce::Rectangle<float> Showroom::cubeArea() const
{
    const auto area = viewArea().toFloat();
    return { area.getRight() - 132.0f, area.getY() + 12.0f, 120.0f, 132.0f };
}

Showroom::CubeHit Showroom::cubeAt (juce::Point<float> at) const
{
    CubeHit hit;
    const auto area = cubeArea();

    if (! area.contains (at))
        return hit;

    const auto box = area.withHeight (120.0f);
    const auto centre = box.getCentre();
    const float s = 34.0f;

    // Its buttons below it: roll left, home, roll right.
    if (at.y > box.getBottom() - 6.0f)
    {
        hit.kind = at.x < area.getCentreX() - 14.0f ? 2 : (at.x > area.getCentreX() + 14.0f ? 3 : 4);
        return hit;
    }

    const auto cam = camera();

    // The faces facing the camera, nearest first.
    std::vector<int> order;

    for (int f = 0; f < 6; ++f)
        if (dot3 (cubeFaces[f].n, cam.c) > 0.02f)
            order.push_back (f);

    std::sort (order.begin(), order.end(), [&cam] (int a, int b) { return dot3 (cubeFaces[a].n, cam.c) > dot3 (cubeFaces[b].n, cam.c); });

    auto toScreen = [&] (const float* p)
    {
        return juce::Point<float> (centre.x + dot3 (p, cam.r) * s, centre.y - dot3 (p, cam.u) * s);
    };

    for (const int f : order)
    {
        const auto& face = cubeFaces[f];
        float p00[3], p10[3], p01[3];

        for (int k = 0; k < 3; ++k)
        {
            p00[k] = face.n[k] - face.u[k] - face.v[k];
            p10[k] = face.n[k] + face.u[k] - face.v[k];
            p01[k] = face.n[k] - face.u[k] + face.v[k];
        }

        const auto a = toScreen (p00), b = toScreen (p10), c = toScreen (p01);
        const auto t = juce::AffineTransform::fromTargetPoints (0.0f, 0.0f, a.x, a.y, 1.0f, 0.0f, b.x, b.y, 0.0f, 1.0f, c.x, c.y);
        auto local = at;
        local.applyTransform (t.inverted());

        if (local.x < 0.0f || local.y < 0.0f || local.x > 1.0f || local.y > 1.0f)
            continue;

        // The middle: the face; near an edge: that edge; a corner: that corner.
        float dir[3] { face.n[0], face.n[1], face.n[2] };
        const float edge = 0.24f;

        for (int k = 0; k < 3; ++k)
        {
            if (local.x < edge)         dir[k] -= face.u[k];
            if (local.x > 1.0f - edge)  dir[k] += face.u[k];
            if (local.y < edge)         dir[k] -= face.v[k];
            if (local.y > 1.0f - edge)  dir[k] += face.v[k];
        }

        const float length = std::sqrt (dot3 (dir, dir));
        hit.kind = 1;

        for (int k = 0; k < 3; ++k)
            hit.dir[k] = dir[k] / length;

        return hit;
    }

    return hit;
}

void Showroom::paintCube (juce::Graphics& g)
{
    const auto area = cubeArea();
    const auto box = area.withHeight (120.0f);
    const auto centre = box.getCentre();
    const float s = 34.0f;
    const auto cam = camera();

    auto toScreen = [&] (const float* p)
    {
        return juce::Point<float> (centre.x + dot3 (p, cam.r) * s, centre.y - dot3 (p, cam.u) * s);
    };

    // Its axes behind it, from its back corner: x red, y green, z blue.
    {
        const float origin[3] { -1.0f, -1.0f, -1.0f };
        const auto o = toScreen (origin);

        for (int k = 0; k < 3; ++k)
        {
            float tip[3] { -1.0f, -1.0f, -1.0f };
            tip[k] += 2.7f;
            g.setColour (axisColours[k].withAlpha (0.9f));
            g.drawLine ({ o, toScreen (tip) }, 2.0f);
            g.setFont (Theme::font (10.5f, true));
            g.drawText (juce::String::charToString ((juce::juce_wchar) ('X' + k)),
                        juce::Rectangle<float> (14.0f, 12.0f).withCentre (toScreen (tip) + (toScreen (tip) - o) * 0.12f), juce::Justification::centred);
        }
    }

    std::vector<int> order;

    for (int f = 0; f < 6; ++f)
        if (dot3 (cubeFaces[f].n, cam.c) > 0.0f)
            order.push_back (f);

    std::sort (order.begin(), order.end(), [&cam] (int a, int b) { return dot3 (cubeFaces[a].n, cam.c) < dot3 (cubeFaces[b].n, cam.c); });

    for (const int f : order)
    {
        const auto& face = cubeFaces[f];
        float corners[4][3];

        for (int k = 0; k < 3; ++k)
        {
            corners[0][k] = face.n[k] - face.u[k] - face.v[k];
            corners[1][k] = face.n[k] + face.u[k] - face.v[k];
            corners[2][k] = face.n[k] + face.u[k] + face.v[k];
            corners[3][k] = face.n[k] - face.u[k] + face.v[k];
        }

        juce::Path outline;
        outline.startNewSubPath (toScreen (corners[0]));

        for (int i = 1; i < 4; ++i)
            outline.lineTo (toScreen (corners[i]));

        outline.closeSubPath();

        const float facing = dot3 (face.n, cam.c);
        bool lit = false;

        if (cubeHover.kind == 1)
        {
            // Lit where the view under the mouse is: the face, its edge or its corner.
            float d = 0.0f;

            for (int k = 0; k < 3; ++k)
                d += cubeHover.dir[k] * face.n[k];

            lit = d > 0.3f;
        }

        g.setColour (juce::Colour (0xff3a3d45).interpolatedWith (juce::Colour (0xff6b707c), facing).withAlpha (0.92f));
        g.fillPath (outline);

        if (lit)
        {
            g.setColour (Theme::accent.withAlpha (0.35f));
            g.fillPath (outline);
        }

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (outline, juce::PathStrokeType (1.2f));

        // Its name, on it.
        if (facing > 0.25f)
        {
            const auto a = toScreen (corners[0]), b = toScreen (corners[1]), c = toScreen (corners[3]);
            const auto t = juce::AffineTransform::fromTargetPoints (0.0f, 0.0f, a.x, a.y, 100.0f, 0.0f, b.x, b.y, 0.0f, 100.0f, c.x, c.y);
            juce::Graphics::ScopedSaveState state (g);
            g.addTransform (t);
            g.setColour (juce::Colours::white.withAlpha (0.25f + 0.7f * facing));
            g.setFont (Theme::font (19.0f, true));
            g.drawText (face.name, juce::Rectangle<float> (0.0f, 0.0f, 100.0f, 100.0f), juce::Justification::centred, false);
        }
    }

    // Roll left, home, roll right.
    const float y = area.getBottom() - 10.0f;

    for (int i = 0; i < 3; ++i)
    {
        const juce::Point<float> at (area.getCentreX() + (float) (i - 1) * 34.0f, y);
        const bool hot = cubeHover.kind == (i == 0 ? 2 : i == 2 ? 3 : 4);
        g.setColour (hot ? Theme::accent : juce::Colour (0xff2a2c32).withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (at));
        g.setColour (juce::Colours::white.withAlpha (0.85f));

        if (i == 1)
        {
            juce::Path house;
            house.addTriangle (at.x - 6.0f, at.y - 1.0f, at.x, at.y - 7.0f, at.x + 6.0f, at.y - 1.0f);
            house.addRectangle (at.x - 4.5f, at.y - 1.0f, 9.0f, 7.0f);
            g.fillPath (house);
        }
        else
        {
            juce::Path arc;
            const float dir = i == 0 ? -1.0f : 1.0f;
            arc.addCentredArc (at.x, at.y, 6.0f, 6.0f, 0.0f, -2.2f * dir, 1.2f * dir, true);
            g.strokePath (arc, juce::PathStrokeType (1.8f));
            const auto end = arc.getCurrentPosition();
            juce::Path head;
            head.addTriangle (end.x - 3.0f, end.y - 3.0f * dir, end.x + 3.0f, end.y - 3.0f * dir, end.x, end.y + 2.5f * dir);
            g.fillPath (head, juce::AffineTransform::rotation (dir * 0.6f, end.x, end.y));
        }
    }
}

//==============================================================================
bool Showroom::chosenFrame (PartFrame& frame) const
{
    const int index = chosenPart != nullptr ? chosenPart() : -1;
    const auto& layout = canvas.getLayout();
    const auto* list = layout.body.getProperty ("parts", {}).getArray();

    if (list == nullptr || ! juce::isPositiveAndBelow (index, list->size()))
        return false;

    frame.part = (*list)[index];
    const auto& p = frame.part;
    const float W = (float) layout.width, H = (float) layout.height;
    const float w = numberOf (p, "w", 100.0f), h = numberOf (p, "h", 20.0f), d = numberOf (p, "d", 100.0f);
    frame.centre[0] = numberOf (p, "x", W * 0.5f) - W * 0.5f;
    frame.centre[1] = numberOf (p, "y", 0.0f) + h * 0.5f;
    frame.centre[2] = numberOf (p, "z", H * 0.5f) - H * 0.5f;
    frame.half[0] = w * 0.5f; frame.half[1] = h * 0.5f; frame.half[2] = d * 0.5f;

    float m[9];

    if (const auto* r = p.getProperty ("rotate", {}).getArray(); r != nullptr && r->size() >= 3)
        fromEuler ((float) (double) (*r)[0], (float) (double) (*r)[1], (float) (double) (*r)[2], m);
    else
        fromEuler (0, 0, 0, m);

    for (int i = 0; i < 9; ++i)
        frame.axes[i] = m[i];

    frame.mirrored = p.getProperty ("mirror", {}).toString().containsIgnoreCase ("x");
    frame.mirrorAt = Body3D::mirrorAtOf (layout.body, layout.width) - W * 0.5f;
    return true;
}

float Showroom::handleLength (const Render3D::Camera& cam, const PartFrame&) const
{
    return 78.0f / juce::jmax (1.0e-3f, cam.fit);   // about 78 pixels on the picture
}

Showroom::Handle Showroom::handleAt (juce::Point<float> at) const
{
    PartFrame frame;

    if (tool == Tool::select || ! chosenFrame (frame))
        return Handle::none;

    const auto area = viewArea().toFloat();
    const auto cam = camera();
    const auto local = at - area.getPosition();
    const float L = handleLength (cam, frame);
    juce::Point<float> c;

    if (! cam.project (frame.centre, c))
        return Handle::none;

    if (tool == Tool::move)
    {
        if (juce::Rectangle<float> (18.0f, 18.0f).withCentre (c).contains (local))
            return Handle::plane;

        for (int k = 0; k < 3; ++k)
        {
            float tip[3] { frame.centre[0], frame.centre[1], frame.centre[2] };
            tip[k] += L;
            juce::Point<float> t;

            if (cam.project (tip, t) && distanceToSegment (local, c, t) < 8.0f)
                return (Handle) ((int) Handle::axisX + k);
        }
    }
    else if (tool == Tool::rotate)
    {
        for (int k = 0; k < 3; ++k)
        {
            const int a = (k + 1) % 3, b = (k + 2) % 3;
            juce::Point<float> last;

            for (int i = 0; i <= 48; ++i)
            {
                const float t = (float) i / 48.0f * 2.0f * pi;
                float p[3] { frame.centre[0], frame.centre[1], frame.centre[2] };
                p[a] += std::cos (t) * L * 0.9f;
                p[b] += std::sin (t) * L * 0.9f;
                juce::Point<float> s;

                if (! cam.project (p, s))
                    continue;

                if (i > 0 && distanceToSegment (local, last, s) < 7.0f)
                    return (Handle) ((int) Handle::ringX + k);

                last = s;
            }
        }
    }
    else
    {
        if (juce::Rectangle<float> (18.0f, 18.0f).withCentre (c).contains (local))
            return Handle::sizeAll;

        for (int k = 0; k < 3; ++k)
        {
            float tip[3];

            for (int i = 0; i < 3; ++i)
                tip[i] = frame.centre[i] + frame.axes[i * 3 + k] * L;

            juce::Point<float> t;

            if (cam.project (tip, t) && (local.getDistanceFrom (t) < 10.0f || distanceToSegment (local, c, t) < 6.0f))
                return (Handle) ((int) Handle::sizeX + k);
        }
    }

    return Handle::none;
}

void Showroom::paintHandles (juce::Graphics& g, const Render3D::Camera& cam, const PartFrame& frame)
{
    // The chosen part's box (and its mirrored copy's).
    auto boxPath = [&] (bool mirror)
    {
        juce::Path path;
        juce::Point<float> s[8];
        bool ok = true;

        for (int i = 0; i < 8; ++i)
        {
            const float l[3] { (i & 1) ? frame.half[0] : -frame.half[0], (i & 2) ? frame.half[1] : -frame.half[1], (i & 4) ? frame.half[2] : -frame.half[2] };
            float p[3];

            for (int k = 0; k < 3; ++k)
                p[k] = frame.centre[k] + frame.axes[k * 3] * l[0] + frame.axes[k * 3 + 1] * l[1] + frame.axes[k * 3 + 2] * l[2];

            if (mirror)
                p[0] = 2.0f * frame.mirrorAt - p[0];

            ok = cam.project (p, s[i]) && ok;
        }

        if (! ok)
            return path;

        const int edges[12][2] { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };

        for (const auto& e : edges)
        {
            path.startNewSubPath (s[e[0]]);
            path.lineTo (s[e[1]]);
        }

        return path;
    };

    g.setColour (chosenColour.withAlpha (0.9f));
    g.strokePath (boxPath (false), juce::PathStrokeType (1.4f));

    if (frame.mirrored)
    {
        juce::Path dashed;
        const float dashes[] { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, boxPath (true), dashes, 2);
        g.setColour (chosenColour.withAlpha (0.6f));
        g.fillPath (dashed);
    }

    if (tool == Tool::select)
        return;

    const float L = handleLength (cam, frame);
    juce::Point<float> c;

    if (! cam.project (frame.centre, c))
        return;

    auto colourFor = [this] (Handle h, int k) { return hovered == h || grab.handle == h ? hoverColour : axisColours[k]; };

    if (tool == Tool::move || tool == Tool::scale)
    {
        for (int k = 0; k < 3; ++k)
        {
            float tip[3];

            for (int i = 0; i < 3; ++i)
                tip[i] = frame.centre[i] + (tool == Tool::move ? (i == k ? L : 0.0f) : frame.axes[i * 3 + k] * L);

            juce::Point<float> t;

            if (! cam.project (tip, t))
                continue;

            const auto handle = (Handle) ((int) (tool == Tool::move ? Handle::axisX : Handle::sizeX) + k);
            g.setColour (colourFor (handle, k));
            g.drawLine ({ c, t }, 2.6f);

            if (tool == Tool::move)
            {
                // An arrowhead.
                const auto along = (t - c) / juce::jmax (1.0f, c.getDistanceFrom (t));
                const juce::Point<float> across (-along.y, along.x);
                juce::Path head;
                head.addTriangle (t + along * 12.0f, t + across * 5.5f, t - across * 5.5f);
                g.fillPath (head);
            }
            else
            {
                g.fillRect (juce::Rectangle<float> (10.0f, 10.0f).withCentre (t));
            }
        }

        const auto middle = tool == Tool::move ? Handle::plane : Handle::sizeAll;
        g.setColour ((hovered == middle || grab.handle == middle ? hoverColour : juce::Colours::white).withAlpha (0.85f));
        g.drawRect (juce::Rectangle<float> (14.0f, 14.0f).withCentre (c), 2.0f);
    }
    else
    {
        for (int k = 0; k < 3; ++k)
        {
            const int a = (k + 1) % 3, b = (k + 2) % 3;
            juce::Path ring;
            bool started = false;

            for (int i = 0; i <= 64; ++i)
            {
                const float t = (float) i / 64.0f * 2.0f * pi;
                float p[3] { frame.centre[0], frame.centre[1], frame.centre[2] };
                p[a] += std::cos (t) * L * 0.9f;
                p[b] += std::sin (t) * L * 0.9f;
                juce::Point<float> s;

                if (! cam.project (p, s))
                {
                    started = false;
                    continue;
                }

                if (! started) ring.startNewSubPath (s);
                else           ring.lineTo (s);

                started = true;
            }

            g.setColour (colourFor ((Handle) ((int) Handle::ringX + k), k));
            g.strokePath (ring, juce::PathStrokeType (2.4f));
        }
    }
}

void Showroom::dragHandle (const juce::MouseEvent& e)
{
    PartFrame frame;

    if (! chosenFrame (frame))
        return;

    const auto& layout = canvas.getLayout();
    const float W = (float) layout.width, H = (float) layout.height;
    const auto& p = frame.part;
    const auto delta = e.position - grab.from;
    const float grid = gridPixels != nullptr ? gridPixels() : 0.0f;
    const auto cam = camera();

    auto snap = [grid] (float v) { return grid > 0.0f ? std::round (v / grid) * grid : v; };

    // How far along the handle's axis the mouse went (world pixels).
    auto along = [&]
    {
        const float len2 = grab.axisOnScreen.x * grab.axisOnScreen.x + grab.axisOnScreen.y * grab.axisOnScreen.y;
        return len2 < 1.0e-3f ? 0.0f : (delta.x * grab.axisOnScreen.x + delta.y * grab.axisOnScreen.y) / len2 * grab.length;
    };

    const bool rotated = std::abs (grab.rot[0] - 1.0f) + std::abs (grab.rot[4] - 1.0f) + std::abs (grab.rot[8] - 1.0f) > 1.0e-4f;

    switch (grab.handle)
    {
        case Handle::axisX: setProperty (p, "x", tidy (snap (grab.x + along()))); break;
        case Handle::axisY: setProperty (p, "y", tidy (snap (grab.y + along()))); break;
        case Handle::axisZ: setProperty (p, "z", tidy (snap (grab.z + along()))); break;

        case Handle::plane:
        {
            // Across the picture: the mouse's way, in the camera's right and up.
            const float rx = delta.x / cam.fit, ry = -delta.y / cam.fit;
            setProperty (p, "x", tidy (snap (grab.x + cam.r[0] * rx + cam.u[0] * ry)));
            setProperty (p, "y", tidy (snap (grab.y + cam.r[1] * rx + cam.u[1] * ry)));
            setProperty (p, "z", tidy (snap (grab.z + cam.r[2] * rx + cam.u[2] * ry)));
            break;
        }

        case Handle::ringX:
        case Handle::ringY:
        case Handle::ringZ:
        {
            const int k = (int) grab.handle - (int) Handle::ringX;
            const auto rel = e.position - grab.centreOnScreen;
            float turned = std::atan2 (rel.y, rel.x) - grab.startAngle;   // on the picture: clockwise
            turned = -turned * grab.facing;                               // round the axis

            if (e.mods.isShiftDown())
                turned = std::round (turned / (pi / 12.0f)) * (pi / 12.0f);

            float r[9], m[9];
            aboutAxis (k, turned, r);
            multiply (r, grab.rot, m);
            float ax, ay, az;
            toEuler (m, ax, ay, az);
            setProperty (p, "rotate", juce::Array<juce::var> { tidy (ax), tidy (ay), tidy (az) });
            break;
        }

        case Handle::sizeX:
        case Handle::sizeY:
        case Handle::sizeZ:
        {
            const int k = (int) grab.handle - (int) Handle::sizeX;
            const float start = k == 0 ? grab.w : (k == 1 ? grab.h : grab.d);
            const float size = juce::jmax (juce::jmax (0.5f, grid), snap (start + 2.0f * along()));

            if (k == 0)      setProperty (p, "w", tidy (size));
            else if (k == 2) setProperty (p, "d", tidy (size));
            else
            {
                setProperty (p, "h", tidy (size));

                if (rotated)
                    setProperty (p, "y", tidy (grab.y + (grab.h - size) * 0.5f));   // turned: about its middle (upright: its foot stays)
            }
            break;
        }

        case Handle::sizeAll:
        {
            const float f = std::exp ((delta.x - delta.y) * 0.006f);
            const float h = juce::jmax (0.5f, grab.h * f);
            setProperty (p, "w", tidy (juce::jmax (0.5f, grab.w * f)));
            setProperty (p, "h", tidy (h));
            setProperty (p, "d", tidy (juce::jmax (0.5f, grab.d * f)));

            if (rotated)
                setProperty (p, "y", tidy (grab.y + (grab.h - h) * 0.5f));
            break;
        }

        case Handle::none:
        default:
            break;
    }

    juce::ignoreUnused (W, H);

    if (onPartEdited != nullptr)
        onPartEdited (false);

    repaint();
}

//==============================================================================
void Showroom::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    moved = false;
    dragFrom = e.position;
    yawFrom = yaw;
    pitchFrom = pitch;
    rollFrom = roll;
    panXFrom = panX;
    panYFrom = panY;
    gliding = false;

    if (! viewArea().contains (e.getPosition()))
    {
        drag = Drag::none;
        return;
    }

    if (cubeArea().contains (e.position))
    {
        drag = Drag::cube;
        return;
    }

    // A handle of the chosen part.
    if (e.mods.isLeftButtonDown() && ! e.mods.isAltDown())
    {
        if (const auto handle = handleAt (e.position); handle != Handle::none)
        {
            PartFrame frame;
            chosenFrame (frame);
            const auto& p = frame.part;
            const auto& layout = canvas.getLayout();
            const auto cam = camera();
            const auto origin = viewArea().getPosition().toFloat();

            grab = {};
            grab.handle = handle;
            grab.from = e.position;
            grab.x = numberOf (p, "x", (float) layout.width * 0.5f);
            grab.y = numberOf (p, "y", 0.0f);
            grab.z = numberOf (p, "z", (float) layout.height * 0.5f);
            grab.w = numberOf (p, "w", 100.0f);
            grab.h = numberOf (p, "h", 20.0f);
            grab.d = numberOf (p, "d", 100.0f);

            for (int i = 0; i < 9; ++i)
                grab.rot[i] = frame.axes[i];

            grab.length = handleLength (cam, frame);
            juce::Point<float> c;
            cam.project (frame.centre, c);
            grab.centreOnScreen = c + origin;

            // Its axis on the picture: a handle's length along it.
            const int k = handle == Handle::axisX || handle == Handle::sizeX || handle == Handle::ringX ? 0
                        : handle == Handle::axisY || handle == Handle::sizeY || handle == Handle::ringY ? 1 : 2;
            float tip[3];

            for (int i = 0; i < 3; ++i)
                tip[i] = frame.centre[i] + ((handle >= Handle::sizeX && handle <= Handle::sizeZ) ? frame.axes[i * 3 + k] : (i == k ? 1.0f : 0.0f)) * grab.length;

            juce::Point<float> t;
            cam.project (tip, t);
            grab.axisOnScreen = t - c;

            const auto rel = e.position - grab.centreOnScreen;
            grab.startAngle = std::atan2 (rel.y, rel.x);
            grab.facing = cam.c[k] >= 0.0f ? 1.0f : -1.0f;

            drag = Drag::handle;
            return;
        }
    }

    // Seeing it: round it, across, or turned.
    if (e.mods.isMiddleButtonDown() || e.mods.isPopupMenu() || e.mods.isShiftDown())
        drag = Drag::pan;
    else if (e.mods.isAltDown())
        drag = Drag::roll;
    else
        drag = Drag::orbit;
}

void Showroom::mouseDrag (const juce::MouseEvent& e)
{
    const auto by = e.position - dragFrom;

    if (! moved && by.getDistanceFromOrigin() < 3.0f)
        return;

    moved = true;
    const auto cam = camera();

    switch (drag)
    {
        case Drag::orbit:
        case Drag::cube:
        {
            // Turned with the view's roll, so dragging right always goes round to the right.
            const float a = -roll * pi / 180.0f;
            const float dx = by.x * std::cos (a) - by.y * std::sin (a), dy = by.x * std::sin (a) + by.y * std::cos (a);
            yaw = std::fmod (yawFrom - dx * 0.4f + 720.0f, 360.0f);
            pitch = juce::jlimit (-90.0f, 90.0f, pitchFrom + dy * 0.35f);
            repaint();
            break;
        }

        case Drag::pan:
            panX = panXFrom - by.x / cam.fit;
            panY = panYFrom + by.y / cam.fit;
            repaint();
            break;

        case Drag::roll:
            roll = std::remainder (rollFrom + by.x * 0.4f, 360.0f);
            repaint();
            break;

        case Drag::handle:
            dragHandle (e);
            break;

        case Drag::none:
        default:
            break;
    }
}

void Showroom::mouseUp (const juce::MouseEvent& e)
{
    const auto was = drag;
    drag = Drag::none;

    if (was == Drag::handle)
    {
        grab.handle = Handle::none;

        if (moved && onPartEdited != nullptr)
            onPartEdited (true);

        repaint();
        return;
    }

    if (moved || was == Drag::none)
        return;

    // A click.
    if (was == Drag::cube)
    {
        const auto hit = cubeAt (e.position);

        if (hit.kind == 1)
        {
            const float newYaw = std::abs (hit.dir[0]) + std::abs (hit.dir[2]) < 1.0e-4f ? yaw : std::atan2 (hit.dir[0], hit.dir[2]) * 180.0f / pi;
            glideTo (newYaw, std::asin (juce::jlimit (-1.0f, 1.0f, hit.dir[1])) * 180.0f / pi, 0.0f);
        }
        else if (hit.kind == 2) glideTo (yaw, pitch, roll - 15.0f);
        else if (hit.kind == 3) glideTo (yaw, pitch, roll + 15.0f);
        else if (hit.kind == 4) resetView();

        return;
    }

    if (was == Drag::orbit && onChoose != nullptr)
    {
        // The part under the mouse (beside the body: none).
        int part = -1;

        if (pickBody != nullptr)
        {
            const auto area = viewArea();
            const auto found = Render3D::partAt (*pickBody, camera(), e.position - area.getPosition().toFloat());

            if (juce::isPositiveAndBelow (found, (int) pickBody->getParts().size()))
                part = pickBody->getParts()[(size_t) found].source;
        }

        onChoose (part);
        repaint();
    }
}

void Showroom::mouseMove (const juce::MouseEvent& e)
{
    const auto cube = cubeAt (e.position);
    const auto handle = cube.kind == 0 ? handleAt (e.position) : Handle::none;

    const float moved3 = std::abs (cube.dir[0] - cubeHover.dir[0]) + std::abs (cube.dir[1] - cubeHover.dir[1]) + std::abs (cube.dir[2] - cubeHover.dir[2]);

    if (cube.kind != cubeHover.kind || moved3 > 1.0e-5f || handle != hovered)
    {
        cubeHover = cube;
        hovered = handle;
        repaint();
    }
}

void Showroom::mouseExit (const juce::MouseEvent&)
{
    cubeHover = {};
    hovered = Handle::none;
    repaint();
}

void Showroom::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (viewArea().contains (e.getPosition()) && ! cubeArea().contains (e.position) && handleAt (e.position) == Handle::none)
        frameAll();
}

void Showroom::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! viewArea().contains (e.getPosition()))
        return;

    zoom = juce::jlimit (0.1f, 12.0f, zoom * (1.0f + wheel.deltaY * 0.6f));
    repaint();
}

bool Showroom::keyPressed (const juce::KeyPress& key)
{
    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    const bool ctrl = key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown();

    if (c == 'q') { setTool (Tool::select); return true; }
    if (c == 'w') { setTool (Tool::move); return true; }
    if (c == 'e') { setTool (Tool::rotate); return true; }
    if (c == 'r') { setTool (Tool::scale); return true; }
    if (c == 'f') { frameAll(); return true; }
    if (key.getKeyCode() == juce::KeyPress::homeKey) { resetView(); return true; }
    if (c == '1' || key.getKeyCode() == juce::KeyPress::numberPad1) { glideTo (ctrl ? 180.0f : 0.0f, 0.0f, 0.0f); return true; }
    if (c == '3' || key.getKeyCode() == juce::KeyPress::numberPad3) { glideTo (ctrl ? -90.0f : 90.0f, 0.0f, 0.0f); return true; }
    if (c == '7' || key.getKeyCode() == juce::KeyPress::numberPad7) { glideTo (0.0f, ctrl ? -90.0f : 90.0f, 0.0f); return true; }

    if (key.getKeyCode() == juce::KeyPress::escapeKey && onChoose != nullptr)
    {
        onChoose (-1);
        repaint();
        return true;
    }

    if ((key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey) && onDelete != nullptr)
    {
        onDelete();
        return true;
    }

    return false;
}

//==============================================================================
void Showroom::savePicture()
{
    chooser = std::make_unique<juce::FileChooser> ("Save the Showroom's picture", juce::File::getSpecialLocation (juce::File::userPicturesDirectory)
                                                   .getChildFile ("Showroom.png"), "*.png");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis = juce::Component::SafePointer<Showroom> (this)] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr || fc.getResult() == juce::File())
            return;

        auto& room = *safeThis;
        const auto file = fc.getResult().withFileExtension ("png");
        const auto& layout = room.canvas.getLayout();
        const int print = room.printBox.getSelectedId();
        const float decalScale = 3.0f;
        const bool clear = room.clearToggle.getToggleState();
        const auto area = room.viewArea();

        auto view = room.viewState (area.getWidth() * 2, area.getHeight() * 2);
        view.decal = print == 3 ? juce::Image() : room.canvas.controlsPicture (decalScale, print == 2);
        view.decalScale = decalScale;

        if (clear)
            view.floor = juce::Colours::transparentBlack;   // no floor: the body (and its shadow) alone

        const auto body = Body3D::fromVar (layout.body, layout.width, layout.height, room.canvas.imagesFolder());

        if (body == nullptr)
            return;

        // Full quality, twice the size: it takes a moment.
        juce::MouseCursor::showWaitCursor();
        auto picture = Render3D::renderShowroom (*body, area.getWidth() * 2, area.getHeight() * 2, view,
                                                 (float) room.lightSlider.getValue(), layout.lightAltitude, 3);

        if (! clear)
            picture = onBackdrop (picture);

        file.deleteFile();
        juce::FileOutputStream out (file);

        if (out.openedOk())
            juce::PNGImageFormat().writeImageToStream (picture, out);

        juce::MouseCursor::hideWaitCursor();
    });
}

//==============================================================================
void Showroom::paint (juce::Graphics& g)
{
    const auto area = viewArea();
    g.setGradientFill (juce::ColourGradient (backdropTop, 0.0f, (float) area.getY(), backdropBottom, 0.0f, (float) area.getBottom(), false));
    g.fillRect (area);

    if (shown.isValid())
    {
        g.setImageResamplingQuality (shownLevel == 2 ? juce::Graphics::highResamplingQuality : juce::Graphics::mediumResamplingQuality);
        g.drawImage (shown, area.toFloat(), juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        g.setColour (Theme::muted);
        g.setFont (Theme::font (13.0f));
        g.drawText (Body3D::hasParts (canvas.getLayout().body) ? juce::String::fromUTF8 ("Setting up the showroom\xe2\x80\xa6")
                                                                : juce::String ("No 3D body yet: add a part (Add, on the left), or pick one in the Gallery"),
                    area, juce::Justification::centred, true);
    }

    // The chosen part: its box, its handles (drawn at once, before the picture catches up).
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (area);
        g.setOrigin (area.getPosition());
        PartFrame frame;

        if (chosenFrame (frame))
            paintHandles (g, camera(), frame);
    }

    g.setColour (juce::Colours::white.withAlpha (0.38f));
    g.setFont (Theme::font (11.5f));
    g.drawText (juce::String::fromUTF8 ("Drag: round it \xc2\xb7 right drag or Shift: across \xc2\xb7 Alt: roll \xc2\xb7 wheel: closer \xc2\xb7 click a part to choose it"),
                area.reduced (12, 8).withTrimmedTop (area.getHeight() - 30), juce::Justification::bottomLeft, true);

    paintCube (g);

    // The bar.
    g.setColour (Theme::panel);
    g.fillRect (getLocalBounds().removeFromBottom (barHeight));
    g.setColour (Theme::outline);
    g.drawHorizontalLine (area.getBottom(), 0.0f, (float) getWidth());
}

void Showroom::resized()
{
    // The tools, over the view's top left.
    {
        auto tools = viewArea().reduced (10).removeFromTop (26);

        for (auto* b : { &selectButton, &moveButton, &rotateButton, &sizeButton })
        {
            b->setBounds (tools.removeFromLeft (b == &rotateButton ? 64 : 56));
            tools.removeFromLeft (3);
        }
    }

    auto bar = getLocalBounds().removeFromBottom (barHeight).reduced (8, 6);
    auto top = bar.removeFromTop (28);
    bar.removeFromTop (6);
    auto bottom = bar;

    for (auto* b : { &frontButton, &backButton, &leftButton, &rightButton, &topButton, &bottomButton, &threeQuarterButton })
    {
        b->setBounds (top.removeFromLeft (b == &threeQuarterButton ? 32 : (b == &bottomButton ? 58 : 48)));
        top.removeFromLeft (3);
    }

    top.removeFromLeft (6);
    frameButton.setBounds (top.removeFromLeft (56));
    top.removeFromLeft (10);

    // The lens and the light share what's left.
    {
        const int each = juce::jmax (60, (top.getWidth() - 120) / 2);
        perspectiveLabel.setBounds (top.removeFromLeft (76));
        perspectiveSlider.setBounds (top.removeFromLeft (each));
        lightLabel.setBounds (top.removeFromLeft (44));
        lightSlider.setBounds (top.removeFromLeft (each));
    }

    printBox.setBounds (bottom.removeFromLeft (200));
    bottom.removeFromLeft (8);
    floorSwatch.setBounds (bottom.removeFromLeft (64));
    bottom.removeFromLeft (10);
    shadowToggle.setBounds (bottom.removeFromLeft (74));
    turnToggle.setBounds (bottom.removeFromLeft (60));
    saveButton.setBounds (bottom.removeFromRight (112));
    bottom.removeFromRight (6);
    clearToggle.setBounds (bottom.removeFromRight (130));

    askedKey.clear();   // drawn again at the new size
}
