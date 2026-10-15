#include "Granular.h"

#include <cmath>

void Granular::prepare (double sampleRate, double seconds)
{
    sr = sampleRate;
    size = juce::jmax (PEAK_BLOCK * 4, (int) (seconds * sampleRate) / PEAK_BLOCK * PEAK_BLOCK);
    buf.assign ((size_t) size, 0.0f);
    peak.assign ((size_t) (size / PEAK_BLOCK), 0.0f);
    clear();
}

void Granular::clear()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    std::fill (peak.begin(), peak.end(), 0.0f);
    for (auto& g : grains) g.on = false;
    w.store (0);
    filled.store (0);
    snapCount.store (0);
    spawnPhase = scanOff = 0;
    peakAcc = 0;
    peakN = 0;
}

float Granular::read (double x) const
{
    // x may be any real index; wrap into [0, size)
    x = std::fmod (x, (double) size);
    if (x < 0) x += size;
    const int i0 = (int) x;
    const int i1 = i0 + 1 == size ? 0 : i0 + 1;
    const float f = (float) (x - i0);
    return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
}

void Granular::spawn (const Params& p, float posMod)
{
    Grain* g = nullptr;
    for (auto& c : grains)
        if (! c.on) { g = &c; break; }
    const int avail = filled.load (std::memory_order_relaxed);
    if (g == nullptr || avail < 64) return;

    const int len = juce::jlimit (16, size / 2, (int) (p.size * sr));
    const float semis = p.pitch + p.rand * (2.0f * rng.nextFloat() - 1.0f);
    const double ratio = std::pow (2.0, semis / 12.0);
    const bool rev = rng.nextFloat() < p.reverse;
    const double span = len * ratio;   // samples the grain reads

    // lag behind the write head: position + spray, kept inside written data
    double lag = (juce::jlimit (0.0f, 1.0f, p.pos + posMod)) * (avail - 1)
               + p.spray * rng.nextFloat() * juce::jmin ((double) avail - 1, 2.0 * sr);
    if (p.freeze)
        lag += scanOff;
    else
    {
        const double minLag = (rev ? 2.0 : juce::jmax (2.0, span - len + 2.0));
        // a reversed grain walks into old data while the write head overwrites it
        const double maxLag = juce::jmax (minLag, avail - (rev ? span + len : 0.0) - 2.0);
        lag = juce::jlimit (minLag, maxLag, lag);
    }
    lag = std::fmod (lag, (double) juce::jmax (1, avail));

    const int wh = w.load (std::memory_order_relaxed);
    g->pos = wh - 1 - lag;
    g->inc = rev ? -ratio : ratio;
    g->len = len;
    g->age = 0;
    const float th = juce::MathConstants<float>::pi / (float) len;
    g->c = 1.0f; g->s = 0.0f;
    g->rc = std::cos (th); g->rs = std::sin (th);
    const float pan = (2.0f * rng.nextFloat() - 1.0f) * p.width;
    const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    g->gl = std::cos (a) * juce::MathConstants<float>::sqrt2;
    g->gr = std::sin (a) * juce::MathConstants<float>::sqrt2;
    // overlapping Hann grains add up: keep the cloud near the source level
    g->amp = 1.0f / std::sqrt (juce::jmax (1.0f, p.density * p.size * 0.375f));
    g->on = true;
}

void Granular::process (const float* in, float* outL, float* outR, int n, const Params& p, float posMod)
{
    int wh = w.load (std::memory_order_relaxed);
    int fl = filled.load (std::memory_order_relaxed);
    const bool active = p.mix > 0.0001f;

    for (int i = 0; i < n; ++i)
    {
        const float x = in[i];
        if (! p.freeze)
        {
            buf[(size_t) wh] = x;
            peakAcc = juce::jmax (peakAcc, std::abs (x));
            if (++peakN == PEAK_BLOCK)
            {
                peak[(size_t) (wh / PEAK_BLOCK)] = peakAcc;
                peakAcc = 0;
                peakN = 0;
            }
            wh = wh + 1 == size ? 0 : wh + 1;
            fl = juce::jmin (fl + 1, size);
            w.store (wh, std::memory_order_relaxed);
            filled.store (fl, std::memory_order_relaxed);
        }
        else if (fl > 0)
        {
            scanOff -= p.scan;   // scan > 0 moves towards newer material
            scanOff = std::fmod (scanOff, (double) fl);
            if (scanOff < 0) scanOff += fl;
        }

        float gl = 0, gr = 0;
        if (active)
        {
            spawnPhase += p.density / sr;
            if (spawnPhase >= 1.0)
            {
                spawnPhase -= 1.0;
                spawn (p, posMod);
            }
            for (auto& g : grains)
            {
                if (! g.on) continue;
                const float env = g.s * g.s * g.amp;
                const float v = read (g.pos) * env;
                gl += v * g.gl;
                gr += v * g.gr;
                g.pos += g.inc;
                const float c = g.c * g.rc - g.s * g.rs;
                g.s = g.s * g.rc + g.c * g.rs;
                g.c = c;
                if (++g.age >= g.len) g.on = false;
            }
        }
        const float dry = 1.0f - p.mix;
        outL[i] = dry * x + p.mix * gl;
        outR[i] = dry * x + p.mix * gr;
    }

    int k = 0;
    for (auto& g : grains)
        if (g.on && k < MAX_GRAINS)
        {
            double q = std::fmod (g.pos, (double) size);
            if (q < 0) q += size;
            snap[(size_t) k++].store ((float) (q / size), std::memory_order_relaxed);
        }
    snapCount.store (k, std::memory_order_relaxed);
}

int Granular::grainSnapshot (std::array<float, MAX_GRAINS>& pos) const
{
    const int k = snapCount.load (std::memory_order_relaxed);
    for (int i = 0; i < k; ++i) pos[(size_t) i] = snap[(size_t) i].load (std::memory_order_relaxed);
    return k;
}
