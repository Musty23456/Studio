#include "WavFile.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace almus {

namespace {

#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    uint32_t chunkSize;
    char wave[4];
};
struct ChunkHeader {
    char id[4];
    uint32_t size;
};
struct FmtChunk {
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

} // namespace

bool readWavFile(const std::string& path, AudioBuffer& outBuffer) {
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) return false;

    WavHeader header{};
    if (fread(&header, sizeof(header), 1, file) != 1 ||
        strncmp(header.riff, "RIFF", 4) != 0 ||
        strncmp(header.wave, "WAVE", 4) != 0) {
        fclose(file);
        return false;
    }

    FmtChunk fmt{};
    bool haveFmt = false;
    std::vector<uint8_t> dataBytes;

    ChunkHeader chunk{};
    while (fread(&chunk, sizeof(chunk), 1, file) == 1) {
        if (strncmp(chunk.id, "fmt ", 4) == 0) {
            fread(&fmt, std::min<uint32_t>(sizeof(fmt), chunk.size), 1, file);
            if (chunk.size > sizeof(fmt)) {
                fseek(file, static_cast<long>(chunk.size - sizeof(fmt)), SEEK_CUR);
            }
            haveFmt = true;
        } else if (strncmp(chunk.id, "data", 4) == 0) {
            dataBytes.resize(chunk.size);
            if (chunk.size > 0) {
                size_t read = fread(dataBytes.data(), 1, chunk.size, file);
                dataBytes.resize(read);
            }
        } else {
            // Skip unknown chunks (LIST, fact, etc.)
            fseek(file, static_cast<long>(chunk.size), SEEK_CUR);
        }
        if (chunk.size % 2 == 1) fseek(file, 1, SEEK_CUR); // chunks are word-aligned
    }
    fclose(file);

    if (!haveFmt || dataBytes.empty()) return false;

    outBuffer.sampleRate = static_cast<int>(fmt.sampleRate);
    outBuffer.numChannels = fmt.numChannels;

    const int bytesPerSample = fmt.bitsPerSample / 8;
    if (bytesPerSample <= 0) return false;
    const int64_t totalSamples = static_cast<int64_t>(dataBytes.size()) / bytesPerSample;
    const int64_t numFrames = totalSamples / std::max(1, static_cast<int>(fmt.numChannels));

    outBuffer.numFrames = numFrames;
    outBuffer.left.resize(numFrames);
    outBuffer.right.resize(numFrames);

    auto sampleToFloat = [&](const uint8_t* p) -> float {
        if (fmt.audioFormat == 3 && fmt.bitsPerSample == 32) {
            float v;
            memcpy(&v, p, 4);
            return v;
        }
        switch (fmt.bitsPerSample) {
            case 16: {
                int16_t v;
                memcpy(&v, p, 2);
                return static_cast<float>(v) / 32768.0f;
            }
            case 24: {
                int32_t v = (p[2] << 16) | (p[1] << 8) | p[0];
                if (v & 0x800000) v |= 0xFF000000; // sign extend
                return static_cast<float>(v) / 8388608.0f;
            }
            case 32: {
                int32_t v;
                memcpy(&v, p, 4);
                return static_cast<float>(v) / 2147483648.0f;
            }
            default:
                return 0.0f;
        }
    };

    const uint8_t* raw = dataBytes.data();
    for (int64_t i = 0; i < numFrames; i++) {
        if (fmt.numChannels >= 2) {
            const uint8_t* base = raw + i * bytesPerSample * fmt.numChannels;
            outBuffer.left[i] = sampleToFloat(base);
            outBuffer.right[i] = sampleToFloat(base + bytesPerSample);
        } else {
            const uint8_t* base = raw + i * bytesPerSample;
            float v = sampleToFloat(base);
            outBuffer.left[i] = v;
            outBuffer.right[i] = v;
        }
    }
    return true;
}

bool writeWavFile(
    const std::string& path,
    const float* interleavedStereo,
    int64_t numFrames,
    int sampleRate
) {
    FILE* file = fopen(path.c_str(), "wb");
    if (!file) return false;

    const int numChannels = 2;
    const int bitsPerSample = 16;
    const int byteRate = sampleRate * numChannels * bitsPerSample / 8;
    const int blockAlign = numChannels * bitsPerSample / 8;
    const uint32_t dataSize = static_cast<uint32_t>(numFrames * blockAlign);

    WavHeader header{};
    memcpy(header.riff, "RIFF", 4);
    memcpy(header.wave, "WAVE", 4);
    header.chunkSize = 36 + dataSize;

    fwrite(&header, sizeof(header), 1, file);

    ChunkHeader fmtHeader{};
    memcpy(fmtHeader.id, "fmt ", 4);
    fmtHeader.size = 16;
    fwrite(&fmtHeader, sizeof(fmtHeader), 1, file);

    FmtChunk fmt{};
    fmt.audioFormat = 1; // PCM
    fmt.numChannels = numChannels;
    fmt.sampleRate = sampleRate;
    fmt.byteRate = byteRate;
    fmt.blockAlign = blockAlign;
    fmt.bitsPerSample = bitsPerSample;
    fwrite(&fmt, sizeof(fmt), 1, file);

    ChunkHeader dataHeader{};
    memcpy(dataHeader.id, "data", 4);
    dataHeader.size = dataSize;
    fwrite(&dataHeader, sizeof(dataHeader), 1, file);

    std::vector<int16_t> pcm(numFrames * numChannels);
    for (int64_t i = 0; i < numFrames * numChannels; i++) {
        float sample = interleavedStereo[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        pcm[i] = static_cast<int16_t>(sample * 32767.0f);
    }
    fwrite(pcm.data(), sizeof(int16_t), pcm.size(), file);

    if (dataSize % 2 == 1) {
        uint8_t pad = 0;
        fwrite(&pad, 1, 1, file);
    }

    fclose(file);
    return true;
}

AudioFileCache& AudioFileCache::instance() {
    static AudioFileCache singleton;
    return singleton;
}

std::shared_ptr<AudioBuffer> AudioFileCache::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(path);
    if (it != cache_.end()) return it->second;

    auto buffer = std::make_shared<AudioBuffer>();
    if (!readWavFile(path, *buffer)) {
        return nullptr;
    }
    cache_[path] = buffer;
    return buffer;
}

void AudioFileCache::invalidate(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(path);
}

} // namespace almus
