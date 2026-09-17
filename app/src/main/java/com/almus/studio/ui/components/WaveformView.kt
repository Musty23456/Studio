package com.almus.studio.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap

/**
 * Renders a min/max peak-pair waveform. [peaks] is expected as an
 * interleaved [min0, max0, min1, max1, ...] array in the range -1..1,
 * as produced by AudioEngine.nativeGetWaveformPeaks.
 */
@Composable
fun WaveformView(
    peaks: FloatArray,
    color: Color,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.fillMaxSize()) {
        if (peaks.isEmpty()) return@Canvas
        val bucketCount = peaks.size / 2
        if (bucketCount <= 0) return@Canvas
        val midY = size.height / 2f
        val xStep = size.width / bucketCount
        for (i in 0 until bucketCount) {
            val min = peaks[i * 2]
            val max = peaks[i * 2 + 1]
            val x = i * xStep
            val yTop = midY - (max.coerceIn(-1f, 1f) * midY)
            val yBottom = midY - (min.coerceIn(-1f, 1f) * midY)
            drawLine(
                color = color,
                start = Offset(x, yTop),
                end = Offset(x, yBottom),
                strokeWidth = xStep.coerceAtLeast(1.2f),
                cap = StrokeCap.Round
            )
        }
    }
}
