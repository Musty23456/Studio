package com.almus.studio.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.MusicNote
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.almus.studio.ui.theme.AlmusAccent
import com.almus.studio.ui.theme.AlmusSurface
import com.almus.studio.ui.theme.AlmusSurfaceAlt
import com.almus.studio.ui.theme.AlmusTextSecondary
import com.almus.studio.viewmodel.StudioViewModel
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun HomeScreen(viewModel: StudioViewModel, onOpenProject: () -> Unit) {
    val state by viewModel.uiState.collectAsState()
    var showNewProjectDialog by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) { viewModel.refreshProjectList() }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Almus Studio", fontWeight = FontWeight.Bold) },
                actions = {
                    IconButton(onClick = { showNewProjectDialog = true }) {
                        Icon(Icons.Filled.Add, contentDescription = "New project")
                    }
                }
            )
        }
    ) { padding ->
        Column(Modifier.padding(padding).fillMaxSize().padding(16.dp)) {
            Text(
                "Offline multitrack recording, mixing and beat making.",
                color = AlmusTextSecondary,
                style = MaterialTheme.typography.bodyMedium
            )
            Spacer(Modifier.height(16.dp))

            if (state.projectList.isEmpty()) {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Icon(
                            Icons.Filled.MusicNote,
                            contentDescription = null,
                            tint = AlmusAccent,
                            modifier = Modifier.size(56.dp)
                        )
                        Spacer(Modifier.height(12.dp))
                        Text("No projects yet", color = AlmusTextSecondary)
                        Spacer(Modifier.height(12.dp))
                        Button(onClick = { showNewProjectDialog = true }) {
                            Text("Create your first project")
                        }
                    }
                }
            } else {
                LazyColumn(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    items(state.projectList) { summary ->
                        val dateStr = remember(summary.updatedAtEpochMs) {
                            SimpleDateFormat("MMM d, HH:mm", Locale.getDefault())
                                .format(Date(summary.updatedAtEpochMs))
                        }
                        Row(
                            Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(12.dp))
                                .background(AlmusSurface)
                                .clickable {
                                    viewModel.openProject(summary.id)
                                    onOpenProject()
                                }
                                .padding(16.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Column {
                                Text(summary.name, style = MaterialTheme.typography.titleMedium)
                                Spacer(Modifier.height(4.dp))
                                Text(
                                    "${summary.trackCount} tracks · edited $dateStr",
                                    style = MaterialTheme.typography.labelSmall,
                                    color = AlmusTextSecondary
                                )
                            }
                            Icon(Icons.Filled.Delete, contentDescription = null, tint = AlmusTextSecondary)
                        }
                    }
                }
            }
        }
    }

    if (showNewProjectDialog) {
        var name by remember { mutableStateOf("") }
        AlertDialog(
            onDismissRequest = { showNewProjectDialog = false },
            containerColor = AlmusSurfaceAlt,
            title = { Text("New project") },
            text = {
                OutlinedTextField(
                    value = name,
                    onValueChange = { name = it },
                    label = { Text("Project name") },
                    singleLine = true
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    viewModel.createNewProject(name)
                    showNewProjectDialog = false
                    onOpenProject()
                }) { Text("Create") }
            },
            dismissButton = {
                TextButton(onClick = { showNewProjectDialog = false }) { Text("Cancel") }
            }
        )
    }
}
