// Plugin / standalone entry of Ward (the Studio links the processor directly).
#include "WardProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WardProcessor(); }
