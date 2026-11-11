#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace bb::core {
class FileReader;
class Rng;
}  // namespace bb::core

namespace bb::core::gfx {
class Pixmap;
}
namespace bb::platform {
class Clock;
class FileSystem;
class System;
}  // namespace bb::platform

namespace bb::app {

class GameFiles;

// The run's options and its deterministic machinery: virtual time,
// recording and replaying the input, fingerprints (one object, owned by
// bbMain()).
class Session {
public:
    Session();
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Parses the command line (both the new --options and the 2002
    // /safe /cheat /quiet switches). Returns false and prints a message on
    // error; the caller should then exit with status 2.
    bool init(int argc, const char* argv[]);

    // Reads what the options name (--data, --pack, --replay) once the
    // platform is up. Same errors as init().
    bool load(const bb::core::FileReader& reader, GameFiles& files);

    // Called once the platform is up and the game is about to start; real
    // (non-virtual) time is read from this clock, the recording is written
    // to these files.
    void start(bb::platform::Clock& clock, const bb::platform::System& system, bb::platform::FileSystem& files);

    // Flushes outputs; returns the process exit status.
    int finish();

    // True when time, input and randomness are virtual (--record/--replay/
    // --deterministic): two such runs with the same input produce the same
    // frames.
    bool deterministic() const;
    bool headless() const;
    // --smooth-motion: the option on from the start (the regression test
    // that it changes no tick).
    bool smoothMotion() const;
    // --widescreen: the option on from the start (the regression replays
    // of the levels in 16:9).
    bool widescreen() const;
    // --display-shots: true at the presented frame asked for, with the
    // directory the pictures of the filters go to.
    bool displayShots(std::uint64_t presented, std::string& dir) const;
    bool safeMode() const;
    // The /cheat and /quiet switches (also replayed from a recording).
    bool cheats() const;
    bool quiet() const;

    // The player closed the window, or the run must stop (end of a replay,
    // --max-frames): every screen then leads to the exit (app_killed).
    bool quitRequested() const;
    void requestQuit();

    // Settings file given with --settings, empty for the default location.
    std::string settingsPath() const;

    // Log file given with --log, empty for the default location.
    std::string logPath() const;

    // The level given with --level ("data/xxx.lvl"), played alone with its
    // level kind (docs/formats/lst.md); empty for the whole game.
    std::string onlyLevel() const;
    int onlyLevelKind() const;

    // Current time in microseconds: virtual in deterministic mode, wall-clock
    // otherwise.
    std::uint64_t nowMicroseconds() const;

    // Called by the present function with the finished frame.
    void onFramePresented(const bb::core::gfx::Pixmap& frame);

    // Called for a frame that waits without presenting anything: sleeps
    // briefly in real time (no more 100 % CPU) or lets one virtual frame pass.
    void idle();

    // Called at the end of every simulation tick with a fingerprint of the
    // world state.
    void onTick(std::uint64_t stateHash);

    // Input exchange with the device input (DeviceInput). Codes are the legacy input
    // codes (SDL key codes below 255, 0x40000000 | (key & 0xFFF) for the other
    // keys, ((slot + 1) << 10) | button for joysticks).
    struct InputSink {
        virtual void clear() = 0;
        virtual void press(std::uint32_t code) = 0;
        virtual void setUnfocused(bool unfocused) = 0;
        virtual ~InputSink() = default;
    };
    struct InputSource {
        virtual void forEachPressed(void (*callback)(std::uint32_t code, void* context),
                                    void* context) const = 0;
        virtual ~InputSource() = default;
    };
    // In deterministic mode, replaces the game-visible input with the input of
    // the current frame (recording the live input first when recording). The
    // focus state is recorded and replayed like the keys.
    // Returns true when the run must stop (window closed or replay finished).
    bool filterInput(const InputSource& live, bool liveQuit, bool liveUnfocused, InputSink& visible);

    // When --trace-ticks is active and the coming tick is in range, the stream
    // where the game writes a readable dump of the world state; NULL otherwise.
    std::FILE* traceStream() const;

    // The game's random generator (bb::game::World::rng).
    bb::core::Rng& rng();

    // Frame counter (presented + idle frames).
    std::uint64_t frame() const;

    // Prints "[frame N] message" in deterministic runs: milestones that help
    // writing input scripts and locating divergences.
    void note(const char* message);

private:
    struct State;
    std::unique_ptr<State> state_;

    void applyLegacySwitch(const std::string& word);
    bool endReached() const;
    void requestStopIfDone();
    void writeCheckpoint(const char* kind, std::uint64_t count, std::uint64_t value);
};

}  // namespace bb::app
