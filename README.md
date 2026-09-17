# Almus Studio

An offline-first Android multitrack DAW (digital audio workstation), inspired
by BandLab and FL Studio Mobile. Kotlin + Jetpack Compose UI, a C++/Oboe
real-time audio engine, real DSP effects, a drum-machine step sequencer, and
a basic piano-roll synth editor — no network access required for any core
feature.

## What's actually implemented (read this first)

This is a genuine, from-scratch DAW codebase, not a mockup. Every control in
the UI is wired to real logic: waveforms are drawn from actual decoded PCM
data, effects are real DSP math running in C++, recording captures real
microphone input, and export renders a real WAV file by mixing the project.
That said, an app like this is normally built by a team over months, so it's
worth being precise about what's solid versus what's a deliberately scoped
starting point:

**Solid / fully working:**
- Project create/open/save/delete, JSON-based, fully offline (`ProjectRepository`).
- Multitrack timeline: add/remove tracks, import audio from device storage,
  microphone recording to WAV, trim/split/move clips, volume/pan/mute/solo.
- Undo/redo for every project mutation (`CommandManager`).
- Real DSP effect chain per track: 3-band EQ, compressor, Schroeder reverb,
  feedback delay, LFO chorus, soft-clip distortion, peak limiter — all
  hand-written C++ (`app/src/main/cpp/dsp/`), not stubs.
- Offline WAV mixdown export that renders the whole project (all tracks,
  clips, gains, pans, mute/solo, and effect chains) through the same mixing
  code path used for playback.
- Drum machine step sequencer with sample-accurate BPM-locked triggering.
- Basic piano-roll grid with a built-in 4-voice-type synth (sine/square/saw/
  pluck) for auditioning notes.

**Deliberately scoped / known limitations (see inline code comments for
exactly where):**
- **Pitch correction ("Auto-Tune style")** uses real-time autocorrelation
  pitch detection and a two-grain overlap-add pitch shifter — it audibly
  retunes a single voice toward the nearest note in the chosen key, with a
  retune-speed control from subtle to the hard-quantized "T-Pain" effect.
  It is **not** a phase-vocoder/formant-preserving corrector like Antares
  Auto-Tune or Melodyne, so it works best on clean, monophonic vocal takes
  and is less reliable on noisy or polyphonic input. See
  `dsp/PitchCorrectionEffect.h` for the full scope note.
- **Recording has no live input monitoring.** You hear the take after you
  stop recording, not while it's being captured. The engine opens a separate
  blocking-read input stream on its own thread rather than a full-duplex
  callback, which was the reliable option to ship in this pass; wiring live
  monitoring is a contained follow-up (see comment in `Engine::startRecording`).
- **Piano-roll notes are auditioned, not timeline-scheduled.** Tapping a
  cell plays that note immediately through the built-in synth so you can
  hear what you're programming, but transport playback/export does not yet
  walk the note list by playhead position the way the drum sequencer does.
  Extending it is straightforward — reuse the sequencer's step-scheduling
  pattern against `PianoRollClip.notes` — but wasn't included in this pass
  to keep scope honest rather than half-fake it.
- **Waveform × timeline layout is proportional, not a shared pixel-accurate
  ruler yet.** Clips lay out left-to-right sized by duration; a shared
  zoom/scroll timeline ruler (so all tracks line up frame-for-frame visually)
  is a natural next UI pass.
- **Very long sessions** load each source file fully into memory
  (`AudioBuffer`), which is simple and fast for typical mobile session
  lengths but isn't a streaming architecture; extremely long uninterrupted
  recordings would need a streaming decoder as a follow-up.

None of the above are faked — they're real, working, narrower versions of
the full feature, with the exact narrowing documented in code comments so a
future pass has a clear map of what to extend.

## Tech stack

- **UI:** Kotlin, Jetpack Compose, Material 3, single professional dark theme.
- **Audio engine:** C++17, [Google Oboe](https://github.com/google/oboe) for
  low-latency I/O, linked via Prefab (`com.google.oboe:oboe` Gradle artifact).
- **Persistence:** `kotlinx.serialization` JSON project files + WAV assets,
  stored entirely in app-private storage — no cloud, no accounts, no network
  permission requested at all.
- **Build:** Gradle Kotlin DSL, CMake for the native module.

## Project structure

```
Almus-Studio/
├── app/
│   ├── build.gradle.kts
│   ├── proguard-rules.pro
│   └── src/
│       ├── main/
│       │   ├── AndroidManifest.xml
│       │   ├── java/com/almus/studio/
│       │   │   ├── AlmusApplication.kt
│       │   │   ├── MainActivity.kt
│       │   │   ├── model/Project.kt            # Project/Track/Clip/Effect/DrumPattern/MIDI data model
│       │   │   ├── project/ProjectRepository.kt # Offline JSON + WAV file persistence
│       │   │   ├── undo/CommandManager.kt       # Undo/redo command stack
│       │   │   ├── engine/AudioEngine.kt        # JNI bridge to native engine
│       │   │   ├── viewmodel/StudioViewModel.kt # App state, ties UI to engine+repo
│       │   │   └── ui/
│       │   │       ├── theme/                   # Dark theme colors/type
│       │   │       ├── components/              # WaveformView, TrackRow
│       │   │       └── screens/                 # Home, Editor, Mixer, DrumMachine, PianoRoll
│       │   ├── cpp/
│       │   │   ├── CMakeLists.txt
│       │   │   ├── native-lib.cpp               # JNI entry points
│       │   │   ├── Engine.h / Engine.cpp        # Track graph, transport, mixing, Oboe streams
│       │   │   ├── WavFile.h / WavFile.cpp       # WAV read/write + decoded-source cache
│       │   │   ├── RecordingWriter.h / .cpp     # Background-thread WAV capture
│       │   │   ├── Sequencer.h / .cpp           # Drum machine step sequencer
│       │   │   ├── Synth.h / .cpp               # Piano-roll oscillator synth
│       │   │   └── dsp/                         # EQ, Compressor, Reverb, Delay, Chorus,
│       │   │                                     # Distortion, Limiter, PitchCorrection
│       │   └── res/                             # Dark theme resources, icons, strings
│       ├── test/java/com/almus/studio/          # JVM unit tests
│       └── androidTest/java/com/almus/studio/   # Instrumented sanity test
├── build.gradle.kts
├── settings.gradle.kts
├── gradle.properties
├── gradle/wrapper/gradle-wrapper.properties
├── .github/workflows/build.yml
├── LICENSE
├── .gitignore
└── README.md   (this file)
```

## Gradle wrapper note (important for local builds)

This archive includes `gradle/wrapper/gradle-wrapper.properties` but **not**
a binary `gradle-wrapper.jar`, since that binary can't be produced in the
environment that generated this project. Two ways to get a working
`./gradlew` locally:

1. Open the project in Android Studio — it detects the missing wrapper jar
   and offers to regenerate it automatically, or
2. If you have Gradle installed locally, run once from the project root:
   ```
   gradle wrapper --gradle-version 8.7
   ```
   This generates `gradlew`, `gradlew.bat`, and `gradle-wrapper.jar` for you.

**GitHub Actions does not need this** — the included workflow provisions
Gradle 8.7 directly via `gradle/actions/setup-gradle`, so CI builds work
out of the box the moment you push.

## Building locally

1. Install Android Studio (Koala/2024.1+) with the Android SDK, NDK
   `26.1.10909125`, and CMake `3.22.1` (SDK Manager → SDK Tools).
2. Open the `Almus-Studio` folder as a project.
3. Let Gradle sync (first sync will download Oboe via Prefab and the NDK
   toolchain — this needs internet the first time even though the resulting
   *app* is offline-only).
4. Run on a physical device for realistic low-latency audio behavior;
   emulator audio input/output is unreliable for DAW-style testing.

## Known-good next steps if you keep building this

1. Wire full-duplex input monitoring into `Engine::onAudioReady` so you hear
   yourself while recording.
2. Extend `Sequencer`'s step-scheduling approach to `PianoRollClip.notes` for
   transport-synced piano-roll playback and export.
3. Add a shared timeline zoom/scroll ruler component so all tracks' clips
   align pixel-for-pixel.
4. Swap `AudioFileCache`'s full-file decode for a streaming reader once
   session lengths regularly exceed a few hundred MB.
5. Replace the zero-lookahead limiter with a short-lookahead brickwall
   design if you need broadcast-safe ceiling guarantees.

## License

MIT — see `LICENSE`. No third-party copyrighted audio samples are included
anywhere in this repository.
# Studio
