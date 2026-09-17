#include "ChorusEffect.h"
#include <cmath>
#include <algorithm>

namespace almus {

void ChorusEffect::setSampleRate(int sampleRate) {
    Effect::setSampleRate(sampleRate);
    const size_t maxSamples = static_cast<size_t>(sampleRate_ * 0.05f) + 16; // 50ms max
    bufferL_.assign(maxSamples, 0.0f);
    bufferR_.assign(maxSamples, 0.0f);
    writeIndex_ = 0;
}

void ChorusEffect::setParam(const std::string& key, float value) {
    if (key == "rateHz") rateHz_ = std::max(0.02f, value);
    else if (key == "depthMs") depthMs_ = std::max(0.0f, value);
    else if (key == "wetMix") wetMix_ = value;
}

void ChorusEffect::reset() {
    std::fill(bufferL_.begin(), bufferL_.end(), 0.0f);
    std::fill(bufferR_.begin(), bufferR_.end(), 0.0f);
    lfoPhase_ = 0.0f;
}

float ChorusEffect::readInterpolated(const std::vector<float>& buffer, float delaySamples) {
    const int size = static_cast<int>(buffer.size());
    float readPos = static_cast<float>(writeIndex_) - delaySamples;
    while (readPos < 0) readPos += size;
    const int i0 = static_cast<int>(readPos) % size;
    const int i1 = (i0 + 1) % size;
    const float frac = readPos - std::floor(readPos);
    return buffer[i0] * (1.0f - frac) + buffer[i1] * frac;
}

void ChorusEffect::process(float* left, float* right, int numFrames) {
    if (bufferL_.empty()) return;
    const int size = static_cast<int>(bufferL_.size());
    const float depthSamples = depthMs_ * sampleRate_ / 1000.0f;
    const float baseDelaySamples = depthSamples + 2.0f; // keep away from zero delay

    for (int i = 0; i < numFrames; i++) {
        lfoPhase_ += rateHz_ / static_cast<float>(sampleRate_);
        if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
        const float lfo = sinf(2.0f * static_cast<float>(M_PI) * lfoPhase_);
        const float modulatedDelay = baseDelaySamples + lfo * depthSamples * 0.5f;

        bufferL_[writeIndex_] = left[i];
        bufferR_[writeIndex_] = right[i];

        const float wetL = readInterpolated(bufferL_, modulatedDelay);
        const float wetR = readInterpolated(bufferR_, modulatedDelay);

        left[i] = left[i] * (1.0f - wetMix_) + wetL * wetMix_;
        right[i] = right[i] * (1.0f - wetMix_) + wetR * wetMix_;

        writeIndex_ = (writeIndex_ + 1) % size;
    }
}

} // namespace almus
