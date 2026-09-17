package com.almus.studio.project

import android.content.Context
import android.net.Uri
import com.almus.studio.model.Project
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.io.File

/**
 * Handles fully-offline local persistence of [Project] objects.
 *
 * Layout on disk (all under app-private storage, no network involved):
 *   /projects/<projectId>/project.json
 *   /projects/<projectId>/audio/<file>.wav      (imported + recorded sources)
 *   /projects/<projectId>/render/<file>.wav     (exported mixdowns)
 *   /projects/<projectId>/samples/<file>.wav    (drum machine samples, copied in)
 */
class ProjectRepository(private val context: Context) {

    private val json = Json {
        prettyPrint = true
        ignoreUnknownKeys = true
        encodeDefaults = true
    }

    private val rootDir: File
        get() = File(context.filesDir, "projects").apply { mkdirs() }

    fun projectDir(projectId: String): File =
        File(rootDir, projectId).apply { mkdirs() }

    fun audioDir(projectId: String): File =
        File(projectDir(projectId), "audio").apply { mkdirs() }

    fun renderDir(projectId: String): File =
        File(projectDir(projectId), "render").apply { mkdirs() }

    fun samplesDir(projectId: String): File =
        File(projectDir(projectId), "samples").apply { mkdirs() }

    fun listProjects(): List<ProjectSummary> {
        val dir = rootDir
        val result = mutableListOf<ProjectSummary>()
        dir.listFiles { f -> f.isDirectory }?.forEach { folder ->
            val jsonFile = File(folder, "project.json")
            if (jsonFile.exists()) {
                runCatching {
                    val project = json.decodeFromString<Project>(jsonFile.readText())
                    result.add(
                        ProjectSummary(
                            id = project.id,
                            name = project.name,
                            updatedAtEpochMs = project.updatedAtEpochMs,
                            trackCount = project.tracks.size
                        )
                    )
                }
            }
        }
        return result.sortedByDescending { it.updatedAtEpochMs }
    }

    fun save(project: Project) {
        val updated = project.copy(updatedAtEpochMs = System.currentTimeMillis())
        val file = File(projectDir(project.id), "project.json")
        file.writeText(json.encodeToString(updated))
    }

    fun load(projectId: String): Project? {
        val file = File(projectDir(projectId), "project.json")
        if (!file.exists()) return null
        return runCatching { json.decodeFromString<Project>(file.readText()) }.getOrNull()
    }

    fun delete(projectId: String) {
        projectDir(projectId).deleteRecursively()
    }

    /**
     * Copies an audio file picked from device storage (via SAF Uri) into the
     * project's audio folder so the project remains self-contained and does
     * not break if the user moves/deletes the original file.
     * Returns the absolute path of the copy, or null on failure.
     */
    fun importAudioFile(projectId: String, sourceUri: Uri, displayName: String): String? {
        return runCatching {
            val safeName = displayName.replace(Regex("[^A-Za-z0-9._-]"), "_")
            val destFile = File(audioDir(projectId), "${System.currentTimeMillis()}_$safeName")
            context.contentResolver.openInputStream(sourceUri)?.use { input ->
                destFile.outputStream().use { output -> input.copyTo(output) }
            }
            destFile.absolutePath
        }.getOrNull()
    }

    fun newRecordingFile(projectId: String, trackName: String): File {
        val safeName = trackName.replace(Regex("[^A-Za-z0-9._-]"), "_")
        return File(audioDir(projectId), "rec_${safeName}_${System.currentTimeMillis()}.wav")
    }

    fun newRenderFile(projectId: String, projectName: String): File {
        val safeName = projectName.replace(Regex("[^A-Za-z0-9._-]"), "_")
        return File(renderDir(projectId), "${safeName}_mixdown_${System.currentTimeMillis()}.wav")
    }
}

data class ProjectSummary(
    val id: String,
    val name: String,
    val updatedAtEpochMs: Long,
    val trackCount: Int
)
