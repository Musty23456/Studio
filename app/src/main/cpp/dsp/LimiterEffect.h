#pragma once
#include "Effect.h"

namespace almus {

/**
 * Fast-attack peak limiter. This is a zero-lookahead limiter (no
 * pre-processing delay), which is appropriate for a realtime track insert;
 * it will not catch every single-sample overshoot as cleanly as a
 * lookahead/brickwall limiter would, but keeps the signal safely under the
 * ceiling in practice and adds no latency to the monitoring path.
 */
class LimiterEffect : public Effect {
public:
    void process(float* left, float* right, int numFrames) override;
    void setParam(const std::string& key, float value) override;
    void reset() override { gainReduction_ = 1.0f; }

private:
    float ceilingDb_ = -0.3f;
    float releaseMs_ = 80.0f;
    float gainReduction_ = 1.0f;
};

} // namespace almus
