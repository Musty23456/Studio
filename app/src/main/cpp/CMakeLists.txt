cmake_minimum_required(VERSION 3.22.1)
project(almus_audio)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Oboe is pulled in via the Gradle "com.google.oboe:oboe" AAR using Prefab,
# which exposes a CMake package config automatically to externalNativeBuild.
find_package(oboe REQUIRED CONFIG)

add_library(almus_audio SHARED
        native-lib.cpp
        Engine.cpp
        WavFile.cpp
        RecordingWriter.cpp
        Sequencer.cpp
        Synth.cpp
        dsp/Biquad.cpp
        dsp/EqualizerEffect.cpp
        dsp/CompressorEffect.cpp
        dsp/ReverbEffect.cpp
        dsp/DelayEffect.cpp
        dsp/ChorusEffect.cpp
        dsp/DistortionEffect.cpp
        dsp/LimiterEffect.cpp
        dsp/PitchCorrectionEffect.cpp
)

target_include_directories(almus_audio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(almus_audio
        oboe::oboe
        ${log-lib}
        ${android-lib}
)

target_compile_options(almus_audio PRIVATE -Wall -Wextra -Wno-unused-parameter)
