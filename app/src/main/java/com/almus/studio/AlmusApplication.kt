package com.almus.studio

import android.app.Application

class AlmusApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // Intentionally no network / analytics / crash-reporting SDKs:
        // Almus Studio is designed to run fully offline.
    }
}
