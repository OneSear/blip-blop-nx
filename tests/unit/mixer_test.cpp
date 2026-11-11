#include <doctest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "bb/core/audio/mixer.hpp"

namespace audio = bb::core::audio;

namespace {

void put16(std::vector<std::uint8_t>& out, unsigned v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void put32(std::vector<std::uint8_t>& out, unsigned v) {
    put16(out, v & 0xFFFF);
    put16(out, v >> 16);
}

// A PCM WAV file of the given samples (8-bit ones are stored unsigned).
std::vector<std::uint8_t> wav(int channels, int rate, int bits, const std::vector<int>& samples) {
    std::vector<std::uint8_t> out;
    const unsigned bytes = static_cast<unsigned>(samples.size()) * static_cast<unsigned>(bits / 8);
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    put32(out, 36 + bytes);
    out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(out, 16);
    put16(out, 1);
    put16(out, static_cast<unsigned>(channels));
    put32(out, static_cast<unsigned>(rate));
    put32(out, static_cast<unsigned>(rate * channels * bits / 8));
    put16(out, static_cast<unsigned>(channels * bits / 8));
    put16(out, static_cast<unsigned>(bits));
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    put32(out, bytes);
    for (const int s : samples) {
        if (bits == 8) {
            out.push_back(static_cast<std::uint8_t>(s + 128));
        } else {
            put16(out, static_cast<unsigned>(s) & 0xFFFF);
        }
    }
    return out;
}

// A music of `frames` stereo frames: left counts up from 1, right is its
// negative.
class CountingStream final : public audio::MusicStream {
public:
    CountingStream(int rate, int frames) : rate_(rate), frames_(frames) {}
    int rate() const override { return rate_; }
    int read(std::int16_t* out, int frames) override {
        int n = 0;
        while (n < frames && next_ < frames_) {
            ++next_;
            *out++ = static_cast<std::int16_t>(next_);
            *out++ = static_cast<std::int16_t>(-next_);
            ++n;
        }
        return n;
    }
    bool rewind() override {
        next_ = 0;
        return true;
    }

private:
    int rate_;
    int frames_;
    int next_ = 0;
};

std::vector<std::int16_t> mix(audio::Mixer& mixer, int frames) {
    std::vector<std::int16_t> out(static_cast<std::size_t>(frames) * 2);
    mixer.mix(out.data(), frames);
    return out;
}

}  // namespace

TEST_CASE("WAV files are read as mono 16-bit samples at the output rate") {
    std::vector<std::int16_t> out;
    CHECK(audio::decodeWav(wav(1, 100, 16, {0, 1000, -1000, 32767}).data(), 52, 100, out));
    CHECK(out == std::vector<std::int16_t>{0, 1000, -1000, 32767});

    // 8-bit samples are unsigned around 128, scaled by 256.
    const auto eight = wav(1, 100, 8, {0, 10, -10});
    CHECK(audio::decodeWav(eight.data(), eight.size(), 100, out));
    CHECK(out == std::vector<std::int16_t>{0, 2560, -2560});

    // Stereo is averaged.
    const auto stereo = wav(2, 100, 16, {100, 300, -50, -150});
    CHECK(audio::decodeWav(stereo.data(), stereo.size(), 100, out));
    CHECK(out == std::vector<std::int16_t>{200, -100});

    // Twice the rate: twice the samples, the new ones between the old.
    const auto slow = wav(1, 50, 16, {0, 1000, 2000});
    CHECK(audio::decodeWav(slow.data(), slow.size(), 100, out));
    CHECK(out == std::vector<std::int16_t>{0, 500, 1000, 1500, 2000, 2000});

    // A data chunk longer than the file is cut to its whole frames.
    auto cut = wav(1, 100, 16, {1, 2, 3});
    cut.pop_back();
    CHECK(audio::decodeWav(cut.data(), cut.size(), 100, out));
    CHECK(out.size() == 2);

    const std::uint8_t junk[16] = {'R', 'I', 'F', 'F'};
    CHECK_FALSE(audio::decodeWav(junk, sizeof junk, 100, out));
    CHECK_FALSE(audio::decodeWav(nullptr, 0, 100, out));
}

TEST_CASE("sounds play on the first free voice, loop, stop and add up") {
    audio::Mixer mixer(100);
    const auto bytes = wav(1, 100, 16, {1000, 2000, 3000});
    const int sound = mixer.loadSound(bytes.data(), bytes.size());
    REQUIRE(sound >= 0);
    CHECK(mixer.loadSound(bytes.data(), 10) == -1);

    CHECK(mixer.play(sound, false) == 0);
    CHECK(mixer.play(sound, true) == 1);
    CHECK(mix(mixer, 4) == std::vector<std::int16_t>{2000, 2000, 4000, 4000, 6000, 6000, 1000, 1000});
    CHECK_FALSE(mixer.playing(0));  // over after its 3 samples
    CHECK(mixer.playing(1));        // looping: back to its first sample
    CHECK(mixer.play(sound, false) == 0);
    mixer.stop(1);
    CHECK_FALSE(mixer.playing(1));

    for (int i = 1; i < audio::Mixer::kVoices; ++i) {
        CHECK(mixer.play(sound, true) == i);
    }
    CHECK(mixer.play(sound, false) == -1);  // all 32 busy
    CHECK(mixer.play(sound + 1, false) == -1);

    // 32 voices of 3000 clip at 32767.
    mix(mixer, 2);
    CHECK(mix(mixer, 1)[0] == 32767);

    mixer.freeSound(sound);
    for (int i = 0; i < audio::Mixer::kVoices; ++i) {
        CHECK_FALSE(mixer.playing(i));
    }
    CHECK(mix(mixer, 1) == std::vector<std::int16_t>{0, 0});
}

TEST_CASE("the music plays once or loops, at the output rate") {
    audio::Mixer mixer(100);
    const int music = mixer.addMusic(std::make_unique<std::vector<std::uint8_t>>(),
                                     std::make_unique<CountingStream>(100, 3));
    REQUIRE(music >= 0);

    mixer.playMusic(music, false);
    CHECK(mix(mixer, 5) == std::vector<std::int16_t>{1, -1, 2, -2, 3, -3, 0, 0, 0, 0});
    CHECK_FALSE(mixer.musicPlaying());

    mixer.playMusic(music, true);
    CHECK(mix(mixer, 7) == std::vector<std::int16_t>{1, -1, 2, -2, 3, -3, 1, -1, 2, -2, 3, -3, 1, -1});
    CHECK(mixer.musicPlaying());
    mixer.stopMusic();
    CHECK(mix(mixer, 1) == std::vector<std::int16_t>{0, 0});

    // A 50 Hz music at 100 Hz: twice the frames, the new ones between
    // (rounded down: halfway from -1 to -2 is -2).
    const int slow = mixer.addMusic(std::make_unique<std::vector<std::uint8_t>>(),
                                    std::make_unique<CountingStream>(50, 3));
    mixer.playMusic(slow, false);
    const auto out = mix(mixer, 8);
    CHECK(out == std::vector<std::int16_t>{1, -1, 1, -2, 2, -2, 2, -3, 3, -3, 3, -3, 0, 0, 0, 0});

    // The music and a sound add up; a freed music stops.
    const auto bytes = wav(1, 100, 16, {100});
    mixer.play(mixer.loadSound(bytes.data(), bytes.size()), false);
    mixer.playMusic(music, true);
    CHECK(mix(mixer, 1) == std::vector<std::int16_t>{101, 99});
    mixer.freeMusic(music);
    CHECK_FALSE(mixer.musicPlaying());

    CHECK(mixer.openMusic({'O', 'g', 'g', 'S', 0, 0}) == -1);
    CHECK(mixer.openMusic({1, 2, 3, 4, 5, 6, 7, 8}) == -1);
}

TEST_CASE("the volumes scale the sounds and the music apart, full volume leaving them as they are") {
    audio::Mixer mixer(100);
    const auto bytes = wav(1, 100, 16, {1000, -1000});
    const int sound = mixer.loadSound(bytes.data(), bytes.size());
    const int music = mixer.addMusic(std::make_unique<std::vector<std::uint8_t>>(),
                                     std::make_unique<CountingStream>(100, 3));
    REQUIRE(sound >= 0);
    REQUIRE(music >= 0);
    CHECK(mixer.soundGain() == audio::Mixer::kFullGain);
    CHECK(mixer.musicGain() == audio::Mixer::kFullGain);

    mixer.setGains(128, 0);  // sounds at half, no music
    mixer.play(sound, true);
    mixer.playMusic(music, true);
    CHECK(mix(mixer, 2) == std::vector<std::int16_t>{500, 500, -500, -500});
    mixer.setGains(0, audio::Mixer::kFullGain);
    CHECK(mix(mixer, 1) == std::vector<std::int16_t>{3, -3});
    mixer.setGains(audio::Mixer::kFullGain, audio::Mixer::kFullGain);
    CHECK(mix(mixer, 1) == std::vector<std::int16_t>{-999, -1001});  // the sound's second sample
    mixer.setGains(-5, 1000);  // kept within 0 to full
    CHECK(mixer.soundGain() == 0);
    CHECK(mixer.musicGain() == audio::Mixer::kFullGain);

    // The options' levels: silent, then louder at each step, full at 10.
    CHECK(audio::volumeGain(0) == 0);
    CHECK(audio::volumeGain(audio::kVolumeLevels) == audio::Mixer::kFullGain);
    CHECK(audio::volumeGain(8) == audio::Mixer::kFullGain / 2);  // 6 dB down
    for (int level = 1; level <= audio::kVolumeLevels; ++level) {
        CHECK(audio::volumeGain(level) > audio::volumeGain(level - 1));
    }
    CHECK(audio::volumeGain(-1) == 0);
    CHECK(audio::volumeGain(11) == audio::Mixer::kFullGain);
}
