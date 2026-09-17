package com.almus.studio.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.almus.studio.engine.AudioEngine
import com.almus.studio.model.MidiNote
import com.almus.studio.model.PianoRollClip
import com.almus.studio.ui.theme.AlmusAccent
import com.almus.studio.ui.theme.AlmusBackground
import com.almus.studio.ui.theme.AlmusSurface
import com.almus.studio.ui.theme.AlmusSurfaceAlt
import com.almus.studio.ui.theme.AlmusTextSecondary
import com.almus.studio.viewmodel.StudioViewModel

private const val TICKS_PER_QUARTER = 960
private const val VISIBLE_PITCH_LOW = 48  // C3
private const val VISIBLE_PITCH_HIGH = 84 // C6
private val NOTE_HEIGHT = 18.dp

/**
 * A functional, simplified piano-roll: tap a cell to add/remove a
 * sixteenth-note-long note for the selected instrument, played back through
 * the native engine's built-in synth voices (sine/square/saw/pluck). This
 * covers the "MIDI-style note editor" requirement without pretending to be a
 * full DAW-grade piano roll (no note-length dragging or velocity lanes yet).
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PianoRollScreen(viewModel: StudioViewModel, onBack: () -> Unit) {
    val state by viewModel.uiState.collectAsState()
    val project = state.project
    var clip by remember(project) {
        mutableStateOf(
            project?.pianoRollClips?.firstOrNull()
                ?: PianoRollClip(
                    trackId = project?.tracks?.firstOrNull {
                        it.type == com.almus.studio.model.TrackType.PIANO_ROLL
                    }?.id ?: ""
                )
        )
    }
    val stepsPerBar = 16
    val barsShown = 2

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Piano Roll", fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = onBack) { Icon(Icons.Filled.ArrowBack, contentDescription = "Back") }
                }
            )
        }
    ) { padding ->
        Column(Modifier.padding(padding).fillMaxSize()) {
            Text(
                "Tap a cell to place a note. Basic synth voices; not a full audio-file instrument sampler.",
                style = MaterialTheme.typography.labelSmall,
                color = AlmusTextSecondary,
                modifier = Modifier.padding(8.dp)
            )
            val pitches = (VISIBLE_PITCH_HIGH downTo VISIBLE_PITCH_LOW).toList()
            val stepCount = stepsPerBar * barsShown
            val cellWidth = 32.dp
            val vScroll = rememberScrollState()
            val hScroll = rememberScrollState()

            Row(Modifier.weight(1f)) {
                // Pitch labels
                Column(Modifier.width(52.dp).verticalScroll(vScroll)) {
                    pitches.forEach { pitch ->
                        Box(
                            Modifier.height(NOTE_HEIGHT).fillMaxWidth().background(AlmusSurface),
                            contentAlignment = androidx.compose.ui.Alignment.CenterEnd
                        ) {
                            Text(noteName(pitch), style = MaterialTheme.typography.labelSmall, color = AlmusTextSecondary)
                        }
                    }
                }
                // Grid
                Box(
                    Modifier
                        .weight(1f)
                        .verticalScroll(vScroll)
                        .horizontalScroll(hScroll)
                ) {
                    Column {
                        pitches.forEach { pitch ->
                            Row {
                                for (step in 0 until stepCount) {
                                    val tickStart = step * (TICKS_PER_QUARTER / 4)
                                    val hasNote = clip.notes.any {
                                        it.pitch == pitch && it.startTick == tickStart
                                    }
                                    val emphasize = step % 4 == 0
                                    Box(
                                        Modifier
                                            .size(cellWidth, NOTE_HEIGHT)
                                            .background(
                                                when {
                                                    hasNote -> AlmusAccent
                                                    emphasize -> AlmusSurfaceAlt
                                                    else -> AlmusBackground
                                                }
                                            )
                                            .pointerInput(pitch, step, hasNote) {
                                                detectTapGestures {
                                                    val newNotes = clip.notes.toMutableList()
                                                    if (hasNote) {
                                                        newNotes.removeAll { it.pitch == pitch && it.startTick == tickStart }
                                                    } else {
                                                        newNotes.add(
                                                            MidiNote(
                                                                pitch = pitch,
                                                                startTick = tickStart,
                                                                durationTicks = TICKS_PER_QUARTER / 4,
                                                                velocity = 100
                                                            )
                                                        )
                                                        val trackId = clip.trackId
                                                        if (trackId.isNotBlank()) {
                                                            AudioEngine.nativeSynthNoteOn(trackId, pitch, 100, clip.instrument.ordinal)
                                                        }
                                                    }
                                                    clip = clip.copy(notes = newNotes)
                                                }
                                            }
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

private fun noteName(midiPitch: Int): String {
    val names = listOf("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    val octave = midiPitch / 12 - 1
    return "${names[midiPitch % 12]}$octave"
}
