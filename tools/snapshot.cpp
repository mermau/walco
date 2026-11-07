// Genera resources/icon.png e gli screenshot dell'interfaccia (uso: ftg_snapshot <cartella>)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "BinaryData.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    auto save = [&] (const juce::Image& img, const char* name)
    {
        juce::File f = dir.getChildFile (name);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    };
    save (FuzzToneEditor::createAppIcon (1024), "icon.png");

    // il solo pedale su fondo trasparente (per il manuale), a 3x
    {
        const float sc = 3.0f;
        const juce::Rectangle<float> body (20.0f, 20.0f, 330.0f, 540.0f);
        juce::Image img (juce::Image::ARGB, (int) (370 * sc), (int) (720 * sc), true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (sc));
        auto panel  = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
        auto script = juce::Typeface::createSystemTypefaceFor (BinaryData::YellowtailRegular_ttf, BinaryData::YellowtailRegular_ttfSize);
        FuzzToneEditor::drawPedal (g, body, panel, script, true);
        PedalLookAndFeel lnf; juce::Slider dummy;
        lnf.drawRotarySlider (g, (int) (body.getX() + 165 - 70), (int) (body.getY() + 122 - 70), 140, 140, 0.75f,
                              juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, dummy);
        SlideSwitch sw; sw.setBounds (0, 0, 40, 62);
        g.saveState();
        g.setOrigin ((int) (body.getX() + 165 - 20), (int) (body.getY() + 417 - 31));
        sw.paintEntireComponent (g, false);
        g.restoreState();
        save (img, "pedale.png");
    }

    FuzzToneProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (FuzzToneEditor::baseW * 2, FuzzToneEditor::baseH * 2);
    save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), "screenshot.png");

    ed.reset();
    proc.apvts.getParameter (ParamIDs::bypass)->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter (ParamIDs::fuzz)->setValueNotifyingHost (0.0f);
    ed.reset (proc.createEditor());
    ed->setSize (FuzzToneEditor::baseW * 2, FuzzToneEditor::baseH * 2);
    save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), "screenshot_off.png");

    for (auto* c : ed->getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == "INTERNO")
            { b->setToggleState (true, juce::dontSendNotification); b->onClick(); }
    save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), "screenshot_interno.png");
    ed.reset();
    return 0;
}
