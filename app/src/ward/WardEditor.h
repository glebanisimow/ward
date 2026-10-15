#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Theme.h"
#include "WardProcessor.h"
#include "Widgets.h"

using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

// One row of the layer list: name, level meter, mute / solo, remove.
class LayerRow final : public juce::Component
{
public:
    LayerRow (WardProcessor&, int layer, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void setSelected (bool s) { selected = s; repaint(); }
    int layer() const { return index; }

private:
    WardProcessor& proc;
    const int index;
    std::function<void (int)> select;
    juce::ToggleButton mute { "M" }, solo { "S" };
    juce::TextButton remove { "x" };
    std::unique_ptr<ButtonAtt> muteAtt, soloAtt;
    bool selected = false;
};

// The layer's granular buffer (newest at the right), grains and read position.
class GrainView final : public juce::Component
{
public:
    GrainView (WardProcessor& p, int layer) : proc (p), index (layer) {}
    void paint (juce::Graphics&) override;

private:
    WardProcessor& proc;
    const int index;
};

// Controls of one layer, grouped in captioned rows.
class LayerEditor final : public juce::Component
{
public:
    LayerEditor (WardProcessor&, int layer);
    void resized() override;
    void paint (juce::Graphics&) override;
    int layer() const { return index; }
    int device() const { return dev; }
    void tick() { grains.repaint(); }

private:
    struct Row { juce::String title; std::vector<juce::Component*> items; };
    WardProcessor& proc;
    const int index, dev;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Row> rows;
    juce::Label title;
    juce::TextButton dup { "Duplicate" };
    juce::ComboBox fType;
    juce::ToggleButton freeze { "freeze" };
    std::unique_ptr<ComboAtt> fTypeAtt;
    std::unique_ptr<ButtonAtt> freezeAtt;
    GrainView grains;

    Knob* knob (const char* name, const juce::String& label);
};

class WardEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WardEditor (WardProcessor&);
    ~WardEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildRows();
    void select (int layer);
    void showAddMenu();
    void toggleRecord();
    void saveScene();
    void loadScene();
    static juce::File userDir();

    WardProcessor& proc;
    theme::Look look;

    juce::Label title, cpuLabel, recLabel, hint;
    juce::ComboBox scenes;
    juce::TextButton addBtn { "+ Layer" }, saveBtn { "Save" }, loadBtn { "Load" };
    juce::TextButton recBtn { "REC" }, folderBtn { "Folder" };
    std::vector<std::unique_ptr<LayerRow>> rows;
    juce::Component rowBox;
    std::unique_ptr<LayerEditor> editor;
    int selected = -1;
    juce::String rowsKey;

    std::vector<std::unique_ptr<Knob>> master;
    juce::ToggleButton revFreeze { "reverb freeze" };
    std::unique_ptr<ButtonAtt> revFreezeAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WardEditor)
};
