#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>

namespace almus {

enum class SynthWaveform { Sine, Square, Saw, Pluck };

/**
 * A small built-in synthesizer used by the piano-roll editor. This is
 * intentionally simple (subtractive-free oscillator synthesis with a basic
 * envelope) rather than a sample-based instrument library, which keeps the
 * app's asset footprint at zero while still making the piano roll audibly
 * real rather than a silent note grid.
 */
class Synth {
public:
    void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }

    void noteOn(const std::string& trackId, int pitch, int velocity, int instrument);
    void noteOff(const std::string& trackId, int pitch);

    /** Mixes active voices for [trackId] into left/right (additive). */
    void render(const std::string& trackId, float* left, float* right, int numFrames);

private:
    struct Voice {
        int pitch = 0;
        float phase = 0.0f;
        float velocity = 1.0f;
        float envelope = 0.0f;
        bool releasing = false;
        SynthWaveform waveform = SynthWaveform::Sine;
        bool active = false;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<Voice>> voicesByTrack_;
    int sampleRate_ = 48000;

    float oscillate(SynthWaveform waveform, float phase);
};

} // namespace almus
