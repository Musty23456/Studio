package com.almus.studio.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.almus.studio.model.EffectInstance
import com.almus.studio.model.EffectType
import com.almus.studio.ui.theme.AlmusSurface
import com.almus.studio.ui.theme.AlmusSurfaceAlt
import com.almus.studio.ui.theme.AlmusTextSecondary
import com.almus.studio.viewmodel.StudioViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MixerScreen(viewModel: StudioViewModel, onBack: () -> Unit) {
    val state by viewModel.uiState.collectAsState()
    val project = state.project
    var selectedTrackId by remember(project) { mutableStateOf(project?.tracks?.firstOrNull()?.id) }
    var showAddEffectFor by remember { mutableStateOf<String?>(null) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Mixer", fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = onBack) { Icon(Icons.Filled.ArrowBack, contentDescription = "Back") }
                }
            )
        }
    ) { padding ->
        if (project == null) return@Scaffold
        Row(Modifier.padding(padding).fillMaxSize()) {
            // Track selector strip
            LazyColumn(
                Modifier.width(120.dp).fillMaxHeight().background(AlmusSurface),
                contentPadding = PaddingValues(8.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                items(project.tracks, key = { it.id }) { track ->
                    val selected = selectedTrackId == track.id
                    Surface(
                        color = if (selected) AlmusSurfaceAlt else AlmusSurface,
                        shape = RoundedCornerShape(8.dp),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            track.name,
                            modifier = Modifier
                                .padding(10.dp)
                                .clip(RoundedCornerShape(8.dp)),
                            style = MaterialTheme.typography.bodyMedium,
                            color = if (selected) MaterialTheme.colorScheme.primary else AlmusTextSecondary,
                        )
                    }
                    Spacer(Modifier.height(0.dp))
                }
            }

            val track = project.tracks.find { it.id == selectedTrackId }
            Column(Modifier.weight(1f).padding(12.dp)) {
                if (track == null) {
                    Text("No track selected", color = AlmusTextSecondary)
                } else {
                    Text("Effect chain — ${track.name}", style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(8.dp))
                    LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.weight(1f)) {
                        items(track.effectChain, key = { it.id }) { effect ->
                            EffectCard(
                                effect = effect,
                                onToggle = { viewModel.toggleEffect(track.id, effect.id) },
                                onRemove = { viewModel.removeEffect(track.id, effect.id) },
                                onParamChange = { key, value ->
                                    viewModel.updateEffectParam(track.id, effect.id, key, value)
                                }
                            )
                        }
                    }
                    Spacer(Modifier.height(8.dp))
                    Button(onClick = { showAddEffectFor = track.id }) {
                        Text("Add effect")
                    }
                }
            }
        }
    }

    if (showAddEffectFor != null) {
        val trackId = showAddEffectFor!!
        AlertDialog(
            onDismissRequest = { showAddEffectFor = null },
            containerColor = AlmusSurfaceAlt,
            title = { Text("Add effect") },
            text = {
                Column {
                    EffectType.entries.forEach { type ->
                        TextButton(onClick = {
                            viewModel.addEffect(trackId, type)
                            showAddEffectFor = null
                        }) {
                            Text(type.displayName())
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showAddEffectFor = null }) { Text("Close") }
            }
        )
    }
}

@Composable
private fun EffectCard(
    effect: EffectInstance,
    onToggle: () -> Unit,
    onRemove: () -> Unit,
    onParamChange: (String, Float) -> Unit
) {
    Surface(color = AlmusSurface, shape = RoundedCornerShape(10.dp), modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(effect.type.displayName(), style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Switch(checked = effect.isEnabled, onCheckedChange = { onToggle() })
                    IconButton(onClick = onRemove) {
                        Icon(Icons.Filled.Delete, contentDescription = "Remove effect", modifier = Modifier.size(18.dp))
                    }
                }
            }
            effect.params.keys.sorted().forEach { key ->
                val value = effect.params[key] ?: 0f
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        key,
                        modifier = Modifier.width(110.dp),
                        style = MaterialTheme.typography.labelSmall,
                        color = AlmusTextSecondary
                    )
                    Slider(
                        value = value,
                        onValueChange = { onParamChange(key, it) },
                        valueRange = rangeFor(key),
                        modifier = Modifier.weight(1f)
                    )
                    Text(
                        "%.2f".format(value),
                        style = MaterialTheme.typography.labelSmall,
                        modifier = Modifier.width(48.dp)
                    )
                }
            }
        }
    }
}

private fun rangeFor(paramKey: String): ClosedFloatingPointRange<Float> = when (paramKey) {
    "lowGainDb", "midGainDb", "highGainDb" -> -24f..24f
    "thresholdDb" -> -60f..0f
    "ratio" -> 1f..20f
    "attackMs" -> 0.5f..200f
    "releaseMs" -> 5f..1000f
    "makeupDb" -> 0f..24f
    "roomSize", "damping", "wetMix", "mix", "tone", "amount" -> 0f..1f
    "delayMs" -> 10f..2000f
    "feedback" -> 0f..0.95f
    "rateHz" -> 0.05f..8f
    "depthMs" -> 0f..20f
    "driveDb" -> 0f..36f
    "ceilingDb" -> -12f..0f
    "keyRootNote" -> 0f..11f
    "scaleType" -> 0f..1f
    "retuneSpeedMs" -> 0f..300f
    else -> 0f..1f
}

private fun EffectType.displayName(): String = when (this) {
    EffectType.EQ_3BAND -> "3-Band EQ"
    EffectType.COMPRESSOR -> "Compressor"
    EffectType.REVERB -> "Reverb"
    EffectType.DELAY -> "Delay"
    EffectType.CHORUS -> "Chorus"
    EffectType.DISTORTION -> "Distortion"
    EffectType.LIMITER -> "Limiter"
    EffectType.PITCH_CORRECTION -> "Pitch Correction (Auto-Tune style)"
}
