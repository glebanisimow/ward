#include "Recorder.h"

bool Recorder::start (const juce::File& f, double sampleRate)
{
    stop();
    f.getParentDirectory().createDirectory();
    auto stream = std::make_unique<juce::FileOutputStream> (f);
    if (! stream->openedOk()) return false;
    juce::WavAudioFormat wav;
    auto* w = wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0);
    if (w == nullptr) return false;
    stream.release();   // owned by the writer now
    rate = sampleRate;
    written.store (0);
    current = f;
    writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w, thread, 1 << 15);
    const juce::ScopedLock sl (lock);
    active.store (writer.get());
    return true;
}

void Recorder::stop()
{
    {
        const juce::ScopedLock sl (lock);
        active.store (nullptr);
    }
    writer.reset();   // flushes and closes the file
}

void Recorder::write (const float* const* data, int n)
{
    const juce::ScopedLock sl (lock);
    if (auto* w = active.load())
    {
        w->write (data, n);
        written.fetch_add (n);
    }
}
