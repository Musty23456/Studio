package com.almus.studio.model

import kotlinx.serialization.Serializable
import java.util.UUID

/**
 * Root data model for an Almus Studio project. This is the single source of
 * truth that gets serialized to disk (as JSON) and reloaded on project open.
 * All audio data referenced by clips lives as separate WAV files inside the
 * project folder; only paths + edit metadata are stored here so files are
 * never duplicated on disk.
 */
@Serializable
data class Project(
    val id: String = UUID.randomUUID().toString(),
    var name: String = "Untitled Project",
    var bpm: Float = 120f,
    var timeSignatureNumerator: Int = 4,
    var timeSignatureDenominator: Int = 4,
    var sampleRate: Int = 48000,
    var masterVolume: Float = 1.0f,
    var tracks: MutableList<Track> = mutableListOf(),
    var patterns: MutableList<DrumPattern> = mutableListOf(),
    var pianoRollClips: MutableList<PianoRollClip> = mutableListOf(),
    var createdAtEpochMs: Long = System.currentTimeMillis(),
    var updatedAtEpochMs: Long = System.currentTimeMillis()
)

@Serializable
data class Track(
    val id: String = UUID.randomUUID().toString(),
    var name: String = "Track",
    var type: TrackType = TrackType.AUDIO,
    var clips: MutableList<AudioClip> = mutableListOf(),
    var volume: Float = 1.0f,       // linear gain 0.0 - 2.0
    var pan: Float = 0.0f,          // -1.0 (left) to 1.0 (right)
    var isMuted: Boolean = false,
    var isSoloed: Boolean = false,
    var isArmedForRecording: Boolean = false,
    var colorHex: String = "#7C4DFF",
    var effectChain: MutableList<EffectInstance> = mutableListOf()
)

enum class TrackType { AUDIO, DRUM, PIANO_ROLL }

/**
 * A single audio region placed on a track's timeline. [sourceFilePath] points
 * at the underlying WAV inside the project's `audio/` folder. Trim points are
 * expressed as frame offsets into that source file, so trimming/splitting
 * never mutates the underlying audio data - only these numbers change. This
 * is what makes undo/redo for edits cheap and lossless.
 */
@Serializable
data class AudioClip(
    val id: String = UUID.randomUUID().toString(),
    var sourceFilePath: String,
    var startFrameInSource: Long,     // first sample of the source file used
    var endFrameInSource: Long,       // exclusive
    var timelineStartFrame: Long,     // where this clip begins on the track timeline
    var gainDb: Float = 0f,
    var fadeInFrames: Long = 0,
    var fadeOutFrames: Long = 0,
    var name: String = "Clip"
) {
    val lengthInFrames: Long get() = endFrameInSource - startFrameInSource
}

@Serializable
data class EffectInstance(
    val id: String = UUID.randomUUID().toString(),
    var type: EffectType,
    var isEnabled: Boolean = true,
    // Generic float parameter bag so new effect types don't require schema
    // migrations; each EffectType documents its own expected keys.
    var params: MutableMap<String, Float> = mutableMapOf()
)

enum class EffectType {
    EQ_3BAND,
    COMPRESSOR,
    REVERB,
    DELAY,
    CHORUS,
    DISTORTION,
    LIMITER,
    PITCH_CORRECTION
}

/** A 16 (or n) step pattern for the drum machine, referencing sample slots. */
@Serializable
data class DrumPattern(
    val id: String = UUID.randomUUID().toString(),
    var name: String = "Pattern 1",
    var stepsPerBar: Int = 16,
    var bars: Int = 1,
    var lanes: MutableList<DrumLane> = mutableListOf()
)

@Serializable
data class DrumLane(
    val id: String = UUID.randomUUID().toString(),
    var samplePath: String,
    var name: String = "Sample",
    var steps: MutableList<Boolean> = MutableList(16) { false },
    var velocity: MutableList<Float> = MutableList(16) { 1.0f },
    var volume: Float = 1.0f
)

/** A simple monophonic/polyphonic note region for the piano-roll editor. */
@Serializable
data class PianoRollClip(
    val id: String = UUID.randomUUID().toString(),
    var trackId: String,
    var timelineStartFrame: Long,
    var notes: MutableList<MidiNote> = mutableListOf(),
    var instrument: SynthInstrument = SynthInstrument.SINE_LEAD
)

@Serializable
data class MidiNote(
    var pitch: Int,           // MIDI note number, 0-127
    var startTick: Int,       // ticks relative to clip start (960 ticks/quarter)
    var durationTicks: Int,
    var velocity: Int = 100
)

enum class SynthInstrument { SINE_LEAD, SQUARE_BASS, SAW_PAD, PLUCK }
