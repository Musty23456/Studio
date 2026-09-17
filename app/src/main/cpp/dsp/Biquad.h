#pragma once

namespace almus {

/** Standard Robert Bristow-Johnson biquad, used to build the 3-band EQ. */
class Biquad {
public:
    enum class Type { LowShelf, HighShelf, Peaking, LowPass, HighPass };

    void setLowShelf(float sampleRate, float freq, float gainDb, float slope = 1.0f);
    void setHighShelf(float sampleRate, float freq, float gainDb, float slope = 1.0f);
    void setPeaking(float sampleRate, float freq, float gainDb, float q = 0.707f);
    void setLowPass(float sampleRate, float freq, float q = 0.707f);
    void setHighPass(float sampleRate, float freq, float q = 0.707f);

    inline float processSample(float in) {
        float out = b0_ * in + z1_;
        z1_ = b1_ * in - a1_ * out + z2_;
        z2_ = b2_ * in - a2_ * out;
        return out;
    }

    void reset() { z1_ = z2_ = 0.0f; }

private:
    void setCoefficients(float b0, float b1, float b2, float a0, float a1, float a2);

    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

} // namespace almus
