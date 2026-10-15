#include "WardEditor.h"

using namespace theme;

namespace
{
const juce::Colour recRed { 0xffe5484d };

juce::Colour layerColour (int i)
{
    static const juce::Colour c[] = { amber, teal, violet, green, juce::Colour (0xffe57a6b), juce::Colour (0xff6b9be5) };
    return c[(size_t) i % 6];
}
} // namespace

//------------------------------------------------------------------ LayerRow
LayerRow::LayerRow (WardProcessor& p, int layer, std::function<void (int)> onSelect)
    : proc (p), index (layer), select (std::move (onSelect))
{
    muteAtt = std::make_unique<ButtonAtt> (proc.apvts, layerId (index, "mute"), mute);
    soloAtt = std::make_unique<ButtonAtt> (proc.apvts, layerId (index, "solo"), solo);
    remove.onClick = [this] { proc.removeLayer (index); };
    for (auto* c : { (juce::Component*) &mute, (juce::Component*) &solo, (juce::Component*) &remove })
        addAndMakeVisible (*c);
}

void LayerRow::resized()
{
    auto b = getLocalBounds().reduced (6, 8);
    auto right = b.removeFromRight (104);
    remove.setBounds (right.removeFromRight (28).withSizeKeepingCentre (26, 24));
    solo.setBounds (right.removeFromRight (38));
    mute.setBounds (right.removeFromRight (38));
}

void LayerRow::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (selected ? raised.brighter (0.08f) : raised);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (layerColour (index));
    g.fillRoundedRectangle (b.withWidth (4.0f), 2.0f);

    auto t = b.reduced (14.0f, 6.0f).withTrimmedRight (104.0f);
    g.setColour (selected ? text : text.withAlpha (0.85f));
    g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    g.drawText (juce::String (index + 1) + "  " + proc.layerLabel (index), t.removeFromTop (20.0f),
                juce::Justification::centredLeft, true);
    // level meter, dB scale -60..0
    const float lv = proc.layers[(size_t) index]->meter();
    const float db = lv > 1e-6f ? 20.0f * std::log10 (lv) : -60.0f;
    auto m = t.removeFromTop (12.0f).reduced (0.0f, 3.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (m, 2.0f);
    g.setColour (layerColour (index).withAlpha (0.85f));
    g.fillRoundedRectangle (m.withWidth (m.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f)), 2.0f);
}

void LayerRow::mouseDown (const juce::MouseEvent&) { select (index); }

//----------------------------------------------------------------- GrainView
void GrainView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (b, 6.0f);
    const auto& gr = proc.layers[(size_t) index]->granular();
    const auto& pk = gr.peaks();
    const int N = (int) pk.size(), size = gr.length();
    if (N == 0 || size == 0) return;
    const int wh = gr.writeHead(), filled = gr.filledSamples();
    const auto area = b.reduced (6.0f, 8.0f);
    const float W = area.getWidth(), mid = area.getCentreY(), h = area.getHeight() * 0.5f;
    const int newest = (wh / Granular::PEAK_BLOCK - 1 + N) % N;
    const int have = filled / Granular::PEAK_BLOCK;
    const auto col = layerColour (index);

    // newest material at the right edge
    g.setColour (col.withAlpha (0.55f));
    for (int x = 0; x < (int) W; ++x)
    {
        const int age = (int) ((W - 1.0f - (float) x) / W * (float) N);
        if (age >= have) continue;
        const float v = juce::jmin (1.0f, pk[(size_t) ((newest - age + N) % N)] * 1.4f);
        g.drawVerticalLine ((int) area.getX() + x, mid - v * h, mid + v * h + 1.0f);
    }
    // grains
    std::array<float, Granular::MAX_GRAINS> pos {};
    const int k = gr.grainSnapshot (pos);
    g.setColour (text.withAlpha (0.8f));
    for (int i = 0; i < k; ++i)
    {
        const float s = pos[(size_t) i] * (float) size;
        const float age = std::fmod ((float) wh - s + (float) size, (float) size);
        const float x = area.getRight() - age / (float) size * W;
        g.fillEllipse (x - 2.5f, mid - 2.5f, 5.0f, 5.0f);
    }
    // read position
    const float p = proc.param (layerId (index, "gPos"));
    const float x = area.getRight() - p * (float) juce::jmax (1, filled) / (float) size * W;
    g.setColour (col);
    g.drawVerticalLine ((int) x, area.getY(), area.getBottom());

    g.setColour (dim);
    g.setFont (juce::FontOptions (11.0f));
    const bool frozen = proc.param (layerId (index, "gFreeze")) > 0.5f;
    g.drawText (frozen ? "FROZEN  12 s buffer" : "12 s buffer, newest right", b.reduced (8.0f, 4.0f),
                juce::Justification::topLeft);
}

//--------------------------------------------------------------- LayerEditor
Knob* LayerEditor::knob (const char* name, const juce::String& label)
{
    knobs.push_back (std::make_unique<Knob> (proc.apvts, layerId (index, name), label));
    addAndMakeVisible (*knobs.back());
    return knobs.back().get();
}

LayerEditor::LayerEditor (WardProcessor& p, int layer)
    : proc (p), index (layer), dev (p.layers[(size_t) layer]->device()), grains (p, layer)
{
    title.setText (juce::String (index + 1) + "  " + proc.layerLabel (index), juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, layerColour (index));
    addAndMakeVisible (title);
    dup.onClick = [this] { proc.duplicateLayer (index); };
    addAndMakeVisible (dup);

    Row src { "source: " + juce::String (devices()[(size_t) dev].name), {} };
    src.items = { knob ("time", "time"), knob ("pitch", "pitch"), knob ("seed", "seed") };
    const auto& ps = proc.layers[(size_t) index]->srcParams (dev);
    for (size_t k = 0; k < ps.size(); ++k)
        src.items.push_back (knob (("p" + juce::String ((int) k + 1)).toRawUTF8(), ps[k].name));

    Row gr { "grain", {} };
    for (auto [n, l] : { std::pair { "gMix", "mix" }, { "gSize", "size" }, { "gDensity", "density" }, { "gPos", "position" },
                         { "gSpray", "spray" }, { "gScan", "scan" }, { "gPitch", "pitch" }, { "gRand", "random pitch" },
                         { "gReverse", "reverse" }, { "gWidth", "width" } })
        gr.items.push_back (knob (n, l));
    freezeAtt = std::make_unique<ButtonAtt> (proc.apvts, layerId (index, "gFreeze"), freeze);
    addAndMakeVisible (freeze);
    gr.items.push_back (&freeze);

    Row shape { "shape", {} };
    fType.addItemList (juce::StringArray (Layer::filterTypes, 4), 1);
    fTypeAtt = std::make_unique<ComboAtt> (proc.apvts, layerId (index, "fType"), fType);
    addAndMakeVisible (fType);
    shape.items.push_back (&fType);
    for (auto [n, l] : { std::pair { "fCut", "cutoff" }, { "fRes", "resonance" }, { "drive", "drive" }, { "bits", "bits" },
                         { "down", "downsample" } })
        shape.items.push_back (knob (n, l));

    Row space { "space", {} };
    for (auto [n, l] : { std::pair { "cFreq", "comb freq" }, { "cFb", "comb fb" }, { "cMix", "comb mix" },
                         { "dlTime", "delay" }, { "dlFb", "delay fb" }, { "dlMix", "delay mix" }, { "send", "reverb send" } })
        space.items.push_back (knob (n, l));

    Row out { "out", {} };
    for (auto [n, l] : { std::pair { "level", "level" }, { "pan", "pan" }, { "drift", "drift" }, { "driftRate", "drift time" } })
        out.items.push_back (knob (n, l));

    rows = { src, gr, shape, space, out };
    addAndMakeVisible (grains);
}

void LayerEditor::resized()
{
    auto b = getLocalBounds().reduced (12, 8);
    auto top = b.removeFromTop (30);
    dup.setBounds (top.removeFromRight (96).withSizeKeepingCentre (90, 26));
    title.setBounds (top);

    const int kw = 76, kh = 82, cap = 18;
    auto place = [&] (const Row& r, juce::Rectangle<int> area)
    {
        int x = area.getX();
        for (auto* c : r.items)
        {
            const bool isKnob = dynamic_cast<Knob*> (c) != nullptr;
            const int w = isKnob ? kw : 96;
            if (isKnob) c->setBounds (x, area.getY(), w, kh);
            else c->setBounds (x + 4, area.getY() + kh / 2 - 13, w - 8, 26);
            x += w + 2;
        }
    };
    const int rowH = cap + kh + 6;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (i == 1)
        {
            b.removeFromTop (cap);
            grains.setBounds (b.removeFromTop (78).withTrimmedRight (8));
            b.removeFromTop (4);
            place (rows[i], b.removeFromTop (kh + 6));
            continue;
        }
        auto r = b.removeFromTop (rowH);
        place (rows[i], r.withTrimmedTop (cap));
    }
}

void LayerEditor::paint (juce::Graphics& g)
{
    g.fillAll (panel);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.setColour (dim);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i].items.empty()) continue;
        const int y = (i == 1 ? grains.getY() : rows[i].items.front()->getY()) - 17;
        g.drawText (rows[i].title.toUpperCase(), 14, y, 600, 16, juce::Justification::centredLeft);
    }
}

//---------------------------------------------------------------- WardEditor
WardEditor::WardEditor (WardProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    title.setText ("WARD", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, amber);
    addAndMakeVisible (title);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        scenes.addItem (proc.getProgramName (i), i + 1);
    scenes.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    scenes.onChange = [this]
    {
        proc.setCurrentProgram (scenes.getSelectedId() - 1);
        selected = -1;
    };
    addAndMakeVisible (scenes);

    addBtn.setColour (juce::TextButton::buttonColourId, amber.withAlpha (0.85f));
    addBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    addBtn.onClick = [this] { showAddMenu(); };
    saveBtn.onClick = [this] { saveScene(); };
    loadBtn.onClick = [this] { loadScene(); };
    recBtn.onClick = [this] { toggleRecord(); };
    folderBtn.onClick = [] { (userDir().getChildFile ("Recordings")).createDirectory();
                             userDir().getChildFile ("Recordings").startAsProcess(); };
    for (auto* b : { &addBtn, &saveBtn, &loadBtn, &recBtn, &folderBtn }) addAndMakeVisible (*b);

    for (auto* l : { &cpuLabel, &recLabel, &hint })
    {
        l->setColour (juce::Label::textColourId, dim);
        addAndMakeVisible (*l);
    }
    cpuLabel.setJustificationType (juce::Justification::centredRight);
    hint.setText ("Add a layer: pick a device, then stretch it (time), transpose it (pitch), freeze and smear it "
                  "(grain), shape and place it. Stack layers into a drone or noise.",
                  juce::dontSendNotification);
    hint.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (rowBox);

    for (auto [id, l] : { std::pair { "m.level", "master" }, { "m.size", "reverb size" }, { "m.damp", "damping" },
                          { "m.width", "width" }, { "m.return", "reverb return" }, { "midi.mode", "MIDI" },
                          { "midi.attack", "attack" }, { "midi.release", "release" }, { "midi.glide", "glide" } })
    {
        master.push_back (std::make_unique<Knob> (proc.apvts, id, l));
        addAndMakeVisible (*master.back());
    }
    revFreezeAtt = std::make_unique<ButtonAtt> (proc.apvts, "m.freeze", revFreeze);
    addAndMakeVisible (revFreeze);

    setResizable (true, true);
    setResizeLimits (1200, 820, 2400, 1600);
    setSize (1340, 900);
    rebuildRows();
    startTimerHz (30);
}

WardEditor::~WardEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::File WardEditor::userDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Ward");
}

void WardEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (panel);
    g.fillRoundedRectangle (rowBox.getBounds().toFloat().expanded (4.0f), 8.0f);
    const auto ms = master.front()->getBounds().getUnion (revFreeze.getBounds()).expanded (10, 6).toFloat();
    g.fillRoundedRectangle (ms, 8.0f);
    g.setColour (dim);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText ("LAYERS  (" + juce::String ((int) rows.size()) + " / " + juce::String (MAX_LAYERS) + ")",
                rowBox.getX(), rowBox.getY() - 22, 200, 16, juce::Justification::centredLeft);
    g.drawText ("MASTER", (int) ms.getX() + 10, (int) ms.getY() - 18, 200, 16, juce::Justification::centredLeft);
}

void WardEditor::resized()
{
    auto b = getLocalBounds().reduced (14);
    auto head = b.removeFromTop (48);
    title.setBounds (head.removeFromLeft (90));
    scenes.setBounds (head.removeFromLeft (190).withSizeKeepingCentre (180, 28));
    loadBtn.setBounds (head.removeFromLeft (62).withSizeKeepingCentre (56, 28));
    saveBtn.setBounds (head.removeFromLeft (62).withSizeKeepingCentre (56, 28));
    head.removeFromLeft (14);
    addBtn.setBounds (head.removeFromLeft (100).withSizeKeepingCentre (96, 30));
    cpuLabel.setBounds (head.removeFromRight (90));
    folderBtn.setBounds (head.removeFromRight (70).withSizeKeepingCentre (64, 28));
    recBtn.setBounds (head.removeFromRight (64).withSizeKeepingCentre (58, 28));
    recLabel.setBounds (head.removeFromRight (260));

    auto foot = b.removeFromBottom (100);
    foot.removeFromTop (20);
    int x = foot.getX() + 10;
    for (auto& k : master)
    {
        k->setBounds (x, foot.getY(), 80, 80);
        x += 84;
    }
    revFreeze.setBounds (x + 6, foot.getY() + 26, 130, 28);
    b.removeFromBottom (8);

    b.removeFromTop (26);
    auto left = b.removeFromLeft (290);
    rowBox.setBounds (left);
    int y = 0;
    for (auto& r : rows)
    {
        r->setBounds (0, y, left.getWidth(), 52);
        y += 54;
    }
    b.removeFromLeft (16);
    b.translate (0, -26);
    b.setHeight (b.getHeight() + 26);
    if (editor != nullptr) editor->setBounds (b);
    hint.setBounds (b.reduced (40));
}

void WardEditor::rebuildRows()
{
    juce::String key;
    for (int i = 0; i < MAX_LAYERS; ++i)
        if (proc.layers[(size_t) i]->isOn()) key << i << ":" << proc.layerLabel (i) << ";";
    if (key == rowsKey) return;
    rowsKey = key;

    rows.clear();
    for (int i = 0; i < MAX_LAYERS; ++i)
        if (proc.layers[(size_t) i]->isOn())
        {
            rows.push_back (std::make_unique<LayerRow> (proc, i, [this] (int l) { select (l); }));
            rowBox.addAndMakeVisible (*rows.back());
        }
    if (selected < 0 || ! proc.layers[(size_t) selected]->isOn())
        select (rows.empty() ? -1 : rows.front()->layer());
    for (auto& r : rows) r->setSelected (r->layer() == selected);
    resized();
    repaint();
}

void WardEditor::select (int layer)
{
    selected = layer;
    editor.reset();
    if (layer >= 0)
    {
        editor = std::make_unique<LayerEditor> (proc, layer);
        addAndMakeVisible (*editor);
    }
    hint.setVisible (layer < 0);
    for (auto& r : rows) r->setSelected (r->layer() == selected);
    resized();
}

void WardEditor::showAddMenu()
{
    juce::PopupMenu menu;
    const auto& d = devices();
    for (size_t i = 0; i < d.size(); ++i)
    {
        juce::PopupMenu sub;
        for (size_t v = 0; v < d[i].variants.size(); ++v)
            sub.addItem ((int) (i * 100 + v + 1), d[i].variants[v].label);
        menu.addSubMenu (d[i].name, sub);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addBtn),
                        [this] (int r)
                        {
                            if (r <= 0) return;
                            const int l = proc.addLayer ((r - 1) / 100, (r - 1) % 100);
                            if (l < 0)
                            {
                                hint.setText ("All " + juce::String (MAX_LAYERS) + " layers are in use.", juce::dontSendNotification);
                                return;
                            }
                            rebuildRows();
                            select (l);
                        });
}

void WardEditor::toggleRecord()
{
    if (proc.recorder.isRecording())
    {
        proc.recorder.stop();
        recLabel.setText ("saved " + proc.recorder.file().getFileName(), juce::dontSendNotification);
        recBtn.setColour (juce::TextButton::buttonColourId, raised);
        return;
    }
    const auto f = userDir().getChildFile ("Recordings")
                       .getChildFile ("ward-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".wav");
    if (proc.recorder.start (f, proc.sampleRate()))
        recBtn.setColour (juce::TextButton::buttonColourId, recRed);
    else
        recLabel.setText ("cannot write " + f.getFullPathName(), juce::dontSendNotification);
}

void WardEditor::timerCallback()
{
    rebuildRows();
    if (editor != nullptr && selected >= 0 && editor->device() != proc.layers[(size_t) selected]->device())
        select (selected);   // source changed (automation): rebuild its controls
    for (auto& r : rows) r->repaint();
    if (editor != nullptr) editor->tick();
    if (proc.recorder.isRecording())
    {
        const int s = (int) proc.recorder.seconds();
        recLabel.setText (juce::String::formatted ("recording  %02d:%02d  ", s / 60, s % 60) + proc.recorder.file().getFileName(),
                          juce::dontSendNotification);
    }
    cpuLabel.setText ("CPU " + juce::String (proc.cpuLoad() * 100.0f, 1) + " %", juce::dontSendNotification);
    if (scenes.getSelectedId() != proc.getCurrentProgram() + 1)
        scenes.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void WardEditor::saveScene()
{
    userDir().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save scene", userDir().getChildFile ("scene.wardscene"), "*.wardscene");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (auto xml = proc.apvts.copyState().createXml())
                                  xml->writeTo (f.withFileExtension ("wardscene"));
                          });
}

void WardEditor::loadScene()
{
    chooser = std::make_unique<juce::FileChooser> ("Load scene", userDir(), "*.wardscene");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (! f.existsAsFile()) return;
                              if (auto xml = juce::XmlDocument::parse (f))
                                  if (xml->hasTagName (proc.apvts.state.getType()))
                                  {
                                      proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                                      for (auto& l : proc.layers) l->requestReset();
                                      selected = -1;
                                  }
                          });
}
