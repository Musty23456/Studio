#include <jni.h>
#include <string>
#include <vector>
#include "Engine.h"
#include "WavFile.h"

using almus::Engine;
using almus::Clip;

namespace {

std::string jstringToStdString(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

jstring stdStringToJstring(JNIEnv* env, const std::string& str) {
    return env->NewStringUTF(str.c_str());
}

std::vector<std::string> jstringArrayToVector(JNIEnv* env, jobjectArray array) {
    std::vector<std::string> result;
    if (!array) return result;
    const jsize length = env->GetArrayLength(array);
    result.reserve(length);
    for (jsize i = 0; i < length; i++) {
        auto jstr = static_cast<jstring>(env->GetObjectArrayElement(array, i));
        result.push_back(jstringToStdString(env, jstr));
        env->DeleteLocalRef(jstr);
    }
    return result;
}

std::vector<float> jfloatArrayToVector(JNIEnv* env, jfloatArray array) {
    std::vector<float> result;
    if (!array) return result;
    const jsize length = env->GetArrayLength(array);
    result.resize(length);
    env->GetFloatArrayRegion(array, 0, length, result.data());
    return result;
}

jfloatArray vectorToJfloatArray(JNIEnv* env, const std::vector<float>& values) {
    jfloatArray array = env->NewFloatArray(static_cast<jsize>(values.size()));
    if (!values.empty()) {
        env->SetFloatArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    }
    return array;
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeInit(JNIEnv*, jobject, jint sampleRate, jint framesPerBurst) {
    return Engine::instance().init(sampleRate, framesPerBurst);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeShutdown(JNIEnv*, jobject) {
    Engine::instance().shutdown();
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativePlay(JNIEnv*, jobject) { Engine::instance().play(); }

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativePause(JNIEnv*, jobject) { Engine::instance().pause(); }

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeStop(JNIEnv*, jobject) { Engine::instance().stop(); }

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSeekToFrame(JNIEnv*, jobject, jlong frame) {
    Engine::instance().seekToFrame(frame);
}

JNIEXPORT jlong JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeGetPlayheadFrame(JNIEnv*, jobject) {
    return Engine::instance().getPlayheadFrame();
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSetBpm(JNIEnv*, jobject, jfloat bpm) {
    Engine::instance().setBpm(bpm);
    Engine::instance().sequencer().setBpm(bpm);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSetLooping(
    JNIEnv*, jobject, jboolean enabled, jlong startFrame, jlong endFrame
) {
    Engine::instance().setLooping(enabled, startFrame, endFrame);
}

JNIEXPORT jint JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeAddTrack(JNIEnv* env, jobject, jstring trackId) {
    Engine::instance().addTrack(jstringToStdString(env, trackId));
    return 0;
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeRemoveTrack(JNIEnv* env, jobject, jstring trackId) {
    Engine::instance().removeTrack(jstringToStdString(env, trackId));
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSetTrackGain(JNIEnv* env, jobject, jstring trackId, jfloat gain) {
    Engine::instance().setTrackGain(jstringToStdString(env, trackId), gain);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSetTrackPan(JNIEnv* env, jobject, jstring trackId, jfloat pan) {
    Engine::instance().setTrackPan(jstringToStdString(env, trackId), pan);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSetTrackMute(JNIEnv* env, jobject, jstring trackId, jboolean muted) {
    Engine::instance().setTrackMute(jstringToStdString(env, trackId), muted);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSetTrackSolo(JNIEnv* env, jobject, jstring trackId, jboolean soloed) {
    Engine::instance().setTrackSolo(jstringToStdString(env, trackId), soloed);
}

JNIEXPORT jboolean JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeAddClip(
    JNIEnv* env, jobject, jstring trackId, jstring clipId, jstring filePath,
    jlong startFrameInSource, jlong endFrameInSource, jlong timelineStartFrame,
    jfloat gainDb, jlong fadeInFrames, jlong fadeOutFrames
) {
    Clip clip;
    clip.id = jstringToStdString(env, clipId);
    clip.filePath = jstringToStdString(env, filePath);
    clip.startFrameInSource = startFrameInSource;
    clip.endFrameInSource = endFrameInSource;
    clip.timelineStartFrame = timelineStartFrame;
    clip.gainDb = gainDb;
    clip.fadeInFrames = fadeInFrames;
    clip.fadeOutFrames = fadeOutFrames;
    return Engine::instance().addClip(jstringToStdString(env, trackId), clip);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeRemoveClip(JNIEnv* env, jobject, jstring trackId, jstring clipId) {
    Engine::instance().removeClip(jstringToStdString(env, trackId), jstringToStdString(env, clipId));
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeUpdateClipTiming(
    JNIEnv* env, jobject, jstring trackId, jstring clipId,
    jlong startFrameInSource, jlong endFrameInSource, jlong timelineStartFrame
) {
    Engine::instance().updateClipTiming(
        jstringToStdString(env, trackId), jstringToStdString(env, clipId),
        startFrameInSource, endFrameInSource, timelineStartFrame
    );
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSetTrackEffect(
    JNIEnv* env, jobject, jstring trackId, jstring effectId, jint effectType, jboolean enabled,
    jobjectArray paramKeys, jfloatArray paramValues
) {
    Engine::instance().setTrackEffect(
        jstringToStdString(env, trackId), jstringToStdString(env, effectId), effectType, enabled,
        jstringArrayToVector(env, paramKeys), jfloatArrayToVector(env, paramValues)
    );
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeRemoveTrackEffect(JNIEnv* env, jobject, jstring trackId, jstring effectId) {
    Engine::instance().removeTrackEffect(jstringToStdString(env, trackId), jstringToStdString(env, effectId));
}

JNIEXPORT jboolean JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeStartRecording(JNIEnv* env, jobject, jstring trackId, jstring outputWavPath) {
    return Engine::instance().startRecording(jstringToStdString(env, trackId), jstringToStdString(env, outputWavPath));
}

JNIEXPORT jstring JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeStopRecording(JNIEnv* env, jobject, jstring trackId) {
    std::string path = Engine::instance().stopRecording(jstringToStdString(env, trackId));
    if (path.empty()) return nullptr;
    return stdStringToJstring(env, path);
}

JNIEXPORT jfloatArray JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeGetWaveformPeaks(JNIEnv* env, jobject, jstring filePath, jint bucketCount) {
    auto buffer = almus::AudioFileCache::instance().load(jstringToStdString(env, filePath));
    if (!buffer || buffer->numFrames <= 0 || bucketCount <= 0) {
        return vectorToJfloatArray(env, {});
    }
    std::vector<float> peaks(static_cast<size_t>(bucketCount) * 2);
    const int64_t framesPerBucket = std::max<int64_t>(1, buffer->numFrames / bucketCount);
    for (int b = 0; b < bucketCount; b++) {
        const int64_t start = static_cast<int64_t>(b) * framesPerBucket;
        const int64_t end = std::min<int64_t>(buffer->numFrames, start + framesPerBucket);
        float minVal = 0.0f, maxVal = 0.0f;
        for (int64_t i = start; i < end; i++) {
            const float l = buffer->left[i];
            if (l < minVal) minVal = l;
            if (l > maxVal) maxVal = l;
        }
        peaks[b * 2] = minVal;
        peaks[b * 2 + 1] = maxVal;
    }
    return vectorToJfloatArray(env, peaks);
}

JNIEXPORT jlong JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeGetAudioFileLengthFrames(JNIEnv* env, jobject, jstring filePath) {
    auto buffer = almus::AudioFileCache::instance().load(jstringToStdString(env, filePath));
    if (!buffer) return -1;
    return buffer->numFrames;
}

JNIEXPORT jfloat JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeGetTrackLevel(JNIEnv* env, jobject, jstring trackId) {
    return Engine::instance().getTrackLevel(jstringToStdString(env, trackId));
}

JNIEXPORT jboolean JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSequencerLoadSample(JNIEnv* env, jobject, jstring laneId, jstring filePath) {
    Engine::instance().sequencer().loadSample(jstringToStdString(env, laneId), jstringToStdString(env, filePath));
    return true;
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSequencerSetPattern(
    JNIEnv* env, jobject, jstring patternId, jint stepsPerBar, jint bars
) {
    Engine::instance().sequencer().setPattern(jstringToStdString(env, patternId), stepsPerBar, bars);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSequencerSetStep(
    JNIEnv* env, jobject, jstring laneId, jint stepIndex, jboolean active, jfloat velocity
) {
    Engine::instance().sequencer().setStep(jstringToStdString(env, laneId), stepIndex, active, velocity);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSequencerStart(JNIEnv*, jobject) {
    Engine::instance().sequencer().start();
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSequencerStop(JNIEnv*, jobject) {
    Engine::instance().sequencer().stop();
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSynthNoteOn(
    JNIEnv* env, jobject, jstring trackId, jint pitch, jint velocity, jint instrument
) {
    Engine::instance().synth().noteOn(jstringToStdString(env, trackId), pitch, velocity, instrument);
}

JNIEXPORT void JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeSynthNoteOff(JNIEnv* env, jobject, jstring trackId, jint pitch) {
    Engine::instance().synth().noteOff(jstringToStdString(env, trackId), pitch);
}

JNIEXPORT jboolean JNICALL
Java_com_almus_studio_engine_AudioEngine_nativeExportMixdown(JNIEnv* env, jobject, jstring outputPath, jint sampleRate) {
    return Engine::instance().exportMixdown(jstringToStdString(env, outputPath), sampleRate);
}

} // extern "C"
