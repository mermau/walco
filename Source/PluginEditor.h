#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PickupLibrary.h"

//==============================================================================
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return juce::Font (juce::FontOptions (juce::jmin (30.0f, (float) b.getHeight() * 0.46f))); }
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.34f), juce::Font::bold));
    }
};

//==============================================================================
// Slider con menu MIDI Learn sul clic destro
class LearnableSlider : public juce::Slider
{
public:
    std::function<void()> onLearnMenu;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag (e); }
    void mouseUp   (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseUp (e); }
};

// Interruttore a slitta ON/OFF del pedale (verticale: ON in alto).
// Stato del pulsante = parametro "bypass": true = OFF (cursore in basso).
class SlideSwitch : public juce::Button
{
public:
    SlideSwitch() : juce::Button ("ON/OFF") { setClickingTogglesState (true); }
    std::function<void()> onLearnMenu;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Button::mouseUp (e); }
};

// Deviatore a slitta a due posizioni del pannello interno (BATT/DC, ALTA/ECO)
class SupplySwitch : public juce::Button
{
public:
    SupplySwitch (juce::String left = "BATT", juce::String right = "DC")
        : juce::Button ("Switch"), leftText (std::move (left)), rightText (std::move (right)) { setClickingTogglesState (true); }
    juce::String leftText, rightText;
    std::function<void()> onLearnMenu;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Button::mouseUp (e); }
};

//==============================================================================
// Pannello "interno" (come aprire il fondo del pedale): alimentazione e transistor
class InteriorPanel : public juce::Component, private juce::Timer
{
public:
    InteriorPanel (FuzzToneProcessor&, juce::Typeface::Ptr panelFace,
                   std::function<void (int, juce::Component*)> learnMenu);
    ~InteriorPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Component* controlFor (int paramIndex);
    std::function<void()> onClose;

    static constexpr float baseW = 380.0f, baseH = 656.0f;

private:
    void timerCallback() override;
    juce::Font font (float pt, float tracking = 0.0f) const;
    juce::Rectangle<int> area (float x, float y, float w, float h) const;
    void resetToOriginal();

    FuzzToneProcessor& proc;
    juce::Typeface::Ptr face;
    SupplySwitch supply, quality { "ALTA 8x", "ECO 4x" };
    LearnableSlider charge, hfe1, hfe2;
    juce::TextButton resetBtn { "VALORI ORIGINALI" }, closeBtn { "CHIUDI" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> supplyA, qualityA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chargeA, hfe1A, hfe2A;
    float k = 1.0f, lastV = -1.0f, lastH1 = -1.0f, lastH2 = -1.0f;
    bool lastDC = false;
};

//==============================================================================
class FuzzToneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FuzzToneEditor (FuzzToneProcessor&);
    ~FuzzToneEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // icona dell'applicazione Standalone (resources/icon.png)
    static juce::Image createAppIcon (int size);
    // disegno del pedale (usato anche dall'icona), coordinate di progetto
    static void drawPedal (juce::Graphics&, juce::Rectangle<float> body,
                           juce::Typeface::Ptr panelFace, juce::Typeface::Ptr scriptFace, bool withExtras);

    // coordinate di progetto: barra di servizio 0-96, pedale sotto
    static constexpr int baseW = 420, baseH = 780;
    static constexpr float pedalY = 96.0f;

private:
    void timerCallback() override;
    void showLearnMenu (int paramIndex, juce::Component* target);
    void renderBackground();
    juce::Component* controlFor (int paramIndex);
    juce::Rectangle<float> base (float x, float y, float w, float h) const;

    // pick-up: preset fissi + chitarre salvate dall'utente
    void rebuildPickupBox();
    void syncPickupBox();
    void pickupChosen();
    void showPickupMenu();
    void askNameAndSave();
    void setParam (const char* id, float realValue);
    float currentR() const;
    float currentL() const;

    FuzzToneProcessor& proc;
    PedalLookAndFeel lnf;

    LearnableSlider fuzz, inTrim, outTrim, gtrVol;
    SlideSwitch onOff;
    juce::ComboBox pickupBox;
    juce::Slider pickR, pickL;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> fuzzA, inA, outA, gtrA;
    std::unique_ptr<BA> bypassA;
    juce::TextButton pickupMenuBtn { juce::String::fromUTF8 ("\xe2\x80\xa6") };
    juce::TextButton interiorBtn { "INTERNO" };
    std::unique_ptr<InteriorPanel> interior;
    PickupLibrary library;
    std::unique_ptr<juce::AlertWindow> nameDialog;
    int lastLibVersion = -1;
    std::unique_ptr<SA> pickRA, pickLA;
    int lastPickup = -1;

    juce::Typeface::Ptr scriptFace, panelFace;
    juce::TooltipWindow tooltips { this, 700 };
    juce::Image background;
    float scale = 1.0f;
    int lastLearn = -2, lastMapVersion = -1, lastCCCount = -1;
    float pulse = 0.0f;
};
