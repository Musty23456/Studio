package com.almus.studio.viewmodel

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.almus.studio.engine.AudioEngine
import com.almus.studio.model.AudioClip
import com.almus.studio.model.EffectInstance
import com.almus.studio.model.EffectType
import com.almus.studio.model.Project
import com.almus.studio.model.Track
import com.almus.studio.model.TrackType
import com.almus.studio.project.ProjectRepository
import com.almus.studio.project.ProjectSummary
import com.almus.studio.undo.CommandManager
import com.almus.studio.undo.LambdaCommand
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File

enum class TransportState { STOPPED, PLAYING, PAUSED, RECORDING }

data class StudioUiState(
    val project: Project? = null,
    val transport: TransportState = TransportState.STOPPED,
    val playheadFrame: Long = 0L,
    val selectedTrackId: String? = null,
    val selectedClipId: String? = null,
    val canUndo: Boolean = false,
    val canRedo: Boolean = false,
    val projectList: List<ProjectSummary> = emptyList(),
    val isExporting: Boolean = false,
    val exportResultPath: String? = null,
    val errorMessage: String? = null
)

class StudioViewModel(application: Application) : AndroidViewModel(application) {

    private val repository = ProjectRepository(application)
    private val commandManager = CommandManager()
    private var engineInitialized = false

    private val _uiState = MutableStateFlow(StudioUiState())
    val uiState: StateFlow<StudioUiState> = _uiState.asStateFlow()

    init {
        refreshProjectList()
    }

    // ---- Engine lifecycle ---------------------------------------------------

    private fun ensureEngine() {
        if (!engineInitialized) {
            engineInitialized = AudioEngine.nativeInit(48000, 192)
        }
    }

    override fun onCleared() {
        if (engineInitialized) AudioEngine.nativeShutdown()
        super.onCleared()
    }

    // ---- Project lifecycle ---------------------------------------------

    fun refreshProjectList() {
        _uiState.update { it.copy(projectList = repository.listProjects()) }
    }

    fun createNewProject(name: String) {
        ensureEngine()
        val project = Project(name = name.ifBlank { "Untitled Project" })
        repository.save(project)
        commandManager.clear()
        loadIntoEngine(project)
        _uiState.update {
            it.copy(project = project, canUndo = false, canRedo = false)
        }
        refreshProjectList()
    }

    fun openProject(projectId: String) {
        ensureEngine()
        val project = repository.load(projectId) ?: return
        commandManager.clear()
        loadIntoEngine(project)
        _uiState.update { it.copy(project = project, canUndo = false, canRedo = false) }
    }

    /** Pushes the full current track/clip/effect graph into the native engine. */
    private fun loadIntoEngine(project: Project) {
        project.tracks.forEach { track ->
            AudioEngine.nativeAddTrack(track.id)
            AudioEngine.nativeSetTrackGain(track.id, track.volume)
            AudioEngine.nativeSetTrackPan(track.id, track.pan)
            AudioEngine.nativeSetTrackMute(track.id, track.isMuted)
            AudioEngine.nativeSetTrackSolo(track.id, track.isSoloed)
            track.clips.forEach { clip -> pushClipToEngine(track.id, clip) }
            track.effectChain.forEach { effect -> pushEffectToEngine(track.id, effect) }
        }
        AudioEngine.nativeSetBpm(project.bpm)
    }

    private fun pushClipToEngine(trackId: String, clip: AudioClip) {
        AudioEngine.nativeAddClip(
            trackId, clip.id, clip.sourceFilePath,
            clip.startFrameInSource, clip.endFrameInSource,
            clip.timelineStartFrame, clip.gainDb,
            clip.fadeInFrames, clip.fadeOutFrames
        )
    }

    private fun pushEffectToEngine(trackId: String, effect: EffectInstance) {
        val keys = effect.params.keys.toTypedArray()
        val values = FloatArray(keys.size) { i -> effect.params[keys[i]] ?: 0f }
        AudioEngine.nativeSetTrackEffect(
            trackId, effect.id, effect.type.ordinal, effect.isEnabled, keys, values
        )
    }

    private fun persist() {
        _uiState.value.project?.let { repository.save(it) }
    }

    private fun mutateProject(label: String, mutate: (Project) -> Unit) {
        val current = _uiState.value.project ?: return
        val before = current.copy()
        val command = LambdaCommand(
            label = label,
            doAction = {
                mutate(current)
                persist()
            },
            undoAction = {
                _uiState.update { it.copy(project = before) }
                persist()
            }
        )
        commandManager.execute(command)
        _uiState.update {
            it.copy(
                project = current,
                canUndo = commandManager.canUndo,
                canRedo = commandManager.canRedo
            )
        }
    }

    fun undo() {
        if (commandManager.undo()) {
            _uiState.update {
                it.copy(canUndo = commandManager.canUndo, canRedo = commandManager.canRedo)
            }
        }
    }

    fun redo() {
        if (commandManager.redo()) {
            _uiState.update {
                it.copy(canUndo = commandManager.canUndo, canRedo = commandManager.canRedo)
            }
        }
    }

    // ---- Tracks -----------------------------------------------------------

    fun addTrack(type: TrackType = TrackType.AUDIO) {
        mutateProject("Add track") { project ->
            val track = Track(name = "Track ${project.tracks.size + 1}", type = type)
            project.tracks.add(track)
            AudioEngine.nativeAddTrack(track.id)
        }
    }

    fun removeTrack(trackId: String) {
        mutateProject("Remove track") { project ->
            project.tracks.removeAll { it.id == trackId }
            AudioEngine.nativeRemoveTrack(trackId)
        }
    }

    fun setTrackVolume(trackId: String, volume: Float) {
        mutateProject("Track volume") { project ->
            project.tracks.find { it.id == trackId }?.volume = volume
            AudioEngine.nativeSetTrackGain(trackId, volume)
        }
    }

    fun setTrackPan(trackId: String, pan: Float) {
        mutateProject("Track pan") { project ->
            project.tracks.find { it.id == trackId }?.pan = pan
            AudioEngine.nativeSetTrackPan(trackId, pan)
        }
    }

    fun toggleMute(trackId: String) {
        mutateProject("Toggle mute") { project ->
            val track = project.tracks.find { it.id == trackId } ?: return@mutateProject
            track.isMuted = !track.isMuted
            AudioEngine.nativeSetTrackMute(trackId, track.isMuted)
        }
    }

    fun toggleSolo(trackId: String) {
        mutateProject("Toggle solo") { project ->
            val track = project.tracks.find { it.id == trackId } ?: return@mutateProject
            track.isSoloed = !track.isSoloed
            AudioEngine.nativeSetTrackSolo(trackId, track.isSoloed)
        }
    }

    fun selectTrack(trackId: String?) {
        _uiState.update { it.copy(selectedTrackId = trackId) }
    }

    // ---- Clips: import, trim, split, move -----------------------------------

    fun importAudio(trackId: String, uri: Uri, displayName: String) {
        val project = _uiState.value.project ?: return
        val path = repository.importAudioFile(project.id, uri, displayName) ?: run {
            _uiState.update { it.copy(errorMessage = "Could not import $displayName") }
            return
        }
        val lengthFrames = AudioEngine.nativeGetAudioFileLengthFrames(path).coerceAtLeast(0L)
        mutateProject("Import audio") { proj ->
            val track = proj.tracks.find { it.id == trackId } ?: return@mutateProject
            val clip = AudioClip(
                sourceFilePath = path,
                startFrameInSource = 0L,
                endFrameInSource = lengthFrames,
                timelineStartFrame = 0L,
                name = displayName
            )
            track.clips.add(clip)
            pushClipToEngine(trackId, clip)
        }
    }

    fun moveClip(trackId: String, clipId: String, newTimelineStartFrame: Long) {
        mutateProject("Move clip") { project ->
            val clip = project.tracks.find { it.id == trackId }
                ?.clips?.find { it.id == clipId } ?: return@mutateProject
            clip.timelineStartFrame = newTimelineStartFrame
            AudioEngine.nativeUpdateClipTiming(
                trackId, clipId, clip.startFrameInSource, clip.endFrameInSource, clip.timelineStartFrame
            )
        }
    }

    fun trimClip(trackId: String, clipId: String, newStartInSource: Long, newEndInSource: Long) {
        mutateProject("Trim clip") { project ->
            val clip = project.tracks.find { it.id == trackId }
                ?.clips?.find { it.id == clipId } ?: return@mutateProject
            clip.startFrameInSource = newStartInSource
            clip.endFrameInSource = newEndInSource
            AudioEngine.nativeUpdateClipTiming(
                trackId, clipId, clip.startFrameInSource, clip.endFrameInSource, clip.timelineStartFrame
            )
        }
    }

    /** Splits a clip at [splitAtTimelineFrame] into two independent clips. */
    fun splitClip(trackId: String, clipId: String, splitAtTimelineFrame: Long) {
        mutateProject("Split clip") { project ->
            val track = project.tracks.find { it.id == trackId } ?: return@mutateProject
            val clip = track.clips.find { it.id == clipId } ?: return@mutateProject
            val offsetIntoClip = splitAtTimelineFrame - clip.timelineStartFrame
            if (offsetIntoClip <= 0 || offsetIntoClip >= clip.lengthInFrames) return@mutateProject

            val splitSourceFrame = clip.startFrameInSource + offsetIntoClip

            val secondHalf = clip.copy(
                id = java.util.UUID.randomUUID().toString(),
                startFrameInSource = splitSourceFrame,
                timelineStartFrame = splitAtTimelineFrame
            )
            clip.endFrameInSource = splitSourceFrame

            track.clips.add(secondHalf)
            AudioEngine.nativeUpdateClipTiming(
                trackId, clip.id, clip.startFrameInSource, clip.endFrameInSource, clip.timelineStartFrame
            )
            pushClipToEngine(trackId, secondHalf)
        }
    }

    fun deleteClip(trackId: String, clipId: String) {
        mutateProject("Delete clip") { project ->
            project.tracks.find { it.id == trackId }?.clips?.removeAll { it.id == clipId }
            AudioEngine.nativeRemoveClip(trackId, clipId)
        }
    }

    // ---- Effects ------------------------------------------------------------

    fun addEffect(trackId: String, type: EffectType) {
        mutateProject("Add effect") { project ->
            val track = project.tracks.find { it.id == trackId } ?: return@mutateProject
            val effect = EffectInstance(type = type, params = defaultParamsFor(type))
            track.effectChain.add(effect)
            pushEffectToEngine(trackId, effect)
        }
    }

    fun updateEffectParam(trackId: String, effectId: String, key: String, value: Float) {
        mutateProject("Edit effect") { project ->
            val track = project.tracks.find { it.id == trackId } ?: return@mutateProject
            val effect = track.effectChain.find { it.id == effectId } ?: return@mutateProject
            effect.params[key] = value
            pushEffectToEngine(trackId, effect)
        }
    }

    fun toggleEffect(trackId: String, effectId: String) {
        mutateProject("Toggle effect") { project ->
            val track = project.tracks.find { it.id == trackId } ?: return@mutateProject
            val effect = track.effectChain.find { it.id == effectId } ?: return@mutateProject
            effect.isEnabled = !effect.isEnabled
            pushEffectToEngine(trackId, effect)
        }
    }

    fun removeEffect(trackId: String, effectId: String) {
        mutateProject("Remove effect") { project ->
            val track = project.tracks.find { it.id == trackId } ?: return@mutateProject
            track.effectChain.removeAll { it.id == effectId }
            AudioEngine.nativeRemoveTrackEffect(trackId, effectId)
        }
    }

    private fun defaultParamsFor(type: EffectType): MutableMap<String, Float> = when (type) {
        EffectType.EQ_3BAND -> mutableMapOf("lowGainDb" to 0f, "midGainDb" to 0f, "highGainDb" to 0f)
        EffectType.COMPRESSOR -> mutableMapOf(
            "thresholdDb" to -18f, "ratio" to 4f, "attackMs" to 10f, "releaseMs" to 120f, "makeupDb" to 0f
        )
        EffectType.REVERB -> mutableMapOf("roomSize" to 0.5f, "damping" to 0.5f, "wetMix" to 0.3f)
        EffectType.DELAY -> mutableMapOf("delayMs" to 350f, "feedback" to 0.35f, "wetMix" to 0.3f)
        EffectType.CHORUS -> mutableMapOf("rateHz" to 1.2f, "depthMs" to 6f, "wetMix" to 0.4f)
        EffectType.DISTORTION -> mutableMapOf("driveDb" to 12f, "tone" to 0.5f, "mix" to 1.0f)
        EffectType.LIMITER -> mutableMapOf("ceilingDb" to -0.3f, "releaseMs" to 80f)
        EffectType.PITCH_CORRECTION -> mutableMapOf(
            "keyRootNote" to 0f, // 0 = C
            "scaleType" to 0f,   // 0 = major/minor chromatic snap
            "retuneSpeedMs" to 30f,
            "amount" to 1.0f
        )
    }

    // ---- Transport ------------------------------------------------------

    fun play() {
        ensureEngine()
        AudioEngine.nativePlay()
        _uiState.update { it.copy(transport = TransportState.PLAYING) }
        watchPlayhead()
    }

    fun pause() {
        AudioEngine.nativePause()
        _uiState.update { it.copy(transport = TransportState.PAUSED) }
    }

    fun stop() {
        AudioEngine.nativeStop()
        _uiState.update { it.copy(transport = TransportState.STOPPED, playheadFrame = 0L) }
    }

    fun seekTo(frame: Long) {
        AudioEngine.nativeSeekToFrame(frame)
        _uiState.update { it.copy(playheadFrame = frame) }
    }

    fun setBpm(bpm: Float) {
        mutateProject("Set BPM") { project ->
            project.bpm = bpm
            AudioEngine.nativeSetBpm(bpm)
        }
    }

    private fun watchPlayhead() {
        viewModelScope.launch(Dispatchers.Default) {
            while (isActive && _uiState.value.transport == TransportState.PLAYING) {
                val frame = AudioEngine.nativeGetPlayheadFrame()
                _uiState.update { it.copy(playheadFrame = frame) }
                kotlinx.coroutines.delay(33) // ~30fps UI refresh
            }
        }
    }

    // ---- Recording ------------------------------------------------------

    fun startRecording(trackId: String) {
        val project = _uiState.value.project ?: return
        val track = project.tracks.find { it.id == trackId } ?: return
        val file = repository.newRecordingFile(project.id, track.name)
        ensureEngine()
        if (AudioEngine.nativeStartRecording(trackId, file.absolutePath)) {
            _uiState.update { it.copy(transport = TransportState.RECORDING) }
        }
    }

    fun stopRecording(trackId: String) {
        val resultPath = AudioEngine.nativeStopRecording(trackId)
        _uiState.update { it.copy(transport = TransportState.STOPPED) }
        if (resultPath != null) {
            val file = File(resultPath)
            if (file.exists()) {
                val lengthFrames = AudioEngine.nativeGetAudioFileLengthFrames(resultPath).coerceAtLeast(0L)
                mutateProject("Record audio") { project ->
                    val track = project.tracks.find { it.id == trackId } ?: return@mutateProject
                    val clip = AudioClip(
                        sourceFilePath = resultPath,
                        startFrameInSource = 0L,
                        endFrameInSource = lengthFrames,
                        timelineStartFrame = AudioEngine.nativeGetPlayheadFrame(),
                        name = file.name
                    )
                    track.clips.add(clip)
                    pushClipToEngine(trackId, clip)
                }
            }
        }
    }

    // ---- Export ---------------------------------------------------------

    fun exportMixdown() {
        val project = _uiState.value.project ?: return
        _uiState.update { it.copy(isExporting = true, exportResultPath = null) }
        viewModelScope.launch(Dispatchers.IO) {
            val outFile = repository.newRenderFile(project.id, project.name)
            val success = AudioEngine.nativeExportMixdown(outFile.absolutePath, project.sampleRate)
            _uiState.update {
                it.copy(
                    isExporting = false,
                    exportResultPath = if (success) outFile.absolutePath else null,
                    errorMessage = if (!success) "Export failed" else null
                )
            }
        }
    }

    fun clearError() {
        _uiState.update { it.copy(errorMessage = null) }
    }
}
