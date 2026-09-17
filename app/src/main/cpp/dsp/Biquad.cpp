#include "Biquad.h"
#include <cmath>

namespace almus {

void Biquad::setCoefficients(float b0, float b1, float b2, float a0, float a1, float a2) {
    b0_ = b0 / a0;
    b1_ = b1 / a0;
    b2_ = b2 / a0;
    a1_ = a1 / a0;
    a2_ = a2 / a0;
}

void Biquad::setLowShelf(float sampleRate, float freq, float gainDb, float slope) {
    const float A = powf(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * static_cast<float>(M_PI) * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / 2.0f * sqrtf((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
    const float twoSqrtAAlpha = 2.0f * sqrtf(A) * alpha;

    const float b0 = A * ((A + 1) - (A - 1) * cosw0 + twoSqrtAAlpha);
    const float b1 = 2 * A * ((A - 1) - (A + 1) * cosw0);
    const float b2 = A * ((A + 1) - (A - 1) * cosw0 - twoSqrtAAlpha);
    const float a0 = (A + 1) + (A - 1) * cosw0 + twoSqrtAAlpha;
    const float a1 = -2 * ((A - 1) + (A + 1) * cosw0);
    const float a2 = (A + 1) + (A - 1) * cosw0 - twoSqrtAAlpha;
    setCoefficients(b0, b1, b2, a0, a1, a2);
}

void Biquad::setHighShelf(float sampleRate, float freq, float gainDb, float slope) {
    const float A = powf(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * static_cast<float>(M_PI) * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / 2.0f * sqrtf((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
    const float twoSqrtAAlpha = 2.0f * sqrtf(A) * alpha;

    const float b0 = A * ((A + 1) + (A - 1) * cosw0 + twoSqrtAAlpha);
    const float b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
    const float b2 = A * ((A + 1) + (A - 1) * cosw0 - twoSqrtAAlpha);
    const float a0 = (A + 1) - (A - 1) * cosw0 + twoSqrtAAlpha;
    const float a1 = 2 * ((A - 1) - (A + 1) * cosw0);
    const float a2 = (A + 1) - (A - 1) * cosw0 - twoSqrtAAlpha;
    setCoefficients(b0, b1, b2, a0, a1, a2);
}

void Biquad::setPeaking(float sampleRate, float freq, float gainDb, float q) {
    const float A = powf(10.0f, gainDb / 40.0f);
    const float w0 = 2.0f * static_cast<float>(M_PI) * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / (2.0f * q);

    const float b0 = 1 + alpha * A;
    const float b1 = -2 * cosw0;
    const float b2 = 1 - alpha * A;
    const float a0 = 1 + alpha / A;
    const float a1 = -2 * cosw0;
    const float a2 = 1 - alpha / A;
    setCoefficients(b0, b1, b2, a0, a1, a2);
}

void Biquad::setLowPass(float sampleRate, float freq, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / (2.0f * q);

    const float b0 = (1 - cosw0) / 2;
    const float b1 = 1 - cosw0;
    const float b2 = (1 - cosw0) / 2;
    const float a0 = 1 + alpha;
    const float a1 = -2 * cosw0;
    const float a2 = 1 - alpha;
    setCoefficients(b0, b1, b2, a0, a1, a2);
}

void Biquad::setHighPass(float sampleRate, float freq, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * freq / sampleRate;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / (2.0f * q);

    const float b0 = (1 + cosw0) / 2;
    const float b1 = -(1 + cosw0);
    const float b2 = (1 + cosw0) / 2;
    const float a0 = 1 + alpha;
    const float a1 = -2 * cosw0;
    const float a2 = 1 - alpha;
    setCoefficients(b0, b1, b2, a0, a1, a2);
}

} // namespace almus
