#include "DistortionEffect.h"
#include <cmath>
#include <algorithm>

namespace almus {

namespace {
inline float softClip(float x) {
    // Cubic soft clipper: smooth saturation without hard digital clipping.
    if (x > 1.0f) return 2.0f / 3.0f;
    if (x < -1.0f) return -2.0f / 3.0f;
    return x - (x * x * x) / 3.0f;
}
} // namespace

void DistortionEffect::setSampleRate(int sampleRate) {
    Effect::setSampleRate(sampleRate);
    rebuildTone();
}

void DistortionEffect::setParam(const std::string& key, float value) {
    if (key == "driveDb") driveDb_ = value;
    else if (key == "tone") tone_ = std::min(1.0f, std::max(0.0f, value));
    else if (key == "mix") mix_ = std::min(1.0f, std::max(0.0f, value));
    rebuildTone();
}

void DistortionEffect::rebuildTone() {
    // tone=0 -> low-pass around 1.5kHz (dark), tone=1 -> high-shelf boost (bright)
    const float freq = 800.0f + tone_ * 6000.0f;
    toneFilterL_.setLowPass(static_cast<float>(sampleRate_), freq, 0.707f);
    toneFilterR_.setLowPass(static_cast<float>(sampleRate_), freq, 0.707f);
}

void DistortionEffect::reset() {
    toneFilterL_.reset();
    toneFilterR_.reset();
}

void DistortionEffect::process(float* left, float* right, int numFrames) {
    const float driveLinear = powf(10.0f, driveDb_ / 20.0f);
    for (int i = 0; i < numFrames; i++) {
        const float dryL = left[i];
        const float dryR = right[i];

        float wetL = softClip(dryL * driveLinear);
        float wetR = softClip(dryR * driveLinear);
        wetL = toneFilterL_.processSample(wetL);
        wetR = toneFilterR_.processSample(wetR);

        // Compensate makeup gain roughly so drive doesn't just get quieter.
        const float compensation = 1.0f / std::max(0.3f, sqrtf(driveLinear));
        wetL *= compensation;
        wetR *= compensation;

        left[i] = dryL * (1.0f - mix_) + wetL * mix_;
        right[i] = dryR * (1.0f - mix_) + wetR * mix_;
    }
}

} // namespace almus
