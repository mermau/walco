#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "FuzzToneCircuit.h"

namespace ParamIDs
{
    inline constexpr const char* fuzz    = "fuzz";      // FUZZ INCREASE
    inline constexpr const char* bypass  = "bypass";    // interruttore ON/OFF (true = OFF)
    inline constexpr const char* input   = "input";
    inline constexpr const char* output  = "output";
    inline constexpr const char* pickup  = "pickup";
    inline constexpr const char* pickupR = "pickupR";
    inline constexpr const char* pickupL = "pickupL";
    inline constexpr const char* gtrVol  = "gtrVol";    // volume della chitarra
    inline constexpr const char* quality = "quality";   // Alta (8x) / Eco (4x)
    // pannello "interno"
    inline constexpr const char* supply  = "supply";    // Batteria / Alimentatore 9 V
    inline constexpr const char* battery = "battery";   // tensione della batteria (V)
    inline constexpr const char* hfeQ1   = "hfeQ1";
    inline constexpr const char* hfeQ2   = "hfeQ2";
}

// indici della scelta "Pickup"
namespace PickupChoice { enum { ideal = 0, single, p90, humbucker, line, custom, count }; }

// Curva di scarica tipica di una 9 V alcalina: carica residua (0..1) in funzione della tensione.
namespace Battery
{
    inline constexpr double fullV = 9.6, emptyV = 5.4, minV = 4.5, nominalV = 9.0, dcV = 9.0;
    inline double chargeFromVolts (double v)
    {
        static const double V[] = { 5.4, 6.6, 7.4, 8.1, 8.7, 9.1, 9.6 };
        static const double C[] = { 0.0, 0.10, 0.25, 0.50, 0.75, 0.90, 1.0 };
        if (v <= V[0]) return (v - emptyV) / (emptyV - minV) * 0.25;
        for (int i = 0; i < 6; ++i)
            if (v <= V[i + 1]) return C[i] + (v - V[i]) / (V[i + 1] - V[i]) * (C[i + 1] - C[i]);
        return 1.0;
    }
}

//==============================================================================
class FuzzToneProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    FuzzToneProcessor();
    ~FuzzToneProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // MIDI CC: mappa e MIDI Learn (senza lock). I CC arrivano sul thread audio, che
    // scrive in una coda di atomici; un timer sul thread dei messaggi li applica ai
    // parametri (così l'host riceve le notifiche dal thread giusto).
    static constexpr int numLearnable = 9;
    static const std::array<const char*, numLearnable> learnableIDs;
    static constexpr int idxFuzz = 0, idxBypass = 1, idxInput = 2, idxOutput = 3,
                         idxSupply = 4, idxBattery = 5, idxHfe1 = 6, idxHfe2 = 7, idxGtrVol = 8;

    int  getLearnIndex (const juce::String& paramID) const;
    void startLearn (int paramIndex)  { learningParam.store (paramIndex); }
    void cancelLearn()                { learningParam.store (-1); }
    int  getLearningParam() const     { return learningParam.load(); }
    int  getCCForParam (int paramIndex) const;
    void clearMappingForParam (int paramIndex);
    void resetDefaultMappings();
    int  getMapVersion() const        { return mapVersion.load(); }

    std::atomic<int> lastCC { -1 }, lastCCValue { 0 }, ccCounter { 0 };

    static ftg::Pickup pickupFor (int choice, float rKohm, float lHenry);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleCC (int cc, int value);
    void timerCallback() override;

    ftg::FuzzToneCircuit circuit;
    // due catene di sovracampionamento pronte: Alta (~384 kHz) ed Eco (~192 kHz)
    std::unique_ptr<juce::dsp::Oversampling<float>> osHigh, osEco;
    juce::dsp::Oversampling<float>* oversampling = nullptr;
    int activeQuality = 0;
    double hostRate = 48000.0;
    void applyQuality (int q);
    int idleBlocks = 0;
    juce::dsp::DryWetMixer<float> mixer { 256 };

    juce::SmoothedValue<float> fuzzSm, inGainSm, outGainSm, vccSm, hfe1Sm, hfe2Sm, gtrSm;

    std::atomic<float>* fuzzRaw    = nullptr;
    std::atomic<float>* bypassRaw  = nullptr;
    std::atomic<float>* inRaw      = nullptr;
    std::atomic<float>* outRaw     = nullptr;
    std::atomic<float>* pickupRaw  = nullptr;
    std::atomic<float>* pickRRaw   = nullptr;
    std::atomic<float>* pickLRaw   = nullptr;
    std::atomic<float>* supplyRaw  = nullptr;
    std::atomic<float>* batteryRaw = nullptr;
    std::atomic<float>* hfe1Raw    = nullptr;
    std::atomic<float>* hfe2Raw    = nullptr;
    std::atomic<float>* gtrRaw     = nullptr;
    std::atomic<float>* qualityRaw = nullptr;
    float targetSupply() const;
    ftg::Pickup currentPickup() const;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::array<juce::RangedAudioParameter*, numLearnable> learnParams {};
    std::array<std::atomic<int>, 128> ccToParam;
    std::array<bool, 128> ccHigh {};
    std::atomic<int> learningParam { -1 }, mapVersion { 0 };

    // valori normalizzati in attesa (−1 = nessuno), scritti dal thread audio
    std::array<std::atomic<float>, numLearnable> pendingValue;

    int maxBlock = 512, osChannels = 2;
    int bypassFadeSamples = 4000, bypassFadeLeft = 0;
    bool lastBypassState = false, circuitIdle = false;

    static constexpr int controlBlock = 32;       // campioni (frequenza base)
    static constexpr float voltsPerFS = 1.0f;     // 0 dBFS = 1 V di picco all'ingresso
    static constexpr float fsPerVolt  = 1.0f;     // livello d'uscita reale del pedale

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzToneProcessor)
};
