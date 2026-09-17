package com.almus.studio

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.runtime.Composable
import androidx.core.content.ContextCompat
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.almus.studio.ui.screens.DrumMachineScreen
import com.almus.studio.ui.screens.EditorScreen
import com.almus.studio.ui.screens.HomeScreen
import com.almus.studio.ui.screens.MixerScreen
import com.almus.studio.ui.screens.PianoRollScreen
import com.almus.studio.ui.theme.AlmusStudioTheme
import com.almus.studio.viewmodel.StudioViewModel

class MainActivity : ComponentActivity() {

    private val viewModel: StudioViewModel by viewModels()

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { /* handled reactively via hasRecordPermission() at record time */ }

    fun hasRecordPermission(): Boolean =
        ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) ==
            PackageManager.PERMISSION_GRANTED

    fun requestRecordPermission() {
        permissionLauncher.launch(Manifest.permission.RECORD_AUDIO)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            AlmusStudioTheme {
                AlmusApp(viewModel = viewModel, activity = this)
            }
        }
    }
}

@Composable
fun AlmusApp(viewModel: StudioViewModel, activity: MainActivity) {
    val navController = rememberNavController()
    NavHost(navController = navController, startDestination = "home") {
        composable("home") {
            HomeScreen(
                viewModel = viewModel,
                onOpenProject = { navController.navigate("editor") }
            )
        }
        composable("editor") {
            EditorScreen(
                viewModel = viewModel,
                activity = activity,
                onOpenMixer = { navController.navigate("mixer") },
                onOpenDrumMachine = { navController.navigate("drums") },
                onOpenPianoRoll = { navController.navigate("piano") },
                onBack = { navController.popBackStack() }
            )
        }
        composable("mixer") {
            MixerScreen(viewModel = viewModel, onBack = { navController.popBackStack() })
        }
        composable("drums") {
            DrumMachineScreen(viewModel = viewModel, onBack = { navController.popBackStack() })
        }
        composable("piano") {
            PianoRollScreen(viewModel = viewModel, onBack = { navController.popBackStack() })
        }
    }
}
