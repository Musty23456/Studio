package com.almus.studio.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable

// Almus Studio ships a single professional dark theme (a light theme would
// hurt contrast against waveforms and meters), regardless of system setting.
private val AlmusColorScheme = darkColorScheme(
    primary = AlmusAccent,
    secondary = AlmusAccentVariant,
    background = AlmusBackground,
    surface = AlmusSurface,
    surfaceVariant = AlmusSurfaceAlt,
    onPrimary = AlmusTextPrimary,
    onBackground = AlmusTextPrimary,
    onSurface = AlmusTextPrimary,
    error = AlmusDanger
)

@Composable
fun AlmusStudioTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = AlmusColorScheme,
        typography = AlmusTypography,
        content = content
    )
}
