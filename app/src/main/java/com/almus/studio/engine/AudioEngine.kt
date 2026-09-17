package com.almus.studio.engine

/**
 * Thin Kotlin wrapper around the native (C++/Oboe) audio engine. All calls
 * are lightweight JNI hops that enqueue work onto the real-time audio thread
 * inside native code - nothing here does audio processing itself, which is
 * the whole point of using Oboe: the JVM/GC never sits on the audio path.
 *
 * The native engine owns:
 *  - the Oboe playback + record streams
 *  - a per-track ring buffer / clip scheduler
 *  - the DSP effect chain (EQ, compressor, reverb, delay, chorus,
 *    distortion, limiter, pitch correction)
 *  - the offline mixdown/render path used for WAV export
 */
object AudioEngine {

    init {
        System.loadLibrary("almus_audio")
    }

    // ---- Engine lifecycle -------------------------------------------------

    external fun nativeInit(sampleRate: Int, framesPerBurst: Int): Boolean
    external fun nativeShutdown()

    // ---- Transport ----------------------------------------------------------

    external fun nativePlay()
    external fun nativePause()
    external fun nativeStop()
    external fun nativeSeekToFrame(frame: Long)
    external fun nativeGetPlayheadFrame(): Long
    external fun nativeSetBpm(bpm: Float)
    external fun nativeSetLooping(enabled: Boolean, startFrame: Long, endFrame: Long)

    // ---- Track / clip graph -------------------------------------------------

    external fun nativeAddTrack(trackId: String): Int
    external fun nativeRemoveTrack(trackId: String)
    external fun nativeSetTrackGain(trackId: String, linearGain: Float)
    external fun nativeSetTrackPan(trackId: String, pan: Float)
    external fun nativeSetTrackMute(trackId: String, muted: Boolean)
    external fun nativeSetTrackSolo(trackId: String, soloed: Boolean)

    external fun nativeAddClip(
        trackId: String,
        clipId: String,
        filePath: String,
        startFrameInSource: Long,
        endFrameInSource: Long,
        timelineStartFrame: Long,
        gainDb: Float,
        fadeInFrames: Long,
        fadeOutFrames: Long
    ): Boolean

    external fun nativeRemoveClip(trackId: String, clipId: String)

    external fun nativeUpdateClipTiming(
        trackId: String,
        clipId: String,
        startFrameInSource: Long,
        endFrameInSource: Long,
        timelineStartFrame: Long
    )

    // ---- Effects --------------------------------------------------------

    /** paramKeys/paramValues are parallel arrays (JNI has no map type). */
    external fun nativeSetTrackEffect(
        trackId: String,
        effectId: String,
        effectType: Int,
        enabled: Boolean,
        paramKeys: Array<String>,
        paramValues: FloatArray
    )

    external fun nativeRemoveTrackEffect(trackId: String, effectId: String)

    // ---- Recording --------------------------------------------------------

    external fun nativeStartRecording(trackId: String, outputWavPath: String): Boolean
    external fun nativeStopRecording(trackId: String): String? // returns final file path

    // ---- Waveform / metering ------------------------------------------------

    /** Returns a downsampled min/max peak envelope (interleaved min,max pairs) suitable for drawing. */
    external fun nativeGetWaveformPeaks(filePath: String, bucketCount: Int): FloatArray

    /** Returns the total sample-frame length of an audio file, or -1 on failure. */
    external fun nativeGetAudioFileLengthFrames(filePath: String): Long

    /** Returns per-track RMS level (0..1) for live meters, most-recent block. */
    external fun nativeGetTrackLevel(trackId: String): Float

    // ---- Drum machine / step sequencer -----------------------------------

    external fun nativeSequencerLoadSample(laneId: String, filePath: String): Boolean
    external fun nativeSequencerSetPattern(
        patternId: String,
        stepsPerBar: Int,
        bars: Int
    )
    external fun nativeSequencerSetStep(
        laneId: String,
        stepIndex: Int,
        active: Boolean,
        velocity: Float
    )
    external fun nativeSequencerStart()
    external fun nativeSequencerStop()

    // ---- Piano roll synth ------------------------------------------------

    external fun nativeSynthNoteOn(trackId: String, pitch: Int, velocity: Int, instrument: Int)
    external fun nativeSynthNoteOff(trackId: String, pitch: Int)

    // ---- Offline export -----------------------------------------------------

    /**
     * Renders the full project (all tracks, effects, volume/pan/mute/solo)
     * offline to a stereo 16-bit PCM WAV file at [outputPath]. This runs on a
     * background thread inside native code and does not use the realtime
     * audio stream, so it is not limited by wall-clock playback time.
     */
    external fun nativeExportMixdown(outputPath: String, sampleRate: Int): Boolean
}
