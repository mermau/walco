#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstdint>

namespace
{
    // smalto rosa-lilla del pedale (campionato dalla foto dell'originale)
    const juce::Colour pinkLight (0xfff9d6fc);
    const juce::Colour pinkMid   (0xfff3c6f6);
    const juce::Colour pinkDark  (0xffe2a8e6);
    const juce::Colour pinkEdge  (0xffc98ccf);
    const juce::Colour ink       (0xff17121a);   // serigrafia nera
    const juce::Colour cream     (0xffe4e5e7);   // testi chiari della barra di servizio
    const juce::Colour stripCol  (0xff121014);
    const juce::Colour lineCol   (0xff3a363e);
    const juce::Colour learnCol  (0xffffb020);
    const juce::Colour accent    (0xffb04cb8);   // evidenziazioni nei menu

    juce::Font uiFont (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font faceFont (juce::Typeface::Ptr face, float pt, float tracking = 0.0f)
    {
        return juce::Font (juce::FontOptions (face).withPointHeight (pt)).withExtraKerningFactor (tracking / pt);
    }

    const char* displayNames[] = { "FUZZ INCREASE", "ON / OFF", "INPUT", "OUTPUT",
                                   "ALIMENTAZIONE", "CARICA", "hFE Q1", "hFE Q2" };

    // classi di guadagno del 2SC828 (Q 130-260, R 180-360, S 260-520; si sovrappongono)
    juce::String hfeClass (float h)
    {
        if (h < 130.0f) return "sotto Q";
        if (h < 180.0f) return "Q";
        if (h <= 260.0f) return "Q/R";
        if (h <= 360.0f) return "R/S";
        if (h <= 520.0f) return "S";
        return "oltre S";
    }

    void drawBaseline (juce::Graphics& g, const juce::Font& f, const juce::String& s,
                       float x, float baseline, juce::Justification j)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, s);
        if (j.testFlags (juce::Justification::horizontallyCentred)) x -= w * 0.5f;
        else if (j.testFlags (juce::Justification::right)) x -= w;
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, x, baseline);
        ga.draw (g);
    }

    juce::ColourGradient chrome (juce::Point<float> c, float r, float darken = 0.0f)
    {
        const float d = 2.0f * r;
        juce::ColourGradient cg (juce::Colours::white.darker (darken), c.x - 0.15f * d, c.y - 0.2f * d,
                                 juce::Colour (0xff55595e).darker (darken), c.x - 0.15f * d + 0.85f * d, c.y - 0.2f * d, true);
        cg.addColour (0.35, juce::Colour (0xffd6d8db).darker (darken));
        cg.addColour (0.75, juce::Colour (0xff868a90).darker (darken));
        return cg;
    }

    // vite a croce cromata
    void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
    {
        using namespace juce;
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (Rectangle<float> (2 * r + 2, 2 * r + 2).withCentre (c.translated (0.5f, 1.2f)));
        g.setGradientFill (chrome (c, r, 0.1f));
        g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c));
        g.setColour (Colour (0xff6d6a60));
        g.drawEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c), 0.6f);
        g.setColour (Colour (0xff7a6a3a));
        const float a = r * 0.62f;
        g.drawLine (c.x - a, c.y - a * 0.3f, c.x + a, c.y + a * 0.3f, r * 0.28f);
        g.drawLine (c.x - a * 0.3f, c.y + a, c.x + a * 0.3f, c.y - a, r * 0.28f);
    }

    // testo disposto lungo un arco (angoli in radianti, 0 = in alto, orario positivo)
    void drawArcText (juce::Graphics& g, const juce::Font& f, const juce::String& s,
                      juce::Point<float> c, float radius, float centreAngle)
    {
        using namespace juce;
        GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        const float total = ga.getBoundingBox (0, -1, true).getWidth();
        float angle = centreAngle - (total / radius) * 0.5f;
        for (int i = 0; i < ga.getNumGlyphs(); ++i)
        {
            auto& gl = ga.getGlyph (i);
            const float w = gl.getRight() - gl.getLeft();
            const float mid = angle + (gl.getLeft() + w * 0.5f) / radius;
            Path p; gl.createPath (p);
            p.applyTransform (AffineTransform::translation (-(gl.getLeft() + w * 0.5f), 0.0f)
                                  .rotated (mid)
                                  .translated (c.getPointOnCircumference (radius, mid)));
            g.fillPath (p);
        }
    }
}

//==============================================================================
PedalLookAndFeel::PedalLookAndFeel()
{
    using namespace juce;
    setColour (PopupMenu::backgroundColourId, Colour (0xff1c1a1e));
    setColour (PopupMenu::textColourId, cream);
    setColour (PopupMenu::highlightedBackgroundColourId, accent);
    setColour (BubbleComponent::backgroundColourId, Colour (0xee111111));
    setColour (BubbleComponent::outlineColourId, cream.withAlpha (0.4f));
    setColour (TooltipWindow::textColourId, cream);
    setColour (ComboBox::backgroundColourId, Colour (0xff1c1a1e));
    setColour (ComboBox::outlineColourId, lineCol);
    setColour (ComboBox::textColourId, cream);
    setColour (ComboBox::arrowColourId, cream.withAlpha (0.7f));
    setColour (TextButton::buttonColourId, Colour (0xff1c1a1e));
    setColour (TextButton::textColourOffId, cream);
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto style = slider.getProperties()["style"].toString();

    if (style == "trimmer")   // trimmer da circuito stampato
    {
        const float side = jmin (b.getWidth(), b.getHeight()) * 0.62f, r = side * 0.36f;
        const auto sq = Rectangle<float> (side, side).withCentre (c);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (sq.translated (0, 2.0f), side * 0.08f);
        g.setGradientFill (ColourGradient (Colour (0xff3d6fd1), sq.getX(), sq.getY(), Colour (0xff1c3c86), sq.getRight(), sq.getBottom(), false));
        g.fillRoundedRectangle (sq, side * 0.08f);
        g.setColour (Colour (0xffd9dde4));
        g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c));
        Path slot; slot.addRoundedRectangle (-r * 0.18f, -r * 0.9f, r * 0.36f, r * 1.8f, r * 0.1f);
        g.setColour (Colour (0xff3a3d42));
        g.fillPath (slot, AffineTransform::rotation (angle).translated (c));
        g.setColour (Colour (0xffc9ccd0).withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre (c.getPointOnCircumference (side * 0.74f, a)));
        }
        return;
    }

    if (style == "charge")    // carica della batteria: scala verde / ambra / rossa
    {
        const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const float vNorm = (float) ((Battery::emptyV - Battery::minV) / (Battery::fullV - Battery::minV));
        auto arc = [&] (float from, float to, Colour col)
        {
            Path a; a.addCentredArc (c.x, c.y, R * 0.86f, R * 0.86f, 0.0f,
                                     startAngle + from * (endAngle - startAngle), startAngle + to * (endAngle - startAngle), true);
            g.setColour (col);
            g.strokePath (a, PathStrokeType (R * 0.1f, PathStrokeType::curved, PathStrokeType::butt));
        };
        const bool on = slider.isEnabled();
        arc (0.0f, vNorm, Colour (0xffc8302b).withAlpha (on ? 1.0f : 0.35f));
        arc (vNorm, vNorm + 0.18f, Colour (0xffd9a32c).withAlpha (on ? 1.0f : 0.35f));
        arc (vNorm + 0.18f, 1.0f, Colour (0xff6fae55).withAlpha (on ? 1.0f : 0.35f));
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillEllipse (Rectangle<float> (R * 1.36f, R * 1.36f).withCentre (c.translated (0, R * 0.06f)));
        ColourGradient body (Colour (0xff3b3b3e), c.x - R * 0.2f, c.y - R * 0.3f, Colour (0xff050505), c.x + R * 0.4f, c.y + R * 0.5f, true);
        g.setGradientFill (body);
        g.fillEllipse (Rectangle<float> (R * 1.3f, R * 1.3f).withCentre (c));
        Path p; p.addRoundedRectangle (-R * 0.05f, -R * 0.62f, R * 0.1f, R * 0.36f, R * 0.05f);
        g.setColour (Colour (0xfff1f2f3).withAlpha (on ? 1.0f : 0.4f));
        g.fillPath (p, AffineTransform::rotation (angle).translated (c));
        return;
    }

    if (w < 80)   // manopole di servizio (INPUT / OUTPUT)
    {
        const float k = jmin (b.getWidth(), b.getHeight()) / 34.0f;
        g.setColour (Colour (0xff1c1c1e)); g.fillEllipse (Rectangle<float> (30 * k, 30 * k).withCentre (c));
        g.setColour (Colour (0xff4a4650)); g.drawEllipse (Rectangle<float> (30 * k, 30 * k).withCentre (c), 1.0f);
        g.setColour (Colour (0xff2b2a2f)); g.fillEllipse (Rectangle<float> (18 * k, 18 * k).withCentre (c));
        Path p; p.addRoundedRectangle (-1.2f * k, -14.0f * k, 2.4f * k, 8.0f * k, 1.2f * k);
        g.setColour (Colour (0xffd9dbde));
        g.fillPath (p, AffineTransform::rotation (angle).translated (c));
        return;
    }

    // manopola principale: cilindro di bachelite nera lucida con punto bianco
    const float k = jmin (b.getWidth(), b.getHeight()) / 140.0f;   // margine per l'ombra

    // ombra portata sullo smalto rosa
    g.setColour (Colour (0xff5a2a60).withAlpha (0.28f));
    g.fillEllipse (Rectangle<float> (104 * k, 104 * k).withCentre (c.translated (3 * k, 6 * k)));
    g.setColour (Colour (0xff5a2a60).withAlpha (0.18f));
    g.fillEllipse (Rectangle<float> (112 * k, 112 * k).withCentre (c.translated (4 * k, 8 * k)));

    // fianco del cilindro (si vede in basso)
    g.setGradientFill (ColourGradient (Colour (0xff2c2a30), c.x, c.y - 50 * k, Colour (0xff050407), c.x, c.y + 52 * k, false));
    g.fillEllipse (Rectangle<float> (100 * k, 100 * k).withCentre (c.translated (0, 2.5f * k)));

    // smusso
    ColourGradient bevel (Colour (0xff4a4850), c.x - 30 * k, c.y - 40 * k, Colour (0xff09080b), c.x + 30 * k, c.y + 40 * k, false);
    g.setGradientFill (bevel);
    g.fillEllipse (Rectangle<float> (96 * k, 96 * k).withCentre (c));

    // faccia superiore
    ColourGradient top (Colour (0xff2e2d33), c.x - 18 * k, c.y - 26 * k, Colour (0xff050407), c.x + 30 * k, c.y + 34 * k, true);
    top.addColour (0.55, Colour (0xff121115));
    g.setGradientFill (top);
    g.fillEllipse (Rectangle<float> (86 * k, 86 * k).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (Rectangle<float> (86 * k, 86 * k).withCentre (c), 0.8f * k);

    // riflesso lucido (fisso: la luce non gira con la manopola)
    ColourGradient spec (Colours::white.withAlpha (0.30f), c.x - 16 * k, c.y - 24 * k,
                         Colours::white.withAlpha (0.0f), c.x - 16 * k, c.y + 6 * k, false);
    g.setGradientFill (spec);
    g.fillEllipse (Rectangle<float> (54 * k, 30 * k).withCentre (c.translated (-10 * k, -20 * k)));

    // punto bianco
    const auto dot = c.getPointOnCircumference (31.0f * k, angle);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (11.5f * k, 11.5f * k).withCentre (dot.translated (0, 0.8f * k)));
    g.setGradientFill (ColourGradient (Colours::white, dot.x - 2 * k, dot.y - 2 * k, Colour (0xffd8d6dc), dot.x + 4 * k, dot.y + 4 * k, true));
    g.fillEllipse (Rectangle<float> (10.5f * k, 10.5f * k).withCentre (dot));
}

//==============================================================================
void LearnableSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onLearnMenu) onLearnMenu(); return; }
    Slider::mouseDown (e);
}

//==============================================================================
void SlideSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onLearnMenu) onLearnMenu(); return; }
    Button::mouseDown (e);
}

void SlideSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    const float k = b.getWidth() / 36.0f;
    const bool off = getToggleState();

    // piastrina nera con la feritoia
    const auto plate = Rectangle<float> (28 * k, 56 * k).withCentre (b.getCentre());
    g.setColour (Colour (0xff5a2a60).withAlpha (0.25f));
    g.fillRoundedRectangle (plate.translated (1.5f * k, 2.5f * k), 3 * k);
    g.setGradientFill (ColourGradient (Colour (0xff2a282d), plate.getX(), plate.getY(), Colour (0xff09080a), plate.getRight(), plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 3 * k);
    const auto slot = plate.reduced (6 * k, 5 * k);
    g.setColour (Colour (0xff020203));
    g.fillRoundedRectangle (slot, 2 * k);

    // cursore zigrinato: in alto = ON, in basso = OFF
    auto knob = slot.withHeight (slot.getHeight() * 0.55f).expanded (2.5f * k, 0.0f);
    if (off) knob = knob.withBottomY (slot.getBottom());
    g.setGradientFill (ColourGradient (Colour (0xff3a383e), knob.getX(), knob.getY(), Colour (0xff141316), knob.getX(), knob.getBottom(), false));
    g.fillRoundedRectangle (knob, 1.5f * k);
    g.setColour (Colour (0xff57545c));
    for (int i = 1; i < 6; ++i)
    {
        const float yy = knob.getY() + knob.getHeight() * (float) i / 6.0f;
        g.drawLine (knob.getX() + 1.5f * k, yy, knob.getRight() - 1.5f * k, yy, 0.9f * k);
    }
    if (highlighted) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (knob, 1.5f * k); }
}

//==============================================================================
void SupplySwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onLearnMenu) onLearnMenu(); return; }
    Button::mouseDown (e);
}

void SupplySwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool dc = getToggleState();
    g.setColour (Colour (0xff0b0b0c));
    g.fillRoundedRectangle (b, b.getHeight() * 0.25f);
    g.setColour (Colour (0xff3a3c40));
    g.drawRoundedRectangle (b, b.getHeight() * 0.25f, 1.0f);
    const auto half = b.withWidth (b.getWidth() * 0.5f).reduced (3.0f);
    const auto knob = dc ? half.withX (b.getCentreX()) : half;
    g.setGradientFill (ColourGradient (Colour (0xffeceef0), knob.getX(), knob.getY(), Colour (0xff8a8e94), knob.getX(), knob.getBottom(), false));
    g.fillRoundedRectangle (knob, knob.getHeight() * 0.2f);
    if (highlighted) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (knob, knob.getHeight() * 0.2f); }
    g.setFont (Font (FontOptions (b.getHeight() * 0.34f, Font::bold)));
    g.setColour (dc ? Colour (0xff9a9da2) : Colour (0xff18191b));
    g.drawText ("BATT", half, Justification::centred);
    g.setColour (dc ? Colour (0xff18191b) : Colour (0xff9a9da2));
    g.drawText ("DC", half.withX (b.getCentreX()), Justification::centred);
}

//==============================================================================
InteriorPanel::InteriorPanel (FuzzToneProcessor& p, juce::Typeface::Ptr panelFace,
                              std::function<void (int, juce::Component*)> learnMenu)
    : proc (p), face (panelFace)
{
    using namespace juce;
    auto setupKnob = [&] (LearnableSlider& s, const char* id, const char* style)
    {
        s.setSliderStyle (Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (220);
        s.getProperties().set ("style", style);
        s.setPopupDisplayEnabled (true, true, this);
        const int idx = proc.getLearnIndex (id);
        s.onLearnMenu = [learnMenu, idx, &s] { learnMenu (idx, &s); };
        s.setTooltip ("Clic destro: MIDI Learn. Doppio clic: valore originale");
        addAndMakeVisible (s);
    };
    setupKnob (charge, ParamIDs::battery, "charge");
    setupKnob (hfe1,   ParamIDs::hfeQ1,   "trimmer");
    setupKnob (hfe2,   ParamIDs::hfeQ2,   "trimmer");
    chargeA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::battery, charge);
    hfe1A   = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::hfeQ1, hfe1);
    hfe2A   = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::hfeQ2, hfe2);
    charge.setDoubleClickReturnValue (true, Battery::nominalV);
    hfe1.setDoubleClickReturnValue (true, 250.0);
    hfe2.setDoubleClickReturnValue (true, 250.0);

    const int supplyIdx = proc.getLearnIndex (ParamIDs::supply);
    supply.onLearnMenu = [learnMenu, supplyIdx, this] { learnMenu (supplyIdx, &supply); };
    supply.setTooltip ("Batteria (tensione regolabile) o alimentatore stabilizzato da 9 V. Clic destro: MIDI Learn");
    addAndMakeVisible (supply);
    supplyA = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::supply, supply);

    resetBtn.setTooltip ("Batteria a 9 V e transistor a hFE 250");
    resetBtn.onClick = [this] { resetToOriginal(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (resetBtn);
    addAndMakeVisible (closeBtn);
    timerCallback();
    startTimerHz (15);
}

juce::Font InteriorPanel::font (float pt, float tracking) const
{
    return juce::Font (juce::FontOptions (face).withPointHeight (pt)).withExtraKerningFactor (tracking / pt);
}

juce::Rectangle<int> InteriorPanel::area (float x, float y, float w, float h) const
{
    return juce::Rectangle<float> (x * k, y * k, w * k, h * k).toNearestInt();
}

void InteriorPanel::resized()
{
    k = (float) getWidth() / baseW;
    supply.setBounds (area (214, 118, 132, 34));
    charge.setBounds (area (230, 186, 100, 100));
    hfe2.setBounds   (area (60, 404, 100, 100));    // Q2 = primo stadio, a sinistra
    hfe1.setBounds   (area (220, 404, 100, 100));   // Q1 = stadio fuzz, a destra
    resetBtn.setBounds (area (44, 594, 180, 32));
    closeBtn.setBounds (area (240, 594, 96, 32));
}

juce::Component* InteriorPanel::controlFor (int idx)
{
    switch (idx)
    {
        case FuzzToneProcessor::idxSupply:  return &supply;
        case FuzzToneProcessor::idxBattery: return &charge;
        case FuzzToneProcessor::idxHfe1:    return &hfe1;
        case FuzzToneProcessor::idxHfe2:    return &hfe2;
        default: return nullptr;
    }
}

void InteriorPanel::resetToOriginal()
{
    auto set = [this] (const char* id, float v)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture();
    };
    set (ParamIDs::supply, 0.0f);
    set (ParamIDs::battery, (float) Battery::nominalV);
    set (ParamIDs::hfeQ1, 250.0f);
    set (ParamIDs::hfeQ2, 250.0f);
}

void InteriorPanel::timerCallback()
{
    const bool dc = proc.apvts.getRawParameterValue (ParamIDs::supply)->load() > 0.5f;
    const float v = proc.apvts.getRawParameterValue (ParamIDs::battery)->load();
    const float h1 = proc.apvts.getRawParameterValue (ParamIDs::hfeQ1)->load();
    const float h2 = proc.apvts.getRawParameterValue (ParamIDs::hfeQ2)->load();
    if (dc != lastDC || std::abs (v - lastV) > 1e-4f || std::abs (h1 - lastH1) > 1e-3f || std::abs (h2 - lastH2) > 1e-3f)
    {
        lastDC = dc; lastV = v; lastH1 = h1; lastH2 = h2;
        charge.setEnabled (! dc);
        repaint();
    }
}

void InteriorPanel::paint (juce::Graphics& g)
{
    using namespace juce;
    g.addTransform (AffineTransform::scale (k));
    const float W = baseW, H = baseH;

    g.setColour (Colour (0xff141216));
    g.fillRoundedRectangle (0, 0, W, H, 14.0f);
    g.setColour (Colour (0xff3a363e));
    g.drawRoundedRectangle (0.5f, 0.5f, W - 1.0f, H - 1.0f, 14.0f, 1.0f);

    g.setColour (cream);
    g.setFont (font (18.0f, 4.0f));
    g.drawText ("INTERNO", Rectangle<float> (0, 22, W, 24), Justification::centred);
    g.setColour (Colour (0xff8a8d92));
    g.setFont (uiFont (10.0f, false));
    g.drawText ("alimentazione e transistor", Rectangle<float> (0, 46, W, 14), Justification::centred);

    // --- alimentazione ---
    const bool dc = lastDC;
    const float v = dc ? (float) Battery::dcV : lastV;
    const double ch = Battery::chargeFromVolts (v);
    g.setColour (Colour (0xffc9ccd0));
    g.setFont (font (11.0f, 2.0f));
    g.drawText ("ALIMENTAZIONE", Rectangle<float> (30, 82, W - 60, 16), Justification::centredLeft);

    const Rectangle<float> bat (54, 122, 104, 168);
    g.setColour (Colour (0xff2a2b2e));
    g.fillRoundedRectangle (bat.getX() + 18, bat.getY() - 14, 22, 16, 3.0f);
    g.fillRoundedRectangle (bat.getRight() - 40, bat.getY() - 14, 22, 16, 3.0f);
    g.setColour (Colour (0xff9a9da2));
    g.drawEllipse (bat.getX() + 22, bat.getY() - 11, 14, 10, 1.6f);
    g.drawRect (bat.getRight() - 36.0f, bat.getY() - 11.0f, 14.0f, 10.0f, 1.6f);
    g.setColour (Colour (0xff202124));
    g.fillRoundedRectangle (bat, 8.0f);
    g.setColour (Colour (0xff4a4d52));
    g.drawRoundedRectangle (bat, 8.0f, 1.5f);
    if (dc)
    {
        g.setColour (Colour (0xff8a8d92));
        g.setFont (font (14.0f, 1.0f));
        g.drawText ("DC", bat, Justification::centred);
    }
    else
    {
        const float lvl = (float) jlimit (0.0, 1.0, ch);
        const Colour col = ch <= 0.0 ? Colour (0xffc8302b) : (ch < 0.25 ? Colour (0xffd9a32c) : Colour (0xff6fae55));
        const auto inner = bat.reduced (8.0f);
        const auto fill = inner.withTrimmedTop (inner.getHeight() * (1.0f - lvl));
        g.setColour (col.withAlpha (0.85f));
        g.fillRoundedRectangle (fill.withHeight (jmax (fill.getHeight(), ch <= 0.0 ? 4.0f : 0.0f)).withBottomY (inner.getBottom()), 4.0f);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.setFont (font (16.0f, 1.0f));
        g.drawText ("9V", bat.withTrimmedTop (16).withHeight (24), Justification::centred);
    }
    g.setColour (cream);
    g.setFont (uiFont (13.0f));
    g.drawText (String (v, 2) + " V", Rectangle<float> (34, 298, 144, 18), Justification::centred);
    g.setColour (Colour (0xff9a9da2));
    g.setFont (uiFont (10.0f, false));
    const String state = dc ? String ("alimentatore stabilizzato")
                       : (ch < 0.0 ? String ("oltre la scarica")
                       : String (roundToInt (ch * 100.0)) + "% di carica");
    g.drawText (state, Rectangle<float> (34, 316, 144, 14), Justification::centred);

    g.setColour (Colour (0xff9a9da2));
    g.setFont (uiFont (9.0f));
    g.drawText ("BATTERIA", Rectangle<float> (214, 154, 66, 12), Justification::centred);
    g.drawText ("ALIMENTATORE", Rectangle<float> (280, 154, 66, 12), Justification::centred);
    g.setColour (Colour (0xffc9ccd0));
    g.setFont (font (10.0f, 1.5f));
    g.drawText ("CARICA", Rectangle<float> (230, 290, 100, 14), Justification::centred);
    g.setColour (Colour (0xff6c6f74));
    g.setFont (uiFont (8.0f, false));
    g.drawText ("4.5 V", Rectangle<float> (214, 270, 40, 10), Justification::centredLeft);
    g.drawText ("9.6 V", Rectangle<float> (306, 270, 40, 10), Justification::centredRight);

    // --- transistor su basetta millefori ---
    const Rectangle<float> pcb (24, 350, W - 48, 222);
    g.setColour (Colour (0xffc98a3e));
    g.fillRoundedRectangle (pcb, 6.0f);
    g.setColour (Colour (0xffe6b06a));
    for (float yy = pcb.getY() + 12; yy < pcb.getBottom() - 6; yy += 15.0f)
        g.fillRect (pcb.getX() + 8, yy, pcb.getWidth() - 16, 9.0f);
    g.setColour (Colour (0xff5a3a14).withAlpha (0.55f));
    for (float yy = pcb.getY() + 16.5f; yy < pcb.getBottom() - 6; yy += 15.0f)
        for (float xx = pcb.getX() + 16; xx < pcb.getRight() - 10; xx += 15.0f)
            g.fillEllipse (xx - 1.6f, yy - 1.6f, 3.2f, 3.2f);
    g.setColour (Colour (0xcc111012));
    g.fillRoundedRectangle (pcb.getX() + 60, 356, pcb.getWidth() - 120, 20, 4.0f);
    g.fillRoundedRectangle (pcb.getX() + 20, 500, pcb.getWidth() - 40, 22, 4.0f);
    g.fillRoundedRectangle (pcb.getX() + 20, 544, pcb.getWidth() - 40, 20, 4.0f);
    g.setColour (Colour (0xffeee8f0));
    g.setFont (font (11.0f, 2.0f));
    g.drawText ("TRANSISTOR 2SC828", Rectangle<float> (pcb.getX(), 358, pcb.getWidth(), 16), Justification::centred);
    for (auto [cx, label, hv] : { std::tuple<float, const char*, float> { 110.0f, "Q2  1\xc2\xb0 STADIO", lastH2 },
                                  { 270.0f, "Q1  FUZZ", lastH1 } })
    {
        g.setColour (Colour (0xcc111012));
        g.fillRoundedRectangle (cx - 66, 384, 132, 20, 4.0f);
        g.setColour (Colour (0xffeee8f0));
        g.setFont (font (10.0f, 0.5f));
        g.drawText (String::fromUTF8 (label), Rectangle<float> (cx - 70, 386, 140, 16), Justification::centred);
        g.setColour (Colour (0xffeee8f0));
        g.setFont (uiFont (11.0f));
        g.drawText ("hFE " + String (roundToInt (hv)) + "  " + hfeClass (hv), Rectangle<float> (cx - 70, 504, 140, 14), Justification::centred);
    }
    g.setColour (Colour (0xffcfc6d2));
    g.setFont (uiFont (9.0f, false));
    g.drawText (String::fromUTF8 ("2SC828: classi Q 130\xe2\x80\x93" "260, R 180\xe2\x80\x93" "360, S 260\xe2\x80\x93" "520"),
                Rectangle<float> (pcb.getX(), 547, pcb.getWidth(), 14), Justification::centred);
}

//==============================================================================
// Il pedale, in unità di progetto: corpo largo 330 e alto 540 (u = scala)
void FuzzToneEditor::drawPedal (juce::Graphics& g, juce::Rectangle<float> body,
                                juce::Typeface::Ptr panelFace, juce::Typeface::Ptr scriptFace, bool withExtras)
{
    using namespace juce;
    const float u = body.getWidth() / 330.0f;
    auto P = [&] (float x, float y) { return Point<float> (body.getX() + x * u, body.getY() + y * u); };
    auto F = [&] (float pt, float tr = 0.0f) { return faceFont (panelFace, pt * u, tr * u); };
    const float cx = body.getCentreX();

    if (withExtras)
    {
        // cavo d'ingresso (in basso a sinistra) e jack d'uscita (in basso a destra)
        const auto cable = P (78, 538);
        Path c; c.startNewSubPath (cable);
        c.cubicTo (cable.translated (0, 40 * u), cable.translated (-14 * u, 70 * u), cable.translated (-6 * u, 140 * u));
        g.setColour (Colour (0xff050405));
        g.strokePath (c, PathStrokeType (13 * u, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xff3a3840));
        g.strokePath (c, PathStrokeType (3 * u, PathStrokeType::curved, PathStrokeType::rounded),
                      AffineTransform::translation (-3 * u, 0));
        g.setColour (Colour (0xff111013));
        g.fillRoundedRectangle (Rectangle<float> (20 * u, 26 * u).withCentre (cable.translated (0, 10 * u)), 5 * u);

        const auto jack = P (262, 540);
        g.setGradientFill (ColourGradient (Colour (0xffe8e9ec), jack.x - 16 * u, 0, Colour (0xff6f737a), jack.x + 16 * u, 0, false));
        g.fillRect (Rectangle<float> (26 * u, 22 * u).withCentre (jack.translated (0, 12 * u)));
        g.setColour (Colour (0xff53565c));
        for (int i = 0; i < 5; ++i)
            g.drawLine (jack.x - 13 * u, jack.y + (4.0f + 4.0f * (float) i) * u, jack.x + 13 * u, jack.y + (5.5f + 4.0f * (float) i) * u, 0.8f * u);
        g.setGradientFill (chrome (jack.translated (0, 2 * u), 20 * u, 0.05f));
        g.fillRect (Rectangle<float> (40 * u, 9 * u).withCentre (jack.translated (0, 2 * u)));
    }

    // alette laterali della clip
    for (float yy : { 26.0f, 496.0f })
        for (float side : { -1.0f, 1.0f })
        {
            const auto r = Rectangle<float> (14 * u, 30 * u).withCentre (P (side < 0 ? 0 : 330, yy + 15));
            g.setGradientFill (ColourGradient (pinkMid, r.getX(), r.getY(), pinkDark, r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6 * u);
            g.setColour (pinkEdge);
            g.drawRoundedRectangle (r, 6 * u, 0.8f * u);
        }
    // viti del jack sul fianco destro
    for (float yy : { 96.0f, 470.0f })
    {
        const auto r = Rectangle<float> (9 * u, 14 * u).withCentre (P (333, yy));
        g.setGradientFill (ColourGradient (Colour (0xfff0f1f3), r.getX(), r.getY(), Colour (0xff7d8187), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 2 * u);
    }

    // scatola smaltata
    g.setColour (Colour (0xff3a1a40).withAlpha (0.35f));
    g.fillRoundedRectangle (body.translated (3 * u, 6 * u), 16 * u);
    ColourGradient enamel (pinkLight, body.getX(), body.getY(), pinkDark, body.getRight(), body.getBottom(), false);
    enamel.addColour (0.45, pinkMid);
    g.setGradientFill (enamel);
    g.fillRoundedRectangle (body, 16 * u);
    // riflesso dello smalto lungo il bordo superiore sinistro
    ColourGradient gloss (Colours::white.withAlpha (0.45f), body.getX(), body.getY(), Colours::white.withAlpha (0.0f), body.getX() + 70 * u, body.getY() + 70 * u, false);
    g.setGradientFill (gloss);
    g.fillRoundedRectangle (body.reduced (2 * u), 15 * u);
    g.setColour (pinkEdge);
    g.drawRoundedRectangle (body, 16 * u, 1.2f * u);
    g.setColour (Colours::white.withAlpha (0.5f));
    g.drawRoundedRectangle (body.reduced (2.5f * u), 14 * u, 0.8f * u);

    g.setColour (ink);

    // scritta ad arco con le frecce, attorno alla manopola
    const auto knobC = P (165, 122);
    const float arcR = 74.0f * u;
    drawArcText (g, F (12.5f, 0.6f), "FUZZ INCREASE", knobC, arcR, 0.0f);
    const float a0 = 0.86f, a1 = 1.20f;   // radianti dal centro
    Path left;  left.addCentredArc  (knobC.x, knobC.y, arcR + 4 * u, arcR + 4 * u, 0.0f, -a1, -a0, true);
    Path right; right.addCentredArc (knobC.x, knobC.y, arcR + 4 * u, arcR + 4 * u, 0.0f, a0, a1, true);
    g.strokePath (left,  PathStrokeType (1.6f * u));
    g.strokePath (right, PathStrokeType (1.6f * u));
    {
        const auto tip = knobC.getPointOnCircumference (arcR + 4 * u, a1 + 0.05f);
        Path head; head.addTriangle (0, 0, -9 * u, -4 * u, -9 * u, 4 * u);
        g.fillPath (head, AffineTransform::rotation (a1 + 0.05f).translated (tip));
    }

    // "FULL FREQUENCY" / FUZZ / TONE GENERATOR (lettering proprio, non quello Walco)
    drawBaseline (g, F (14.0f, 1.6f), "FULL FREQUENCY", cx, P (0, 214).y, Justification::horizontallyCentred);
    {
        const Font f = F (60.0f, 2.0f);
        GlyphArrangement ga;
        ga.addLineOfText (f, "FUZZ", 0, 0);
        Path p; ga.createPath (p);
        const auto bb = p.getBounds();
        auto tr = AffineTransform::translation (-bb.getCentreX(), -bb.getBottom())
                      .scaled (1.22f, 1.0f).sheared (-0.22f, 0.0f)
                      .translated (cx + 6 * u, P (0, 276).y);
        // eco "vibrante" dietro le lettere
        g.setColour (ink.withAlpha (0.9f));
        g.strokePath (p, PathStrokeType (1.3f * u), tr.translated (5 * u, -4 * u));
        g.strokePath (p, PathStrokeType (1.0f * u), tr.translated (10 * u, -8 * u));
        g.setColour (pinkMid);
        g.fillPath (p, tr.translated (2 * u, -2 * u));
        g.setColour (ink);
        g.fillPath (p, tr);
    }
    drawBaseline (g, F (25.0f, 1.0f), "TONE", cx, P (0, 310).y, Justification::horizontallyCentred);
    drawBaseline (g, F (25.0f, 1.0f), "GENERATOR", cx, P (0, 340).y, Justification::horizontallyCentred);

    // viti e scritte dell'interruttore
    drawScrew (g, P (165, 360), 5.2f * u);
    drawScrew (g, P (165, 474), 5.2f * u);
    g.setColour (ink);
    drawBaseline (g, F (12.0f, 1.0f), "ON",  cx, P (0, 383).y, Justification::horizontallyCentred);
    drawBaseline (g, F (12.0f, 1.0f), "OFF", cx, P (0, 461).y, Justification::horizontallyCentred);

    // marchio in basso
    if (scriptFace != nullptr)
    {
        g.setColour (ink);
        drawBaseline (g, Font (FontOptions (scriptFace).withPointHeight (30.0f * u)), "Collateral",
                      P (34, 0).x, P (0, 517).y, Justification::left);
        drawBaseline (g, F (13.0f, 2.2f), "EFFECTS CO.", P (300, 0).x, P (0, 515).y, Justification::right);
    }
    g.setColour (ink.withAlpha (0.55f));
    drawBaseline (g, F (9.0f, 1.5f), "FTG-1", P (306, 0).x, P (0, 26).y, Justification::right);
}

juce::Image FuzzToneEditor::createAppIcon (int size)
{
    using namespace juce;
    Image img (Image::ARGB, size, size, true);
    Graphics g (img);
    const float u = (float) size / 1024.0f;
    const auto body = Rectangle<float> (100 * u, 100 * u, 824 * u, 824 * u);
    Path bodyPath; bodyPath.addRoundedRectangle (body, 185.0f * u);
    DropShadow (Colours::black.withAlpha (0.5f), roundToInt (28 * u), { 0, roundToInt (12 * u) }).drawForPath (g, bodyPath);
    ColourGradient enamel (pinkLight, body.getX(), body.getY(), pinkDark, body.getRight(), body.getBottom(), false);
    enamel.addColour (0.45, pinkMid);
    g.setGradientFill (enamel);
    g.fillPath (bodyPath);
    g.setColour (pinkEdge);
    g.strokePath (bodyPath, PathStrokeType (4.0f * u));

    PedalLookAndFeel lnf;
    Slider dummy;
    const auto kb = Rectangle<float> (512 * u - 250 * u, 410 * u - 250 * u, 500 * u, 500 * u).toNearestInt();
    lnf.drawRotarySlider (g, kb.getX(), kb.getY(), kb.getWidth(), kb.getHeight(), 0.8f,
                          MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, dummy);

    auto face = Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
    GlyphArrangement ga;
    ga.addLineOfText (faceFont (face, 150.0f * u, 4.0f * u), "FUZZ", 0, 0);
    Path p; ga.createPath (p);
    const auto bb = p.getBounds();
    const auto tr = AffineTransform::translation (-bb.getCentreX(), -bb.getBottom())
                        .scaled (1.1f, 1.0f).sheared (-0.22f, 0.0f).translated (520 * u, 830 * u);
    g.setColour (ink);
    g.strokePath (p, PathStrokeType (4 * u), tr.translated (16 * u, -12 * u));
    g.setColour (pinkMid);
    g.fillPath (p, tr.translated (6 * u, -5 * u));
    g.setColour (ink);
    g.fillPath (p, tr);
    return img;
}

//==============================================================================
FuzzToneEditor::FuzzToneEditor (FuzzToneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    scriptFace = juce::Typeface::createSystemTypefaceFor (BinaryData::YellowtailRegular_ttf, BinaryData::YellowtailRegular_ttfSize);
    panelFace  = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);

    auto setupKnob = [this] (LearnableSlider& s, const char* id, bool small)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setPopupDisplayEnabled (true, true, this);
        s.setDoubleClickReturnValue (true, (double) proc.apvts.getParameter (id)->getDefaultValue()
                                               * (small ? 48.0 : 1.0) + (small ? -24.0 : 0.0));
        s.setMouseDragSensitivity (small ? 160 : 240);
        s.setTooltip ("Clic destro: MIDI Learn. Doppio clic: valore predefinito");
        const int idx = proc.getLearnIndex (id);
        s.onLearnMenu = [this, idx, &s] { showLearnMenu (idx, &s); };
        addAndMakeVisible (s);
    };

    setupKnob (fuzz,    ParamIDs::fuzz,   false);
    setupKnob (inTrim,  ParamIDs::input,  true);
    setupKnob (outTrim, ParamIDs::output, true);
    fuzz.setTooltip ("FUZZ INCREASE: in senso orario dal segnale del primo stadio al ramo fuzz. Clic destro: MIDI Learn");

    fuzzA = std::make_unique<SA> (proc.apvts, ParamIDs::fuzz,   fuzz);
    inA   = std::make_unique<SA> (proc.apvts, ParamIDs::input,  inTrim);
    outA  = std::make_unique<SA> (proc.apvts, ParamIDs::output, outTrim);

    onOff.onLearnMenu = [this] { showLearnMenu (FuzzToneProcessor::idxBypass, &onOff); };
    onOff.setTooltip ("Interruttore ON/OFF. Clic destro: MIDI Learn");
    addAndMakeVisible (onOff);
    bypassA = std::make_unique<BA> (proc.apvts, ParamIDs::bypass, onOff);

    pickupBox.setTooltip ("Pick-up della chitarra collegata (o sorgente di linea): l'ingresso del pedale lo carica pesantemente");
    pickupBox.onChange = [this] { pickupChosen(); };
    addAndMakeVisible (pickupBox);
    pickupMenuBtn.setTooltip ("Salva i valori attuali con il nome della chitarra, o elimina una chitarra salvata");
    pickupMenuBtn.onClick = [this] { showPickupMenu(); };
    addAndMakeVisible (pickupMenuBtn);
    rebuildPickupBox();

    interior = std::make_unique<InteriorPanel> (proc, panelFace, [this] (int idx, juce::Component* c) { showLearnMenu (idx, c); });
    interior->onClose = [this] { interior->setVisible (false); interiorBtn.setToggleState (false, juce::dontSendNotification); };
    addChildComponent (*interior);
    interiorBtn.setClickingTogglesState (true);
    interiorBtn.setTooltip ("Apre l'interno del pedale: batteria o alimentatore, carica, guadagno dei transistor");
    interiorBtn.setColour (juce::TextButton::buttonOnColourId, accent);
    interiorBtn.onClick = [this] { interior->setVisible (interiorBtn.getToggleState()); interior->toFront (false); };
    addAndMakeVisible (interiorBtn);
    for (auto* s : { &pickR, &pickL })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour (juce::Slider::textBoxTextColourId, cream);
        s->setColour (juce::Slider::trackColourId, juce::Colour (0xff6e6a74));
        s->setColour (juce::Slider::thumbColourId, cream);
        addAndMakeVisible (s);
    }
    pickR.setTooltip ("Resistenza in continua del pick-up (dato del costruttore)");
    pickL.setTooltip ("Induttanza del pick-up");
    pickRA = std::make_unique<SA> (proc.apvts, ParamIDs::pickupR, pickR);
    pickLA = std::make_unique<SA> (proc.apvts, ParamIDs::pickupL, pickL);
    pickR.setTextValueSuffix (juce::String::fromUTF8 (" k\xce\xa9"));
    pickL.setTextValueSuffix (" H");
    pickR.setNumDecimalPlacesToDisplay (1);
    pickL.setNumDecimalPlacesToDisplay (2);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    timerCallback();
    startTimerHz (30);
}

FuzzToneEditor::~FuzzToneEditor()
{
    proc.cancelLearn();
    setLookAndFeel (nullptr);
}

juce::Rectangle<float> FuzzToneEditor::base (float x, float y, float w, float h) const
{
    return { x * scale, y * scale, w * scale, h * scale };
}

namespace { const juce::Rectangle<float> pedalBody (45.0f, FuzzToneEditor::pedalY + 22.0f, 330.0f, 540.0f); }

void FuzzToneEditor::resized()
{
    scale = (float) getWidth() / (float) baseW;
    auto place = [this] (juce::Component& c, float cx, float cy, float w, float h)
    { c.setBounds (base (cx - w * 0.5f, cy - h * 0.5f, w, h).toNearestInt()); };

    // barra di servizio
    place (inTrim,  40.0f,  25.0f, 34.0f, 34.0f);
    place (outTrim, 380.0f, 25.0f, 34.0f, 34.0f);
    pickupBox.setBounds     (base (62.0f,  64.0f, 168.0f, 24.0f).toNearestInt());
    pickupMenuBtn.setBounds (base (234.0f, 64.0f, 26.0f,  24.0f).toNearestInt());
    pickR.setBounds         (base (264.0f, 64.0f, 76.0f,  24.0f).toNearestInt());
    pickL.setBounds         (base (342.0f, 64.0f, 72.0f,  24.0f).toNearestInt());
    interiorBtn.setBounds   (base (298.0f, 12.0f, 52.0f,  36.0f).toNearestInt());
    if (interior != nullptr)
        interior->setBounds (base (20.0f, pedalY + 14.0f, InteriorPanel::baseW, InteriorPanel::baseH).toNearestInt());

    // pedale
    place (fuzz,  pedalBody.getX() + 165.0f, pedalBody.getY() + 122.0f, 140.0f, 140.0f);
    place (onOff, pedalBody.getX() + 165.0f, pedalBody.getY() + 417.0f, 40.0f, 62.0f);

    renderBackground();
}

void FuzzToneEditor::renderBackground()
{
    using namespace juce;
    background = Image (Image::ARGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));

    // fondo: piano di lavoro scuro
    g.setGradientFill (ColourGradient (Colour (0xff201c22), 0, pedalY, Colour (0xff0a090b), 0, (float) baseH, false));
    g.fillRect (0.0f, 0.0f, (float) baseW, (float) baseH);

    // barra di servizio (non fa parte del pedale)
    g.setColour (stripCol);
    g.fillRect (0.0f, 0.0f, (float) baseW, 96.0f);
    g.setColour (lineCol);
    g.fillRect (0.0f, 95.0f, (float) baseW, 1.0f);
    g.setColour (Colour (0xffc9ccd0));
    g.setFont (uiFont (9.0f));
    g.drawText ("INPUT",  Rectangle<float> (14.0f, 42.0f, 52.0f, 12.0f), Justification::centred);
    g.drawText ("OUTPUT", Rectangle<float> (354.0f, 42.0f, 52.0f, 12.0f), Justification::centred);
    g.drawText ("PICKUP", Rectangle<float> (14.0f, 64.0f, 46.0f, 24.0f), Justification::centredLeft);

    drawPedal (g, pedalBody, panelFace, scriptFace, true);
}

//==============================================================================
void FuzzToneEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.drawImageAt (background, 0, 0);
    g.addTransform (AffineTransform::scale (scale));

    // display MIDI
    const Rectangle<float> disp (76.0f, 8.0f, 216.0f, 44.0f);
    g.setColour (Colour (0xff0a110b));
    g.fillRoundedRectangle (disp, 4.0f);
    g.setColour (Colour (0xff23262a));
    g.drawRoundedRectangle (disp, 4.0f, 1.0f);

    g.setColour (Colour (0xff9be08a));
    g.setFont (uiFont (10.0f));
    String line1 = "MIDI", line2;
    const int learn = proc.getLearningParam();
    if (learn >= 0)
    {
        line1 = String ("LEARN: ") + displayNames[learn];
        line2 = "muovi un controllo sulla tastiera";
    }
    else if (const int cc = proc.lastCC.load(); cc >= 0)
    {
        int idx = -1;
        for (int i = 0; i < FuzzToneProcessor::numLearnable; ++i)
            if (proc.getCCForParam (i) == cc) { idx = i; break; }
        line2 = "CC " + String (cc) + " = " + String (proc.lastCCValue.load())
                + (idx >= 0 ? String ("  -> ") + displayNames[idx] : String ("  (non assegnato)"));
    }
    else
        line2 = "clic destro su un comando: Learn";
    g.drawText (line1, disp.reduced (10.0f, 5.0f).removeFromTop (16.0f), Justification::centredLeft);
    g.setFont (uiFont (10.0f, false));
    g.setColour (Colour (0xff8fcf80));
    g.drawText (line2, disp.reduced (10.0f, 5.0f).removeFromBottom (16.0f), Justification::centredLeft);
}

void FuzzToneEditor::paintOverChildren (juce::Graphics& g)
{
    const int learn = proc.getLearningParam();
    if (auto* c = controlFor (learn))
    {
        const float a = 0.45f + 0.45f * std::sin (pulse);
        g.setColour (learnCol.withAlpha (a));
        g.drawRoundedRectangle (getLocalArea (c, c->getLocalBounds()).toFloat().expanded (3.0f * scale), 8.0f * scale, 2.0f * scale);
    }
}

juce::Component* FuzzToneEditor::controlFor (int idx)
{
    switch (idx)
    {
        case FuzzToneProcessor::idxFuzz:   return &fuzz;
        case FuzzToneProcessor::idxBypass: return &onOff;
        case FuzzToneProcessor::idxInput:  return &inTrim;
        case FuzzToneProcessor::idxOutput: return &outTrim;
        case FuzzToneProcessor::idxSupply: case FuzzToneProcessor::idxBattery:
        case FuzzToneProcessor::idxHfe1:   case FuzzToneProcessor::idxHfe2:
            return interior != nullptr && interior->isVisible() ? interior->controlFor (idx) : nullptr;
        default: return nullptr;
    }
}

void FuzzToneEditor::timerCallback()
{
    const int learn = proc.getLearningParam();
    const int mv = proc.getMapVersion();
    const int cnt = proc.ccCounter.load();

    if (learn >= 0) pulse += 0.35f;
    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    syncPickupBox();
    if (pu != lastPickup)
    {
        lastPickup = pu;
        const bool custom = pu == PickupChoice::custom;
        pickR.setVisible (custom); pickL.setVisible (custom);
    }
    if (learn >= 0 || learn != lastLearn || mv != lastMapVersion || cnt != lastCCCount)
    {
        lastLearn = learn; lastMapVersion = mv; lastCCCount = cnt;
        repaint();
    }
}

void FuzzToneEditor::showLearnMenu (int idx, juce::Component* target)
{
    if (idx < 0) return;
    juce::PopupMenu m;
    const int cc = proc.getCCForParam (idx);
    const bool learning = proc.getLearningParam() == idx;

    m.addSectionHeader (displayNames[idx]);
    m.addItem (1, learning ? "Annulla MIDI Learn" : "MIDI Learn (poi muovi un controllo)");
    m.addItem (2, cc >= 0 ? "Rimuovi assegnazione CC " + juce::String (cc) : "Nessun CC assegnato", cc >= 0);
    m.addSeparator();
    m.addItem (3, "Ripristina mappatura predefinita");

    juce::Component::SafePointer<FuzzToneEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                     [safe, idx, learning] (int r)
                     {
                         if (safe == nullptr) return;
                         auto& p = safe->proc;
                         if (r == 1) { if (learning) p.cancelLearn(); else p.startLearn (idx); }
                         else if (r == 2) p.clearMappingForParam (idx);
                         else if (r == 3) p.resetDefaultMappings();
                         safe->repaint();
                     });
}

//==============================================================================
// Pick-up: ID del menu 1-6 = scelte fisse (6 = Personalizzato), 100+ = chitarre salvate
namespace { const juce::Identifier pickupNameProp ("pickupName"); }

float FuzzToneEditor::currentR() const
{
    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    if (pu == PickupChoice::custom) return proc.apvts.getRawParameterValue (ParamIDs::pickupR)->load();
    return (float) (FuzzToneProcessor::pickupFor (pu, 0, 0).R / 1000.0);
}

float FuzzToneEditor::currentL() const
{
    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    if (pu == PickupChoice::custom) return proc.apvts.getRawParameterValue (ParamIDs::pickupL)->load();
    const auto p = FuzzToneProcessor::pickupFor (pu, 0, 0);
    return p.ideal_ ? 0.0f : (float) p.L;
}

void FuzzToneEditor::setParam (const char* id, float realValue)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void FuzzToneEditor::rebuildPickupBox()
{
    pickupBox.clear (juce::dontSendNotification);
    pickupBox.addItem (juce::String::fromUTF8 ("Ideale (10 k\xce\xa9)"), 1);
    pickupBox.addItem (juce::String::fromUTF8 ("Single coil (6 k\xce\xa9, 2.5 H)"), 2);
    pickupBox.addItem (juce::String::fromUTF8 ("P-90 (8 k\xce\xa9, 6 H)"), 3);
    pickupBox.addItem (juce::String::fromUTF8 ("Humbucker (10 k\xce\xa9, 5 H)"), 4);
    pickupBox.addItem (juce::String::fromUTF8 ("Linea / synth (300 \xce\xa9)"), 5);
    pickupBox.addItem ("Personalizzato", 6);

    const auto& entries = library.getEntries();
    if (! entries.empty())
    {
        pickupBox.addSectionHeading ("Le mie chitarre");
        for (size_t i = 0; i < entries.size(); ++i)
            pickupBox.addItem (entries[i].name, 100 + (int) i);
    }
    lastLibVersion = library.getVersion();
    syncPickupBox();
}

void FuzzToneEditor::syncPickupBox()
{
    if (library.getVersion() != lastLibVersion) { rebuildPickupBox(); return; }

    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    int id = juce::jlimit (1, (int) PickupChoice::count, pu + 1);
    if (pu == PickupChoice::custom)
    {
        const auto name = proc.apvts.state.getProperty (pickupNameProp).toString();
        const int idx = library.indexOf (name);
        if (idx >= 0)
        {
            const auto& e = library.getEntries()[(size_t) idx];
            if (std::abs (e.rKohm - currentR()) < 0.051f && std::abs (e.lHenry - currentL()) < 0.026f)
                id = 100 + idx;
        }
    }
    if (pickupBox.getSelectedId() != id) pickupBox.setSelectedId (id, juce::dontSendNotification);
}

void FuzzToneEditor::pickupChosen()
{
    const int id = pickupBox.getSelectedId();
    if (id >= 1 && id <= PickupChoice::count)
    {
        proc.apvts.state.setProperty (pickupNameProp, juce::String(), nullptr);
        setParam (ParamIDs::pickup, (float) (id - 1));
    }
    else if (id >= 100)
    {
        const auto& entries = library.getEntries();
        const auto i = (size_t) (id - 100);
        if (i >= entries.size()) return;
        setParam (ParamIDs::pickupR, entries[i].rKohm);
        setParam (ParamIDs::pickupL, entries[i].lHenry);
        setParam (ParamIDs::pickup, (float) PickupChoice::custom);
        proc.apvts.state.setProperty (pickupNameProp, entries[i].name, nullptr);
    }
    syncPickupBox();
}

void FuzzToneEditor::showPickupMenu()
{
    library.reload();
    juce::PopupMenu m, del;
    m.addItem (1, "Salva i valori attuali come chitarra...");
    const auto& entries = library.getEntries();
    for (size_t i = 0; i < entries.size(); ++i) del.addItem (1000 + (int) i, entries[i].name);
    m.addSubMenu ("Elimina chitarra", del, ! entries.empty());

    juce::Component::SafePointer<FuzzToneEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pickupMenuBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) { safe->askNameAndSave(); return; }
        const auto& e = safe->library.getEntries();
        const auto i = (size_t) (r - 1000);
        if (i < e.size()) safe->library.remove (e[i].name);
        safe->rebuildPickupBox();
    });
}

void FuzzToneEditor::askNameAndSave()
{
    const float r = currentR(), l = currentL();
    const auto current = proc.apvts.state.getProperty (pickupNameProp).toString();

    nameDialog = std::make_unique<juce::AlertWindow> (
        "Salva pick-up",
        juce::String::fromUTF8 ("Valori attuali: ") + juce::String (r, 1) + juce::String::fromUTF8 (" k\xce\xa9, ")
            + juce::String (l, 2) + juce::String::fromUTF8 (" H.\nNome della chitarra (se esiste gi\xc3\xa0, viene aggiornata):"),
        juce::MessageBoxIconType::NoIcon, this);
    nameDialog->addTextEditor ("name", current, "Nome");
    nameDialog->addButton ("Salva", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameDialog->addButton ("Annulla", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<FuzzToneEditor> safe (this);
    nameDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe, r, l] (int result)
    {
        if (safe == nullptr) return;
        const auto name = safe->nameDialog != nullptr ? safe->nameDialog->getTextEditorContents ("name").trim() : juce::String();
        if (result == 1 && name.isNotEmpty())
        {
            safe->library.save (name, r, l);
            safe->setParam (ParamIDs::pickupR, r);
            safe->setParam (ParamIDs::pickupL, l);
            safe->setParam (ParamIDs::pickup, (float) PickupChoice::custom);
            safe->proc.apvts.state.setProperty (pickupNameProp, name, nullptr);
            safe->rebuildPickupBox();
        }
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->nameDialog.reset(); });
    }), false);
}
