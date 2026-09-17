#pragma once
#include "Effect.h"

namespace almus {

/** Feed-forward peak compressor with attack/release envelope and makeup gain. */
class CompressorEffect : public Effect {
public:
    void process(float* left, float* right, int numFrames) override;
    void setParam(const std::string& key, float value) override;
    void reset() override { envelope_ = 0.0f; }

private:
    float thresholdDb_ = -18.0f;
    float ratio_ = 4.0f;
    float attackMs_ = 10.0f;
    float releaseMs_ = 120.0f;
    float makeupDb_ = 0.0f;
    float envelope_ = 0.0f;
};

} // namespace almus
