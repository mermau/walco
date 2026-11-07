#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::array<const char*, FuzzToneProcessor::numLearnable> FuzzToneProcessor::learnableIDs {
    ParamIDs::fuzz, ParamIDs::bypass, ParamIDs::input, ParamIDs::output,
    ParamIDs::supply, ParamIDs::battery, ParamIDs::hfeQ1, ParamIDs::hfeQ2, ParamIDs::gtrVol
};

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout FuzzToneProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto trim = [] (const char* id, const char* name)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB"));
    };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::fuzz, 1 }, "Fuzz Increase", NormalisableRange<float> (0.0f, 1.0f), 0.75f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v * 10.0f, 1); })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Off (Bypass)", false));
    layout.add (trim (ParamIDs::input,  "Input Trim"));
    layout.add (trim (ParamIDs::output, "Output Trim"));

    // pick-up (o sorgente) collegato: l'ingresso del pedale è in massa virtuale e lo carica
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::pickup, 1 }, "Pickup",
        StringArray { "Ideale", "Single coil", "P-90", "Humbucker", "Linea", "Personalizzato" }, PickupChoice::single));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::pickupR, 1 }, "Pickup R",
        NormalisableRange<float> (2.0f, 20.0f, 0.1f), 8.0f, AudioParameterFloatAttributes().withLabel ("kOhm")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::pickupL, 1 }, "Pickup L",
        NormalisableRange<float> (0.5f, 10.0f, 0.05f), 4.0f, AudioParameterFloatAttributes().withLabel ("H")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gtrVol, 1 }, "Guitar Volume",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 10.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    // pannello interno
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::supply, 1 }, "Supply",
        StringArray { "Battery", "DC 9 V" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::battery, 1 }, "Battery",
        NormalisableRange<float> ((float) Battery::minV, (float) Battery::fullV, 0.01f), (float) Battery::nominalV,
        AudioParameterFloatAttributes().withLabel ("V")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));
    auto hfeRange = [] { NormalisableRange<float> r (60.0f, 560.0f, 1.0f); r.setSkewForCentre (250.0f); return r; };
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::hfeQ1, 1 }, "hFE Q1", hfeRange(), 250.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::hfeQ2, 1 }, "hFE Q2", hfeRange(), 250.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::quality, 1 }, "Quality",
        StringArray { "Alta (8x)", "Eco (4x)" }, 0));
    return layout;
}

FuzzToneProcessor::FuzzToneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    fuzzRaw    = apvts.getRawParameterValue (ParamIDs::fuzz);
    bypassRaw  = apvts.getRawParameterValue (ParamIDs::bypass);
    inRaw      = apvts.getRawParameterValue (ParamIDs::input);
    outRaw     = apvts.getRawParameterValue (ParamIDs::output);
    pickupRaw  = apvts.getRawParameterValue (ParamIDs::pickup);
    pickRRaw   = apvts.getRawParameterValue (ParamIDs::pickupR);
    pickLRaw   = apvts.getRawParameterValue (ParamIDs::pickupL);
    supplyRaw  = apvts.getRawParameterValue (ParamIDs::supply);
    batteryRaw = apvts.getRawParameterValue (ParamIDs::battery);
    hfe1Raw    = apvts.getRawParameterValue (ParamIDs::hfeQ1);
    hfe2Raw    = apvts.getRawParameterValue (ParamIDs::hfeQ2);
    gtrRaw     = apvts.getRawParameterValue (ParamIDs::gtrVol);
    qualityRaw = apvts.getRawParameterValue (ParamIDs::quality);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::bypass));

    for (int i = 0; i < numLearnable; ++i)
    {
        learnParams[(size_t) i] = apvts.getParameter (learnableIDs[(size_t) i]);
        pendingValue[(size_t) i].store (-1.0f);
    }

    resetDefaultMappings();
    startTimerHz (60);
}

//==============================================================================
void FuzzToneProcessor::resetDefaultMappings()
{
    for (auto& m : ccToParam) m.store (-1);
    // come FYN-2: CC 20 per la manopola, CC 64 (pedale sustain) per l'interruttore
    ccToParam[20].store (idxFuzz);
    ccToParam[64].store (idxBypass);
    ++mapVersion;
}

int FuzzToneProcessor::getLearnIndex (const juce::String& paramID) const
{
    for (int i = 0; i < numLearnable; ++i)
        if (paramID == learnableIDs[(size_t) i]) return i;
    return -1;
}

int FuzzToneProcessor::getCCForParam (int paramIndex) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (ccToParam[(size_t) cc].load() == paramIndex) return cc;
    return -1;
}

void FuzzToneProcessor::clearMappingForParam (int paramIndex)
{
    for (auto& m : ccToParam)
        if (m.load() == paramIndex) m.store (-1);
    ++mapVersion;
}

// thread audio: niente chiamate all'host, solo atomici
void FuzzToneProcessor::handleCC (int cc, int value)
{
    lastCC.store (cc); lastCCValue.store (value); ++ccCounter;

    int learn = learningParam.load();
    if (learn >= 0 && learningParam.compare_exchange_strong (learn, -1))
    {
        for (auto& m : ccToParam)
            if (m.load() == learn) m.store (-1);
        ccToParam[(size_t) cc].store (learn);
        ccHigh[(size_t) cc] = value >= 64;
        ++mapVersion;
        return; // il messaggio usato per l'apprendimento non modifica il parametro
    }

    const int idx = ccToParam[(size_t) cc].load();
    if (idx < 0) return;
    auto& pending = pendingValue[(size_t) idx];

    if (idx == idxBypass || idx == idxSupply)   // interruttori: commutano alla pressione
    {
        const bool high = value >= 64;
        if (high && ! ccHigh[(size_t) cc])
        {
            const float cur = pending.load() >= 0.0f ? pending.load() : learnParams[(size_t) idx]->getValue();
            pending.store (cur > 0.5f ? 0.0f : 1.0f);
        }
        ccHigh[(size_t) cc] = high;
    }
    else
    {
        pending.store ((float) value / 127.0f);
    }
}

// thread dei messaggi: applica i CC ricevuti ai parametri (notifica all'host)
void FuzzToneProcessor::timerCallback()
{
    for (int i = 0; i < numLearnable; ++i)
    {
        const float v = pendingValue[(size_t) i].exchange (-1.0f);
        if (v < 0.0f) continue;
        auto* p = learnParams[(size_t) i];
        if (std::abs (p->getValue() - v) < 1.0e-6f) continue;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    }
}

float FuzzToneProcessor::targetSupply() const
{
    return supplyRaw->load() > 0.5f ? (float) Battery::dcV : batteryRaw->load();
}

ftg::Pickup FuzzToneProcessor::pickupFor (int choice, float rKohm, float lHenry)
{
    switch (choice)
    {
        case PickupChoice::ideal:     return ftg::Pickup::ideal();
        case PickupChoice::single:    return ftg::Pickup::singleCoil();
        case PickupChoice::p90:       return ftg::Pickup::p90();
        case PickupChoice::humbucker: return ftg::Pickup::humbucker();
        case PickupChoice::line:      return ftg::Pickup::line();
        default:                      return { rKohm * 1000.0, (double) lHenry, 150e-12, 250e3 };
    }
}

ftg::Pickup FuzzToneProcessor::currentPickup() const
{
    return pickupFor ((int) pickupRaw->load(), pickRRaw->load(), pickLRaw->load());
}

//==============================================================================
bool FuzzToneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void FuzzToneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = (size_t) juce::jmax (1, getTotalNumOutputChannels());
    maxBlock = juce::jmax (1, samplesPerBlock);
    osChannels = (int) numCh;

    // Alta: il circuito gira a circa 350-390 kHz (8x a 44.1/48 kHz, 4x a 88.2/96 kHz,
    // 2x a 176.4/192 kHz). Eco: metà della frequenza interna, metà del carico.
    hostRate = sampleRate;
    const int order = sampleRate > 300000.0 ? 0 : (sampleRate > 120000.0 ? 1 : (sampleRate > 60000.0 ? 2 : 3));
    using OS = juce::dsp::Oversampling<float>;
    osHigh = std::make_unique<OS> (1, (size_t) order, OS::filterHalfBandPolyphaseIIR, true, true);
    osEco  = std::make_unique<OS> (1, (size_t) juce::jmax (0, order - 1), OS::filterHalfBandPolyphaseIIR, true, true);
    osHigh->initProcessing ((size_t) samplesPerBlock);
    osEco->initProcessing ((size_t) samplesPerBlock);

    circuit.setPickup (currentPickup());
    circuit.setSupply (targetSupply());
    circuit.setHfe (hfe1Raw->load(), hfe2Raw->load());
    circuit.setFuzz (fuzzRaw->load());
    circuit.setGuitarVolume (gtrRaw->load() / 10.0f);

    mixer.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    applyQuality ((int) qualityRaw->load());

    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    mixer.setWetMixProportion (bypassRaw->load() > 0.5f ? 0.0f : 1.0f);
    idleBlocks = 0;

    bypassFadeSamples = juce::roundToInt (0.08 * sampleRate);
    lastBypassState = bypassRaw->load() > 0.5f;
    bypassFadeLeft = 0;
    circuitIdle = lastBypassState;

    auto initSm = [sampleRate] (juce::SmoothedValue<float>& s, float v, double sec)
    { s.reset (sampleRate, sec); s.setCurrentAndTargetValue (v); };
    initSm (fuzzSm,    fuzzRaw->load(), 0.03);
    initSm (inGainSm,  juce::Decibels::decibelsToGain (inRaw->load()),  0.03);
    initSm (outGainSm, juce::Decibels::decibelsToGain (outRaw->load()), 0.03);
    initSm (vccSm,  targetSupply(),  0.15);
    initSm (hfe1Sm, hfe1Raw->load(), 0.10);
    initSm (hfe2Sm, hfe2Raw->load(), 0.10);
    initSm (gtrSm,  gtrRaw->load() / 10.0f, 0.03);
}

// cambia la catena di sovracampionamento e riporta il circuito al punto di lavoro
// alla nuova frequenza interna (meno di 1 ms, si può fare sul thread audio)
void FuzzToneProcessor::applyQuality (int q)
{
    activeQuality = q;
    oversampling = (q == 1) ? osEco.get() : osHigh.get();
    oversampling->reset();
    circuit.prepare (hostRate * (double) oversampling->getOversamplingFactor());
    const auto latency = oversampling->getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));
    mixer.setWetLatency (latency);
}

void FuzzToneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isController()) handleCC (m.getControllerNumber(), m.getControllerValue());
    }
    midi.clear();

    const int numCh = juce::jmin (getTotalNumInputChannels(), osChannels);
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh < 1) return;

    circuit.setPickup (currentPickup());
    fuzzSm.setTargetValue (fuzzRaw->load());
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (inRaw->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (outRaw->load()));
    vccSm.setTargetValue (targetSupply());
    hfe1Sm.setTargetValue (hfe1Raw->load());
    hfe2Sm.setTargetValue (hfe2Raw->load());
    gtrSm.setTargetValue (gtrRaw->load() / 10.0f);
    if ((int) qualityRaw->load() != activeQuality) applyQuality ((int) qualityRaw->load());
    const bool bypassed = bypassRaw->load() > 0.5f;
    mixer.setWetMixProportion (bypassed ? 0.0f : 1.0f);

    // interruttore su OFF: dopo la dissolvenza il circuito resta fermo e non consuma CPU;
    // riaccendendo riparte dal punto di riposo
    if (bypassed && ! lastBypassState) bypassFadeLeft = bypassFadeSamples;
    if (! bypassed && circuitIdle)
    {
        vccSm.setCurrentAndTargetValue (targetSupply());
        hfe1Sm.setCurrentAndTargetValue (hfe1Raw->load());
        hfe2Sm.setCurrentAndTargetValue (hfe2Raw->load());
        fuzzSm.setCurrentAndTargetValue (fuzzRaw->load());
        gtrSm.setCurrentAndTargetValue (gtrRaw->load() / 10.0f);
        circuit.setGuitarVolume (gtrSm.getCurrentValue());
        circuit.setSupply (vccSm.getCurrentValue());
        circuit.setHfe (hfe1Sm.getCurrentValue(), hfe2Sm.getCurrentValue());
        circuit.setFuzz (fuzzSm.getCurrentValue());
        circuit.reset();
        oversampling->reset();
        circuitIdle = false;
    }
    lastBypassState = bypassed;
    if (bypassed && ! circuitIdle)
    {
        bypassFadeLeft -= buffer.getNumSamples();
        if (bypassFadeLeft < -buffer.getNumSamples()) circuitIdle = true;
    }

    juce::dsp::AudioBlock<float> full (buffer);
    full = full.getSubsetChannelBlock (0, (size_t) numCh);
    const int factor = (int) oversampling->getOversamplingFactor();
    if (circuitIdle) idleBlocks = 0;

    for (int offset = 0; offset < buffer.getNumSamples(); offset += maxBlock)
    {
        const int n = juce::jmin (maxBlock, buffer.getNumSamples() - offset);
        auto block = full.getSubBlock ((size_t) offset, (size_t) n);
        auto mono  = block.getSingleChannelBlock (0);

        mixer.pushDrySamples (block);
        if (circuitIdle) { mixer.mixWetSamples (block); continue; }

        auto* x = mono.getChannelPointer (0);

        // la chitarra può arrivare su uno qualsiasi dei due ingressi: se i canali sono
        // copie identiche si usa il primo, altrimenti si sommano
        if (numCh > 1)
        {
            const auto* r = block.getChannelPointer (1);
            bool identical = true;
            for (int i = 0; i < n && identical; ++i) identical = std::abs (x[i] - r[i]) < 1.0e-7f;
            if (! identical) juce::FloatVectorOperations::add (x, r, n);
        }

        for (int i = 0; i < n; ++i) x[i] *= inGainSm.getNextValue() * voltsPerFS;

        // silenzio in ingresso, circuito a riposo e comandi fermi: il calcolo si ferma
        // (l'uscita del pedale è comunque nulla) e riparte appena arriva segnale
        const bool silent = juce::FloatVectorOperations::findMaximum (x, n) < 1.0e-7f
                         && juce::FloatVectorOperations::findMinimum (x, n) > -1.0e-7f;
        const bool still = ! (fuzzSm.isSmoothing() || vccSm.isSmoothing() || hfe1Sm.isSmoothing()
                              || hfe2Sm.isSmoothing() || gtrSm.isSmoothing());
        idleBlocks = (silent && still && circuit.isSettled()) ? idleBlocks + 1 : 0;
        if (idleBlocks > 2)
        {
            juce::FloatVectorOperations::clear (x, n);
            outGainSm.skip (n);
            for (int ch = 1; ch < numCh; ++ch) juce::FloatVectorOperations::clear (block.getChannelPointer ((size_t) ch), n);
            mixer.mixWetSamples (block);
            continue;
        }

        auto up = oversampling->processSamplesUp (mono);
        auto* d = up.getChannelPointer (0);

        for (int start = 0; start < n; start += controlBlock)
        {
            const int len = juce::jmin (controlBlock, n - start);
            circuit.setFuzz (fuzzSm.skip (len));
            circuit.setGuitarVolume (gtrSm.skip (len));
            circuit.setSupply (vccSm.skip (len));
            circuit.setHfe (hfe1Sm.skip (len), hfe2Sm.skip (len));
            for (int i = start * factor, end = (start + len) * factor; i < end; ++i)
                d[i] = (float) circuit.process ((double) d[i]);
        }

        oversampling->processSamplesDown (mono);

        for (int i = 0; i < n; ++i) x[i] *= outGainSm.getNextValue() * fsPerVolt;
        for (int ch = 1; ch < numCh; ++ch)
            juce::FloatVectorOperations::copy (block.getChannelPointer ((size_t) ch), x, n);

        mixer.mixWetSamples (block);
    }
}

//==============================================================================
void FuzzToneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("FTGState");
    if (auto params = apvts.copyState().createXml()) root.addChildElement (params.release());

    auto* map = root.createNewChildElement ("MIDIMAP");
    for (int cc = 0; cc < 128; ++cc)
        if (const int idx = ccToParam[(size_t) cc].load(); idx >= 0)
        {
            auto* e = map->createNewChildElement ("CC");
            e->setAttribute ("num", cc);
            e->setAttribute ("param", learnableIDs[(size_t) idx]);
        }
    copyXmlToBinary (root, destData);
}

void FuzzToneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("FTGState")) return;

    if (auto* params = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    if (auto* map = xml->getChildByName ("MIDIMAP"))
    {
        for (auto& m : ccToParam) m.store (-1);
        for (auto* e : map->getChildWithTagNameIterator ("CC"))
        {
            const int cc = e->getIntAttribute ("num", -1);
            const int idx = getLearnIndex (e->getStringAttribute ("param"));
            if (juce::isPositiveAndBelow (cc, 128) && idx >= 0) ccToParam[(size_t) cc].store (idx);
        }
        ++mapVersion;
    }
}

juce::AudioProcessorEditor* FuzzToneProcessor::createEditor() { return new FuzzToneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FuzzToneProcessor(); }
