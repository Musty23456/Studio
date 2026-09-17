#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace almus {

/**
 * Decoded audio held entirely in memory as interleaved-free float channels.
 * Phase 1 targets short-to-medium length clips (typical mobile session
 * lengths), which keeps random access for trimming/splitting trivial - no
 * seeking/streaming decoder is needed. Very long recordings are still
 * supported but will use proportionally more RAM; a streaming path is a
 * natural follow-up once sessions regularly exceed a few hundred MB.
 */
struct AudioBuffer {
    int sampleRate = 48000;
    int numChannels = 1;
    std::vector<float> left;
    std::vector<float> right; // mirrors left for mono sources
    int64_t numFrames = 0;
};

/** Reads a 16-bit or 32-bit float PCM WAV file fully into memory. */
bool readWavFile(const std::string& path, AudioBuffer& outBuffer);

/** Writes a stereo interleaved buffer to a 16-bit PCM WAV file. */
bool writeWavFile(
    const std::string& path,
    const float* interleavedStereo,
    int64_t numFrames,
    int sampleRate
);

/**
 * Process-wide cache of decoded source audio, keyed by file path, so the
 * same imported/recorded file backing multiple clips (or repeated trims of
 * one clip) is only decoded once.
 */
class AudioFileCache {
public:
    static AudioFileCache& instance();
    std::shared_ptr<AudioBuffer> load(const std::string& path);
    void invalidate(const std::string& path);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<AudioBuffer>> cache_;
};

} // namespace almus
