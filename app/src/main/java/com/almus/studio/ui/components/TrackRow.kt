package com.almus.studio.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.almus.studio.engine.AudioEngine
import com.almus.studio.model.AudioClip
import com.almus.studio.model.Track
import com.almus.studio.ui.theme.*

@Composable
fun TrackRow(
    track: Track,
    isSelected: Boolean,
    isRecording: Boolean,
    onSelect: () -> Unit,
    onVolumeChange: (Float) -> Unit,
    onPanChange: (Float) -> Unit,
    onToggleMute: () -> Unit,
    onToggleSolo: () -> Unit,
    onImportAudio: () -> Unit,
    onRecord: () -> Unit,
    onDeleteTrack: () -> Unit,
    onSplitClip: (clipId: String, timelineFrame: Long) -> Unit,
    onDeleteClip: (clipId: String) -> Unit,
    onMoveClip: (clipId: String, newTimelineFrame: Long) -> Unit
) {
    var expandedClipId by remember { mutableStateOf<String?>(null) }
    val trackColor = remember(track.colorHex) {
        runCatching { Color(android.graphics.Color.parseColor(track.colorHex)) }.getOrDefault(AlmusAccent)
    }

    Row(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(if (isSelected) AlmusSurfaceAlt else AlmusSurface)
            .clickable { onSelect() }
            .padding(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // ---- Track header / controls ----
        Column(Modifier.width(140.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    Modifier.size(8.dp).clip(RoundedCornerShape(4.dp)).background(trackColor)
                )
                Spacer(Modifier.width(6.dp))
                Text(
                    track.name,
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.Medium,
                    maxLines = 1
                )
            }
            Spacer(Modifier.height(4.dp))
            Row {
                IconToggleButton(checked = track.isMuted, onCheckedChange = { onToggleMute() }) {
                    Icon(
                        Icons.AutoMirrored.Filled.VolumeOff,
                        contentDescription = "Mute",
                        tint = if (track.isMuted) AlmusDanger else AlmusTextSecondary,
                        modifier = Modifier.size(18.dp)
                    )
                }
                IconToggleButton(checked = track.isSoloed, onCheckedChange = { onToggleSolo() }) {
                    Text(
                        "S",
                        color = if (track.isSoloed) AlmusAccentVariant else AlmusTextSecondary,
                        fontWeight = FontWeight.Bold
                    )
                }
                IconButton(onClick = onImportAudio, modifier = Modifier.size(32.dp)) {
                    Icon(Icons.Filled.FileUpload, contentDescription = "Import audio", modifier = Modifier.size(16.dp))
                }
                IconButton(onClick = onRecord, modifier = Modifier.size(32.dp)) {
                    Icon(
                        Icons.Filled.FiberManualRecord,
                        contentDescription = "Record",
                        tint = if (isRecording) AlmusDanger else AlmusTextSecondary,
                        modifier = Modifier.size(16.dp)
                    )
                }
                IconButton(onClick = onDeleteTrack, modifier = Modifier.size(32.dp)) {
                    Icon(Icons.Filled.Delete, contentDescription = "Delete track", modifier = Modifier.size(16.dp))
                }
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Vol", style = MaterialTheme.typography.labelSmall, color = AlmusTextSecondary)
                Slider(
                    value = track.volume,
                    onValueChange = onVolumeChange,
                    valueRange = 0f..2f,
                    modifier = Modifier.width(80.dp).height(20.dp)
                )
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Pan", style = MaterialTheme.typography.labelSmall, color = AlmusTextSecondary)
                Slider(
                    value = track.pan,
                    onValueChange = onPanChange,
                    valueRange = -1f..1f,
                    modifier = Modifier.width(80.dp).height(20.dp)
                )
            }
        }

        Spacer(Modifier.width(8.dp))

        // ---- Clip timeline lane ----
        Box(
            Modifier
                .weight(1f)
                .height(84.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(AlmusBackground)
        ) {
            // A simple proportional lay-out: each clip's width is proportional to
            // its length in frames, placed left-to-right in track.clips order.
            // (A full pixel-accurate ruler is driven by the DAW-wide horizontal
            // zoom/scroll state, wired the same way once a shared timeline
            // scale is introduced.)
            Row(Modifier.fillMaxSize()) {
                track.clips.sortedBy { it.timelineStartFrame }.forEach { clip ->
                    ClipView(
                        clip = clip,
                        color = trackColor,
                        isExpanded = expandedClipId == clip.id,
                        onTap = { expandedClipId = if (expandedClipId == clip.id) null else clip.id },
                        onSplit = {
                            val mid = clip.timelineStartFrame + clip.lengthInFrames / 2
                            onSplitClip(clip.id, mid)
                            expandedClipId = null
                        },
                        onDelete = {
                            onDeleteClip(clip.id)
                            expandedClipId = null
                        }
                    )
                }
                if (track.clips.isEmpty()) {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Text(
                            "No clips — import or record audio",
                            style = MaterialTheme.typography.labelSmall,
                            color = AlmusTextSecondary
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ClipView(
    clip: AudioClip,
    color: Color,
    isExpanded: Boolean,
    onTap: () -> Unit,
    onSplit: () -> Unit,
    onDelete: () -> Unit
) {
    var peaks by remember(clip.sourceFilePath) { mutableStateOf(FloatArray(0)) }
    LaunchedEffect(clip.sourceFilePath, clip.startFrameInSource, clip.endFrameInSource) {
        peaks = runCatching {
            AudioEngine.nativeGetWaveformPeaks(clip.sourceFilePath, 80)
        }.getOrDefault(FloatArray(0))
    }

    Column(
        Modifier
            .width(160.dp)
            .fillMaxHeight()
            .padding(2.dp)
            .clip(RoundedCornerShape(6.dp))
            .background(color.copy(alpha = 0.22f))
            .clickable { onTap() }
            .padding(4.dp)
    ) {
        Text(
            clip.name,
            style = MaterialTheme.typography.labelSmall,
            maxLines = 1,
            color = AlmusTextPrimary
        )
        WaveformView(
            peaks = peaks,
            color = color,
            modifier = Modifier.weight(1f).fillMaxWidth()
        )
        if (isExpanded) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                TextButton(onClick = onSplit, contentPadding = PaddingValues(2.dp)) {
                    Text("Split", style = MaterialTheme.typography.labelSmall)
                }
                TextButton(onClick = onDelete, contentPadding = PaddingValues(2.dp)) {
                    Text("Delete", style = MaterialTheme.typography.labelSmall, color = AlmusDanger)
                }
            }
        }
    }
}
