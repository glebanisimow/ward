// Transparent look-ahead peak limiter (stereo, linked).
// Below the threshold the gain is exactly 1, so the signal passes unchanged
// (delayed by the look-ahead). Above it, the gain is the sliding minimum of
// the required gain over look-ahead + 1 samples, averaged over the look-ahead
// window: every output sample is at or under the threshold and the gain
// moves smoothly (no instantaneous gain steps inside a waveform, unlike
// juce::dsp::Limiter, whose always-on -10 dB first stage and instant final
// stage add distortion and make-up gain). Release is a one-pole.
#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <vector>

class PeakLimiter
{
public:
    void prepare (double sampleRate, float lookaheadMs = 3.0f, float releaseMs = 150.0f, float thresholdDb = -1.0f)
    {
        L = juce::jmax (1, (int) std::lround (sampleRate * lookaheadMs * 0.001));
        thr = std::pow (10.0f, thresholdDb / 20.0f);
        rel = 1.0f - std::exp (-1.0f / ((float) sampleRate * releaseMs * 0.001f));
        dl.assign ((size_t) L, 0.0f);
        dr.assign ((size_t) L, 0.0f);
        req.assign ((size_t) L + 1, 1.0f);
        win.assign ((size_t) L, 1.0f);
        sum = (double) L;
        pos = rpos = wpos = 0;
        gain = 1.0f;
    }

    int latency() const { return L; }

    void process (float* left, float* right, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float l = left[i], r = right != nullptr ? right[i] : l;
            const float peak = std::max (std::abs (l), std::abs (r));
            req[(size_t) rpos] = peak > thr ? thr / peak : 1.0f;
            rpos = rpos + 1 == (int) req.size() ? 0 : rpos + 1;
            float g1 = 1.0f;
            for (float v : req) g1 = std::min (g1, v);       // sliding min, L + 1 samples
            sum += g1 - win[(size_t) wpos];                   // box average, L samples
            win[(size_t) wpos] = g1;
            wpos = wpos + 1 == L ? 0 : wpos + 1;
            const float g2 = (float) (sum / L);
            gain = g2 < gain ? g2 : gain + (g2 - gain) * rel;
            gain = std::min (gain, 1.0f);

            const float ol = dl[(size_t) pos], orr = dr[(size_t) pos];
            dl[(size_t) pos] = l;
            dr[(size_t) pos] = r;
            pos = pos + 1 == L ? 0 : pos + 1;
            const float g = gain >= 0.99999f ? 1.0f : gain;  // exact pass-through when idle
            left[i] = juce::jlimit (-1.0f, 1.0f, ol * g);
            if (right != nullptr) right[i] = juce::jlimit (-1.0f, 1.0f, orr * g);
        }
    }

private:
    int L = 1, pos = 0, rpos = 0, wpos = 0;
    float thr = 0.891f, rel = 0.001f, gain = 1.0f;
    double sum = 1.0;
    std::vector<float> dl, dr, req, win;
};
