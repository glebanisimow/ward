// Ward: layers of medical-device sources (engines/devices/*.dsp), each with
// its own granular / filter / drive / comb / delay chain, into a master
// reverb and limiter. Records the master output to WAV.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Layer.h"
#include "MidiMono.h"
#include "PeakLimiter.h"
#include "Recorder.h"

#include <array>
#include <atomic>

struct SceneLayer
{
    const char* device;
    int variant;
    std::vector<std::pair<const char*, float>> values;   // layer params ("gMix") or device params ("src:time")
};

struct Scene
{
    const char* name;
    std::vector<SceneLayer> layers;
    std::vector<std::pair<const char*, float>> master;
};

class WardProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int CHUNK = 256;

    WardProcessor();
    ~WardProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ward"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentScene; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- editor access (message thread) ---
    static const std::vector<Scene>& scenes();
    int addLayer (int device, int variant);   // -1 if all slots are in use
    int duplicateLayer (int layer);
    void removeLayer (int layer);
    juce::String layerLabel (int layer) const;
    void setLayerLabel (int layer, const juce::String&);
    float param (const juce::String& id) const;
    void setParam (const juce::String& id, float value);
    void resetLayerParams (int layer);
    juce::String srcTextFor (int layer, int k, float norm) const;

    float cpuLoad() const { return cpu.load (std::memory_order_relaxed); }
    double sampleRate() const { return sr; }

    juce::AudioProcessorValueTreeState apvts;
    std::vector<std::unique_ptr<Layer>> layers;   // after apvts: they read its parameters
    Recorder recorder;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *mLevel, *mSize, *mDamp, *mWidth, *mReturn, *mFreeze;
    std::array<std::vector<float>, 4> mix;   // L, R, send L, send R
    juce::Reverb reverb;
    PeakLimiter limiter;
    juce::SmoothedValue<float> masterGain, returnGain;
    MidiMono mono;
    float transpose = 0.0f;   // semitones, glides towards the MIDI note

    std::atomic<float> cpu { 0.0f };
    double sr = 48000.0;
    int currentScene = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WardProcessor)
};
