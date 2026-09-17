#pragma once
#include "Effect.h"
#include <vector>

namespace almus {

/**
 * Vocal pitch correction ("Auto-Tune style").
 *
 * Honest scope note: this implements time-domain autocorrelation pitch
 * detection plus a two-grain overlap-add pitch shifter snapping detected
 * pitch to the nearest note of the selected scale. It works well on clean,
 * monophonic vocal input (a single sung note at a time) and is tunable from
 * subtle correction to the hard-quantized "T-Pain" effect via retune speed.
 * It is NOT a phase-vocoder or spectral formant-preserving corrector like
 * commercial plugins (Antares Auto-Tune, Melodyne) - on polyphonic material,
 * noisy input, or very fast passages it will track less reliably. This is
 * the best reliable implementation achievable without a much larger FFT/
 * phase-vocoder pipeline, and is a reasonable phase-2 upgrade path.
 */
class PitchCorrectionEffect : public Effect {
public:
    void process(float* left, float* right, int numFrames) override;
    void setParam(const std::string& key, float value) override;
    void setSampleRate(int sampleRate) override;
    void reset() override;

private:
    float detectPitchHz(const std::vector<float>& monoBlock);
    float nearestScaleFrequency(float detectedHz);
    float readGrainBuffer(float readPos);

    // Analysis
    std::vector<float> analysisRing_;
    int analysisWriteIndex_ = 0;
    static constexpr int kAnalysisSize = 2048;

    // Pitch shifting (granular overlap-add)
    std::vector<float> shiftRing_;
    int shiftWriteIndex_ = 0;
    float grainReadPosA_ = 0.0f;
    float grainReadPosB_ = 0.0f;
    bool grainBInitialized_ = false;
    static constexpr int kShiftRingSize = 1 << 16; // 65536 samples, power of two

    float currentRatio_ = 1.0f;
    float smoothedRatio_ = 1.0f;

    // Parameters
    int keyRootNote_ = 0;      // 0 = C
    int scaleType_ = 0;        // 0 = chromatic/major-minor snap, 1 = major only
    float retuneSpeedMs_ = 30.0f;
    float amount_ = 1.0f;      // 0 = bypass (dry), 1 = full correction
};

} // namespace almus
