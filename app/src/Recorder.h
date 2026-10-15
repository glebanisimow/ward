// WAV recorder for a stereo master bus: 24-bit, written on a background
// thread (JUCE ThreadedWriter). write() is called from the audio thread;
// start() / stop() from the message thread.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>

class Recorder
{
public:
    Recorder() { thread.startThread(); }
    ~Recorder() { stop(); thread.stopThread (2000); }
    bool start (const juce::File&, double sampleRate);
    void stop();
    void write (const float* const* data, int n);
    bool isRecording() const { return active.load() != nullptr; }
    double seconds() const { return (double) written.load() / juce::jmax (1.0, rate); }
    juce::File file() const { return current; }

private:
    juce::TimeSliceThread thread { "WAV recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::CriticalSection lock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> active { nullptr };
    std::atomic<juce::int64> written { 0 };
    double rate = 48000;
    juce::File current;
};
