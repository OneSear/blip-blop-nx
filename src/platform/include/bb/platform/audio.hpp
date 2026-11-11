#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bb::platform {

// Sound effects and streamed music, on the in-house mixer
// (bb::core::audio::Mixer, Phase 9; SDL2_mixer until then).
class Audio {
public:
    using Sample = int;  // -1 = invalid
    using Music = int;   // -1 = invalid

    virtual ~Audio() = default;

    // False when no output device could be opened (sound then stays off).
    virtual bool available() const = 0;

    // A complete RIFF/WAVE file in memory.
    virtual Sample loadSample(const std::uint8_t* data, std::size_t size) = 0;
    virtual void freeSample(Sample sample) = 0;

    // Plays on a free voice; returns the voice, or -1 when none is free.
    // A looping sample repeats until stop().
    virtual int play(Sample sample, bool loop) = 0;
    virtual void stop(int voice) = 0;

    // Music streamed from a complete Ogg Vorbis or MP3 file in memory (the
    // audio keeps the bytes while the music is open). Only one music plays
    // at a time: playMusic() replaces the current one, stopMusic() stops
    // whichever is playing.
    virtual Music openMusic(std::vector<std::uint8_t> file) = 0;
    virtual void freeMusic(Music music) = 0;
    virtual void playMusic(Music music, bool loop) = 0;
    virtual void stopMusic() = 0;

    // The volumes of the sounds and of the music, as gains in 1/256ths from 0
    // to 256 (bb::core::audio::Mixer::setGains()); 256 for both until set.
    virtual void setGains(int soundGain, int musicGain) = 0;
};

}  // namespace bb::platform
