#pragma once
#include "Effect.h"
#include <vector>

namespace almus {

/** Stereo feedback delay line (ping-pong-free, same tap on both channels). */
class DelayEffect : public Effect {
public:
    void process(float* left, float* right, int numFrames) override;
    void setParam(const std::string& key, float value) override;
    void setSampleRate(int sampleRate) override;
    void reset() override;

private:
    void resizeBuffers();

    std::vector<float> bufferL_, bufferR_;
    int writeIndex_ = 0;
    float delayMs_ = 350.0f;
    float feedback_ = 0.35f;
    float wetMix_ = 0.3f;
};

} // namespace almus
