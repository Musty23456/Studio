package com.almus.studio

import com.almus.studio.model.AudioClip
import com.almus.studio.model.Project
import com.almus.studio.model.Track
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ProjectModelTest {

    @Test
    fun `new project has sensible defaults`() {
        val project = Project()
        assertEquals(120f, project.bpm)
        assertEquals(4, project.timeSignatureNumerator)
        assertEquals(48000, project.sampleRate)
        assertTrue(project.tracks.isEmpty())
    }

    @Test
    fun `clip length is computed from source frame bounds`() {
        val clip = AudioClip(
            sourceFilePath = "/fake/path.wav",
            startFrameInSource = 1000,
            endFrameInSource = 5000,
            timelineStartFrame = 0
        )
        assertEquals(4000, clip.lengthInFrames)
    }

    @Test
    fun `track defaults to unity gain and centered pan`() {
        val track = Track()
        assertEquals(1.0f, track.volume)
        assertEquals(0.0f, track.pan)
        assertTrue(!track.isMuted)
        assertTrue(!track.isSoloed)
    }

    @Test
    fun `splitting math matches expected clip boundaries`() {
        // Mirrors the split calculation in StudioViewModel.splitClip: the
        // clip is cut in-place at the split point and a new clip continues
        // from there, both referencing the same source file.
        val original = AudioClip(
            sourceFilePath = "/fake/path.wav",
            startFrameInSource = 0,
            endFrameInSource = 10000,
            timelineStartFrame = 0
        )
        val splitAtTimelineFrame = 4000L
        val offsetIntoClip = splitAtTimelineFrame - original.timelineStartFrame
        val splitSourceFrame = original.startFrameInSource + offsetIntoClip

        assertEquals(4000L, splitSourceFrame)
        assertTrue(offsetIntoClip in 1 until original.lengthInFrames)
    }
}
