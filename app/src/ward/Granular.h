// Granular stage of a layer: the source is written into a ring buffer, grains
// (Hann window, linear interpolation) read it back at a position behind the
// write head, transposed and optionally reversed. Freeze stops writing; scan
// then moves the read position through the frozen buffer.
#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <vector>

class Granular
{
public:
    static constexpr int MAX_GRAINS = 48, PEAK_BLOCK = 256;

    struct Params
    {
        float mix = 0, size = 0.2f, density = 12, pos = 0.1f, spray = 0.1f, scan = 0;
        float pitch = 0, rand = 0, reverse = 0, width = 0.5f;
        bool freeze = false;
    };

    void prepare (double sampleRate, double seconds);
    void clear();
    // in: mono source; out: stereo, dry/grain mixed by p.mix. posMod shifts pos.
    void process (const float* in, float* outL, float* outR, int n, const Params& p, float posMod);

    // --- UI snapshot (benign races: display only) ---
    int length() const { return size; }
    int writeHead() const { return w.load (std::memory_order_relaxed); }
    int filledSamples() const { return filled.load (std::memory_order_relaxed); }
    const std::vector<float>& peaks() const { return peak; }
    int grainSnapshot (std::array<float, MAX_GRAINS>& pos) const;   // buffer positions / size

private:
    struct Grain
    {
        double pos = 0, inc = 1;
        int len = 0, age = 0;
        float c = 1, s = 0, rc = 1, rs = 0;   // rotating sin for the Hann window
        float gl = 0, gr = 0, amp = 1;
        bool on = false;
    };

    void spawn (const Params& p, float posMod);
    float read (double x) const;

    double sr = 48000;
    std::vector<float> buf, peak;
    int size = 0;
    std::atomic<int> w { 0 }, filled { 0 };
    std::array<Grain, MAX_GRAINS> grains;
    std::array<std::atomic<float>, MAX_GRAINS> snap {};
    std::atomic<int> snapCount { 0 };
    double spawnPhase = 0, scanOff = 0;
    float peakAcc = 0;
    int peakN = 0;
    juce::Random rng { 12345 };
};
