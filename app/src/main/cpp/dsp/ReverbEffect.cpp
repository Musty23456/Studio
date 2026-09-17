#include "ReverbEffect.h"
#include <algorithm>

namespace almus {

namespace {
// Comb delay lengths in samples at 44.1kHz (Schroeder/Moorer classic values),
// scaled to the actual sample rate in rebuild(). Slightly detuned between L/R
// for stereo width.
constexpr int kCombTuningL[4] = {1116, 1188, 1277, 1356};
constexpr int kCombTuningR[4] = {1139, 1211, 1300, 1379};
constexpr int kAllpassTuningL[2] = {556, 441};
constexpr int kAllpassTuningR[2] = {579, 464};
} // namespace

void ReverbEffect::setSampleRate(int sampleRate) {
    Effect::setSampleRate(sampleRate);
    rebuild();
}

void ReverbEffect::setParam(const std::string& key, float value) {
    if (key == "roomSize") roomSize_ = value;
    else if (key == "damping") damping_ = value;
    else if (key == "wetMix") wetMix_ = value;
    rebuild();
}

void ReverbEffect::rebuild() {
    const float srScale = static_cast<float>(sampleRate_) / 44100.0f;
    for (int i = 0; i < kNumCombs; i++) {
        combsL_[i].init(static_cast<int>(kCombTuningL[i] * srScale));
        combsR_[i].init(static_cast<int>(kCombTuningR[i] * srScale));
        combsL_[i].feedback = combsR_[i].feedback = 0.28f + roomSize_ * 0.7f;
        combsL_[i].damp = combsR_[i].damp = damping_;
    }
    for (int i = 0; i < kNumAllpass; i++) {
        allpassL_[i].init(static_cast<int>(kAllpassTuningL[i] * srScale));
        allpassR_[i].init(static_cast<int>(kAllpassTuningR[i] * srScale));
        allpassL_[i].feedback = allpassR_[i].feedback = 0.5f;
    }
}

void ReverbEffect::reset() { rebuild(); }

float ReverbEffect::processChannel(float input, CombFilter* combs, AllPassFilter* allpasses) {
    float combSum = 0.0f;
    for (int i = 0; i < kNumCombs; i++) combSum += combs[i].process(input);
    combSum /= static_cast<float>(kNumCombs);
    for (int i = 0; i < kNumAllpass; i++) combSum = allpasses[i].process(combSum);
    return combSum;
}

void ReverbEffect::process(float* left, float* right, int numFrames) {
    for (int i = 0; i < numFrames; i++) {
        const float wetL = processChannel(left[i], combsL_, allpassL_);
        const float wetR = processChannel(right[i], combsR_, allpassR_);
        left[i] = left[i] * (1.0f - wetMix_) + wetL * wetMix_;
        right[i] = right[i] * (1.0f - wetMix_) + wetR * wetMix_;
    }
}

} // namespace almus
