package com.almus.studio.ui.screens

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.almus.studio.MainActivity
import com.almus.studio.model.TrackType
import com.almus.studio.ui.components.TrackRow
import com.almus.studio.ui.theme.AlmusAccent
import com.almus.studio.ui.theme.AlmusSurface
import com.almus.studio.ui.theme.AlmusTextSecondary
import com.almus.studio.viewmodel.StudioViewModel
import com.almus.studio.viewmodel.TransportState

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EditorScreen(
    viewModel: StudioViewModel,
    activity: MainActivity,
    onOpenMixer: () -> Unit,
    onOpenDrumMachine: () -> Unit,
    onOpenPianoRoll: () -> Unit,
    onBack: () -> Unit
) {
    val state by viewModel.uiState.collectAsState()
    val project = state.project
    var importTargetTrackId by remember { mutableStateOf<String?>(null) }

    val filePicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        val trackId = importTargetTrackId
        if (uri != null && trackId != null) {
            activity.contentResolver.takePersistableUriPermission(
                uri, android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
            val name = uri.lastPathSegment ?: "import.wav"
            viewModel.importAudio(trackId, uri, name)
        }
        importTargetTrackId = null
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(project?.name ?: "Editor", fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = { viewModel.undo() }, enabled = state.canUndo) {
                        Icon(Icons.AutoMirrored.Filled.Undo, contentDescription = "Undo")
                    }
                    IconButton(onClick = { viewModel.redo() }, enabled = state.canRedo) {
                        Icon(Icons.AutoMirrored.Filled.Redo, contentDescription = "Redo")
                    }
                    IconButton(onClick = onOpenMixer) {
                        Icon(Icons.Filled.Tune, contentDescription = "Mixer")
                    }
                    IconButton(onClick = onOpenDrumMachine) {
                        Icon(Icons.Filled.GridView, contentDescription = "Drum machine")
                    }
                    IconButton(onClick = onOpenPianoRoll) {
                        Icon(Icons.Filled.Piano, contentDescription = "Piano roll")
                    }
                }
            )
        },
        bottomBar = {
            TransportBar(
                state = state,
                onPlay = { viewModel.play() },
                onPause = { viewModel.pause() },
                onStop = { viewModel.stop() },
                onBpmChange = { viewModel.setBpm(it) },
                onExport = { viewModel.exportMixdown() }
            )
        }
    ) { padding ->
        Column(Modifier.padding(padding).fillMaxSize()) {
            if (project == null) {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator()
                }
                return@Column
            }

            LazyColumn(
                Modifier.weight(1f).fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(6.dp),
                contentPadding = PaddingValues(8.dp)
            ) {
                items(project.tracks, key = { it.id }) { track ->
                    TrackRow(
                        track = track,
                        isSelected = state.selectedTrackId == track.id,
                        isRecording = state.transport == TransportState.RECORDING &&
                            state.selectedTrackId == track.id,
                        onSelect = { viewModel.selectTrack(track.id) },
                        onVolumeChange = { viewModel.setTrackVolume(track.id, it) },
                        onPanChange = { viewModel.setTrackPan(track.id, it) },
                        onToggleMute = { viewModel.toggleMute(track.id) },
                        onToggleSolo = { viewModel.toggleSolo(track.id) },
                        onImportAudio = {
                            importTargetTrackId = track.id
                            filePicker.launch(arrayOf("audio/*"))
                        },
                        onRecord = {
                            if (!activity.hasRecordPermission()) {
                                activity.requestRecordPermission()
                            } else if (state.transport == TransportState.RECORDING) {
                                viewModel.stopRecording(track.id)
                            } else {
                                viewModel.selectTrack(track.id)
                                viewModel.startRecording(track.id)
                            }
                        },
                        onDeleteTrack = { viewModel.removeTrack(track.id) },
                        onSplitClip = { clipId, frame -> viewModel.splitClip(track.id, clipId, frame) },
                        onDeleteClip = { clipId -> viewModel.deleteClip(track.id, clipId) },
                        onMoveClip = { clipId, frame -> viewModel.moveClip(track.id, clipId, frame) }
                    )
                }
                item {
                    Row(Modifier.fillMaxWidth().padding(vertical = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedButton(onClick = { viewModel.addTrack(TrackType.AUDIO) }) {
                            Icon(Icons.Filled.Add, contentDescription = null, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(4.dp))
                            Text("Audio track")
                        }
                        OutlinedButton(onClick = { viewModel.addTrack(TrackType.DRUM) }) {
                            Text("Drum track")
                        }
                        OutlinedButton(onClick = { viewModel.addTrack(TrackType.PIANO_ROLL) }) {
                            Text("MIDI track")
                        }
                    }
                }
            }

            state.exportResultPath?.let { path ->
                Surface(color = AlmusSurface, modifier = Modifier.fillMaxWidth()) {
                    Text(
                        "Exported to: $path",
                        modifier = Modifier.padding(12.dp),
                        color = AlmusAccent,
                        style = MaterialTheme.typography.labelSmall
                    )
                }
            }
        }
    }
}

@Composable
private fun TransportBar(
    state: com.almus.studio.viewmodel.StudioUiState,
    onPlay: () -> Unit,
    onPause: () -> Unit,
    onStop: () -> Unit,
    onBpmChange: (Float) -> Unit,
    onExport: () -> Unit
) {
    Surface(color = AlmusSurface, tonalElevation = 4.dp) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 8.dp).horizontalScroll(rememberScrollState()),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            IconButton(onClick = onStop) {
                Icon(Icons.Filled.Stop, contentDescription = "Stop")
            }
            if (state.transport == TransportState.PLAYING) {
                FilledIconButton(onClick = onPause) {
                    Icon(Icons.Filled.Pause, contentDescription = "Pause")
                }
            } else {
                FilledIconButton(onClick = onPlay) {
                    Icon(Icons.Filled.PlayArrow, contentDescription = "Play")
                }
            }
            Text(
                text = "Frame ${state.playheadFrame}",
                color = AlmusTextSecondary,
                style = MaterialTheme.typography.labelSmall
            )
            Spacer(Modifier.width(8.dp))
            Text("BPM", color = AlmusTextSecondary, style = MaterialTheme.typography.labelSmall)
            Slider(
                value = state.project?.bpm ?: 120f,
                onValueChange = onBpmChange,
                valueRange = 40f..220f,
                modifier = Modifier.width(140.dp)
            )
            Text(
                text = "${(state.project?.bpm ?: 120f).toInt()}",
                color = AlmusAccent,
                style = MaterialTheme.typography.labelSmall
            )
            Spacer(Modifier.weight(1f))
            Button(onClick = onExport, enabled = !state.isExporting) {
                if (state.isExporting) {
                    CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
                    Spacer(Modifier.width(6.dp))
                }
                Text(if (state.isExporting) "Exporting…" else "Export WAV")
            }
        }
    }
}
