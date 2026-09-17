#pragma once
#include "Effect.h"
#include <vector>

namespace almus {

/** Modulated short delay line (LFO-swept) - classic analog chorus voicing. */
class ChorusEffect : public Effect {
public:
    void process(float* left, float* right, int numFrames) override;
    void setParam(const std::string& key, float value) override;
    void setSampleRate(int sampleRate) override;
    void reset() override;

private:
    std::vector<float> bufferL_, bufferR_;
    int writeIndex_ = 0;
    float lfoPhase_ = 0.0f;
    float rateHz_ = 1.2f;
    float depthMs_ = 6.0f;
    float wetMix_ = 0.4f;

    float readInterpolated(const std::vector<float>& buffer, float delaySamples);
};

} // namespace almus
