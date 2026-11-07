// Genera resources/icon.png e gli screenshot dell'interfaccia (uso: ftg_snapshot <cartella>)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

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
