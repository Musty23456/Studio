#include "RecordingWriter.h"
#include "WavFile.h"

namespace almus {

RecordingWriter::RecordingWriter(std::string outputPath) : outputPath_(std::move(outputPath)) {}

RecordingWriter::~RecordingWriter() {
    if (running_) stop();
}

void RecordingWriter::start(int sampleRate) {
    sampleRate_ = sampleRate;
    running_ = true;
    allSamples_.clear();
    writerThread_ = std::thread(&RecordingWriter::writerLoop, this);
}

void RecordingWriter::pushFrames(const float* interleavedStereo, int numFrames) {
    if (!running_) return;
    std::lock_guard<std::mutex> lock(queueMutex_);
    pendingSamples_.insert(
        pendingSamples_.end(), interleavedStereo, interleavedStereo + numFrames * 2
    );
    queueCv_.notify_one();
}

void RecordingWriter::writerLoop() {
    while (running_) {
        std::vector<float> batch;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait_for(lock, std::chrono::milliseconds(50), [this] {
                return !pendingSamples_.empty() || !running_;
            });
            batch.swap(pendingSamples_);
        }
        if (!batch.empty()) {
            allSamples_.insert(allSamples_.end(), batch.begin(), batch.end());
        }
    }
    // Drain anything left after the stop signal.
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (!pendingSamples_.empty()) {
        allSamples_.insert(allSamples_.end(), pendingSamples_.begin(), pendingSamples_.end());
        pendingSamples_.clear();
    }
}

std::string RecordingWriter::stop() {
    running_ = false;
    queueCv_.notify_all();
    if (writerThread_.joinable()) writerThread_.join();

    const int64_t numFrames = static_cast<int64_t>(allSamples_.size() / 2);
    if (numFrames > 0) {
        writeWavFile(outputPath_, allSamples_.data(), numFrames, sampleRate_);
        return outputPath_;
    }
    return "";
}

} // namespace almus
