#include "EqualizerEffect.h"

namespace almus {

void EqualizerEffect::setSampleRate(int sampleRate) {
    Effect::setSampleRate(sampleRate);
    rebuildCoefficients();
}

void EqualizerEffect::setParam(const std::string& key, float value) {
    if (key == "lowGainDb") lowGainDb_ = value;
    else if (key == "midGainDb") midGainDb_ = value;
    else if (key == "highGainDb") highGainDb_ = value;
    rebuildCoefficients();
}

void EqualizerEffect::rebuildCoefficients() {
    const float sr = static_cast<float>(sampleRate_);
    lowL_.setLowShelf(sr, 200.0f, lowGainDb_);
    lowR_.setLowShelf(sr, 200.0f, lowGainDb_);
    midL_.setPeaking(sr, 1000.0f, midGainDb_, 0.9f);
    midR_.setPeaking(sr, 1000.0f, midGainDb_, 0.9f);
    highL_.setHighShelf(sr, 5000.0f, highGainDb_);
    highR_.setHighShelf(sr, 5000.0f, highGainDb_);
}

void EqualizerEffect::reset() {
    lowL_.reset(); lowR_.reset();
    midL_.reset(); midR_.reset();
    highL_.reset(); highR_.reset();
}

void EqualizerEffect::process(float* left, float* right, int numFrames) {
    for (int i = 0; i < numFrames; i++) {
        float l = left[i];
        l = lowL_.processSample(l);
        l = midL_.processSample(l);
        l = highL_.processSample(l);
        left[i] = l;

        float r = right[i];
        r = lowR_.processSample(r);
        r = midR_.processSample(r);
        r = highR_.processSample(r);
        right[i] = r;
    }
}

} // namespace almus
