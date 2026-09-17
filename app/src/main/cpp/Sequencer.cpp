#include "Sequencer.h"
#include <algorithm>

namespace almus {

int64_t Sequencer::framesPerStep() const {
    // 16 steps per bar == sixteenth notes at the given BPM (4/4 assumed).
    const double secondsPerBeat = 60.0 / std::max(1.0f, bpm_);
    const double beatsPerStep = 4.0 / static_cast<double>(std::max(1, stepsPerBar_));
    return static_cast<int64_t>(secondsPerBeat * beatsPerStep * sampleRate_);
}

void Sequencer::setPattern(const std::string& /*patternId*/, int stepsPerBar, int bars) {
    std::lock_guard<std::mutex> lock(mutex_);
    stepsPerBar_ = std::max(1, stepsPerBar);
    bars_ = std::max(1, bars);
}

void Sequencer::loadSample(const std::string& laneId, const std::string& filePath) {
    auto buffer = AudioFileCache::instance().load(filePath);
    std::lock_guard<std::mutex> lock(mutex_);
    auto& lane = lanes_[laneId];
    lane.id = laneId;
    lane.sample = buffer;
    if (lane.steps.empty()) {
        lane.steps.assign(stepsPerBar_ * bars_, false);
        lane.velocity.assign(stepsPerBar_ * bars_, 1.0f);
    }
}

void Sequencer::setStep(const std::string& laneId, int stepIndex, bool active, float velocity) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lanes_.find(laneId);
    if (it == lanes_.end()) return;
    auto& lane = it->second;
    if (stepIndex < 0) return;
    if (stepIndex >= static_cast<int>(lane.steps.size())) {
        lane.steps.resize(stepIndex + 1, false);
        lane.velocity.resize(stepIndex + 1, 1.0f);
    }
    lane.steps[stepIndex] = active;
    lane.velocity[stepIndex] = velocity;
}

void Sequencer::render(float* left, float* right, int numFrames) {
    if (!running_) return;
    std::lock_guard<std::mutex> lock(mutex_);

    const int64_t stepFrames = framesPerStep();
    if (stepFrames <= 0) return;
    const int totalSteps = stepsPerBar_ * bars_;

    for (int i = 0; i < numFrames; i++) {
        const int currentStep = static_cast<int>((framePositionInPattern_ / stepFrames) % std::max(1, totalSteps));

        if (currentStep != lastStepTriggered_) {
            lastStepTriggered_ = currentStep;
            for (auto& [id, lane] : lanes_) {
                if (currentStep < static_cast<int>(lane.steps.size()) && lane.steps[currentStep] && lane.sample) {
                    SequencerLane::Voice voice;
                    voice.position = 0;
                    voice.gain = lane.velocity[currentStep];
                    voice.active = true;
                    lane.voices.push_back(voice);
                }
            }
        }

        float mixL = 0.0f, mixR = 0.0f;
        for (auto& [id, lane] : lanes_) {
            if (!lane.sample) continue;
            for (auto& voice : lane.voices) {
                if (!voice.active) continue;
                if (voice.position >= lane.sample->numFrames) {
                    voice.active = false;
                    continue;
                }
                mixL += lane.sample->left[voice.position] * voice.gain;
                mixR += lane.sample->right[voice.position] * voice.gain;
                voice.position++;
            }
            lane.voices.erase(
                std::remove_if(lane.voices.begin(), lane.voices.end(),
                    [](const SequencerLane::Voice& v) { return !v.active; }),
                lane.voices.end()
            );
        }

        left[i] += mixL;
        right[i] += mixR;

        framePositionInPattern_++;
        if (framePositionInPattern_ >= stepFrames * totalSteps) {
            framePositionInPattern_ = 0;
        }
    }
}

} // namespace almus
