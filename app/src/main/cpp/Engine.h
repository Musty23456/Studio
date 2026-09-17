#pragma once

#include <oboe/Oboe.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "WavFile.h"
#include "dsp/Effect.h"
#include "RecordingWriter.h"
#include "Sequencer.h"
#include "Synth.h"

namespace almus {

struct Clip {
    std::string id;
    std::string filePath;
    int64_t startFrameInSource = 0;
    int64_t endFrameInSource = 0;
    int64_t timelineStartFrame = 0;
    float gainDb = 0.0f;
    int64_t fadeInFrames = 0;
    int64_t fadeOutFrames = 0;
    std::shared_ptr<AudioBuffer> source; // resolved lazily from AudioFileCache

    int64_t lengthInFrames() const { return endFrameInSource - startFrameInSource; }
};

struct Track {
    std::string id;
    std::vector<Clip> clips;
    std::vector<std::unique_ptr<Effect>> effects;
    std::unordered_map<std::string, size_t> effectIndexById;
    float gain = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;
    std::atomic<float> currentLevel{0.0f};

    // Recording state
    bool isRecording = false;
    std::unique_ptr<RecordingWriter> recordingWriter;
};

/**
 * Owns the full audio graph and the two Oboe streams (output + input). All
 * mutation methods lock a single graph mutex; the realtime callback also
 * locks it, but only for the duration of pointer/parameter reads copied
 * into local variables up front, keeping the critical section short and
 * avoiding any allocation on the audio thread itself.
 */
class Engine : public oboe::AudioStreamCallback {
public:
    static Engine& instance();

    bool init(int sampleRate, int framesPerBurst);
    void shutdown();

    // Transport
    void play();
    void pause();
    void stop();
    void seekToFrame(int64_t frame);
    int64_t getPlayheadFrame() const { return playheadFrame_.load(); }
    void setBpm(float bpm) { bpm_ = bpm; }
    void setLooping(bool enabled, int64_t startFrame, int64_t endFrame);

    // Tracks
    void addTrack(const std::string& trackId);
    void removeTrack(const std::string& trackId);
    void setTrackGain(const std::string& trackId, float linearGain);
    void setTrackPan(const std::string& trackId, float pan);
    void setTrackMute(const std::string& trackId, bool muted);
    void setTrackSolo(const std::string& trackId, bool soloed);
    float getTrackLevel(const std::string& trackId);

    // Clips
    bool addClip(const std::string& trackId, const Clip& clip);
    void removeClip(const std::string& trackId, const std::string& clipId);
    void updateClipTiming(
        const std::string& trackId, const std::string& clipId,
        int64_t startFrameInSource, int64_t endFrameInSource, int64_t timelineStartFrame
    );

    // Effects
    void setTrackEffect(
        const std::string& trackId, const std::string& effectId, int effectType,
        bool enabled, const std::vector<std::string>& keys, const std::vector<float>& values
    );
    void removeTrackEffect(const std::string& trackId, const std::string& effectId);

    // Recording
    bool startRecording(const std::string& trackId, const std::string& outputPath);
    std::string stopRecording(const std::string& trackId);

    // Offline export - runs entirely off the realtime thread.
    bool exportMixdown(const std::string& outputPath, int sampleRate);

    // Sequencer / synth pass-through (see Sequencer.h / Synth.h for detail)
    Sequencer& sequencer() { return sequencer_; }
    Synth& synth() { return synth_; }

    // oboe::AudioStreamCallback
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* stream, void* audioData, int32_t numFrames
    ) override;
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

private:
    Engine() = default;

    /** Renders [numFrames] of the full mix (all tracks incl. sequencer/synth) into left/right. */
    void renderBlock(float* left, float* right, int numFrames, int64_t startFrame);
    void renderTrackInto(Track& track, float* left, float* right, int numFrames, int64_t startFrame);
    Effect* createEffect(int effectType);

    std::mutex graphMutex_;
    std::unordered_map<std::string, std::unique_ptr<Track>> tracks_;

    std::shared_ptr<oboe::AudioStream> outputStream_;
    std::shared_ptr<oboe::AudioStream> inputStream_;

    std::atomic<bool> isPlaying_{false};
    std::atomic<int64_t> playheadFrame_{0};
    float bpm_ = 120.0f;
    int sampleRate_ = 48000;

    bool loopEnabled_ = false;
    int64_t loopStart_ = 0;
    int64_t loopEnd_ = 0;

    Sequencer sequencer_;
    Synth synth_;

    // Scratch buffers reused every callback to avoid realtime allocation.
    // trackScratch* holds one track's rendered output before it's summed
    // into the block-level mix buffers (blockMix*) - these must stay
    // separate, since renderBlock() both writes into and reads out of them
    // for different purposes in the same pass.
    std::vector<float> trackScratchLeft_;
    std::vector<float> trackScratchRight_;
    std::vector<float> blockMixLeft_;
    std::vector<float> blockMixRight_;
};

} // namespace almus
