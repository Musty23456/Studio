#include "Synth.h"
#include <cmath>
#include <algorithm>

namespace almus {

namespace {
inline float midiToFreq(int pitch) {
    return 440.0f * powf(2.0f, (pitch - 69) / 12.0f);
}
} // namespace

float Synth::oscillate(SynthWaveform waveform, float phase) {
    switch (waveform) {
        case SynthWaveform::Sine:
            return sinf(2.0f * static_cast<float>(M_PI) * phase);
        case SynthWaveform::Square:
            return phase < 0.5f ? 1.0f : -1.0f;
        case SynthWaveform::Saw:
            return 2.0f * (phase - floorf(phase + 0.5f));
        case SynthWaveform::Pluck: {
            // Cheap "plucked" timbre: fundamental + fast-decaying harmonics,
            // decay is applied via the amplitude envelope in render().
            float v = sinf(2.0f * static_cast<float>(M_PI) * phase);
            v += 0.5f * sinf(4.0f * static_cast<float>(M_PI) * phase);
            v += 0.25f * sinf(6.0f * static_cast<float>(M_PI) * phase);
            return v * 0.6f;
        }
    }
    return 0.0f;
}

void Synth::noteOn(const std::string& trackId, int pitch, int velocity, int instrument) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& voices = voicesByTrack_[trackId];
    Voice v;
    v.pitch = pitch;
    v.phase = 0.0f;
    v.velocity = std::min(1.0f, velocity / 127.0f);
    v.envelope = 0.0f;
    v.releasing = false;
    v.waveform = static_cast<SynthWaveform>(std::min(3, std::max(0, instrument)));
    v.active = true;
    voices.push_back(v);
}

void Synth::noteOff(const std::string& trackId, int pitch) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = voicesByTrack_.find(trackId);
    if (it == voicesByTrack_.end()) return;
    for (auto& v : it->second) {
        if (v.pitch == pitch && v.active) v.releasing = true;
    }
}

void Synth::render(const std::string& trackId, float* left, float* right, int numFrames) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = voicesByTrack_.find(trackId);
    if (it == voicesByTrack_.end()) return;
    auto& voices = it->second;

    constexpr float kAttackPerSample = 1.0f / (48000.0f * 0.01f);  // ~10ms attack
    constexpr float kReleasePerSample = 1.0f / (48000.0f * 0.25f); // ~250ms release

    for (int i = 0; i < numFrames; i++) {
        float mix = 0.0f;
        for (auto& v : voices) {
            if (!v.active) continue;
            const float freq = midiToFreq(v.pitch);
            v.phase += freq / sampleRate_;
            if (v.phase >= 1.0f) v.phase -= 1.0f;

            if (v.releasing) {
                v.envelope -= kReleasePerSample;
                if (v.envelope <= 0.0f) { v.envelope = 0.0f; v.active = false; }
            } else if (v.envelope < 1.0f) {
                v.envelope = std::min(1.0f, v.envelope + kAttackPerSample);
            }

            mix += oscillate(v.waveform, v.phase) * v.envelope * v.velocity * 0.3f;
        }
        left[i] += mix;
        right[i] += mix;
    }

    voices.erase(std::remove_if(voices.begin(), voices.end(),
        [](const Voice& v) { return !v.active; }), voices.end());
}

} // namespace almus
