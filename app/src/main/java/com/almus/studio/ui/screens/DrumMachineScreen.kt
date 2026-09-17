package com.almus.studio.ui.screens

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.almus.studio.engine.AudioEngine
import com.almus.studio.model.DrumLane
import com.almus.studio.model.DrumPattern
import com.almus.studio.model.Project
import com.almus.studio.project.ProjectRepository
import com.almus.studio.ui.theme.AlmusAccent
import com.almus.studio.ui.theme.AlmusSurface
import com.almus.studio.ui.theme.AlmusSurfaceAlt
import com.almus.studio.ui.theme.AlmusTextSecondary
import com.almus.studio.viewmodel.StudioViewModel

/**
 * A 16-step drum machine. Each lane triggers one sample. This screen edits
 * [Project.patterns] directly through the repository since patterns are a
 * project-level (not per-track) concept; playback is driven by the native
 * sequencer engine which schedules samples sample-accurately against BPM.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DrumMachineScreen(viewModel: StudioViewModel, onBack: () -> Unit) {
    val state by viewModel.uiState.collectAsState()
    val project = state.project
    val appContext = androidx.compose.ui.platform.LocalContext.current.applicationContext
    var isPlaying by remember { mutableStateOf(false) }
    var pattern by remember(project) {
        mutableStateOf(project?.patterns?.firstOrNull() ?: DrumPattern())
    }

    val samplePicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        if (uri != null && project != null) {
            val repo = ProjectRepository(appContext)
            val path = repo.importAudioFile(project.id, uri, uri.lastPathSegment ?: "sample.wav")
            if (path != null) {
                val lane = DrumLane(samplePath = path, name = uri.lastPathSegment ?: "Sample")
                AudioEngine.nativeSequencerLoadSample(lane.id, path)
                pattern = pattern.copy(lanes = (pattern.lanes + lane).toMutableList())
            }
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Drum Machine", fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = onBack) { Icon(Icons.Filled.ArrowBack, contentDescription = "Back") }
                },
                actions = {
                    IconButton(onClick = {
                        isPlaying = !isPlaying
                        if (isPlaying) {
                            AudioEngine.nativeSequencerSetPattern(pattern.id, pattern.stepsPerBar, pattern.bars)
                            AudioEngine.nativeSequencerStart()
                        } else {
                            AudioEngine.nativeSequencerStop()
                        }
                    }) {
                        Icon(if (isPlaying) Icons.Filled.Stop else Icons.Filled.PlayArrow, contentDescription = "Play pattern")
                    }
                }
            )
        }
    ) { padding ->
        Column(Modifier.padding(padding).fillMaxSize().padding(12.dp)) {
            Text(
                "Tap steps to program a beat. Real sample playback via the native sequencer engine.",
                style = MaterialTheme.typography.labelSmall,
                color = AlmusTextSecondary
            )
            Spacer(Modifier.height(12.dp))
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.weight(1f)) {
                items(pattern.lanes, key = { it.id }) { lane ->
                    DrumLaneRow(
                        lane = lane,
                        onStepToggle = { stepIndex ->
                            val newSteps = lane.steps.toMutableList()
                            newSteps[stepIndex] = !newSteps[stepIndex]
                            AudioEngine.nativeSequencerSetStep(lane.id, stepIndex, newSteps[stepIndex], lane.velocity.getOrElse(stepIndex) { 1f })
                            val updatedLane = lane.copy(steps = newSteps)
                            pattern = pattern.copy(
                                lanes = pattern.lanes.map { if (it.id == lane.id) updatedLane else it }.toMutableList()
                            )
                        }
                    )
                }
            }
            OutlinedButton(onClick = { samplePicker.launch(arrayOf("audio/*")) }) {
                Icon(Icons.Filled.Add, contentDescription = null, modifier = Modifier.size(18.dp))
                Spacer(Modifier.width(4.dp))
                Text("Add sample lane")
            }
        }
    }
}

@Composable
private fun DrumLaneRow(lane: DrumLane, onStepToggle: (Int) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(
            lane.name,
            modifier = Modifier.width(90.dp),
            style = MaterialTheme.typography.labelSmall,
            maxLines = 1
        )
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            lane.steps.forEachIndexed { index, active ->
                val emphasize = index % 4 == 0
                Box(
                    Modifier
                        .size(26.dp)
                        .clip(RoundedCornerShape(4.dp))
                        .background(
                            when {
                                active -> AlmusAccent
                                emphasize -> AlmusSurfaceAlt
                                else -> AlmusSurface
                            }
                        )
                        .clickable { onStepToggle(index) }
                )
            }
        }
    }
}
