#pragma once
#include <string>
#include <unordered_map>

namespace almus {

/**
 * Common interface for all per-track DSP effects. Effects process audio
 * in-place, one stereo block at a time, entirely on the real-time audio
 * thread - no allocation or locking is allowed inside process().
 */
class Effect {
public:
    virtual ~Effect() = default;

    /** Processes [numFrames] stereo frames in place. */
    virtual void process(float* left, float* right, int numFrames) = 0;

    virtual void setParam(const std::string& key, float value) = 0;
    virtual void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }
    virtual void reset() {}

    bool enabled = true;

protected:
    int sampleRate_ = 48000;
};

} // namespace almus
