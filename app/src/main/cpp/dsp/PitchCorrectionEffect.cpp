#include "PitchCorrectionEffect.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace almus {

namespace {
constexpr float kMinHz = 70.0f;   // ~low male voice
constexpr float kMaxHz = 1000.0f; // ~high female voice / falsetto
constexpr int kGrainSize = 1024;  // ~21ms at 48kHz, good for voice-range shifting

// Major-scale semitone offsets from root, used when scaleType == 1.
constexpr int kMajorScale[7] = {0, 2, 4, 5, 7, 9, 11};
} // namespace

void PitchCorrectionEffect::setSampleRate(int sampleRate) {
    Effect::setSampleRate(sampleRate);
    reset();
}

void PitchCorrectionEffect::setParam(const std::string& key, float value) {
    if (key == "keyRootNote") keyRootNote_ = static_cast<int>(value) % 12;
    else if (key == "scaleType") scaleType_ = static_cast<int>(value);
    else if (key == "retuneSpeedMs") retuneSpeedMs_ = std::max(1.0f, value);
    else if (key == "amount") amount_ = std::min(1.0f, std::max(0.0f, value));
}

void PitchCorrectionEffect::reset() {
    analysisRing_.assign(kAnalysisSize, 0.0f);
    analysisWriteIndex_ = 0;
    shiftRing_.assign(kShiftRingSize, 0.0f);
    shiftWriteIndex_ = 0;
    grainReadPosA_ = 0.0f;
    grainReadPosB_ = static_cast<float>(kGrainSize) / 2.0f;
    grainBInitialized_ = true;
    currentRatio_ = 1.0f;
    smoothedRatio_ = 1.0f;
}

/**
 * Time-domain autocorrelation pitch detector. Searches lag range implied by
 * [kMinHz, kMaxHz] for the strongest periodicity. Adequate for monophonic
 * voice; will report unstable results on chords, breathy/unvoiced sections,
 * or heavy background noise - callers should expect occasional glitches on
 * such material rather than treat this as a lab-grade pitch tracker.
 */
float PitchCorrectionEffect::detectPitchHz(const std::vector<float>& block) {
    const int n = static_cast<int>(block.size());
    const int minLag = std::max(2, static_cast<int>(sampleRate_ / kMaxHz));
    const int maxLag = std::min(n - 1, static_cast<int>(sampleRate_ / kMinHz));

    float bestCorrelation = 0.0f;
    int bestLag = -1;

    // Normalize energy so quiet passages don't get spurious peaks.
    float energy = 0.0f;
    for (int i = 0; i < n; i++) energy += block[i] * block[i];
    if (energy < 1e-6f) return 0.0f;

    for (int lag = minLag; lag <= maxLag; lag++) {
        float correlation = 0.0f;
        for (int i = 0; i < n - lag; i++) {
            correlation += block[i] * block[i + lag];
        }
        correlation /= static_cast<float>(n - lag);
        if (correlation > bestCorrelation) {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }

    if (bestLag <= 0 || bestCorrelation < energy / n * 0.3f) {
        return 0.0f; // unvoiced / no confident pitch found
    }
    return static_cast<float>(sampleRate_) / static_cast<float>(bestLag);
}

float PitchCorrectionEffect::nearestScaleFrequency(float detectedHz) {
    if (detectedHz <= 0.0f) return detectedHz;

    // MIDI note number (fractional) referenced to A4 = 440Hz = note 69.
    const float midiFloat = 69.0f + 12.0f * log2f(detectedHz / 440.0f);
    const int midiRounded = static_cast<int>(std::lround(midiFloat));

    int bestNote = midiRounded;
    if (scaleType_ == 1) {
        // Snap to nearest note of the major scale rooted at keyRootNote_.
        int bestDistance = std::numeric_limits<int>::max();
        for (int octave = -1; octave <= 1; octave++) {
            for (int degree : kMajorScale) {
                const int candidate = midiRounded + octave * 12
                    - ((midiRounded % 12 + 12) % 12) + keyRootNote_ + degree;
                const int distance = std::abs(candidate - midiRounded);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestNote = candidate;
                }
            }
        }
    }
    // scaleType_ == 0: chromatic snap - every semitone is "in scale".

    const float targetHz = 440.0f * powf(2.0f, (bestNote - 69) / 12.0f);
    return targetHz;
}

float PitchCorrectionEffect::readGrainBuffer(float readPos) {
    const int size = kShiftRingSize;
    while (readPos < 0) readPos += size;
    const int i0 = static_cast<int>(readPos) % size;
    const int i1 = (i0 + 1) % size;
    const float frac = readPos - std::floor(readPos);
    return shiftRing_[i0] * (1.0f - frac) + shiftRing_[i1] * frac;
}

void PitchCorrectionEffect::process(float* left, float* right, int numFrames) {
    if (amount_ <= 0.001f) return; // bypassed: leave dry signal untouched

    const float retuneCoeff = expf(-1.0f / (sampleRate_ * (retuneSpeedMs_ / 1000.0f)));

    for (int i = 0; i < numFrames; i++) {
        const float monoIn = 0.5f * (left[i] + right[i]);

        // --- Analysis: feed ring buffer, re-run pitch detection periodically ---
        analysisRing_[analysisWriteIndex_] = monoIn;
        analysisWriteIndex_ = (analysisWriteIndex_ + 1) % kAnalysisSize;

        if (analysisWriteIndex_ % 512 == 0) {
            // Unroll the ring into a contiguous block for autocorrelation.
            std::vector<float> block(kAnalysisSize);
            for (int k = 0; k < kAnalysisSize; k++) {
                block[k] = analysisRing_[(analysisWriteIndex_ + k) % kAnalysisSize];
            }
            const float detectedHz = detectPitchHz(block);
            if (detectedHz > 0.0f) {
                const float targetHz = nearestScaleFrequency(detectedHz);
                currentRatio_ = targetHz / detectedHz;
            } else {
                currentRatio_ = 1.0f; // no confident pitch: pass through
            }
        }

        // Smooth ratio changes at the user-controlled retune speed to avoid
        // zipper artifacts (fast retuneSpeedMs => classic "hard-tuned" snap).
        smoothedRatio_ = retuneCoeff * smoothedRatio_ + (1.0f - retuneCoeff) * currentRatio_;

        // --- Pitch shifting: two-grain overlap-add granular resampler ---
        shiftRing_[shiftWriteIndex_] = monoIn;

        auto hannWindow = [](float phase01) {
            return 0.5f - 0.5f * cosf(2.0f * static_cast<float>(M_PI) * phase01);
        };

        const float distA = std::fmod(
            static_cast<float>(shiftWriteIndex_) - grainReadPosA_ + kShiftRingSize,
            static_cast<float>(kShiftRingSize)
        );
        const float distB = std::fmod(
            static_cast<float>(shiftWriteIndex_) - grainReadPosB_ + kShiftRingSize,
            static_cast<float>(kShiftRingSize)
        );
        const float phaseA = std::fmod(distA, static_cast<float>(kGrainSize)) / kGrainSize;
        const float phaseB = std::fmod(distB, static_cast<float>(kGrainSize)) / kGrainSize;

        const float sampleA = readGrainBuffer(grainReadPosA_) * hannWindow(phaseA);
        const float sampleB = readGrainBuffer(grainReadPosB_) * hannWindow(phaseB);
        const float shifted = sampleA + sampleB;

        grainReadPosA_ = std::fmod(grainReadPosA_ + smoothedRatio_ + kShiftRingSize, static_cast<float>(kShiftRingSize));
        grainReadPosB_ = std::fmod(grainReadPosB_ + smoothedRatio_ + kShiftRingSize, static_cast<float>(kShiftRingSize));

        shiftWriteIndex_ = (shiftWriteIndex_ + 1) % kShiftRingSize;

        const float corrected = monoIn * (1.0f - amount_) + shifted * amount_;

        // Re-apply original left/right balance around the corrected mono signal.
        left[i] = corrected;
        right[i] = corrected;
    }
}

} // namespace almus
