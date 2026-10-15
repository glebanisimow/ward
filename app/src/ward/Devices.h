// Registry of the single-device Faust engines (engines/devices/*.dsp) and
// their named variants (starting points in the "+ Layer" menu).
#pragma once

#include "FaustBridge.h"

#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

struct DeviceVariant
{
    const char* label;
    std::vector<std::pair<const char*, float>> values;   // device param name -> value
};

struct DeviceInfo
{
    const char* key;          // engine file name
    const char* name;         // shown in the UI
    std::unique_ptr<dsp> (*make)();
    std::vector<DeviceVariant> variants;
};

const std::vector<DeviceInfo>& devices();
int deviceIndex (const juce::String& key);

// A device's own controls (group "params"), in [N] order, at most MAX_SRC.
constexpr int MAX_SRC = 8;
std::vector<FaustParam> sourceParams (const ParamCollector&);

// host value 0..1 <-> device value (log scale and integer steps respected)
float srcValue (const FaustParam&, float norm);
float srcNorm (const FaustParam&, float value);
juce::String srcText (const FaustParam&, float value);
