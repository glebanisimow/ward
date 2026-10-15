#include "WardProcessor.h"
#include "WardEditor.h"

#include <cmath>

//------------------------------------------------------------- WardProcessor
WardProcessor::WardProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Ward", createLayout())
{
    for (int i = 0; i < MAX_LAYERS; ++i)
        layers.push_back (std::make_unique<Layer> (i, apvts));
    mLevel = apvts.getRawParameterValue ("m.level");
    mSize = apvts.getRawParameterValue ("m.size");
    mDamp = apvts.getRawParameterValue ("m.damp");
    mWidth = apvts.getRawParameterValue ("m.width");
    mReturn = apvts.getRawParameterValue ("m.return");
    mFreeze = apvts.getRawParameterValue ("m.freeze");
    mono.bind (apvts);
}

juce::AudioProcessorValueTreeState::ParameterLayout WardProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    for (int i = 0; i < MAX_LAYERS; ++i)
        Layer::addParams (L, i, [this] (int layer, int k, float v) { return srcTextFor (layer, k, v); });

    const auto attrs = [] (const char* unit, int dec)
    {
        const juce::String u (unit);
        return juce::AudioParameterFloatAttributes().withLabel (u).withStringFromValueFunction (
            [u, dec] (float v, int) { return juce::String (v, dec) + (u.isEmpty() ? "" : " " + u); });
    };
    const auto add = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit, int dec)
    {
        L.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                            juce::NormalisableRange<float> (lo, hi), def, attrs (unit, dec)));
    };
    add ("m.level", "Master level", -60, 6, -3, "dB", 1);
    add ("m.size", "Reverb size", 0, 1, 0.85f, "", 2);
    add ("m.damp", "Reverb damping", 0, 1, 0.4f, "", 2);
    add ("m.width", "Reverb width", 0, 1, 1, "", 2);
    add ("m.return", "Reverb return", 0, 1, 0.7f, "", 2);
    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "m.freeze", 1 }, "Reverb freeze", false));
    MidiMono::addParams (L);
    return L;
}

juce::String WardProcessor::srcTextFor (int layer, int k, float norm) const
{
    if (! juce::isPositiveAndBelow (layer, (int) layers.size()))
        return juce::String (norm, 3);
    const auto& l = *layers[(size_t) layer];
    const auto& ps = l.srcParams (l.device());
    if (! juce::isPositiveAndBelow (k, (int) ps.size()))
        return "-";
    return srcText (ps[(size_t) k], srcValue (ps[(size_t) k], norm));
}

void WardProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& l : layers) l->prepare (sampleRate, CHUNK);
    for (auto& b : mix) b.assign (CHUNK, 0.0f);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    limiter.prepare (sampleRate);
    setLatencySamples (limiter.latency());
    masterGain.reset (sampleRate, 0.05);
    returnGain.reset (sampleRate, 0.05);
    mono.prepare (sampleRate, samplesPerBlock);
    transpose = 0.0f;
}

bool WardProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WardProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int total = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
    {
        buffer.clear();
        return;
    }

    bool solo = false;
    for (int i = 0; i < MAX_LAYERS; ++i)
        solo |= layers[(size_t) i]->isOn() && param (layerId (i, "solo")) > 0.5f;

    juce::Reverb::Parameters rp;
    rp.roomSize = mSize->load();
    rp.damping = mDamp->load();
    rp.width = mWidth->load();
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.freezeMode = mFreeze->load() > 0.5f ? 1.0f : 0.0f;
    reverb.setParameters (rp);
    const float lv = mLevel->load();
    masterGain.setTargetValue (lv <= -59.9f ? 0.0f : std::pow (10.0f, lv / 20.0f));
    returnGain.setTargetValue (mReturn->load());

    // MIDI: the note transposes every layer relative to C4, gliding over midi.glide
    mono.process (midi, total);
    const int note = mono.currentNote();
    const float target = note >= 0 ? (float) (note - 60) : 0.0f;
    const float glide = mono.glide();
    transpose = glide <= 0.001f ? target
                                : transpose + (target - transpose) * (1.0f - std::exp (-3.0f * (float) total / ((float) sr * glide)));

    for (int pos = 0; pos < total; pos += CHUNK)
    {
        const int n = juce::jmin (CHUNK, total - pos);
        for (auto& b : mix) std::fill (b.begin(), b.begin() + n, 0.0f);
        for (auto& l : layers)
            l->process (n, mix[0].data(), mix[1].data(), mix[2].data(), mix[3].data(), solo, transpose);

        reverb.processStereo (mix[2].data(), mix[3].data(), n);
        float* oL = buffer.getWritePointer (0) + pos;
        float* oR = buffer.getWritePointer (1) + pos;
        for (int i = 0; i < n; ++i)
        {
            const float g = masterGain.getNextValue() * mono.gainAt (pos + i), r = returnGain.getNextValue();
            oL[i] = g * (mix[0][(size_t) i] + r * mix[2][(size_t) i]);
            oR[i] = g * (mix[1][(size_t) i] + r * mix[3][(size_t) i]);
        }
        limiter.process (oL, oR, n);
        const float* rec[2] = { oL, oR };
        recorder.write (rec, n);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, total);

    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const float load = (float) (secs / juce::jmax (1e-9, total / sr));
    cpu.store (0.95f * cpu.load() + 0.05f * load, std::memory_order_relaxed);
}

//-------------------------------------------------------------------- layers
float WardProcessor::param (const juce::String& id) const
{
    auto* p = apvts.getRawParameterValue (id);
    return p != nullptr ? p->load() : 0.0f;
}

void WardProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void WardProcessor::resetLayerParams (int layer)
{
    const auto prefix = "L" + juce::String (layer + 1) + ".";
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID().startsWith (prefix) && ! rp->getParameterID().endsWith (".on"))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
}

int WardProcessor::addLayer (int device, int variant)
{
    for (int i = 0; i < MAX_LAYERS; ++i)
    {
        if (layers[(size_t) i]->isOn()) continue;
        resetLayerParams (i);
        setParam (layerId (i, "source"), (float) device);
        setParam (layerId (i, "seed"), (float) juce::Random::getSystemRandom().nextInt (10000));
        const auto& info = devices()[(size_t) device];
        const bool hasVariant = juce::isPositiveAndBelow (variant, (int) info.variants.size());
        const auto& ps = layers[(size_t) i]->srcParams (device);
        for (size_t k = 0; k < ps.size(); ++k)
        {
            float v = ps[k].init;
            if (hasVariant)
                for (auto& [name, value] : info.variants[(size_t) variant].values)
                    if (ps[k].name == name) v = value;
            setParam (layerId (i, ("p" + juce::String ((int) k + 1)).toRawUTF8()), srcNorm (ps[k], v));
        }
        juce::String label = info.name;
        if (hasVariant && info.variants.size() > 1)
            label << ": " << info.variants[(size_t) variant].label;
        setLayerLabel (i, label);
        layers[(size_t) i]->requestReset();
        setParam (layerId (i, "on"), 1.0f);
        return i;
    }
    return -1;
}

int WardProcessor::duplicateLayer (int src)
{
    for (int i = 0; i < MAX_LAYERS; ++i)
    {
        if (layers[(size_t) i]->isOn()) continue;
        const auto from = "L" + juce::String (src + 1) + ".", to = "L" + juce::String (i + 1) + ".";
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (rp->getParameterID().startsWith (from) && ! rp->getParameterID().endsWith (".on"))
                    if (auto* dst = apvts.getParameter (to + rp->getParameterID().fromFirstOccurrenceOf (".", false, false)))
                    {
                        dst->beginChangeGesture();
                        dst->setValueNotifyingHost (rp->getValue());
                        dst->endChangeGesture();
                    }
        setLayerLabel (i, layerLabel (src) + " (copy)");
        layers[(size_t) i]->requestReset();
        setParam (layerId (i, "on"), 1.0f);
        return i;
    }
    return -1;
}

void WardProcessor::removeLayer (int layer)
{
    setParam (layerId (layer, "on"), 0.0f);
}

juce::String WardProcessor::layerLabel (int layer) const
{
    const auto l = apvts.state.getProperty ("label" + juce::String (layer + 1)).toString();
    return l.isNotEmpty() ? l : juce::String (devices()[(size_t) layers[(size_t) layer]->device()].name);
}

void WardProcessor::setLayerLabel (int layer, const juce::String& s)
{
    apvts.state.setProperty ("label" + juce::String (layer + 1), s, nullptr);
}

//-------------------------------------------------------------------- scenes
const std::vector<Scene>& WardProcessor::scenes()
{
    static const std::vector<Scene> list {
        { "Empty", {}, {} },
        { "MRI drone",
          { { "mri", 0, { { "time", 0.15f }, { "pitch", -12 }, { "gMix", 0.65f }, { "gSize", 0.6f }, { "gDensity", 14 },
                          { "gSpray", 0.35f }, { "gPos", 0.2f }, { "gWidth", 0.9f }, { "fType", 1 }, { "fCut", 1800 },
                          { "send", 0.55f }, { "drift", 0.4f } } },
            { "mri", 2, { { "time", 0.05f }, { "pitch", -24 }, { "level", -10 }, { "cFreq", 55 }, { "cFb", 0.9f },
                          { "cMix", 0.6f }, { "send", 0.7f } } },
            { "room", 0, { { "level", -14 }, { "pitch", -12 } } } },
          { { "m.size", 0.92f } } },
        { "Heart cathedral",
          { { "monitor", 2, { { "time", 0.25f }, { "src:beepLength", 1.5f }, { "cFreq", 110 }, { "cFb", 0.85f },
                              { "cMix", 0.5f }, { "send", 0.8f }, { "level", -8 } } },
            { "doppler", 0, { { "time", 0.3f }, { "pitch", -12 }, { "fType", 1 }, { "fCut", 900 }, { "level", -6 },
                              { "send", 0.4f } } } },
          { { "m.size", 0.95f }, { "m.return", 0.9f } } },
        { "Ventilator tide",
          { { "ventilator", 0, { { "time", 0.3f }, { "pitch", -7 }, { "gMix", 0.5f }, { "gSize", 0.8f }, { "gDensity", 10 },
                                 { "gPitch", -12 }, { "gRand", 2 }, { "dlTime", 0.6f }, { "dlFb", 0.5f }, { "dlMix", 0.3f },
                                 { "send", 0.6f } } },
            { "concentrator", 0, { { "time", 0.5f }, { "pitch", -12 }, { "level", -12 }, { "send", 0.3f } } } },
          {} },
        { "Alarm cloud",
          { { "alarm", 0, { { "time", 0.5f }, { "gMix", 1 }, { "gSize", 0.12f }, { "gDensity", 60 }, { "gRand", 12 },
                            { "gSpray", 0.6f }, { "gWidth", 1 }, { "fType", 2 }, { "fCut", 1500 }, { "fRes", 2 },
                            { "send", 0.9f }, { "level", -8 } } },
            { "pump", 1, { { "time", 0.2f }, { "pitch", -24 }, { "gMix", 0.8f }, { "gSize", 1.2f }, { "gDensity", 6 },
                           { "level", -16 }, { "send", 0.8f } } } },
          { { "m.size", 0.9f } } },
        { "Defib storm",
          { { "defib", 0, { { "src:interval", 6 }, { "src:chargeTime", 2 }, { "src:readyHold", 1 }, { "drive", 18 },
                            { "bits", 6 }, { "dlTime", 0.25f }, { "dlFb", 0.6f }, { "dlMix", 0.4f }, { "send", 0.4f },
                            { "level", -8 } } },
            { "mri", 6, { { "time", 2 }, { "drive", 12 }, { "down", 4 }, { "level", -12 } } } },
          {} },
    };
    return list;
}

int WardProcessor::getNumPrograms() { return (int) scenes().size(); }

const juce::String WardProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? scenes()[(size_t) index].name : "";
}

void WardProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentScene = index;
    const auto& sc = scenes()[(size_t) index];
    for (int i = 0; i < MAX_LAYERS; ++i) removeLayer (i);
    for (auto* id : { "m.level", "m.size", "m.damp", "m.width", "m.return", "m.freeze" })
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue());
    for (auto& [id, v] : sc.master) setParam (id, v);

    for (auto& sl : sc.layers)
    {
        const int dev = deviceIndex (sl.device);
        const int i = dev < 0 ? -1 : addLayer (dev, sl.variant);
        if (i < 0) continue;
        const auto& ps = layers[(size_t) i]->srcParams (dev);
        for (auto& [name, v] : sl.values)
        {
            const juce::String n (name);
            if (n.startsWith ("src:"))
            {
                for (size_t k = 0; k < ps.size(); ++k)
                    if (ps[k].name == n.substring (4).toStdString())
                        setParam (layerId (i, ("p" + juce::String ((int) k + 1)).toRawUTF8()), srcNorm (ps[k], v));
            }
            else
                setParam (layerId (i, name), v);
        }
    }
}

//--------------------------------------------------------------------- state
void WardProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("scene", currentScene, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WardProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentScene = state.getProperty ("scene", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* WardProcessor::createEditor() { return new WardEditor (*this); }
