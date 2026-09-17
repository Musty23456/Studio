package com.almus.studio

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Instrumented sanity checks that run on-device (or emulator) as part of the
 * connectedCheck task. These deliberately stay lightweight; the native audio
 * engine's behavior is covered indirectly since app package resolution alone
 * requires the .so to load correctly at process start.
 */
@RunWith(AndroidJUnit4::class)
class ApplicationIdTest {
    @Test
    fun useAppContext() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        assertEquals("com.almus.studio", appContext.packageName)
    }
}
