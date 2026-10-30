// The session: options, virtual time, recording and replaying the input,
// fingerprints (session.cpp of Phases 0 to 3).

#include "session.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "bb/core/assets/pack.hpp"
#include "bb/core/frame_clock.hpp"
#include "bb/core/gfx/bmp.hpp"
#include "bb/core/gfx/fingerprint.hpp"
#include "bb/core/hash.hpp"
#include "bb/core/input/codes.hpp"
#include "bb/core/input_timeline.hpp"
#include "bb/core/rng.hpp"
#include "bb/core/file_reader.hpp"
#include "bb/platform/clock.hpp"
#include "bb/platform/file_system.hpp"
#include "bb/platform/system.hpp"

#include "files.hpp"

namespace bb::app {

namespace {

// atoi(), without its undefined behaviour on overflow.
int toInt(const char* text) {
    return static_cast<int>(std::strtol(text, nullptr, 10));
}

// ---------------------------------------------------------------------------
// Input names used in replay files
// ---------------------------------------------------------------------------

struct NamedCode {
    const char* name;
    std::uint32_t code;
};

const std::vector<NamedCode>& namedKeys() {
    namespace in = bb::core::input;
    static const std::vector<NamedCode> table = [] {
        std::vector<NamedCode> t = {
            {"RIGHT", in::kKeyRight},
            {"LEFT", in::kKeyLeft},
            {"UP", in::kKeyUp},
            {"DOWN", in::kKeyDown},
            {"RETURN", in::kKeyReturn},
            {"ESCAPE", in::kKeyEscape},
            {"SPACE", in::kKeySpace},
            {"TAB", in::kKeyTab},
            {"BACKSPACE", in::kKeyBackspace},
            {"LCTRL", in::kKeyLeftCtrl},
            {"RCTRL", in::kKeyRightCtrl},
            {"LALT", in::kKeyLeftAlt},
            {"RALT", in::kKeyRightAlt},
            {"LSHIFT", in::kKeyLeftShift},
            {"RSHIFT", in::kKeyRightShift},
            {"MENU", in::kKeyMenu},
            {"F1", in::kKeyF1},
            {"F2", in::kKeyF2},
            {"F3", in::kKeyF3},
            {"F4", in::kKeyF4},
            {"F5", in::kKeyF5},
            {"F6", in::kKeyF6},
            {"F7", in::kKeyF7},
            {"F8", in::kKeyF8},
            {"F9", in::kKeyF9},
            {"F10", in::kKeyF10},
            {"F11", in::kKeyF11},
            {"F12", in::kKeyF12},
        };
        for (char c = 'A'; c <= 'Z'; ++c) {
            static char names[26][2];
            names[c - 'A'][0] = c;
            names[c - 'A'][1] = '\0';
            t.push_back({names[c - 'A'], static_cast<std::uint32_t>(c - 'A' + 'a')});
        }
        for (char c = '0'; c <= '9'; ++c) {
            static char names[10][5];
            std::snprintf(names[c - '0'], sizeof(names[0]), "K%c", c);
            t.push_back({names[c - '0'], static_cast<std::uint32_t>(c)});
        }
        return t;
    }();
    return table;
}

std::optional<std::uint32_t> codeFromName(std::string_view name) {
    for (const auto& entry : namedKeys()) {
        if (name == entry.name) {
            return entry.code;
        }
    }
    // JOY<slot>_B<button> or JOY<slot>_UP/DOWN/LEFT/RIGHT (slots from 1)
    if (name.size() > 5 && name.substr(0, 3) == "JOY") {
        const std::size_t underscore = name.find('_');
        if (underscore != std::string_view::npos) {
            const int slot = toInt(std::string(name.substr(3, underscore - 3)).c_str());
            const std::string_view rest = name.substr(underscore + 1);
            int button = -1;
            if (rest == "UP") {
                button = 100;
            } else if (rest == "DOWN") {
                button = 101;
            } else if (rest == "LEFT") {
                button = 102;
            } else if (rest == "RIGHT") {
                button = 103;
            } else if (rest.size() > 1 && rest[0] == 'B') {
                button = toInt(std::string(rest.substr(1)).c_str());
            }
            if (slot >= 1 && slot <= 255 && button >= 0 && button < 128 + 4) {
                return (static_cast<std::uint32_t>(slot) << 10) | static_cast<std::uint32_t>(button);
            }
        }
    }
    return std::nullopt;
}

std::string nameFromCode(std::uint32_t code) {
    for (const auto& entry : namedKeys()) {
        if (entry.code == code) {
            return entry.name;
        }
    }
    if (code >= (1u << 10) && code < 0x40000000u) {
        const std::uint32_t slot = code >> 10;
        const std::uint32_t button = code & 0x3FFu;
        const char* dir = button == 100   ? "UP"
                          : button == 101 ? "DOWN"
                          : button == 102 ? "LEFT"
                          : button == 103 ? "RIGHT"
                                          : nullptr;
        return "JOY" + std::to_string(slot) + "_" + (dir ? std::string(dir) : "B" + std::to_string(button));
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%X", code);
    return buffer;
}

// ---------------------------------------------------------------------------

void usage() {
    std::fprintf(stderr,
                 "usage: blipblop [options] [/safe] [/cheat] [/quiet]\n"
                 "  --data <dir>          game data directory (default: ./data)\n"
                 "  --pack <file>         read the game assets from this runtime pack instead of\n"
                 "                        the data directory (built by the bbassets tool)\n"
                 "  --overlay <file>      a runtime pack whose assets replace the game's (the\n"
                 "                        level editor's test of a level)\n"
                 "  --level <file> <kind>  play this level only (\"data/snuf1.lvl\"; kind as in\n"
                 "                        bb.lst: 0 bonus ... 5 end), then quit\n"
                 "  --settings <file>     settings file (default: in the user's preferences directory)\n"
                 "  --log <file>          log file (default: BlipBlop.log in the user's preferences\n"
                 "                        directory; in the working directory for deterministic runs)\n"
                 "  --record <file>       play normally and record the input (deterministic run)\n"
                 "  --replay <file>       replay a recorded or hand-written input file\n"
                 "  --deterministic       virtual time and fixed seed without input file\n"
                 "  --hash <file>         write frame and state fingerprints (deterministic runs)\n"
                 "  --hash-every <n>      fingerprint checkpoint interval, default 60\n"
                 "  --max-frames <n>      stop after n frames\n"
                 "  --seed <n>            random seed, default 1\n"
                 "  --headless            no window, no sound device, no frame pacing\n"
                 "  --smooth-motion       start with the smooth motion option on (also in\n"
                 "                        deterministic runs, which use the default settings)\n"
                 "  --dump-frames <dir> <from> <to>   save presented frames as BMP files\n"
                 "  --dump-every <n>      with --dump-frames, keep one frame in n\n"
                 "  --trace-ticks <file> <from> <to>  write the world state of these ticks\n"
                 "  /safe /cheat /quiet   legacy switches (safe mode, cheat keys, no music)\n");
}

}  // namespace

struct Session::State {
    // Options
    bool deterministic = false;
    bool headless = false;
    bool smoothMotion = false;
    bool safe = false;
    std::string recordPath;
    std::string replayPath;
    std::string hashPath;
    std::string dumpDir;
    std::uint64_t dumpFrom = 0;
    std::uint64_t dumpTo = 0;
    std::uint64_t dumpEvery = 1;
    std::uint64_t maxFrames = 0;
    std::uint32_t hashEvery = 60;
    std::string tracePath;
    std::string settingsPath;
    std::string logPath;
    std::string packPath;
    std::string dataRoot;
    std::string onlyLevel;
    std::string overlayPath;
    std::uint64_t onlyLevelKind = 1;
    std::uint64_t realFrames = 0;
    std::uint64_t traceFrom = 0;
    std::uint64_t traceTo = 0;
    std::FILE* traceOut = nullptr;
    bool seedGiven = false;
    bool cheats = false;  // /cheat
    bool quiet = false;   // /quiet
    bool quit = false;    // app_killed
    std::uint32_t seed = bb::core::Rng::kDefaultSeed;

    // Runtime
    bb::core::FrameClock frames{60};
    bb::core::Rng rng;
    bb::core::InputTimeline timeline;
    std::uint64_t lastLatchedFrame = ~0ull;
    bool liveQuitSeen = false;
    bool stopRequested = false;
    bool stopNoted = false;

    std::FILE* hashOut = nullptr;
    bb::core::Fnv1a64 frameChain;
    bb::core::Fnv1a64 stateChain;
    std::uint64_t presented = 0;
    std::uint64_t ticks = 0;

    bb::platform::Clock* clock = nullptr;
    const bb::platform::System* system = nullptr;
    bb::platform::FileSystem* files = nullptr;

    // The benchmark of headless runs: the real time between presented
    // frames (the game's work per frame, fingerprints included, as nothing
    // paces them).
    std::uint64_t lastPresent = 0;
    std::uint64_t workSum = 0;
    std::uint64_t workMax = 0;
    std::uint64_t workFrames = 0;
};

Session::Session() : state_(std::make_unique<State>()) {}
Session::~Session() = default;

void Session::applyLegacySwitch(const std::string& word) {
    State& g = *state_;
    if (word == "/safe") {
        g.safe = true;
    } else if (word == "/cheat") {
        g.cheats = true;
    } else if (word == "/quiet") {
        g.quiet = true;
    }
}

bool Session::endReached() const {
    const State& g = *state_;
    const std::uint64_t f = g.frames.frame();
    if (g.timeline.endFrame && f >= *g.timeline.endFrame) {
        return true;
    }
    return g.maxFrames != 0 && f >= g.maxFrames;
}

void Session::requestStopIfDone() {
    State& g = *state_;
    if (g.deterministic && endReached()) {
        g.stopRequested = true;
    }
    if (g.stopRequested) {
        g.quit = true;
        if (!g.stopNoted) {
            // Milestone: from here every screen must lead to the exit.
            g.stopNoted = true;
            note(g.deterministic && endReached() ? "stop: end of the replay" : "stop: quit requested");
        }
    }
}

void Session::writeCheckpoint(const char* kind, std::uint64_t count, std::uint64_t value) {
    State& g = *state_;
    if (g.hashOut != nullptr) {
        std::fprintf(g.hashOut, "%s %llu %016llx\n", kind, static_cast<unsigned long long>(count),
                     static_cast<unsigned long long>(value));
    }
}

// ---------------------------------------------------------------------------

bool Session::init(int argc, const char* argv[]) {
    State& g = *state_;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "blipblop: %s needs %s\n", arg.c_str(), what);
                return nullptr;
            }
            return argv[++i];
        };
        auto number = [&](const char* text, std::uint64_t& out) {
            char* end = nullptr;
            out = std::strtoull(text, &end, 10);
            if (end == text || *end != '\0') {
                std::fprintf(stderr, "blipblop: '%s' is not a number\n", text);
                return false;
            }
            return true;
        };

        if (arg == "--record" || arg == "--replay" || arg == "--hash") {
            const char* v = next("a file name");
            if (!v) {
                return false;
            }
            (arg == "--record" ? g.recordPath : arg == "--replay" ? g.replayPath : g.hashPath) = v;
        } else if (arg == "--settings") {
            const char* v = next("a file name");
            if (!v) {
                return false;
            }
            g.settingsPath = v;
        } else if (arg == "--log") {
            const char* v = next("a file name");
            if (!v) {
                return false;
            }
            g.logPath = v;
        } else if (arg == "--data") {
            const char* v = next("a directory");
            if (!v) {
                return false;
            }
            g.dataRoot = v;
        } else if (arg == "--pack") {
            const char* v = next("a file name");
            if (!v) {
                return false;
            }
            g.packPath = v;
        } else if (arg == "--overlay") {
            const char* v = next("a file name");
            if (!v) {
                return false;
            }
            g.overlayPath = v;
        } else if (arg == "--level") {
            const char* v = next("a level file and its kind");
            const char* kind = v ? next("a level kind (0 to 5)") : nullptr;
            if (!kind || !number(kind, g.onlyLevelKind) || g.onlyLevelKind > 5) {
                return false;
            }
            g.onlyLevel = v;
        } else if (arg == "--deterministic") {
            g.deterministic = true;
        } else if (arg == "--headless") {
            g.headless = true;
        } else if (arg == "--smooth-motion") {
            g.smoothMotion = true;
        } else if (arg == "--hash-every" || arg == "--max-frames" || arg == "--seed" ||
                   arg == "--dump-every") {
            const char* v = next("a number");
            std::uint64_t n = 0;
            if (!v || !number(v, n)) {
                return false;
            }
            if (arg == "--hash-every") {
                g.hashEvery = static_cast<std::uint32_t>(n == 0 ? 1 : n);
            } else if (arg == "--max-frames") {
                g.maxFrames = n;
            } else if (arg == "--dump-every") {
                g.dumpEvery = n == 0 ? 1 : n;
            } else {
                g.seed = static_cast<std::uint32_t>(n);
                g.seedGiven = true;
            }
        } else if (arg == "--trace-ticks") {
            const char* file = next("a file and a tick range");
            const char* from = file ? next("a first tick") : nullptr;
            const char* to = from ? next("a last tick") : nullptr;
            if (!to || !number(from, g.traceFrom) || !number(to, g.traceTo)) {
                return false;
            }
            g.tracePath = file;
        } else if (arg == "--dump-frames") {
            const char* dir = next("a directory and a frame range");
            const char* from = dir ? next("a first frame") : nullptr;
            const char* to = from ? next("a last frame") : nullptr;
            if (!to || !number(from, g.dumpFrom) || !number(to, g.dumpTo)) {
                return false;
            }
            g.dumpDir = dir;
        } else if (arg == "--help" || arg == "-h") {
            usage();
            std::exit(0);
        } else if (!arg.empty() && arg[0] == '/') {
            applyLegacySwitch(arg);
        } else {
            std::fprintf(stderr, "blipblop: unknown option '%s'\n", arg.c_str());
            usage();
            return false;
        }
    }

    if (!g.recordPath.empty() && !g.replayPath.empty()) {
        std::fprintf(stderr, "blipblop: --record and --replay are exclusive\n");
        return false;
    }
    return true;
}

bool Session::load(const bb::core::FileReader& reader, GameFiles& files) {
    State& g = *state_;
    if (!g.dataRoot.empty()) {
        files.setDataRoot(g.dataRoot);
    }
    if (!g.packPath.empty()) {
        std::string error;
        std::shared_ptr<const bb::core::assets::PackFile> pack =
            bb::core::assets::PackFile::open(g.packPath, error, reader);
        if (!pack) {
            std::fprintf(stderr, "blipblop: %s\n", error.c_str());
            return false;
        }
        files.setAssetSource(std::make_unique<bb::core::assets::PackSource>(pack));
    }
    if (!g.overlayPath.empty()) {
        std::string error;
        std::shared_ptr<const bb::core::assets::PackFile> overlay =
            bb::core::assets::PackFile::open(g.overlayPath, error, reader);
        if (!overlay) {
            std::fprintf(stderr, "blipblop: %s\n", error.c_str());
            return false;
        }
        files.setOverlay(overlay);
    }
    if (!g.replayPath.empty()) {
        const auto bytes = reader.readAll(g.replayPath);
        if (!bytes) {
            std::fprintf(stderr, "blipblop: cannot read '%s'\n", g.replayPath.c_str());
            return false;
        }
        const std::string text(bytes->begin(), bytes->end());
        std::string error;
        auto parsed = bb::core::InputTimeline::parse(text, codeFromName, error);
        if (!parsed) {
            std::fprintf(stderr, "blipblop: %s: %s\n", g.replayPath.c_str(), error.c_str());
            return false;
        }
        g.timeline = std::move(*parsed);
        for (const auto& option : g.timeline.options) {
            applyLegacySwitch(option);
        }
        if (!g.seedGiven) {
            g.seed = g.timeline.seed;
        }
        g.deterministic = true;
    }
    if (!g.recordPath.empty()) {
        g.deterministic = true;
        g.timeline.seed = g.seed;
        if (g.cheats) {
            g.timeline.options.emplace_back("/cheat");
        }
        if (g.quiet) {
            g.timeline.options.emplace_back("/quiet");
        }
    }
    if (!g.hashPath.empty() && !g.deterministic) {
        std::fprintf(stderr, "blipblop: --hash needs --record, --replay or --deterministic\n");
        return false;
    }

    g.rng.seed(g.seed);

    if (g.deterministic) {
        // Milestones must reach logs even if the run crashes.
        std::setvbuf(stdout, nullptr, _IOLBF, 0);
    }

    return true;
}

void Session::start(bb::platform::Clock& clock, const bb::platform::System& system,
                    bb::platform::FileSystem& files) {
    State& g = *state_;
    g.clock = &clock;
    g.system = &system;
    g.files = &files;
    // Binary: the same bytes on every platform (no CRLF on Windows), so
    // fingerprints recorded anywhere compare and diff alike.
    if (!g.tracePath.empty()) {
        g.traceOut = std::fopen(g.tracePath.c_str(), "wb");
    }
    if (!g.hashPath.empty()) {
        g.hashOut = std::fopen(g.hashPath.c_str(), "wb");
        if (g.hashOut == nullptr) {
            std::fprintf(stderr, "blipblop: cannot write '%s'\n", g.hashPath.c_str());
        } else {
            std::fprintf(g.hashOut, "bbhash 1\n");
        }
    }
}

int Session::finish() {
    State& g = *state_;
    if (g.traceOut != nullptr) {
        std::fclose(g.traceOut);
        g.traceOut = nullptr;
    }
    if (g.hashOut != nullptr) {
        std::fprintf(g.hashOut, "final frames %llu %016llx ticks %llu %016llx\n",
                     static_cast<unsigned long long>(g.presented),
                     static_cast<unsigned long long>(g.frameChain.value()),
                     static_cast<unsigned long long>(g.ticks),
                     static_cast<unsigned long long>(g.stateChain.value()));
        std::fclose(g.hashOut);
        g.hashOut = nullptr;
    }
    if (!g.recordPath.empty()) {
        g.timeline.endFrame = g.frames.frame();
        const std::string text = g.timeline.serialize(nameFromCode);
        if (g.files == nullptr || !g.files->writeAll(g.recordPath, text.data(), text.size())) {
            std::fprintf(stderr, "blipblop: cannot write '%s'\n", g.recordPath.c_str());
            return 1;
        }
    }
    note("exit");
    if (g.deterministic) {
        std::printf("session: %llu frames presented, %llu ticks, frame hash %016llx, state hash %016llx\n",
                    static_cast<unsigned long long>(g.presented), static_cast<unsigned long long>(g.ticks),
                    static_cast<unsigned long long>(g.frameChain.value()),
                    static_cast<unsigned long long>(g.stateChain.value()));
    }
    if (g.headless && g.workFrames > 0) {
        // Against the desktop budget (migration.md 3.14): drawing 4 ms and
        // simulation 2 ms per frame.
        const double seconds = static_cast<double>(g.workSum) / 1e6;
        std::printf(
            "bench: %llu frames, %.2f ms per frame on average, %.2f ms at worst, %.0f ticks per second, "
            "peak memory %.1f MB\n",
            static_cast<unsigned long long>(g.workFrames),
            static_cast<double>(g.workSum) / 1000.0 / static_cast<double>(g.workFrames),
            static_cast<double>(g.workMax) / 1000.0,
            seconds > 0 ? static_cast<double>(g.ticks) / seconds : 0.0,
            g.system != nullptr ? static_cast<double>(g.system->peakMemoryBytes()) / (1024.0 * 1024.0) : 0.0);
    }
    return 0;
}

bool Session::deterministic() const {
    const State& g = *state_;
    return g.deterministic;
}
bool Session::headless() const {
    const State& g = *state_;
    return g.headless;
}
bool Session::smoothMotion() const {
    const State& g = *state_;
    return g.smoothMotion;
}
bool Session::safeMode() const {
    const State& g = *state_;
    return g.safe;
}
bool Session::cheats() const {
    const State& g = *state_;
    return g.cheats;
}
bool Session::quiet() const {
    const State& g = *state_;
    return g.quiet;
}
bool Session::quitRequested() const {
    const State& g = *state_;
    return g.quit;
}
void Session::requestQuit() {
    State& g = *state_;
    g.quit = true;
}
std::string Session::settingsPath() const {
    const State& g = *state_;
    return g.settingsPath;
}
std::string Session::logPath() const {
    const State& g = *state_;
    return g.logPath;
}

std::string Session::onlyLevel() const {
    return state_->onlyLevel;
}

int Session::onlyLevelKind() const {
    return static_cast<int>(state_->onlyLevelKind);
}
std::uint64_t Session::frame() const {
    const State& g = *state_;
    return g.frames.frame();
}

std::uint64_t Session::nowMicroseconds() const {
    const State& g = *state_;
    if (g.deterministic) {
        return g.frames.microseconds();
    }
    return g.clock != nullptr ? g.clock->microseconds() : 0;
}

void Session::onFramePresented(const bb::core::gfx::Pixmap& frame) {
    State& g = *state_;
    if (g.headless && g.clock != nullptr) {
        const std::uint64_t now = g.clock->microseconds();
        if (g.lastPresent != 0) {
            const std::uint64_t work = now - g.lastPresent;
            g.workSum += work;
            g.workMax = std::max(g.workMax, work);
            g.workFrames += 1;
        }
        g.lastPresent = now;
    }
    if (!g.deterministic) {
        // --max-frames also bounds normal runs (smoke tests of the real-time path).
        if (g.maxFrames != 0 && ++g.realFrames >= g.maxFrames) {
            g.quit = true;
        }
        return;
    }
    ++g.presented;
    g.frameChain.add(bb::core::gfx::fingerprint(frame));
    if (g.presented % g.hashEvery == 0) {
        writeCheckpoint("frame", g.presented, g.frameChain.value());
    }
    if (!g.dumpDir.empty() && g.presented >= g.dumpFrom && g.presented <= g.dumpTo &&
        (g.presented - g.dumpFrom) % g.dumpEvery == 0) {
        char path[1024];
        std::snprintf(path, sizeof(path), "%s/frame_%06llu.bmp", g.dumpDir.c_str(),
                      static_cast<unsigned long long>(g.presented));
        bb::core::gfx::writeBmp(frame, path);
    }
    g.frames.advanceFrame();
    if (!g.headless && g.clock != nullptr) {
        // Keep recorded and watched replays at real speed even when the
        // display does not refresh at 60 Hz.
        const std::uint64_t realUs = g.clock->microseconds();
        const std::uint64_t virtualUs = g.frames.microseconds();
        if (virtualUs > realUs + 1000) {
            const std::uint64_t wait = virtualUs - realUs;
            g.clock->sleep(wait > 100000 ? 100000 : wait);
        }
    }
    requestStopIfDone();
}

void Session::idle() {
    State& g = *state_;
    if (g.deterministic) {
        g.frames.advanceFrame();
        requestStopIfDone();
    } else if (g.clock != nullptr) {
        g.clock->sleep(5000);
    }
}

void Session::onTick(std::uint64_t stateHash) {
    State& g = *state_;
    if (!g.deterministic) {
        return;
    }
    ++g.ticks;
    g.stateChain.add(stateHash);
    if (g.ticks % g.hashEvery == 0) {
        writeCheckpoint("tick", g.ticks, g.stateChain.value());
    }
}

bool Session::filterInput(const InputSource& live, bool liveQuit, bool liveUnfocused, InputSink& visible) {
    State& g = *state_;
    if (!g.deterministic) {
        return liveQuit;
    }
    const std::uint64_t f = g.frames.frame();
    if (!g.recordPath.empty() && f != g.lastLatchedFrame) {
        bb::core::InputState state;
        live.forEachPressed([](std::uint32_t code,
                               void* context) { static_cast<bb::core::InputState*>(context)->press(code); },
                            &state);
        state.quit = liveQuit || g.liveQuitSeen;
        state.unfocused = liveUnfocused;
        g.timeline.record(f, state);
        g.lastLatchedFrame = f;
    } else if (liveQuit) {
        // Closing the window aborts a replay.
        g.stopRequested = true;
    }
    if (liveQuit) {
        g.liveQuitSeen = true;
    }

    const bb::core::InputState& state = g.timeline.at(f);
    visible.clear();
    for (const auto code : state.codes) {
        visible.press(code);
    }
    visible.setUnfocused(state.unfocused);
    if (state.quit) {
        g.stopRequested = true;
    }
    requestStopIfDone();
    return g.stopRequested;
}

std::FILE* Session::traceStream() const {
    const State& g = *state_;
    const std::uint64_t next = g.ticks + 1;
    if (g.traceOut != nullptr && next >= g.traceFrom && next <= g.traceTo) {
        return g.traceOut;
    }
    return nullptr;
}

void Session::note(const char* message) {
    State& g = *state_;
    if (g.deterministic) {
        std::printf("[frame %llu, tick %llu] %s\n", static_cast<unsigned long long>(g.frames.frame()),
                    static_cast<unsigned long long>(g.ticks), message);
    }
}

bb::core::Rng& Session::rng() {
    State& g = *state_;
    return g.rng;
}

}  // namespace bb::app
