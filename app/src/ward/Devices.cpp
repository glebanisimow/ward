#include "Devices.h"

#include <algorithm>
#include <cmath>

// Factories, one translation unit per generated class (CMake writes them).
#define WARD_DEVICES(X) X (monitor) X (alarm) X (ventilator) X (pump) X (concentrator) \
                        X (doppler) X (defib) X (mri) X (room)
#define WARD_DECLARE(n) std::unique_ptr<dsp> createDev_##n();
WARD_DEVICES (WARD_DECLARE)
#undef WARD_DECLARE

const std::vector<DeviceInfo>& devices()
{
    static const std::vector<DeviceInfo> list {
        { "mri", "MRI", createDev_mri,
          { { "3D GRE (Bravo)", { { "sequence", 0 } } },
            { "T2 TSE", { { "sequence", 1 } } },
            { "EPI (fMRI)", { { "sequence", 2 } } },
            { "Utrecht 1", { { "sequence", 3 } } },
            { "Utrecht 2 (tone)", { { "sequence", 4 } } },
            { "Utrecht 4", { { "sequence", 5 } } },
            { "GE Signa", { { "sequence", 6 } } },
            { "Exam (seeded sequences)", { { "exam", 1 }, { "seqLength", 20 } } } } },
        { "monitor", "Heart monitor", createDev_monitor,
          { { "Normal, 72 bpm", {} },
            { "Tachycardia, SpO2 85", { { "heartRate", 140 }, { "spo2", 85 } } },
            { "Bradycardia, long beep", { { "heartRate", 38 }, { "beepLength", 0.3f } } } } },
        { "alarm", "Monitor alarm", createDev_alarm,
          { { "High priority", { { "priority", 2 } } },
            { "Medium priority", { { "priority", 1 } } } } },
        { "ventilator", "Ventilator", createDev_ventilator,
          { { "Adult, 16 /min", {} },
            { "Fast, 30 /min", { { "rate", 30 }, { "ieRatio", 1.5f } } } } },
        { "pump", "Infusion pump", createDev_pump,
          { { "Dosing", {} },
            { "Occlusion alarm", { { "alarm", 1 }, { "motorLevel", 0.3f } } } } },
        { "concentrator", "O2 concentrator", createDev_concentrator, { { "Home concentrator", {} } } },
        { "doppler", "Doppler", createDev_doppler,
          { { "Carotid", {} },
            { "Fast, soft (fetal-like)", { { "heartRate", 140 }, { "peak", 900 }, { "diastole", 0.6f } } } } },
        { "defib", "Defibrillator", createDev_defib,
          { { "Charge and shock", {} },
            { "Charge, disarm", { { "shock", 0 } } } } },
        { "room", "Room tone", createDev_room,
          { { "ICU room, 50 Hz", {} },
            { "60 Hz mains", { { "mains", 60 } } } } },
    };
    return list;
}

int deviceIndex (const juce::String& key)
{
    const auto& d = devices();
    for (size_t i = 0; i < d.size(); ++i)
        if (key == d[i].key) return (int) i;
    return -1;
}

std::vector<FaustParam> sourceParams (const ParamCollector& ui)
{
    std::vector<FaustParam> v;
    for (auto& p : ui.params)
        if (p.group == "params") v.push_back (p);
    std::stable_sort (v.begin(), v.end(), [] (const FaustParam& a, const FaustParam& b) { return a.order < b.order; });
    if (v.size() > (size_t) MAX_SRC) v.resize ((size_t) MAX_SRC);
    return v;
}

float srcValue (const FaustParam& p, float n)
{
    n = juce::jlimit (0.0f, 1.0f, n);
    float v = p.log && p.min > 0.0f ? p.min * std::pow (p.max / p.min, n) : p.min + n * (p.max - p.min);
    if (p.step >= 1.0f) v = p.min + std::round ((v - p.min) / p.step) * p.step;
    return juce::jlimit (p.min, p.max, v);
}

float srcNorm (const FaustParam& p, float v)
{
    v = juce::jlimit (p.min, p.max, v);
    if (p.max <= p.min) return 0.0f;
    return p.log && p.min > 0.0f ? std::log (v / p.min) / std::log (p.max / p.min) : (v - p.min) / (p.max - p.min);
}

juce::String srcText (const FaustParam& p, float v)
{
    const int dec = p.step >= 1.0f ? 0 : (std::abs (v) >= 100.0f ? 0 : (std::abs (v) >= 10.0f ? 1 : 2));
    return juce::String (v, dec) + (p.unit.empty() ? "" : " " + juce::String (p.unit));
}
