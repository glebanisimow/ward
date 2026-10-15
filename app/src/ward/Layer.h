// One layer: a device engine -> granular -> filter -> drive / crush -> comb
// -> delay -> level / pan, plus a post-fader send to the master reverb.
// Host parameters are "L<n>.<name>", n = 1..MAX_LAYERS; every slot exists
// for the host, the "on" parameter decides whether the layer is in use.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Devices.h"
#include "Granular.h"

#include <array>
#include <atomic>
#include <functional>

constexpr int MAX_LAYERS = 12;

juce::String layerId (int layer, const char* name);

class Layer
{
public:
    using SrcTextFn = std::function<juce::String (int layer, int k, float norm)>;
    static void addParams (juce::AudioProcessorValueTreeState::ParameterLayout&, int layer, SrcTextFn);
    static const char* const filterTypes[];

    Layer (int index, juce::AudioProcessorValueTreeState&);

    void prepare (double sampleRate, int maxBlock);
    // Adds the layer into out and send (stereo). Returns false if it was idle.
    // transpose: semitones added to the layer pitch (MIDI note relative to C4)
    bool process (int n, float* outL, float* outR, float* sendL, float* sendR, bool soloActive, float transpose);

    // --- message thread ---
    bool isOn() const { return pOn->load() > 0.5f; }
    int device() const { return juce::jlimit (0, (int) devices().size() - 1, (int) pSource->load()); }
    const std::vector<FaustParam>& srcParams (int dev) const { return devs[(size_t) dev].params; }
    float meter() const { return level.load (std::memory_order_relaxed); }
    void requestReset() { resetReq.store (true); }   // a slot reused for a new layer
    const Granular& granular() const { return gran; }

private:
    struct Dev
    {
        std::unique_ptr<dsp> d;
        ParamCollector ui;
        float* seed = nullptr; float* time = nullptr; float* pitch = nullptr;
        std::vector<FaustParam> params;
    };

    std::atomic<float>* raw (const char* name) const;
    void resetState();

    const int index;
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Dev> devs;

    std::atomic<float> *pOn, *pSource, *pMute, *pSolo, *pLevel, *pPan, *pSeed, *pTime, *pPitch;
    std::array<std::atomic<float>*, MAX_SRC> pSrc {};
    std::atomic<float> *gMix, *gSize, *gDens, *gPos, *gSpray, *gScan, *gPitch, *gRand, *gRev, *gWidth, *gFreeze;
    std::atomic<float> *fType, *fCut, *fRes, *dDrive, *dBits, *dDown;
    std::atomic<float> *cFreq, *cFb, *cMix, *dlTime, *dlFb, *dlMix, *pSend, *pDrift, *pDriftRate;

    double sr = 48000;
    int cur = 0;
    bool wasOn = false;
    float switchGain = 1.0f;       // fade for device changes
    juce::SmoothedValue<float> onGain, gainL, gainR, sendGain, combMix, dlMixS;
    juce::SmoothedValue<float> dlSamples;

    std::vector<float> mono, bufL, bufR;
    Granular gran;
    juce::dsp::StateVariableTPTFilter<float> filter;
    int lastType = -1;
    std::array<std::vector<float>, 2> comb;
    int combPos = 0;
    std::array<float, 2> combLp {}, holdV {};
    int holdN = 0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 1 << 18 };
    std::array<float, 3> drift {};
    juce::Random rng;
    std::atomic<float> level { 0.0f };
    std::atomic<bool> resetReq { false };
};
