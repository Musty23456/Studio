#pragma once
#include "Effect.h"
#include "Biquad.h"

namespace almus {

/** Soft-clip waveshaper distortion with drive and a post tone-tilt filter. */
class DistortionEffect : public Effect {
public:
    void process(float* left, float* right, int numFrames) override;
    void setParam(const std::string& key, float value) override;
    void setSampleRate(int sampleRate) override;
    void reset() override;

private:
    void rebuildTone();

    float driveDb_ = 12.0f;
    float tone_ = 0.5f; // 0 = dark, 1 = bright
    float mix_ = 1.0f;
    Biquad toneFilterL_, toneFilterR_;
};

} // namespace almus
