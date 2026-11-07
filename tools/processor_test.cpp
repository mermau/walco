// Test del processore come lo userebbe una DAW: blocchi stereo a 48 kHz, CC MIDI,
// interruttore ON/OFF, MIDI Learn, stato salvato/ricaricato, CPU.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/PluginProcessor.h"
#include <chrono>

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    bool ok = true;
    auto check = [&] (bool c, const char* what) { std::printf ("%-58s %s\n", what, c ? "OK" : "ERRORE"); ok &= c; };

    FuzzToneProcessor p;
    const double sr = 48000; const int bs = 512;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    std::printf ("latenza: %d campioni\n", p.getLatencySamples());

    juce::AudioBuffer<float> buf (2, bs);
    double phase = 0;
    auto fill = [&] (float amp)
    {
        for (int i = 0; i < bs; ++i)
        {
            const float s = amp * (float) std::sin (phase); phase += 2 * juce::MathConstants<double>::pi * 110.0 / sr;
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
    };
    auto rms = [&] { return buf.getRMSLevel (0, 0, bs); };
    juce::MidiBuffer none;

    // suono a regime
    float r = 0; bool finite = true;
    for (int b = 0; b < 100; ++b) { fill (0.3f); p.processBlock (buf, none); r = rms(); finite &= std::isfinite (r); }
    std::printf ("uscita a FUZZ 7.5: %.3f rms\n", r);
    check (finite && r > 0.01f, "audio elaborato, nessun NaN");

    // CC 20 -> FUZZ INCREASE
    juce::MidiBuffer m; m.addEvent (juce::MidiMessage::controllerEvent (1, 20, 0), 0);
    fill (0.3f); p.processBlock (buf, m); pump (100);
    check (p.apvts.getRawParameterValue (ParamIDs::fuzz)->load() < 0.01f, "CC 20 = 0 porta FUZZ INCREASE a 0");

    // CC 64 premuto -> OFF (bypass)
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    fill (0.3f); p.processBlock (buf, m); pump (100);
    check (p.apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f, "CC 64 commuta l'interruttore su OFF");
    for (int b = 0; b < 40; ++b) { fill (0.3f); p.processBlock (buf, none); }
    {
        juce::AudioBuffer<float> ref (2, bs);
        fill (0.3f); ref.makeCopyOf (buf); p.processBlock (buf, none);
        float diff = 0;
        // in OFF il segnale passa invariato (ritardato della latenza)
        const int L = p.getLatencySamples();
        for (int i = L; i < bs; ++i) diff = std::max (diff, std::abs (buf.getSample (0, i) - ref.getSample (0, i - L)));
        check (diff < 1e-4f, "OFF = segnale diretto (true bypass)");
    }
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);    // rilascio: niente
    m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 10);            // nuova pressione: ON
    fill (0.3f); p.processBlock (buf, m); pump (100);
    check (p.apvts.getRawParameterValue (ParamIDs::bypass)->load() < 0.5f, "seconda pressione: di nuovo ON");

    // MIDI Learn: hFE Q2 su CC 74
    p.startLearn (FuzzToneProcessor::idxHfe2);
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 74, 10), 0);
    fill (0.3f); p.processBlock (buf, m);
    check (p.getCCForParam (FuzzToneProcessor::idxHfe2) == 74, "MIDI Learn assegna CC 74 a hFE Q2");
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 74, 127), 0);
    fill (0.3f); p.processBlock (buf, m); pump (100);
    check (p.apvts.getRawParameterValue (ParamIDs::hfeQ2)->load() > 550.0f, "CC 74 = 127 porta hFE Q2 al massimo");

    // stato
    juce::MemoryBlock st; p.getStateInformation (st);
    FuzzToneProcessor q; q.setStateInformation (st.getData(), (int) st.getSize());
    check (q.getCCForParam (FuzzToneProcessor::idxHfe2) == 74 && q.apvts.getRawParameterValue (ParamIDs::hfeQ2)->load() > 550.0f,
           "stato salvato e ricaricato (parametri + mappa MIDI)");

    // cambio di pick-up e batteria scarica mentre suona
    finite = true;
    for (int b = 0; b < 60; ++b)
    {
        if (b == 10) p.apvts.getParameter (ParamIDs::pickup)->setValueNotifyingHost (p.apvts.getParameter (ParamIDs::pickup)->convertTo0to1 (3.0f));
        if (b == 30) p.apvts.getParameter (ParamIDs::battery)->setValueNotifyingHost (0.0f);
        fill (0.5f); p.processBlock (buf, none); finite &= std::isfinite (rms());
    }
    check (finite, "cambio pick-up e batteria a 4.5 V senza errori");

    // qualità ECO e ritorno ad ALTA mentre suona
    {
        auto* q = p.apvts.getParameter (ParamIDs::quality);
        q->setValueNotifyingHost (1.0f);
        finite = true;
        for (int b = 0; b < 20; ++b) { fill (0.4f); p.processBlock (buf, none); finite &= std::isfinite (rms()) && rms() > 1e-4f; }
        const int latEco = p.getLatencySamples();
        q->setValueNotifyingHost (0.0f);
        for (int b = 0; b < 20; ++b) { fill (0.4f); p.processBlock (buf, none); finite &= std::isfinite (rms()) && rms() > 1e-4f; }
        std::printf ("latenza ECO %d, ALTA %d campioni\n", latEco, p.getLatencySamples());
        check (finite, "commutazione ALTA / ECO durante la riproduzione");
    }

    // silenzio: il calcolo si ferma e riparte senza salti
    {
        juce::AudioBuffer<float> z (2, bs);
        float maxOut = 0;
        for (int b = 0; b < 200; ++b) { buf.clear(); p.processBlock (buf, none); if (b >= 150) maxOut = std::max (maxOut, buf.getMagnitude (0, bs)); }
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < 400; ++b) { buf.clear(); p.processBlock (buf, none); }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("CPU in silenzio: %.2f%% di un core\n", sec / (400.0 * bs / sr) * 100.0);
        float rr = 0;
        for (int b = 0; b < 4; ++b) { fill (0.3f); p.processBlock (buf, none); rr = rms(); }
        std::printf ("  coda in silenzio %.2e, dopo la ripartenza %.3f rms\n", maxOut, rr);
        check (std::isfinite (rr) && rr > 1e-3f && maxOut < 1e-3f, "pausa in silenzio e ripartenza");
    }

    // volume chitarra via CC appreso
    p.startLearn (FuzzToneProcessor::idxGtrVol);
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 75, 10), 0);
    fill (0.3f); p.processBlock (buf, m);
    m.clear(); m.addEvent (juce::MidiMessage::controllerEvent (1, 75, 64), 0);
    fill (0.3f); p.processBlock (buf, m); pump (100);
    check (std::abs (p.apvts.getRawParameterValue (ParamIDs::gtrVol)->load() - 5.04f) < 0.05f, "MIDI Learn sul volume della chitarra");
    p.apvts.getParameter (ParamIDs::gtrVol)->setValueNotifyingHost (1.0f);

    // CPU
    p.apvts.getParameter (ParamIDs::fuzz)->setValueNotifyingHost (1.0f);
    const auto t0 = std::chrono::steady_clock::now();
    const int blocks = (int) (10 * sr / bs);
    for (int b = 0; b < blocks; ++b) { fill (0.4f); p.processBlock (buf, none); }
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("CPU (48 kHz, 8x interno): %.1f%% di un core\n", sec / 10.0 * 100.0);
    return ok ? 0 : 1;
}
