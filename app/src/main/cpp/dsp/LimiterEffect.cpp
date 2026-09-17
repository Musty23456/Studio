#include "LimiterEffect.h"
#include <cmath>
#include <algorithm>

namespace almus {

void LimiterEffect::setParam(const std::string& key, float value) {
    if (key == "ceilingDb") ceilingDb_ = value;
    else if (key == "releaseMs") releaseMs_ = std::max(1.0f, value);
}

void LimiterEffect::process(float* left, float* right, int numFrames) {
    const float ceiling = powf(10.0f, ceilingDb_ / 20.0f);
    const float releaseCoeff = expf(-1.0f / (sampleRate_ * (releaseMs_ / 1000.0f)));

    for (int i = 0; i < numFrames; i++) {
        const float peak = std::max(fabsf(left[i]), fabsf(right[i])) * gainReduction_;
        float targetGain = gainReduction_;
        if (peak > ceiling) {
            targetGain = ceiling / std::max(1e-6f, std::max(fabsf(left[i]), fabsf(right[i])));
        } else {
            targetGain = 1.0f;
        }

        if (targetGain < gainReduction_) {
            gainReduction_ = targetGain; // instant attack: never let peaks through
        } else {
            gainReduction_ = releaseCoeff * gainReduction_ + (1.0f - releaseCoeff) * targetGain;
        }
        gainReduction_ = std::min(1.0f, gainReduction_);

        left[i] *= gainReduction_;
        right[i] *= gainReduction_;
    }
}

} // namespace almus
