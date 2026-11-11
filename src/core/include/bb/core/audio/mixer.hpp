#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace bb::core::audio {

// Reads a RIFF/WAVE file (PCM, 8 or 16 bits, mono or stereo) and converts
// it to mono 16-bit samples at outputRate (stereo is averaged, the rate
// converted by linear interpolation). False when the bytes are not such a
// file. A data chunk longer than the file is cut to the whole frames there.
bool decodeWav(const std::uint8_t* data, std::size_t size, int outputRate, std::vector<std::int16_t>& out);

// A music being decoded: interleaved stereo 16-bit frames at the file's
// rate (mono is played on both sides).
class MusicStream {
public:
    virtual ~MusicStream() = default;
    virtual int rate() const = 0;
    // Up to `frames` frames into out (2 samples each); fewer at the end of
    // the music, 0 once it is over.
    virtual int read(std::int16_t* out, int frames) = 0;
    // Back to the start; false when the decoder cannot.
    virtual bool rewind() = 0;
};

// An Ogg Vorbis (stb_vorbis) or MP3 (dr_mp3) decoder over a whole file in
// memory, told apart by their first bytes; null when neither reads it. The
// bytes must outlive the stream. (audio_codecs.cpp, built without the
// strict warnings: third-party code.)
std::unique_ptr<MusicStream> openMusicStream(const std::vector<std::uint8_t>& file);

// The in-house software mixer (migration.md DEC-06, Phase 9), in place of
// SDL2_mixer: 32 voices of sounds and one music, summed into interleaved
// stereo 16-bit frames at full volume and clipped, as SDL_mixer mixed them
// for the game. Sounds are converted to the output rate when loaded;
// the music is decoded and its rate converted as it plays.
//
// Platform-free and not thread-safe: the platform calls mix() from its
// audio callback and must keep the other calls from running at the same
// time (the SDL backend locks the audio device around them).
class Mixer {
public:
    // The 2002 game asked FMOD for 32 software channels (CHANNEL_NUMBER).
    static constexpr int kVoices = 32;

    explicit Mixer(int outputRate = 44100);
    ~Mixer();
    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;

    int outputRate() const noexcept { return outputRate_; }

    // A sound: samples from decodeWav() at outputRate(). Returns its
    // handle, reused after freeSound().
    int addSound(std::vector<std::int16_t> samples);
    // decodeWav() then addSound(); -1 when the bytes are not a WAV file.
    int loadSound(const std::uint8_t* data, std::size_t size);
    // Stops the voices that play it.
    void freeSound(int sound);

    // Plays on the first free voice and returns it; -1 when every voice
    // plays or the handle is invalid. A looping sound repeats until stop().
    int play(int sound, bool loop);
    void stop(int voice);
    bool playing(int voice) const;

    // A music over a whole file: the stream from openMusicStream(file).
    // Returns its handle; the mixer keeps both.
    int addMusic(std::unique_ptr<std::vector<std::uint8_t>> file, std::unique_ptr<MusicStream> stream);
    // openMusicStream() then addMusic(); -1 when no decoder reads it.
    int openMusic(std::vector<std::uint8_t> file);
    // Stops it if it is playing.
    void freeMusic(int music);
    // Only one music plays: this one replaces the current one, from its
    // start.
    void playMusic(int music, bool loop);
    void stopMusic();
    bool musicPlaying() const noexcept { return music_ >= 0; }

    // The volumes of the sounds and of the music (the options' MUSIC and
    // SOUNDS): a gain in 1/256ths, from 0 (silent) to kFullGain (the samples
    // as they are, as before the volumes existed).
    static constexpr int kFullGain = 256;
    void setGains(int soundGain, int musicGain) noexcept;
    int soundGain() const noexcept { return soundGain_; }
    int musicGain() const noexcept { return musicGain_; }

    // Writes `frames` frames (2 samples each) of everything playing.
    void mix(std::int16_t* out, int frames);

private:
    struct Voice {
        int sound = -1;  // -1: free
        std::size_t position = 0;
        bool loop = false;
    };
    struct Music {
        std::unique_ptr<std::vector<std::uint8_t>> file;
        std::unique_ptr<MusicStream> stream;
    };

    void mixMusic(std::int32_t* sum, int frames);
    // Drops the frames the music has played and decodes more; false when
    // nothing more comes.
    bool refillMusic();

    int outputRate_;
    std::vector<std::unique_ptr<std::vector<std::int16_t>>> sounds_;
    Voice voices_[kVoices];
    std::vector<Music> musics_;

    int music_ = -1;  // playing, -1 for none
    bool musicLoop_ = false;
    bool musicEnded_ = false;                // the stream gave its last frame
    std::vector<std::int16_t> musicFrames_;  // decoded, stereo
    std::uint64_t musicPosition_ = 0;        // in musicFrames_, 16.16 fixed point

    std::vector<std::int32_t> sum_;  // mix()'s accumulator
    int soundGain_ = kFullGain;
    int musicGain_ = kFullGain;
};

// The gain of a volume level of the options, 0 (silent) to 10 (full): 3 dB
// a step, so that each step sounds about as much quieter as the last.
constexpr int kVolumeLevels = 10;
int volumeGain(int level) noexcept;

}  // namespace bb::core::audio
