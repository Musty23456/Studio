#include "Engine.h"
#include "dsp/EqualizerEffect.h"
#include "dsp/CompressorEffect.h"
#include "dsp/ReverbEffect.h"
#include "dsp/DelayEffect.h"
#include "dsp/ChorusEffect.h"
#include "dsp/DistortionEffect.h"
#include "dsp/LimiterEffect.h"
#include "dsp/PitchCorrectionEffect.h"

#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <thread>

#define LOG_TAG "AlmusEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace almus {

Engine& Engine::instance() {
    static Engine engine;
    return engine;
}

bool Engine::init(int sampleRate, int framesPerBurst) {
    sampleRate_ = sampleRate;
    sequencer_.setSampleRate(sampleRate);
    sequencer_.setBpm(bpm_);
    synth_.setSampleRate(sampleRate);

    trackScratchLeft_.assign(sampleRate, 0.0f);
    trackScratchRight_.assign(sampleRate, 0.0f);
    blockMixLeft_.assign(sampleRate, 0.0f);
    blockMixRight_.assign(sampleRate, 0.0f);

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setSampleRate(sampleRate)
        ->setFramesPerDataCallback(framesPerBurst)
        ->setCallback(this);

    const oboe::Result result = builder.openStream(outputStream_);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open output stream: %s", oboe::convertToText(result));
        return false;
    }
    outputStream_->requestStart();
    return true;
}

void Engine::shutdown() {
    if (outputStream_) {
        outputStream_->requestStop();
        outputStream_->close();
        outputStream_.reset();
    }
    if (inputStream_) {
        inputStream_->requestStop();
        inputStream_->close();
        inputStream_.reset();
    }
}

// ---- Transport ----------------------------------------------------------

void Engine::play() { isPlaying_ = true; }
void Engine::pause() { isPlaying_ = false; }
void Engine::stop() { isPlaying_ = false; playheadFrame_ = 0; }
void Engine::seekToFrame(int64_t frame) { playheadFrame_ = std::max<int64_t>(0, frame); }

void Engine::setLooping(bool enabled, int64_t startFrame, int64_t endFrame) {
    loopEnabled_ = enabled;
    loopStart_ = startFrame;
    loopEnd_ = endFrame;
}

// ---- Tracks ---------------------------------------------------------------

void Engine::addTrack(const std::string& trackId) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto track = std::make_unique<Track>();
    track->id = trackId;
    tracks_[trackId] = std::move(track);
}

void Engine::removeTrack(const std::string& trackId) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    tracks_.erase(trackId);
}

void Engine::setTrackGain(const std::string& trackId, float linearGain) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    if (auto it = tracks_.find(trackId); it != tracks_.end()) it->second->gain = linearGain;
}

void Engine::setTrackPan(const std::string& trackId, float pan) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    if (auto it = tracks_.find(trackId); it != tracks_.end()) it->second->pan = pan;
}

void Engine::setTrackMute(const std::string& trackId, bool muted) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    if (auto it = tracks_.find(trackId); it != tracks_.end()) it->second->muted = muted;
}

void Engine::setTrackSolo(const std::string& trackId, bool soloed) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    if (auto it = tracks_.find(trackId); it != tracks_.end()) it->second->soloed = soloed;
}

float Engine::getTrackLevel(const std::string& trackId) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    if (auto it = tracks_.find(trackId); it != tracks_.end()) return it->second->currentLevel.load();
    return 0.0f;
}

// ---- Clips ------------------------------------------------------------

bool Engine::addClip(const std::string& trackId, const Clip& clipIn) {
    auto source = AudioFileCache::instance().load(clipIn.filePath);
    if (!source) {
        LOGE("addClip: failed to load %s", clipIn.filePath.c_str());
        return false;
    }
    Clip clip = clipIn;
    clip.source = source;

    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = tracks_.find(trackId);
    if (it == tracks_.end()) return false;
    auto& clips = it->second->clips;
    auto existing = std::find_if(clips.begin(), clips.end(),
        [&](const Clip& c) { return c.id == clip.id; });
    if (existing != clips.end()) {
        *existing = clip;
    } else {
        clips.push_back(clip);
    }
    return true;
}

void Engine::removeClip(const std::string& trackId, const std::string& clipId) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = tracks_.find(trackId);
    if (it == tracks_.end()) return;
    auto& clips = it->second->clips;
    clips.erase(std::remove_if(clips.begin(), clips.end(),
        [&](const Clip& c) { return c.id == clipId; }), clips.end());
}

void Engine::updateClipTiming(
    const std::string& trackId, const std::string& clipId,
    int64_t startFrameInSource, int64_t endFrameInSource, int64_t timelineStartFrame
) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = tracks_.find(trackId);
    if (it == tracks_.end()) return;
    for (auto& clip : it->second->clips) {
        if (clip.id == clipId) {
            clip.startFrameInSource = startFrameInSource;
            clip.endFrameInSource = endFrameInSource;
            clip.timelineStartFrame = timelineStartFrame;
            break;
        }
    }
}

// ---- Effects --------------------------------------------------------------

Effect* Engine::createEffect(int effectType) {
    // Ordinal order must match com.almus.studio.model.EffectType exactly.
    switch (effectType) {
        case 0: return new EqualizerEffect();
        case 1: return new CompressorEffect();
        case 2: return new ReverbEffect();
        case 3: return new DelayEffect();
        case 4: return new ChorusEffect();
        case 5: return new DistortionEffect();
        case 6: return new LimiterEffect();
        case 7: return new PitchCorrectionEffect();
        default: return nullptr;
    }
}

void Engine::setTrackEffect(
    const std::string& trackId, const std::string& effectId, int effectType,
    bool enabled, const std::vector<std::string>& keys, const std::vector<float>& values
) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = tracks_.find(trackId);
    if (it == tracks_.end()) return;
    auto& track = *it->second;

    Effect* effect = nullptr;
    auto idxIt = track.effectIndexById.find(effectId);
    if (idxIt != track.effectIndexById.end()) {
        effect = track.effects[idxIt->second].get();
    } else {
        std::unique_ptr<Effect> newEffect(createEffect(effectType));
        if (!newEffect) return;
        newEffect->setSampleRate(sampleRate_);
        track.effectIndexById[effectId] = track.effects.size();
        track.effects.push_back(std::move(newEffect));
        effect = track.effects.back().get();
    }

    effect->enabled = enabled;
    for (size_t i = 0; i < keys.size() && i < values.size(); i++) {
        effect->setParam(keys[i], values[i]);
    }
}

void Engine::removeTrackEffect(const std::string& trackId, const std::string& effectId) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = tracks_.find(trackId);
    if (it == tracks_.end()) return;
    auto& track = *it->second;
    auto idxIt = track.effectIndexById.find(effectId);
    if (idxIt == track.effectIndexById.end()) return;

    const size_t removedIndex = idxIt->second;
    track.effects.erase(track.effects.begin() + static_cast<long>(removedIndex));
    track.effectIndexById.erase(idxIt);
    // Shift indices for effects that came after the removed one.
    for (auto& [id, index] : track.effectIndexById) {
        if (index > removedIndex) index--;
    }
}

// ---- Recording --------------------------------------------------------

bool Engine::startRecording(const std::string& trackId, const std::string& outputPath) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = tracks_.find(trackId);
    if (it == tracks_.end()) return false;
    auto& track = *it->second;
    if (track.isRecording) return false;

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setSampleRate(sampleRate_);

    if (builder.openStream(inputStream_) != oboe::Result::OK) {
        LOGE("startRecording: failed to open input stream");
        return false;
    }

    track.recordingWriter = std::make_unique<RecordingWriter>(outputPath);
    track.recordingWriter->start(sampleRate_);
    track.isRecording = true;
    inputStream_->requestStart();

    // Blocking-read capture thread: kept intentionally simple (phase 1) -
    // there is no live pass-through monitoring of this input into the
    // output mix while recording, only after stopRecording() adds the
    // resulting clip to the timeline. Wiring live monitoring is a
    // straightforward follow-up (mix inputStream_ reads into the output
    // callback's buffer) once full-duplex latency has been tuned per device.
    std::thread([this, trackId]() {
        constexpr int kChunkFrames = 256;
        std::vector<float> chunk(kChunkFrames * 2);
        while (true) {
            {
                std::lock_guard<std::mutex> innerLock(graphMutex_);
                auto trackIt = tracks_.find(trackId);
                if (trackIt == tracks_.end() || !trackIt->second->isRecording) break;
            }
            oboe::ResultWithValue<int32_t> result =
                inputStream_->read(chunk.data(), kChunkFrames, 1'000'000'000);
            if (result != oboe::Result::OK) break;
            const int framesRead = result.value();
            if (framesRead <= 0) continue;
            std::lock_guard<std::mutex> innerLock(graphMutex_);
            auto trackIt = tracks_.find(trackId);
            if (trackIt != tracks_.end() && trackIt->second->recordingWriter) {
                trackIt->second->recordingWriter->pushFrames(chunk.data(), framesRead);
            }
        }
    }).detach();

    return true;
}

std::string Engine::stopRecording(const std::string& trackId) {
    std::string resultPath;
    {
        std::lock_guard<std::mutex> lock(graphMutex_);
        auto it = tracks_.find(trackId);
        if (it == tracks_.end() || !it->second->isRecording) return "";
        it->second->isRecording = false;
        if (it->second->recordingWriter) {
            resultPath = it->second->recordingWriter->stop();
            it->second->recordingWriter.reset();
        }
    }
    if (inputStream_) {
        inputStream_->requestStop();
        inputStream_->close();
        inputStream_.reset();
    }
    if (!resultPath.empty()) {
        AudioFileCache::instance().invalidate(resultPath);
    }
    return resultPath;
}

// ---- Rendering ------------------------------------------------------------

void Engine::renderTrackInto(Track& track, float* left, float* right, int numFrames, int64_t startFrame) {
    std::fill(left, left + numFrames, 0.0f);
    std::fill(right, right + numFrames, 0.0f);

    for (auto& clip : track.clips) {
        if (!clip.source) continue;
        const int64_t clipTimelineEnd = clip.timelineStartFrame + clip.lengthInFrames();
        if (clipTimelineEnd <= startFrame || clip.timelineStartFrame >= startFrame + numFrames) {
            continue; // no overlap with this block
        }

        const float gainLinear = powf(10.0f, clip.gainDb / 20.0f);

        for (int i = 0; i < numFrames; i++) {
            const int64_t timelineFrame = startFrame + i;
            if (timelineFrame < clip.timelineStartFrame || timelineFrame >= clipTimelineEnd) continue;

            const int64_t sourceFrame = clip.startFrameInSource + (timelineFrame - clip.timelineStartFrame);
            if (sourceFrame < 0 || sourceFrame >= clip.source->numFrames) continue;

            float fadeGain = 1.0f;
            const int64_t posInClip = timelineFrame - clip.timelineStartFrame;
            if (clip.fadeInFrames > 0 && posInClip < clip.fadeInFrames) {
                fadeGain *= static_cast<float>(posInClip) / static_cast<float>(clip.fadeInFrames);
            }
            const int64_t framesFromEnd = clipTimelineEnd - timelineFrame;
            if (clip.fadeOutFrames > 0 && framesFromEnd < clip.fadeOutFrames) {
                fadeGain *= static_cast<float>(framesFromEnd) / static_cast<float>(clip.fadeOutFrames);
            }

            left[i] += clip.source->left[sourceFrame] * gainLinear * fadeGain;
            right[i] += clip.source->right[sourceFrame] * gainLinear * fadeGain;
        }
    }

    // Per-track effect chain (pre-fader).
    for (auto& effect : track.effects) {
        if (effect->enabled) effect->process(left, right, numFrames);
    }

    // Piano-roll live-audition synth voices for this track (see Synth.h note
    // on scope: these are auditioned notes, not yet timeline-scheduled).
    synth_.render(track.id, left, right, numFrames);

    // Gain + equal-power pan law.
    const float panRadians = (track.pan + 1.0f) * 0.25f * static_cast<float>(M_PI);
    const float leftPan = cosf(panRadians);
    const float rightPan = sinf(panRadians);

    float peak = 0.0f;
    for (int i = 0; i < numFrames; i++) {
        left[i] *= track.gain * leftPan * 1.4142f;
        right[i] *= track.gain * rightPan * 1.4142f;
        peak = std::max({peak, fabsf(left[i]), fabsf(right[i])});
    }
    track.currentLevel.store(peak);
}

void Engine::renderBlock(float* left, float* right, int numFrames, int64_t startFrame) {
    std::fill(left, left + numFrames, 0.0f);
    std::fill(right, right + numFrames, 0.0f);

    std::lock_guard<std::mutex> lock(graphMutex_);

    bool anySoloed = false;
    for (auto& [id, track] : tracks_) {
        if (track->soloed) { anySoloed = true; break; }
    }

    if (static_cast<int>(trackScratchLeft_.size()) < numFrames) {
        trackScratchLeft_.assign(numFrames, 0.0f);
        trackScratchRight_.assign(numFrames, 0.0f);
    }

    for (auto& [id, track] : tracks_) {
        const bool audible = anySoloed ? track->soloed : !track->muted;
        if (!audible) { track->currentLevel.store(0.0f); continue; }

        renderTrackInto(*track, trackScratchLeft_.data(), trackScratchRight_.data(), numFrames, startFrame);
        for (int i = 0; i < numFrames; i++) {
            left[i] += trackScratchLeft_[i];
            right[i] += trackScratchRight_[i];
        }
    }

    sequencer_.render(left, right, numFrames);
}

// ---- Realtime callback ----------------------------------------------------

oboe::DataCallbackResult Engine::onAudioReady(
    oboe::AudioStream* stream, void* audioData, int32_t numFrames
) {
    auto* out = static_cast<float*>(audioData);

    if (!isPlaying_) {
        std::fill(out, out + numFrames * 2, 0.0f);
        return oboe::DataCallbackResult::Continue;
    }

    const int64_t startFrame = playheadFrame_.load();

    if (static_cast<int>(blockMixLeft_.size()) < numFrames) {
        blockMixLeft_.assign(numFrames, 0.0f);
        blockMixRight_.assign(numFrames, 0.0f);
    }
    renderBlock(blockMixLeft_.data(), blockMixRight_.data(), numFrames, startFrame);

    for (int i = 0; i < numFrames; i++) {
        out[i * 2] = std::clamp(blockMixLeft_[i], -1.0f, 1.0f);
        out[i * 2 + 1] = std::clamp(blockMixRight_[i], -1.0f, 1.0f);
    }

    int64_t nextFrame = startFrame + numFrames;
    if (loopEnabled_ && nextFrame >= loopEnd_) {
        nextFrame = loopStart_;
    }
    playheadFrame_.store(nextFrame);

    return oboe::DataCallbackResult::Continue;
}

void Engine::onErrorAfterClose(oboe::AudioStream* /*stream*/, oboe::Result error) {
    LOGE("Stream closed with error: %s", oboe::convertToText(error));
}

// ---- Offline export -----------------------------------------------------

bool Engine::exportMixdown(const std::string& outputPath, int sampleRate) {
    int64_t totalFrames = 0;
    {
        std::lock_guard<std::mutex> lock(graphMutex_);
        for (auto& [id, track] : tracks_) {
            for (auto& clip : track->clips) {
                totalFrames = std::max(totalFrames, clip.timelineStartFrame + clip.lengthInFrames());
            }
        }
    }
    if (totalFrames <= 0) totalFrames = sampleRate; // export at least 1s of silence rather than fail

    constexpr int kBlockSize = 4096;
    std::vector<float> interleaved(static_cast<size_t>(totalFrames) * 2);
    std::vector<float> blockLeft(kBlockSize), blockRight(kBlockSize);

    for (int64_t frame = 0; frame < totalFrames; frame += kBlockSize) {
        const int framesThisBlock = static_cast<int>(std::min<int64_t>(kBlockSize, totalFrames - frame));
        renderBlock(blockLeft.data(), blockRight.data(), framesThisBlock, frame);
        for (int i = 0; i < framesThisBlock; i++) {
            interleaved[(frame + i) * 2] = blockLeft[i];
            interleaved[(frame + i) * 2 + 1] = blockRight[i];
        }
    }

    return writeWavFile(outputPath, interleaved.data(), totalFrames, sampleRate);
}

} // namespace almus
