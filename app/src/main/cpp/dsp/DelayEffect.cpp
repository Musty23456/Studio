#include "DelayEffect.h"
#include <algorithm>

namespace almus {

void DelayEffect::setSampleRate(int sampleRate) {
    Effect::setSampleRate(sampleRate);
    resizeBuffers();
}

void DelayEffect::setParam(const std::string& key, float value) {
    if (key == "delayMs") delayMs_ = std::max(1.0f, value);
    else if (key == "feedback") feedback_ = std::min(0.98f, std::max(0.0f, value));
    else if (key == "wetMix") wetMix_ = value;
    resizeBuffers();
}

void DelayEffect::resizeBuffers() {
    // Allocate generously (2 seconds) once, so parameter changes never need
    // to reallocate on the audio thread; only the active tap length moves.
    const size_t maxSamples = static_cast<size_t>(sampleRate_ * 2.0f);
    if (bufferL_.size() != maxSamples) {
        bufferL_.assign(maxSamples, 0.0f);
        bufferR_.assign(maxSamples, 0.0f);
        writeIndex_ = 0;
    }
}

void DelayEffect::reset() {
    std::fill(bufferL_.begin(), bufferL_.end(), 0.0f);
    std::fill(bufferR_.begin(), bufferR_.end(), 0.0f);
}

void DelayEffect::process(float* left, float* right, int numFrames) {
    if (bufferL_.empty()) resizeBuffers();
    const int delaySamples = std::min(
        static_cast<int>(bufferL_.size()) - 1,
        std::max(1, static_cast<int>(delayMs_ * sampleRate_ / 1000.0f))
    );

    for (int i = 0; i < numFrames; i++) {
        const int readIndex = (writeIndex_ - delaySamples + static_cast<int>(bufferL_.size()))
            % static_cast<int>(bufferL_.size());

        const float delayedL = bufferL_[readIndex];
        const float delayedR = bufferR_[readIndex];

        bufferL_[writeIndex_] = left[i] + delayedL * feedback_;
        bufferR_[writeIndex_] = right[i] + delayedR * feedback_;

        left[i] = left[i] * (1.0f - wetMix_) + delayedL * wetMix_;
        right[i] = right[i] * (1.0f - wetMix_) + delayedR * wetMix_;

        writeIndex_ = (writeIndex_ + 1) % static_cast<int>(bufferL_.size());
    }
}

} // namespace almus
