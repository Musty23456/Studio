#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdio>

namespace almus {

/**
 * Captures microphone input handed to it (in small chunks) from the
 * real-time audio callback and writes it to a WAV file on a dedicated
 * background thread. The callback thread only ever pushes into a
 * mutex-protected staging buffer and notifies - it never touches the
 * filesystem directly, which keeps file I/O off the real-time path.
 */
class RecordingWriter {
public:
    explicit RecordingWriter(std::string outputPath);
    ~RecordingWriter();

    void start(int sampleRate);
    /** Called from the audio callback thread with interleaved stereo frames. */
    void pushFrames(const float* interleavedStereo, int numFrames);
    /** Stops capture and finalizes the WAV file header. Blocks briefly for the writer thread to drain. */
    std::string stop();

private:
    void writerLoop();

    std::string outputPath_;
    int sampleRate_ = 48000;

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::vector<float> pendingSamples_;

    std::thread writerThread_;
    std::atomic<bool> running_{false};

    std::vector<float> allSamples_; // accumulated on writer thread only
};

} // namespace almus
