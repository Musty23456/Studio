#pragma once
#include "Effect.h"
#include "Biquad.h"

namespace almus {

/** Low shelf (200Hz) / mid peaking (1kHz) / high shelf (5kHz) EQ, per channel. */
class EqualizerEffect : public Effect {
public:
    void process(float* left, float* right, int numFrames) override;
    void setParam(const std::string& key, float value) override;
    void setSampleRate(int sampleRate) override;
    void reset() override;

private:
    void rebuildCoefficients();

    float lowGainDb_ = 0.0f, midGainDb_ = 0.0f, highGainDb_ = 0.0f;
    Biquad lowL_, midL_, highL_;
    Biquad lowR_, midR_, highR_;
};

} // namespace almus
