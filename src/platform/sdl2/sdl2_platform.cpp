#include "bb/platform/sdl2/sdl2_platform.hpp"

#include <SDL.h>

#include "bb/core/audio/mixer.hpp"
#include "bb/core/gfx/viewport.hpp"
#include "bb/core/log.hpp"

#if defined(__SWITCH__)
#include "switch/switch_system.hpp"
#else
#include <sys/resource.h>
#endif

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace bb::platform::sdl2 {

namespace {

class Sdl2Clock final : public Clock {
public:
    Sdl2Clock() : start_(SDL_GetPerformanceCounter()), frequency_(SDL_GetPerformanceFrequency()) {}

    std::uint64_t microseconds() const override {
        const std::uint64_t ticks = SDL_GetPerformanceCounter() - start_;
        return ticks / frequency_ * 1000000u + (ticks % frequency_) * 1000000u / frequency_;
    }

    void sleep(std::uint64_t microseconds) override {
        SDL_Delay(static_cast<Uint32>((microseconds + 999u) / 1000u));
    }

private:
    std::uint64_t start_;
    std::uint64_t frequency_;
};

class Sdl2System final : public System {
public:
    explicit Sdl2System(bool headless) : headless_(headless) {}

    void showError(const std::string& title, const std::string& message) override {
        show(SDL_MESSAGEBOX_ERROR, title, message);
    }

    void showWarning(const std::string& title, const std::string& message) override {
        show(SDL_MESSAGEBOX_WARNING, title, message);
    }

    std::uint64_t peakMemoryBytes() const override {
#if defined(__SWITCH__)
        return nx::memoryInUse();  // the Switch keeps no peak
#else
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return 0;
        }
#if defined(__APPLE__)
        return static_cast<std::uint64_t>(usage.ru_maxrss);  // bytes on macOS
#else
        return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;  // kilobytes on Linux
#endif
#endif
    }

    std::string languageCode() const override {
#if defined(__SWITCH__)
        return nx::languageCode();
#else
        std::string code;
        if (SDL_Locale* locales = SDL_GetPreferredLocales()) {
            if (locales[0].language != nullptr) {
                code = locales[0].language;
            }
            SDL_free(locales);
        }
        return code;
#endif
    }

private:
    void show(Uint32 flags, const std::string& title, const std::string& message) const {
        if (headless_) {
            return;
        }
#if defined(__SWITCH__)
        // SDL has no message boxes there: the system's error applet.
        (void)flags;
        nx::showError(title, message);
#else
        if (SDL_ShowSimpleMessageBox(flags, title.c_str(), message.c_str(), nullptr) != 0) {
            SDL_Log("%s: %s (no message box: %s)", title.c_str(), message.c_str(), SDL_GetError());
        }
#endif
    }

    bool headless_;
};

// Files through SDL_RWops (UTF-8 paths on every system); SDL 2 cannot list
// directories, the C++ library does (from UTF-8 paths as well).
class Sdl2File final : public core::ReadableFile {
public:
    Sdl2File(SDL_RWops* rw, std::uint64_t size) noexcept : rw_(rw), size_(size) {}
    ~Sdl2File() override { SDL_RWclose(rw_); }
    Sdl2File(const Sdl2File&) = delete;
    Sdl2File& operator=(const Sdl2File&) = delete;

    std::uint64_t size() const override { return size_; }

    bool readAt(std::uint64_t offset, void* out, std::size_t count) override {
        if (count == 0) {
            return true;
        }
        if (offset > size_ || SDL_RWseek(rw_, static_cast<Sint64>(offset), RW_SEEK_SET) < 0) {
            return false;
        }
        return SDL_RWread(rw_, out, 1, count) == count;
    }

private:
    SDL_RWops* rw_;
    std::uint64_t size_;
};

class Sdl2FileSystem final : public FileSystem {
public:
    std::unique_ptr<core::ReadableFile> open(const std::string& path) const override {
        SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
        if (rw == nullptr) {
            return nullptr;
        }
        const Sint64 size = SDL_RWsize(rw);
        if (size < 0) {
            SDL_RWclose(rw);
            return nullptr;
        }
        return std::make_unique<Sdl2File>(rw, static_cast<std::uint64_t>(size));
    }

    Kind kind(const std::string& path) const override {
        std::error_code ec;
        const std::filesystem::file_status status = std::filesystem::status(std::filesystem::u8path(path), ec);
        if (std::filesystem::is_regular_file(status)) {
            return Kind::File;
        }
        if (std::filesystem::is_directory(status)) {
            return Kind::Directory;
        }
        return Kind::Missing;  // nor the other kinds (devices, sockets)
    }

    std::vector<std::string> list(const std::string& directory) const override {
        std::vector<std::string> names;
        std::error_code ec;
        for (std::filesystem::directory_iterator it(std::filesystem::u8path(directory), ec), end;
             !ec && it != end; it.increment(ec)) {
            names.push_back(it->path().filename().u8string());
        }
        return names;
    }

    bool writeAll(const std::string& path, const void* data, std::size_t size) override {
        SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "wb");
        if (rw == nullptr) {
            return false;
        }
        const bool written = size == 0 || SDL_RWwrite(rw, data, 1, size) == size;
        return SDL_RWclose(rw) == 0 && written;
    }

    std::string baseDirectory() const override {
        char* path = SDL_GetBasePath();  // romfs:/ on the Switch
        if (path == nullptr) {
            return {};
        }
        std::string result(path);
        SDL_free(path);
        return result;
    }

    std::string preferencesDirectory() const override {
#if defined(__SWITCH__)
        // SDL's is the current directory: the folder of the .nro, or none
        // for an installed title.
        return nx::preferencesDirectory();
#else
        char* path = SDL_GetPrefPath("LOADED Studio", "Blip and Blop");
        if (path == nullptr) {
            return {};
        }
        std::string result(path);
        SDL_free(path);
        return result;
#endif
    }
};

class Sdl2Input final : public InputSource {
public:
    static constexpr int kMaxPads = 8;

    Sdl2Input() {
        SDL_JoystickEventState(SDL_ENABLE);
        SDL_GameControllerEventState(SDL_ENABLE);
        // Pads already connected at start-up get slots 0, 1, ... in SDL's
        // device order; they also arrive as SDL_JOYDEVICEADDED events. On
        // the Switch SDL always has 8, the system's players 1 to 8.
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            open(i);
        }
    }

    ~Sdl2Input() override {
        for (auto& pad : pads_) {
            close(pad);
        }
    }

    bool poll(InputEvent& out) override {
        // A focus loss and its return in the same update would cancel out
        // (DeviceInput): the return waits for the next update. That is the
        // case of a game the Switch suspended in the background, which
        // reads both when it comes back.
        if (pendingFocusGain_ && !focusLostInUpdate_) {
            pendingFocusGain_ = false;
            out = {};
            out.type = InputEvent::Type::FocusGained;
            return true;
        }
        while (next(out)) {
            if (out.type == InputEvent::Type::FocusLost) {
                focusLostInUpdate_ = true;
                pendingFocusGain_ = false;
            } else if (out.type == InputEvent::Type::FocusGained && focusLostInUpdate_) {
                pendingFocusGain_ = true;
                continue;
            }
            return true;
        }
        focusLostInUpdate_ = false;
        return false;
    }

    int padSlots() const override { return slots_; }
    bool padConnected(int slot) const override {
        return slot >= 0 && slot < kMaxPads && pads_[slot].handle != nullptr;
    }
    std::string padName(int slot) const override {
        return padConnected(slot) ? pads_[slot].name : std::string();
    }
    bool padIsController(int slot) const override {
        return padConnected(slot) && pads_[slot].controller != nullptr;
    }

    void requestPlayers(int count) override {
#if defined(__SWITCH__)
        nx::requestPlayers(count);
#else
        (void)count;
#endif
    }

    bool beginTextEntry() override {
        if (!SDL_HasScreenKeyboardSupport()) {
            return false;
        }
        textEntry_ = true;
        SDL_StartTextInput();
        return true;
    }

    bool textEntryShown() const override { return textEntry_ && SDL_IsTextInputActive() == SDL_TRUE; }

private:
    // The triggers are axes of 0 to 32767; as buttons they are held from
    // half their travel and released under a quarter.
    static constexpr int kTriggerPress = 16384;
    static constexpr int kTriggerRelease = 8192;

    struct Pad {
        SDL_Joystick* handle = nullptr;
        SDL_GameController* controller = nullptr;  // for a recognised game controller
        SDL_JoystickID id = -1;
        std::string name;
        int dpad = 0;  // InputEvent::kHat* bits of the controller's D-pad
        bool leftTrigger = false;
        bool rightTrigger = false;
    };

    // The next event of the system, translated; false when there is none.
    bool next(InputEvent& out) {
#if defined(__SWITCH__)
        // HOME and sleep, seen while SDL pumped its events.
        if (bool focused = false; focus_.take(focused)) {
            out = {};
            out.type = focused ? InputEvent::Type::FocusGained : InputEvent::Type::FocusLost;
            return true;
        }
#endif
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    out = {};
                    out.type = InputEvent::Type::Quit;
                    return true;
                case SDL_APP_WILLENTERBACKGROUND:
                case SDL_APP_DIDENTERFOREGROUND:
                    // Suspend and resume (mobile and console systems).
                    out = {};
                    out.type = e.type == SDL_APP_WILLENTERBACKGROUND ? InputEvent::Type::FocusLost
                                                                     : InputEvent::Type::FocusGained;
                    return true;
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                        e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                        out = {};
                        out.type = e.window.event == SDL_WINDOWEVENT_FOCUS_LOST
                                       ? InputEvent::Type::FocusLost
                                       : InputEvent::Type::FocusGained;
                        return true;
                    }
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    out = {};
                    out.type = e.type == SDL_KEYDOWN ? InputEvent::Type::KeyDown : InputEvent::Type::KeyUp;
                    out.code = core::input::fromSdl2Key(e.key.keysym.sym);
                    return true;
                case SDL_TEXTINPUT:
                    // Only for the on-screen keyboard: desktop systems send
                    // text for every key typed.
                    if (textEntry_) {
                        out = {};
                        out.type = InputEvent::Type::Text;
                        out.text = e.text.text;
                        return true;
                    }
                    break;
                case SDL_JOYDEVICEADDED: {
                    const int slot = open(e.jdevice.which);
                    if (slot >= 0) {
                        out = {};
                        out.type = InputEvent::Type::PadAdded;
                        out.pad = slot;
                        return true;
                    }
                    break;
                }
                case SDL_JOYDEVICEREMOVED: {
                    const int slot = slotOf(e.jdevice.which);
                    if (slot >= 0) {
                        close(pads_[slot]);
                        out = {};
                        out.type = InputEvent::Type::PadRemoved;
                        out.pad = slot;
                        return true;
                    }
                    break;
                }
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP: {
                    const int slot = joystickSlotOf(e.jbutton.which);
                    if (slot >= 0) {
                        out = {};
                        out.type = e.type == SDL_JOYBUTTONDOWN ? InputEvent::Type::PadButtonDown
                                                               : InputEvent::Type::PadButtonUp;
                        out.pad = slot;
                        out.code = e.jbutton.button;
                        return true;
                    }
                    break;
                }
                case SDL_JOYAXISMOTION: {
                    const int slot = joystickSlotOf(e.jaxis.which);
                    if (slot >= 0) {
                        out = {};
                        out.type = InputEvent::Type::PadAxis;
                        out.pad = slot;
                        out.axis = e.jaxis.axis;
                        out.value = e.jaxis.value;
                        return true;
                    }
                    break;
                }
                case SDL_JOYHATMOTION: {
                    const int slot = joystickSlotOf(e.jhat.which);
                    if (slot >= 0) {
                        out = {};
                        out.type = InputEvent::Type::PadHat;
                        out.pad = slot;
                        out.axis = e.jhat.hat;
                        out.value = (e.jhat.value & SDL_HAT_UP ? InputEvent::kHatUp : 0) |
                                    (e.jhat.value & SDL_HAT_RIGHT ? InputEvent::kHatRight : 0) |
                                    (e.jhat.value & SDL_HAT_DOWN ? InputEvent::kHatDown : 0) |
                                    (e.jhat.value & SDL_HAT_LEFT ? InputEvent::kHatLeft : 0);
                        return true;
                    }
                    break;
                }
                // Game controllers: SDL's button numbers (codes.hpp), the
                // D-pad as a hat, the left stick as axes 0 and 1, the
                // triggers as buttons.
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP: {
                    const int slot = controllerSlotOf(e.cbutton.which);
                    if (slot < 0) {
                        break;
                    }
                    const bool down = e.type == SDL_CONTROLLERBUTTONDOWN;
                    if (const int bit = dpadBit(e.cbutton.button); bit != 0) {
                        Pad& pad = pads_[slot];
                        pad.dpad = down ? (pad.dpad | bit) : (pad.dpad & ~bit);
                        out = {};
                        out.type = InputEvent::Type::PadHat;
                        out.pad = slot;
                        out.value = pad.dpad;
                        return true;
                    }
                    out = {};
                    out.type = down ? InputEvent::Type::PadButtonDown : InputEvent::Type::PadButtonUp;
                    out.pad = slot;
                    out.code = e.cbutton.button;
                    return true;
                }
                case SDL_CONTROLLERAXISMOTION: {
                    const int slot = controllerSlotOf(e.caxis.which);
                    if (slot < 0) {
                        break;
                    }
                    if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX || e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        out = {};
                        out.type = InputEvent::Type::PadAxis;
                        out.pad = slot;
                        out.axis = e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ? 0 : 1;
                        out.value = e.caxis.value;
                        return true;
                    }
                    if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
                        e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                        const bool left = e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT;
                        bool& held = left ? pads_[slot].leftTrigger : pads_[slot].rightTrigger;
                        const bool now = e.caxis.value > (held ? kTriggerRelease : kTriggerPress);
                        if (now != held) {
                            held = now;
                            out = {};
                            out.type = now ? InputEvent::Type::PadButtonDown : InputEvent::Type::PadButtonUp;
                            out.pad = slot;
                            out.code =
                                left ? core::input::kPadButtonLeftTrigger : core::input::kPadButtonRightTrigger;
                            return true;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
        // The on-screen keyboard closed: its text, if any, came before.
        if (textEntry_ && SDL_IsTextInputActive() == SDL_FALSE) {
            textEntry_ = false;
        }
        return false;
    }

    static int dpadBit(Uint8 button) {
        switch (button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                return InputEvent::kHatUp;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                return InputEvent::kHatRight;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                return InputEvent::kHatDown;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                return InputEvent::kHatLeft;
            default:
                return 0;
        }
    }

    static void close(Pad& pad) {
        if (pad.controller != nullptr) {
            SDL_GameControllerClose(pad.controller);
        } else if (pad.handle != nullptr) {
            SDL_JoystickClose(pad.handle);
        }
        pad = Pad{};
    }

    // The slot of a pad whose joystick events are used (not a game
    // controller, whose controller events are used instead), or -1.
    int joystickSlotOf(SDL_JoystickID id) const {
        const int slot = slotOf(id);
        return slot >= 0 && pads_[slot].controller == nullptr ? slot : -1;
    }

    int controllerSlotOf(SDL_JoystickID id) const {
        const int slot = slotOf(id);
        return slot >= 0 && pads_[slot].controller != nullptr ? slot : -1;
    }

    int slotOf(SDL_JoystickID id) const {
        for (int i = 0; i < kMaxPads; ++i) {
            if (pads_[i].handle != nullptr && pads_[i].id == id) {
                return i;
            }
        }
        return -1;
    }

    // Opens device `index` in the first free slot, unless it is already open.
    int open(int index) {
        const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
        if (slotOf(id) >= 0) {
            return -1;
        }
        for (int i = 0; i < kMaxPads; ++i) {
            if (pads_[i].handle == nullptr) {
                // A pad SDL knows the layout of (its mapping database) is
                // opened as a game controller, others as a joystick.
                SDL_GameController* controller =
                    SDL_IsGameController(index) ? SDL_GameControllerOpen(index) : nullptr;
                SDL_Joystick* handle = controller != nullptr ? SDL_GameControllerGetJoystick(controller)
                                                             : SDL_JoystickOpen(index);
                if (handle == nullptr) {
                    return -1;
                }
                const char* name =
                    controller != nullptr ? SDL_GameControllerName(controller) : SDL_JoystickName(handle);
                pads_[i] = Pad{handle, controller, SDL_JoystickInstanceID(handle), name != nullptr ? name : "",
                               0, false, false};
                if (i + 1 > slots_) {
                    slots_ = i + 1;
                }
                return i;
            }
        }
        return -1;
    }

    Pad pads_[kMaxPads];
    int slots_ = 0;
    bool textEntry_ = false;  // the on-screen keyboard was asked for
    bool focusLostInUpdate_ = false;
    bool pendingFocusGain_ = false;
#if defined(__SWITCH__)
    nx::FocusWatch focus_;
#endif
};

class Sdl2Audio final : public Audio {
public:
    // The output the 2014 port asked SDL_mixer for: 44.1 kHz stereo 16-bit,
    // 1024 frames per callback. The mixer runs at the rate the device has
    // (48 kHz on the Switch), so that SDL converts nothing.
    static constexpr int kRate = 44100;

    Sdl2Audio() {
        SDL_AudioSpec wanted{};
        wanted.freq = kRate;
        wanted.format = AUDIO_S16SYS;
        wanted.channels = 2;
        wanted.samples = 1024;
        wanted.callback = &Sdl2Audio::callback;
        wanted.userdata = this;
        SDL_AudioSpec obtained{};
        // Opened paused: the callback does not run before the mixer exists.
        device_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        if (device_ == 0) {
            SDL_Log("SDL_OpenAudioDevice: %s", SDL_GetError());
            mixer_ = std::make_unique<core::audio::Mixer>(kRate);
            return;
        }
        rate_ = obtained.freq;
        mixer_ = std::make_unique<core::audio::Mixer>(rate_);
        SDL_PauseAudioDevice(device_, 0);
    }

    ~Sdl2Audio() override {
        if (device_ != 0) {
            SDL_CloseAudioDevice(device_);  // no callback runs after this
        }
    }

    Sdl2Audio(const Sdl2Audio&) = delete;
    Sdl2Audio& operator=(const Sdl2Audio&) = delete;

    bool available() const override { return device_ != 0; }

    Sample loadSample(const std::uint8_t* data, std::size_t size) override {
        if (device_ == 0) {
            return -1;
        }
        // Converted outside the lock: the callback goes on meanwhile.
        std::vector<std::int16_t> samples;
        if (!core::audio::decodeWav(data, size, rate_, samples)) {
            SDL_Log("loadSample: not a PCM WAV file");
            return -1;
        }
        const Lock lock(device_);
        return mixer_->addSound(std::move(samples));
    }

    void freeSample(Sample sample) override {
        const Lock lock(device_);
        mixer_->freeSound(sample);
    }

    int play(Sample sample, bool loop) override {
        const Lock lock(device_);
        return mixer_->play(sample, loop);
    }

    void stop(int voice) override {
        const Lock lock(device_);
        mixer_->stop(voice);
    }

    Music openMusic(std::vector<std::uint8_t> file) override {
        if (device_ == 0) {
            return -1;
        }
        auto bytes = std::make_unique<std::vector<std::uint8_t>>(std::move(file));
        auto stream = core::audio::openMusicStream(*bytes);
        if (!stream) {
            SDL_Log("openMusic: neither Ogg Vorbis nor MP3");
            return -1;
        }
        const Lock lock(device_);
        return mixer_->addMusic(std::move(bytes), std::move(stream));
    }

    void freeMusic(Music music) override {
        const Lock lock(device_);
        mixer_->freeMusic(music);
    }

    void playMusic(Music music, bool loop) override {
        const Lock lock(device_);
        mixer_->playMusic(music, loop);
    }

    void stopMusic() override {
        const Lock lock(device_);
        mixer_->stopMusic();
    }

    bool voicePlaying(int voice) {
        const Lock lock(device_);
        return mixer_->playing(voice);
    }

private:
    // The audio callback and these calls never run at the same time.
    class Lock {
    public:
        explicit Lock(SDL_AudioDeviceID device) : device_(device) {
            if (device_ != 0) {
                SDL_LockAudioDevice(device_);
            }
        }
        ~Lock() {
            if (device_ != 0) {
                SDL_UnlockAudioDevice(device_);
            }
        }
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;

    private:
        SDL_AudioDeviceID device_;
    };

    static void SDLCALL callback(void* self, Uint8* stream, int bytes) {
        static_cast<Sdl2Audio*>(self)->mixer_->mix(reinterpret_cast<std::int16_t*>(stream), bytes / 4);
    }

    SDL_AudioDeviceID device_ = 0;
    int rate_ = kRate;
    std::unique_ptr<core::audio::Mixer> mixer_;
};

const char* filterName(core::gfx::Filter filter) {
    switch (filter) {
        case core::gfx::Filter::Sharp:
            return "sharp";
        case core::gfx::Filter::Crisp:
            return "crisp";
        case core::gfx::Filter::Smooth:
            return "smooth";
    }
    return "?";
}

class Sdl2Display final : public Display {
public:
    Sdl2Display(SDL_Window* window, SDL_Renderer* renderer, SDL_Texture* texture, int width, int height)
        : window_(window), renderer_(renderer), texture_(texture), width_(width), height_(height) {
        SDL_RendererInfo info;
        vsync_ = renderer_ != nullptr && SDL_GetRendererInfo(renderer_, &info) == 0 &&
                 (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
        if (renderer_ != nullptr) {
            SDL_GetRendererOutputSize(renderer_, &outputWidth_, &outputHeight_);
        }
    }

    ~Sdl2Display() override {
        if (enlarged_ != nullptr) {
            SDL_DestroyTexture(enlarged_);
        }
        if (texture_ != nullptr) {
            SDL_DestroyTexture(texture_);
        }
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
    }

    void present(const core::gfx::Pixmap& frame) override {
        if (renderer_ == nullptr || frame.width() != width_ || frame.height() != height_) {
            return;
        }
        // The output follows the window: resized on the desktop, 1280x720
        // or 1920x1080 as the Switch is undocked or docked.
        int w = 0;
        int h = 0;
        SDL_GetRendererOutputSize(renderer_, &w, &h);
        if (w != outputWidth_ || h != outputHeight_) {
            outputWidth_ = w;
            outputHeight_ = h;
            core::Log::info("Display: " + describe());
        }
        const core::gfx::Viewport view = viewport();
        const SDL_Rect picture{view.picture.x, view.picture.y, view.picture.w, view.picture.h};

        // One upload per frame; the frame is opaque, so no blending.
        SDL_UpdateTexture(texture_, nullptr, frame.data(), static_cast<int>(frame.pitch()));
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        if (view.prescale > 1 && enlarge(view.prescale)) {
            // CRISP: sharp pixels up to a whole multiple, then filtered down.
            SDL_SetTextureScaleMode(texture_, SDL_ScaleModeNearest);
            SDL_SetRenderTarget(renderer_, enlarged_);
            SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
            SDL_SetRenderTarget(renderer_, nullptr);
            SDL_RenderClear(renderer_);
            SDL_RenderCopy(renderer_, enlarged_, nullptr, &picture);
        } else {
            SDL_SetTextureScaleMode(texture_, view.smooth || view.prescale > 1 ? SDL_ScaleModeLinear
                                                                               : SDL_ScaleModeNearest);
            SDL_RenderClear(renderer_);
            SDL_RenderCopy(renderer_, texture_, nullptr, &picture);
        }
        SDL_RenderPresent(renderer_);
    }

    void setScaling(bool integer, core::gfx::Filter filter) override {
        integer_ = integer;
        filter_ = filter;
    }

    int refreshRate() const override {
        SDL_DisplayMode mode;
        if (window_ != nullptr && SDL_GetWindowDisplayMode(window_, &mode) == 0) {
            return mode.refresh_rate;
        }
        return 0;
    }

    std::string describe() const override {
        if (renderer_ == nullptr) {
            return {};
        }
        int w = 0;
        int h = 0;
        SDL_GetWindowSize(window_, &w, &h);
        SDL_RendererInfo info;
        const char* name = SDL_GetRendererInfo(renderer_, &info) == 0 ? info.name : "?";
        const core::gfx::Viewport view = viewport();
        std::string text = "window " + std::to_string(w) + "x" + std::to_string(h) + ", drawn at " +
                           std::to_string(outputWidth_) + "x" + std::to_string(outputHeight_) +
                           " pixels, renderer " + name + ", picture " + std::to_string(view.picture.w) + "x" +
                           std::to_string(view.picture.h) + " at " + std::to_string(view.picture.x) + "," +
                           std::to_string(view.picture.y) + ", scaling " + (integer_ ? "integer" : "fit") +
                           ", filter " + filterName(filter_);
        if (view.prescale > 1) {
            text += " (x" + std::to_string(view.prescale) + " first)";
        }
        return text;
    }

    bool vsync() const override { return vsync_; }

    bool readBack(core::gfx::Pixmap& out) const {
        if (renderer_ == nullptr) {
            return false;
        }
        int w = 0;
        int h = 0;
        if (SDL_GetRendererOutputSize(renderer_, &w, &h) != 0) {
            return false;
        }
        out = core::gfx::Pixmap(w, h);
        return SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ABGR8888, out.data(),
                                    static_cast<int>(out.pitch())) == 0;
    }

private:
    core::gfx::Viewport viewport() const {
        return core::gfx::viewport(width_, height_, outputWidth_, outputHeight_, integer_, filter_);
    }

    // The texture CRISP enlarges the frame into, `factor` times its size;
    // false when the renderer cannot draw into a texture (CRISP is then
    // drawn smoothed).
    bool enlarge(int factor) {
        if (enlarged_ != nullptr && enlargedFactor_ == factor) {
            return true;
        }
        if (enlarged_ != nullptr) {
            SDL_DestroyTexture(enlarged_);
            enlarged_ = nullptr;
        }
        if (SDL_RenderTargetSupported(renderer_) != SDL_TRUE) {
            return false;
        }
        enlarged_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                      width_ * factor, height_ * factor);
        if (enlarged_ == nullptr) {
            SDL_Log("CRISP texture %dx%d: %s", width_ * factor, height_ * factor, SDL_GetError());
            return false;
        }
        SDL_SetTextureBlendMode(enlarged_, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(enlarged_, SDL_ScaleModeLinear);
        enlargedFactor_ = factor;
        return true;
    }

    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
    SDL_Texture* enlarged_ = nullptr;
    int enlargedFactor_ = 0;
    int width_;
    int height_;
    int outputWidth_ = 0;
    int outputHeight_ = 0;
    bool integer_ = false;
    core::gfx::Filter filter_ = core::gfx::Filter::Crisp;
    bool vsync_ = false;
};

class Sdl2Platform final : public Platform {
public:
    explicit Sdl2Platform(bool headless) : headless_(headless), system_(headless) {}
    ~Sdl2Platform() override { SDL_Quit(); }

    std::unique_ptr<Display> createDisplay(const DisplayOptions& options, std::string& error) override {
        const bool headless = headless_ || options.headless;
        int width = options.width;
        int height = options.height;
#if defined(__SWITCH__)
        // The window is the screen: created at the size of the console's
        // mode (SDL would otherwise let the system stretch a 640x480 one to
        // 16:9), and resizable, so that SDL resizes it when the console is
        // docked or undocked.
        if (!headless) {
            nx::screenSize(width, height);
        }
#endif
        SDL_Window* window =
            SDL_CreateWindow(options.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                             height,
                             // Drawn at the screen's resolution (Retina); the
                             // frame keeps its 4:3 shape when resized.
                             headless ? SDL_WINDOW_HIDDEN
                                      : SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (window == nullptr) {
            error = SDL_GetError();
            return nullptr;
        }
        if (headless) {
            // Headless runs never present anything.
            return std::make_unique<Sdl2Display>(window, nullptr, nullptr, options.width, options.height);
        }
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer == nullptr) {
            SDL_Log("Accelerated renderer unavailable (%s), using the software renderer", SDL_GetError());
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }
        if (renderer == nullptr) {
            error = SDL_GetError();
            SDL_DestroyWindow(window);
            return nullptr;
        }
        // Pixmap pixels are 0xAABBGGRR integers: SDL's ABGR8888 on any byte order.
        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                                 SDL_TEXTUREACCESS_STREAMING, options.width, options.height);
        if (texture == nullptr) {
            error = SDL_GetError();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            return nullptr;
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
        auto display =
            std::make_unique<Sdl2Display>(window, renderer, texture, options.width, options.height);
        display->setScaling(options.integerScaling, options.filter);
        return display;
    }

    Clock& clock() override { return clock_; }
    InputSource& input() override { return input_; }
    Audio& audio() override { return audio_; }
    FileSystem& files() override { return files_; }
    System& system() override { return system_; }

private:
    bool headless_;
    Sdl2Clock clock_;
    Sdl2System system_;
    Sdl2FileSystem files_;
    Sdl2Input input_;
    Sdl2Audio audio_;
};

}  // namespace

bool readBack(Display& display, core::gfx::Pixmap& out) {
    // Every Display this backend creates is an Sdl2Display.
    return static_cast<const Sdl2Display&>(display).readBack(out);
}

bool voicePlaying(Audio& audio, int voice) {
    // Every Audio this backend creates is an Sdl2Audio.
    return static_cast<Sdl2Audio&>(audio).voicePlaying(voice);
}

std::unique_ptr<Platform> createPlatform(const Options& options, std::string& error) {
    if (options.headless) {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    }
    // Every pad by position, Nintendo's too (SDL 2 lays them out by label by
    // default): the bottom button is SDL's A everywhere, which the Switch
    // calls B (codes.hpp).
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS |
                 SDL_INIT_TIMER) != 0) {
        error = SDL_GetError();
        return nullptr;
    }
    return std::make_unique<Sdl2Platform>(options.headless);
}

}  // namespace bb::platform::sdl2
