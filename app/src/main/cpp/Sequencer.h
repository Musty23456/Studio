#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include "WavFile.h"

namespace almus {

struct SequencerLane {
    std::string id;
    std::shared_ptr<AudioBuffer> sample;
    std::vector<bool> steps;
    std::vector<float> velocity;

    // Currently-sounding one-shot voices for this lane (supports overlap
    // when a step retriggers before the previous hit finished playing).
    struct Voice { int64_t position = 0; float gain = 1.0f; bool active = false; };
    std::vector<Voice> voices;
};

/**
 * Sample-accurate step sequencer for the drum machine. Advances in lockstep
 * with the main transport's frame counter so patterns stay locked to BPM
 * even if UI updates lag behind.
 */
class Sequencer {
public:
    void setSampleRate(int sampleRate) { sampleRate_ = sampleRate; }
    void setBpm(float bpm) { bpm_ = bpm; }
    void setPattern(const std::string& patternId, int stepsPerBar, int bars);
    void loadSample(const std::string& laneId, const std::string& filePath);
    void setStep(const std::string& laneId, int stepIndex, bool active, float velocity);
    void start() { running_ = true; framePositionInPattern_ = 0; lastStepTriggered_ = -1; }
    void stop() { running_ = false; }
    bool isRunning() const { return running_; }

    /** Mixes [numFrames] of sequencer output into left/right (additive). */
    void render(float* left, float* right, int numFrames);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, SequencerLane> lanes_;
    int stepsPerBar_ = 16;
    int bars_ = 1;
    int sampleRate_ = 48000;
    float bpm_ = 120.0f;
    std::atomic<bool> running_{false};
    int64_t framePositionInPattern_ = 0;
    int lastStepTriggered_ = -1;

    int64_t framesPerStep() const;
};

} // namespace almus
