#include <doctest.h>

#include <vector>

#include "bb/core/assets/asset_types.hpp"
#include "bb/game/audio.hpp"
#include "bb/platform/audio.hpp"

namespace {

// Records what the banks ask of the device.
class FakeAudio : public bb::platform::Audio {
public:
    bool available() const override { return true; }
    Sample loadSample(const std::uint8_t*, std::size_t) override { return samples++; }
    void freeSample(Sample) override { ++freed; }
    int play(Sample sample, bool loop) override {
        plays.push_back({sample, loop});
        return nextVoice++;
    }
    void stop(int voice) override { stopped.push_back(voice); }
    Music openMusic(std::vector<std::uint8_t>) override { return musics++; }
    void freeMusic(Music) override {}
    void playMusic(Music music, bool loop) override { musicPlays.push_back({music, loop}); }
    void stopMusic() override { ++musicStops; }
    void setGains(int, int) override {}

    struct Play {
        int what;
        bool loop;
    };
    int samples = 0;
    int freed = 0;
    int nextVoice = 10;
    int musics = 0;
    int musicStops = 0;
    std::vector<Play> plays;
    std::vector<int> stopped;
    std::vector<Play> musicPlays;
};

bb::core::assets::SoundBank twoSounds() {
    bb::core::assets::SoundBank bank;
    bank.sounds.resize(2);
    bank.sounds[0].wav = {1, 2, 3};
    bank.sounds[1].wav = {4, 5, 6};
    return bank;
}

}  // namespace

TEST_CASE("A looping sound stops when every looping play is stopped") {
    FakeAudio audio;
    bb::game::AudioOutput output;
    output.device = &audio;
    bb::game::SoundBank bank(output);
    bank.assign(twoSounds());

    bank.play(1, bb::game::SoundPlayer::kLoop);
    bank.play(1, bb::game::SoundPlayer::kLoop);  // counted, not played again
    REQUIRE(audio.plays.size() == 1);
    CHECK(audio.plays[0].loop);

    bank.stop(1);
    CHECK(audio.stopped.empty());
    bank.stop(1);
    REQUIRE(audio.stopped.size() == 1);
    CHECK(audio.stopped[0] == 10);

    bank.stop(1);  // no voice any more
    CHECK(audio.stopped.size() == 1);
}

TEST_CASE("Nothing plays with the sound off or past the end of the bank") {
    FakeAudio audio;
    bb::game::AudioOutput output;
    output.device = &audio;
    bb::game::SoundBank bank(output);
    bank.assign(twoSounds());

    output.soundOn = false;
    bank.play(0);
    output.soundOn = true;
    bank.play(2);
    bank.play(-1);
    CHECK(audio.plays.empty());

    bank.clear();
    CHECK(audio.freed == 2);
}

TEST_CASE("Stopping a music that is not playing does nothing") {
    FakeAudio audio;
    bb::game::AudioOutput output;
    output.device = &audio;
    bb::game::MusicBank bank(output);
    std::vector<std::vector<std::uint8_t>> files(2, std::vector<std::uint8_t>{0});
    bank.assign(std::move(files), false);

    bank.stop(0);
    CHECK(audio.musicStops == 0);
    bank.play(1);
    REQUIRE(audio.musicPlays.size() == 1);
    CHECK(audio.musicPlays[0].what == 1);
    CHECK_FALSE(audio.musicPlays[0].loop);
    bank.stopAll();
    CHECK(audio.musicStops == 1);
}
