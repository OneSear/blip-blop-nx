#include "bb/core/audio/mixer.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace bb::core::audio {

namespace {

std::uint32_t read32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t read16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::int16_t clip(std::int32_t v) {
    return static_cast<std::int16_t>(std::clamp<std::int32_t>(v, -32768, 32767));
}

// The rate conversion of mix() and decodeWav(): 16.16 fixed point.
constexpr int kFractionBits = 16;
constexpr std::uint64_t kOne = std::uint64_t{1} << kFractionBits;

std::int32_t between(std::int32_t a, std::int32_t b, std::uint64_t position) {
    const auto fraction = static_cast<std::int64_t>(position & (kOne - 1));
    return a + static_cast<std::int32_t>((static_cast<std::int64_t>(b - a) * fraction) >> kFractionBits);
}

// Frames the music decodes at a time.
constexpr int kMusicChunk = 4096;

// A sample at a gain of 1/256ths: the sample itself at full gain.
std::int32_t scaled(std::int32_t sample, int gain) {
    return gain == Mixer::kFullGain ? sample : sample * gain / Mixer::kFullGain;
}

}  // namespace

int volumeGain(int level) noexcept {
    // 256 * 10^(-3 dB * (10 - level) / 20), rounded.
    static constexpr int kGains[kVolumeLevels + 1] = {0,  11, 16,  23,  32,  45,
                                                      64, 91, 128, 181, Mixer::kFullGain};
    return kGains[std::clamp(level, 0, kVolumeLevels)];
}

void Mixer::setGains(int soundGain, int musicGain) noexcept {
    soundGain_ = std::clamp(soundGain, 0, kFullGain);
    musicGain_ = std::clamp(musicGain, 0, kFullGain);
}

bool decodeWav(const std::uint8_t* data, std::size_t size, int outputRate, std::vector<std::int16_t>& out) {
    if (data == nullptr || size < 12 || std::memcmp(data, "RIFF", 4) != 0 ||
        std::memcmp(data + 8, "WAVE", 4) != 0 || outputRate <= 0) {
        return false;
    }
    int channels = 0;
    std::uint32_t rate = 0;
    int bits = 0;
    const std::uint8_t* samples = nullptr;
    std::size_t bytes = 0;
    std::size_t at = 12;
    while (at + 8 <= size) {
        const std::uint8_t* chunk = data + at;
        const std::size_t length = read32(chunk + 4);
        const std::size_t available = size - at - 8;
        if (std::memcmp(chunk, "fmt ", 4) == 0 && length >= 16 && available >= 16) {
            if (read16(chunk + 8) != 1) {  // PCM only
                return false;
            }
            channels = read16(chunk + 10);
            rate = read32(chunk + 12);
            bits = read16(chunk + 22);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            samples = chunk + 8;
            bytes = std::min(length, available);
            break;
        }
        at += 8 + length + (length & 1);  // chunks are padded to even sizes
    }
    if (samples == nullptr || (channels != 1 && channels != 2) || (bits != 8 && bits != 16) || rate == 0) {
        return false;
    }

    // Mono 16-bit at the file's rate.
    const std::size_t frameBytes = static_cast<std::size_t>(channels * bits / 8);
    const std::size_t frames = bytes / frameBytes;
    std::vector<std::int32_t> mono(frames);
    for (std::size_t f = 0; f < frames; ++f) {
        std::int32_t total = 0;
        for (int c = 0; c < channels; ++c) {
            const std::uint8_t* s = samples + f * frameBytes + static_cast<std::size_t>(c * bits / 8);
            total += bits == 8 ? (static_cast<std::int32_t>(s[0]) - 128) * 256
                               : static_cast<std::int32_t>(static_cast<std::int16_t>(read16(s)));
        }
        mono[f] = total / channels;
    }

    // Then at the output rate.
    const std::uint64_t step =
        (static_cast<std::uint64_t>(rate) << kFractionBits) / static_cast<std::uint64_t>(outputRate);
    const std::size_t length =
        frames == 0
            ? 0
            : static_cast<std::size_t>(
                  (static_cast<std::uint64_t>(frames) * static_cast<std::uint64_t>(outputRate)) / rate);
    out.resize(length);
    std::uint64_t position = 0;
    for (std::size_t i = 0; i < length; ++i) {
        const auto f = static_cast<std::size_t>(position >> kFractionBits);
        const std::int32_t a = mono[std::min(f, frames - 1)];
        const std::int32_t b = mono[std::min(f + 1, frames - 1)];
        out[i] = clip(between(a, b, position));
        position += step;
    }
    return true;
}

Mixer::Mixer(int outputRate) : outputRate_(outputRate > 0 ? outputRate : 44100) {}

Mixer::~Mixer() = default;

int Mixer::addSound(std::vector<std::int16_t> samples) {
    auto sound = std::make_unique<std::vector<std::int16_t>>(std::move(samples));
    for (std::size_t i = 0; i < sounds_.size(); ++i) {
        if (!sounds_[i]) {
            sounds_[i] = std::move(sound);
            return static_cast<int>(i);
        }
    }
    sounds_.push_back(std::move(sound));
    return static_cast<int>(sounds_.size() - 1);
}

int Mixer::loadSound(const std::uint8_t* data, std::size_t size) {
    std::vector<std::int16_t> samples;
    if (!decodeWav(data, size, outputRate_, samples)) {
        return -1;
    }
    return addSound(std::move(samples));
}

void Mixer::freeSound(int sound) {
    if (sound < 0 || static_cast<std::size_t>(sound) >= sounds_.size()) {
        return;
    }
    for (Voice& v : voices_) {
        if (v.sound == sound) {
            v.sound = -1;
        }
    }
    sounds_[static_cast<std::size_t>(sound)].reset();
}

int Mixer::play(int sound, bool loop) {
    if (sound < 0 || static_cast<std::size_t>(sound) >= sounds_.size() ||
        !sounds_[static_cast<std::size_t>(sound)]) {
        return -1;
    }
    for (int i = 0; i < kVoices; ++i) {
        Voice& v = voices_[i];
        if (v.sound < 0) {
            v.sound = sound;
            v.position = 0;
            v.loop = loop;
            return i;
        }
    }
    return -1;
}

void Mixer::stop(int voice) {
    if (voice >= 0 && voice < kVoices) {
        voices_[voice].sound = -1;
    }
}

bool Mixer::playing(int voice) const {
    return voice >= 0 && voice < kVoices && voices_[voice].sound >= 0;
}

int Mixer::addMusic(std::unique_ptr<std::vector<std::uint8_t>> file, std::unique_ptr<MusicStream> stream) {
    Music music{std::move(file), std::move(stream)};
    for (std::size_t i = 0; i < musics_.size(); ++i) {
        if (!musics_[i].stream) {
            musics_[i] = std::move(music);
            return static_cast<int>(i);
        }
    }
    musics_.push_back(std::move(music));
    return static_cast<int>(musics_.size() - 1);
}

int Mixer::openMusic(std::vector<std::uint8_t> file) {
    auto bytes = std::make_unique<std::vector<std::uint8_t>>(std::move(file));
    auto stream = openMusicStream(*bytes);
    if (!stream) {
        return -1;
    }
    return addMusic(std::move(bytes), std::move(stream));
}

void Mixer::freeMusic(int music) {
    if (music < 0 || static_cast<std::size_t>(music) >= musics_.size()) {
        return;
    }
    if (music == music_) {
        stopMusic();
    }
    musics_[static_cast<std::size_t>(music)] = Music();
}

void Mixer::playMusic(int music, bool loop) {
    if (music < 0 || static_cast<std::size_t>(music) >= musics_.size() ||
        !musics_[static_cast<std::size_t>(music)].stream) {
        return;
    }
    MusicStream& stream = *musics_[static_cast<std::size_t>(music)].stream;
    if (!stream.rewind()) {
        stopMusic();
        return;
    }
    music_ = music;
    musicLoop_ = loop;
    musicEnded_ = false;
    musicFrames_.clear();
    musicPosition_ = 0;
}

void Mixer::stopMusic() {
    music_ = -1;
    musicFrames_.clear();
    musicPosition_ = 0;
}

bool Mixer::refillMusic() {
    if (musicEnded_) {
        return false;
    }
    // Keep the frame being played and those after it.
    const std::size_t played = std::min<std::size_t>(
        static_cast<std::size_t>(musicPosition_ >> kFractionBits), musicFrames_.size() / 2);
    musicFrames_.erase(musicFrames_.begin(), musicFrames_.begin() + static_cast<std::ptrdiff_t>(played * 2));
    musicPosition_ -= static_cast<std::uint64_t>(played) << kFractionBits;

    MusicStream& stream = *musics_[static_cast<std::size_t>(music_)].stream;
    const std::size_t kept = musicFrames_.size();
    musicFrames_.resize(kept + std::size_t{kMusicChunk} * 2);
    int got = stream.read(musicFrames_.data() + kept, kMusicChunk);
    if (got == 0 && musicLoop_ && stream.rewind()) {
        got = stream.read(musicFrames_.data() + kept, kMusicChunk);
    }
    musicFrames_.resize(kept + static_cast<std::size_t>(got) * 2);
    if (got == 0) {
        musicEnded_ = true;
        return false;
    }
    return true;
}

void Mixer::mixMusic(std::int32_t* sum, int frames) {
    if (music_ < 0) {
        return;
    }
    const MusicStream& stream = *musics_[static_cast<std::size_t>(music_)].stream;
    const std::uint64_t step = (static_cast<std::uint64_t>(stream.rate()) << kFractionBits) /
                               static_cast<std::uint64_t>(outputRate_);
    for (std::size_t i = 0; i < static_cast<std::size_t>(frames); ++i) {
        // The two frames around the position; the last one alone at the end.
        while (static_cast<std::size_t>(musicPosition_ >> kFractionBits) + 1 >= musicFrames_.size() / 2 &&
               refillMusic()) {
        }
        const auto f = static_cast<std::size_t>(musicPosition_ >> kFractionBits);
        const std::size_t count = musicFrames_.size() / 2;
        if (f >= count) {
            stopMusic();  // over (a looping music never is)
            return;
        }
        const std::size_t g = std::min(f + 1, count - 1);
        sum[2 * i] += scaled(between(musicFrames_[2 * f], musicFrames_[2 * g], musicPosition_), musicGain_);
        sum[2 * i + 1] +=
            scaled(between(musicFrames_[2 * f + 1], musicFrames_[2 * g + 1], musicPosition_), musicGain_);
        musicPosition_ += step;
    }
}

void Mixer::mix(std::int16_t* out, int frames) {
    if (out == nullptr || frames <= 0) {
        return;
    }
    sum_.assign(static_cast<std::size_t>(frames) * 2, 0);
    std::int32_t* sum = sum_.data();

    mixMusic(sum, frames);

    for (Voice& v : voices_) {
        if (v.sound < 0) {
            continue;
        }
        const std::vector<std::int16_t>& samples = *sounds_[static_cast<std::size_t>(v.sound)];
        for (std::size_t i = 0; i < static_cast<std::size_t>(frames); ++i) {
            if (v.position >= samples.size()) {
                if (!v.loop || samples.empty()) {
                    v.sound = -1;
                    break;
                }
                v.position = 0;
            }
            const std::int32_t s = scaled(samples[v.position++], soundGain_);
            sum[2 * i] += s;
            sum[2 * i + 1] += s;
        }
    }

    for (std::size_t i = 0; i < sum_.size(); ++i) {
        out[i] = clip(sum[i]);
    }
}

}  // namespace bb::core::audio
