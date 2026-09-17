#pragma once
#include "Effect.h"
#include <vector>

namespace almus {

/**
 * Classic Schroeder reverb: four parallel comb filters feeding two series
 * all-pass filters, run independently per channel for a wide stereo image.
 * This is a lightweight, CPU-cheap reverb appropriate for a mobile DAW -
 * it does not aim to match convolution-reverb realism.
 */
class ReverbEffect : public Effect {
public:
    void process(float* left, float* right, int numFrames) override;
    void setParam(const std::string& key, float value) override;
    void setSampleRate(int sampleRate) override;
    void reset() override;

private:
    struct CombFilter {
        std::vector<float> buffer;
        int index = 0;
        float feedback = 0.5f;
        float damp = 0.5f;
        float filterState = 0.0f;

        void init(int delaySamples) {
            buffer.assign(std::max(1, delaySamples), 0.0f);
            index = 0;
            filterState = 0.0f;
        }
        float process(float input) {
            const float output = buffer[index];
            filterState = output * (1.0f - damp) + filterState * damp;
            buffer[index] = input + filterState * feedback;
            index = (index + 1) % static_cast<int>(buffer.size());
            return output;
        }
    };

    struct AllPassFilter {
        std::vector<float> buffer;
        int index = 0;
        float feedback = 0.5f;

        void init(int delaySamples) {
            buffer.assign(std::max(1, delaySamples), 0.0f);
            index = 0;
        }
        float process(float input) {
            const float bufOut = buffer[index];
            const float output = -input + bufOut;
            buffer[index] = input + bufOut * feedback;
            index = (index + 1) % static_cast<int>(buffer.size());
            return output;
        }
    };

    void rebuild();
    float processChannel(float input, CombFilter* combs, AllPassFilter* allpasses);

    static constexpr int kNumCombs = 4;
    static constexpr int kNumAllpass = 2;
    CombFilter combsL_[kNumCombs], combsR_[kNumCombs];
    AllPassFilter allpassL_[kNumAllpass], allpassR_[kNumAllpass];

    float roomSize_ = 0.5f;
    float damping_ = 0.5f;
    float wetMix_ = 0.3f;
};

} // namespace almus
