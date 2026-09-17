#include "CompressorEffect.h"
#include <cmath>
#include <algorithm>

namespace almus {

void CompressorEffect::setParam(const std::string& key, float value) {
    if (key == "thresholdDb") thresholdDb_ = value;
    else if (key == "ratio") ratio_ = std::max(1.0f, value);
    else if (key == "attackMs") attackMs_ = std::max(0.1f, value);
    else if (key == "releaseMs") releaseMs_ = std::max(1.0f, value);
    else if (key == "makeupDb") makeupDb_ = value;
}

void CompressorEffect::process(float* left, float* right, int numFrames) {
    const float attackCoeff = expf(-1.0f / (sampleRate_ * (attackMs_ / 1000.0f)));
    const float releaseCoeff = expf(-1.0f / (sampleRate_ * (releaseMs_ / 1000.0f)));
    const float makeupLinear = powf(10.0f, makeupDb_ / 20.0f);

    for (int i = 0; i < numFrames; i++) {
        const float peak = std::max(fabsf(left[i]), fabsf(right[i]));
        const float peakDb = 20.0f * log10f(std::max(peak, 1e-6f));

        // Envelope follower.
        if (peakDb > envelope_) {
            envelope_ = attackCoeff * envelope_ + (1.0f - attackCoeff) * peakDb;
        } else {
            envelope_ = releaseCoeff * envelope_ + (1.0f - releaseCoeff) * peakDb;
        }

        float gainReductionDb = 0.0f;
        if (envelope_ > thresholdDb_) {
            gainReductionDb = (thresholdDb_ - envelope_) * (1.0f - 1.0f / ratio_);
        }
        const float gainLinear = powf(10.0f, gainReductionDb / 20.0f) * makeupLinear;

        left[i] *= gainLinear;
        right[i] *= gainLinear;
    }
}

} // namespace almus
