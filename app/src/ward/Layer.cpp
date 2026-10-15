#include "Layer.h"

#include <cmath>

using APF = juce::AudioParameterFloat;

juce::String layerId (int layer, const char* name)
{
    return "L" + juce::String (layer + 1) + "." + name;
}

const char* const Layer::filterTypes[] = { "off", "low-pass", "band-pass", "high-pass" };

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float t) { return s * std::pow (e / s, t); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

juce::AudioParameterFloatAttributes fmt (const juce::String& unit, int dec)
{
    return juce::AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (
        [unit, dec] (float v, int) { return juce::String (v, dec) + (unit.isEmpty() ? "" : " " + unit); });
}

float db2lin (float db) { return db <= -59.9f ? 0.0f : std::pow (10.0f, db / 20.0f); }
} // namespace

void Layer::addParams (juce::AudioProcessorValueTreeState::ParameterLayout& L, int i, SrcTextFn srcText)
{
    const auto id = [i] (const char* n) { return juce::ParameterID { layerId (i, n), 1 }; };
    const auto nm = [i] (const char* n) { return "L" + juce::String (i + 1) + " " + n; };
    const auto lin = [&] (const char* n, float lo, float hi, float def, const char* unit, int dec, float step = 0.0f)
    { L.add (std::make_unique<APF> (id (n), nm (n), juce::NormalisableRange<float> (lo, hi, step), def, fmt (unit, dec))); };
    const auto lg = [&] (const char* n, float lo, float hi, float def, const char* unit, int dec)
    { L.add (std::make_unique<APF> (id (n), nm (n), logRange (lo, hi), def, fmt (unit, dec))); };
    const auto flag = [&] (const char* n, bool def)
    { L.add (std::make_unique<juce::AudioParameterBool> (id (n), nm (n), def)); };

    flag ("on", false);
    juce::StringArray names;
    for (auto& d : devices()) names.add (d.name);
    L.add (std::make_unique<juce::AudioParameterChoice> (id ("source"), nm ("source"), names, 0));
    flag ("mute", false);
    flag ("solo", false);
    lin ("level", -60, 12, -6, "dB", 1);
    lin ("pan", -1, 1, 0, "", 2);
    lin ("seed", 0, 9999, 1, "", 0, 1);
    lg ("time", 0.01f, 8, 1, "x", 3);
    lin ("pitch", -48, 24, 0, "st", 2);
    for (int k = 0; k < MAX_SRC; ++k)
    {
        const auto n = "p" + juce::String (k + 1);
        L.add (std::make_unique<APF> (juce::ParameterID { layerId (i, n.toRawUTF8()), 1 }, nm (n.toRawUTF8()),
                                      juce::NormalisableRange<float> (0, 1), 0.5f,
                                      juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                          [srcText, i, k] (float v, int) { return srcText (i, k, v); })));
    }
    lin ("gMix", 0, 1, 0, "", 2);
    lg ("gSize", 0.005f, 2, 0.2f, "s", 3);
    lg ("gDensity", 0.5f, 100, 12, "/s", 1);
    lin ("gPos", 0, 1, 0.1f, "", 3);
    lin ("gSpray", 0, 1, 0.1f, "", 2);
    lin ("gScan", -2, 2, 0, "x", 3);
    lin ("gPitch", -24, 24, 0, "st", 2);
    lin ("gRand", 0, 12, 0, "st", 2);
    lin ("gReverse", 0, 1, 0, "", 2);
    lin ("gWidth", 0, 1, 0.5f, "", 2);
    flag ("gFreeze", false);
    L.add (std::make_unique<juce::AudioParameterChoice> (id ("fType"), nm ("filter"),
                                                         juce::StringArray (filterTypes, 4), 0));
    lg ("fCut", 20, 20000, 2000, "Hz", 0);
    lg ("fRes", 0.3f, 10, 0.7f, "", 2);
    lin ("drive", 0, 36, 0, "dB", 1);
    lin ("bits", 1, 16, 16, "", 0, 1);
    lin ("down", 1, 64, 1, "x", 0, 1);
    lg ("cFreq", 20, 2000, 110, "Hz", 1);
    lin ("cFb", 0, 0.98f, 0, "", 2);
    lin ("cMix", 0, 1, 0, "", 2);
    lg ("dlTime", 0.005f, 3, 0.35f, "s", 3);
    lin ("dlFb", 0, 0.95f, 0.3f, "", 2);
    lin ("dlMix", 0, 1, 0, "", 2);
    lin ("send", 0, 1, 0.2f, "", 2);
    lin ("drift", 0, 1, 0, "", 2);
    lg ("driftRate", 1, 120, 20, "s", 1);
}

Layer::Layer (int i, juce::AudioProcessorValueTreeState& s) : index (i), apvts (s)
{
    for (auto& info : devices())
    {
        auto& d = devs.emplace_back();
        d.d = info.make();
        d.d->buildUserInterface (&d.ui);
        for (auto& p : d.ui.params)
            if (p.group == "common")
            {
                if (p.name == "seed")  d.seed = p.zone;
                if (p.name == "time")  d.time = p.zone;
                if (p.name == "pitch") d.pitch = p.zone;
            }
        d.params = sourceParams (d.ui);
    }

    pOn = raw ("on"); pSource = raw ("source"); pMute = raw ("mute"); pSolo = raw ("solo");
    pLevel = raw ("level"); pPan = raw ("pan"); pSeed = raw ("seed"); pTime = raw ("time"); pPitch = raw ("pitch");
    for (int k = 0; k < MAX_SRC; ++k)
        pSrc[(size_t) k] = raw (("p" + juce::String (k + 1)).toRawUTF8());
    gMix = raw ("gMix"); gSize = raw ("gSize"); gDens = raw ("gDensity"); gPos = raw ("gPos");
    gSpray = raw ("gSpray"); gScan = raw ("gScan"); gPitch = raw ("gPitch"); gRand = raw ("gRand");
    gRev = raw ("gReverse"); gWidth = raw ("gWidth"); gFreeze = raw ("gFreeze");
    fType = raw ("fType"); fCut = raw ("fCut"); fRes = raw ("fRes");
    dDrive = raw ("drive"); dBits = raw ("bits"); dDown = raw ("down");
    cFreq = raw ("cFreq"); cFb = raw ("cFb"); cMix = raw ("cMix");
    dlTime = raw ("dlTime"); dlFb = raw ("dlFb"); dlMix = raw ("dlMix");
    pSend = raw ("send"); pDrift = raw ("drift"); pDriftRate = raw ("driftRate");
    rng.setSeed ((juce::int64) (7919 * (i + 1)));
}

std::atomic<float>* Layer::raw (const char* name) const
{
    auto* p = apvts.getRawParameterValue (layerId (index, name));
    jassert (p != nullptr);
    return p;
}

void Layer::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (auto& d : devs) d.d->init ((int) sampleRate);
    mono.assign ((size_t) maxBlock, 0.0f);
    bufL.assign ((size_t) maxBlock, 0.0f);
    bufR.assign ((size_t) maxBlock, 0.0f);
    gran.prepare (sampleRate, 12.0);
    filter.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    for (auto& c : comb) c.assign ((size_t) (sampleRate / 20.0) + 8, 0.0f);
    delay.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    delay.setMaximumDelayInSamples ((int) (3.1 * sampleRate) + 8);
    for (auto* s : { &onGain, &gainL, &gainR, &sendGain, &combMix, &dlMixS })
        s->reset (sampleRate, 0.02);
    dlSamples.reset (sampleRate, 0.15);
    onGain.setCurrentAndTargetValue (0.0f);
    wasOn = false;
    resetState();
}

void Layer::resetState()
{
    cur = device();
    devs[(size_t) cur].d->instanceClear();
    switchGain = 1.0f;
    gran.clear();
    filter.reset();
    for (auto& c : comb) std::fill (c.begin(), c.end(), 0.0f);
    combPos = 0;
    combLp = {}; holdV = {}; holdN = 0;
    delay.reset();
    drift = {};
    dlSamples.setCurrentAndTargetValue ((float) (dlTime->load() * sr));
}

bool Layer::process (int n, float* outL, float* outR, float* sendL, float* sendR, bool soloActive, float transpose)
{
    const bool on = isOn();
    if (on && (! wasOn || resetReq.exchange (false)))
    {
        resetState();
        onGain.setCurrentAndTargetValue (0.0f);
    }
    wasOn = on;
    onGain.setTargetValue (on ? 1.0f : 0.0f);
    if (! on && ! onGain.isSmoothing())
    {
        level.store (0.0f, std::memory_order_relaxed);
        return false;
    }

    // --- device, with a 5 ms fade around a change of source
    const int want = device();
    auto& D = devs[(size_t) cur];
    if (D.seed != nullptr)  *D.seed = pSeed->load();
    if (D.time != nullptr)  *D.time = pTime->load();
    if (D.pitch != nullptr) *D.pitch = pPitch->load() + transpose;
    for (size_t k = 0; k < D.params.size(); ++k)
        *D.params[k].zone = srcValue (D.params[k], pSrc[k]->load());
    float* out[1] = { mono.data() };
    D.d->compute (n, nullptr, out);
    const float step = 1.0f / (0.005f * (float) sr);
    for (int i = 0; i < n; ++i)
    {
        switchGain = want == cur ? juce::jmin (1.0f, switchGain + step) : juce::jmax (0.0f, switchGain - step);
        mono[(size_t) i] *= switchGain;
    }
    if (want != cur && switchGain <= 0.0f)
    {
        cur = want;
        devs[(size_t) cur].d->instanceClear();
    }

    // --- drift: three slow seeded walks (cutoff, pan, grain position)
    const float driftAmt = pDrift->load();
    {
        const float dt = (float) n / (float) sr, tau = pDriftRate->load();
        for (auto& v : drift)
        {
            const float u = (2.0f * rng.nextFloat() - 1.0f) * 1.732f;
            v = juce::jlimit (-1.0f, 1.0f, v - v * dt / tau + std::sqrt (2.0f * dt / tau) * u);
        }
    }

    // --- granular
    Granular::Params gp;
    gp.mix = gMix->load(); gp.size = gSize->load(); gp.density = gDens->load(); gp.pos = gPos->load();
    gp.spray = gSpray->load(); gp.scan = gScan->load(); gp.pitch = gPitch->load(); gp.rand = gRand->load();
    gp.reverse = gRev->load(); gp.width = gWidth->load(); gp.freeze = gFreeze->load() > 0.5f;
    gran.process (mono.data(), bufL.data(), bufR.data(), n, gp, drift[2] * driftAmt * 0.2f);

    // --- filter
    const int type = (int) fType->load();
    if (type != lastType)
    {
        using T = juce::dsp::StateVariableTPTFilterType;
        filter.setType (type == 3 ? T::highpass : type == 2 ? T::bandpass : T::lowpass);
        if (lastType <= 0) filter.reset();
        lastType = type;
    }
    if (type > 0)
    {
        const float fc = fCut->load() * std::pow (2.0f, 2.0f * driftAmt * drift[0]);
        filter.setCutoffFrequency (juce::jlimit (20.0f, 0.45f * (float) sr, fc));
        filter.setResonance (fRes->load());
        for (int i = 0; i < n; ++i)
        {
            bufL[(size_t) i] = filter.processSample (0, bufL[(size_t) i]);
            bufR[(size_t) i] = filter.processSample (1, bufR[(size_t) i]);
        }
    }

    // --- drive, bit depth, sample-rate reduction
    const float driveDb = dDrive->load();
    const int bits = (int) dBits->load(), down = juce::jmax (1, (int) dDown->load());
    if (driveDb > 0.05f || bits < 16 || down > 1)
    {
        const float g = std::pow (10.0f, driveDb / 20.0f), comp = 1.0f / std::sqrt (g);
        const float q = std::pow (2.0f, (float) (bits - 1));
        for (int i = 0; i < n; ++i)
        {
            float* s[2] = { &bufL[(size_t) i], &bufR[(size_t) i] };
            if (++holdN >= down)
            {
                holdN = 0;
                for (int c = 0; c < 2; ++c)
                {
                    float v = *s[c];
                    if (driveDb > 0.05f) v = std::tanh (g * v) * comp;
                    if (bits < 16) v = std::round (v * q) / q;
                    holdV[(size_t) c] = v;
                }
            }
            for (int c = 0; c < 2; ++c) *s[c] = holdV[(size_t) c];
        }
    }

    // --- comb resonator (feedback, damped)
    combMix.setTargetValue (cMix->load());
    if (combMix.getTargetValue() > 0.0f || combMix.isSmoothing())
    {
        const int len = (int) comb[0].size();
        const float d = juce::jlimit (2.0f, (float) len - 3.0f, (float) (sr / cFreq->load()));
        const float fb = cFb->load();
        for (int i = 0; i < n; ++i)
        {
            const float m = combMix.getNextValue();
            float rp = (float) combPos - d;
            if (rp < 0) rp += (float) len;
            const int i0 = (int) rp;
            const int i1 = i0 + 1 == len ? 0 : i0 + 1;
            const float fr = rp - (float) i0;
            float* s[2] = { &bufL[(size_t) i], &bufR[(size_t) i] };
            for (int c = 0; c < 2; ++c)
            {
                auto& cb = comb[(size_t) c];
                const float rd = cb[(size_t) i0] + fr * (cb[(size_t) i1] - cb[(size_t) i0]);
                combLp[(size_t) c] = 0.7f * rd + 0.3f * combLp[(size_t) c];
                const float y = *s[c] + fb * combLp[(size_t) c];
                cb[(size_t) combPos] = y;
                *s[c] += m * (y * (1.0f - fb) - *s[c]);
            }
            combPos = combPos + 1 == len ? 0 : combPos + 1;
        }
    }

    // --- delay
    dlMixS.setTargetValue (dlMix->load());
    dlSamples.setTargetValue ((float) (dlTime->load() * sr));
    if (dlMixS.getTargetValue() > 0.0f || dlMixS.isSmoothing())
    {
        const float fb = dlFb->load();
        for (int i = 0; i < n; ++i)
        {
            const float m = dlMixS.getNextValue();
            const float ds = dlSamples.getNextValue();
            delay.setDelay (juce::jmin (ds, (float) delay.getMaximumDelayInSamples() - 2.0f));
            float* s[2] = { &bufL[(size_t) i], &bufR[(size_t) i] };
            for (int c = 0; c < 2; ++c)
            {
                const float y = delay.popSample (c);
                delay.pushSample (c, *s[c] + fb * y);
                *s[c] += m * y;
            }
        }
    }
    else
        dlSamples.skip (n);

    // --- level, pan, mute / solo, send
    const bool audible = pMute->load() < 0.5f && (! soloActive || pSolo->load() > 0.5f);
    const float g = audible ? db2lin (pLevel->load()) : 0.0f;
    const float pan = juce::jlimit (-1.0f, 1.0f, pPan->load() + 0.5f * driftAmt * drift[1]);
    const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    gainL.setTargetValue (g * std::cos (a) * juce::MathConstants<float>::sqrt2);
    gainR.setTargetValue (g * std::sin (a) * juce::MathConstants<float>::sqrt2);
    sendGain.setTargetValue (pSend->load());
    float acc = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float o = onGain.getNextValue();
        const float l = bufL[(size_t) i] * gainL.getNextValue() * o;
        const float r = bufR[(size_t) i] * gainR.getNextValue() * o;
        const float sg = sendGain.getNextValue();
        outL[i] += l;
        outR[i] += r;
        sendL[i] += sg * l;
        sendR[i] += sg * r;
        acc += 0.5f * (l * l + r * r);
    }
    const float rms = std::sqrt (acc / (float) juce::jmax (1, n));
    level.store (juce::jmax (rms, 0.9f * level.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    return true;
}
