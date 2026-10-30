#include "switch_system.hpp"

#include <switch.h>
#include <sys/stat.h>

#include <deque>

namespace bb::platform::sdl2::nx {

void screenSize(int& width, int& height) {
    if (appletGetOperationMode() == AppletOperationMode_Handheld) {
        width = 1280;
        height = 720;
    } else {
        width = 1920;
        height = 1080;
    }
}

std::string preferencesDirectory() {
    // mkdir() fails harmlessly when the folder is there.
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/blipblop", 0777);
    return "sdmc:/switch/blipblop/";
}

void showError(const std::string& title, const std::string& message) {
    // The dialog shows the short text, its "Details" page the whole one.
    const std::string details = title + "\n\n" + message;
    ErrorApplicationConfig config;
    if (R_SUCCEEDED(errorApplicationCreate(&config, message.c_str(), details.c_str()))) {
        errorApplicationShow(&config);
    }
}

void requestPlayers(int count) {
    HidLaControllerSupportArg arg;
    hidLaCreateControllerSupportArg(&arg);
    arg.hdr.player_count_min = static_cast<s8>(count);
    arg.hdr.player_count_max = static_cast<s8>(count);
    // Joy-Con pairs and single Joy-Cons held sideways (the style set and the
    // horizontal hold SDL configures) are both allowed.
    arg.hdr.enable_permit_joy_dual = 1;
    hidLaShowControllerSupport(nullptr, &arg);
}

std::string languageCode() {
    std::string code = "en";
    if (R_SUCCEEDED(setInitialize())) {
        u64 languageCode = 0;
        SetLanguage language = SetLanguage_ENUS;
        if (R_SUCCEEDED(setGetSystemLanguage(&languageCode)) &&
            R_SUCCEEDED(setMakeLanguage(languageCode, &language)) &&
            (language == SetLanguage_FR || language == SetLanguage_FRCA)) {
            code = "fr";
        }
        setExit();
    }
    return code;
}

std::uint64_t memoryInUse() {
    u64 used = 0;
    if (R_FAILED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0))) {
        return 0;
    }
    return used;
}

struct FocusWatch::State {
    AppletHookCookie cookie{};
    std::deque<bool> changes;  // true: gained, false: lost
    bool focused = true;
};

namespace {

void onAppletHook(AppletHookType hook, void* param) {
    if (hook != AppletHookType_OnFocusState) {
        return;
    }
    auto* state = static_cast<FocusWatch::State*>(param);
    const bool focused = appletGetFocusState() == AppletFocusState_InFocus;
    if (focused != state->focused) {
        state->focused = focused;
        state->changes.push_back(focused);
    }
}

}  // namespace

FocusWatch::FocusWatch() : state_(new State) {
    appletHook(&state_->cookie, onAppletHook, state_);
}

FocusWatch::~FocusWatch() {
    appletUnhook(&state_->cookie);
    delete state_;
}

bool FocusWatch::take(bool& focused) {
    if (state_->changes.empty()) {
        return false;
    }
    focused = state_->changes.front();
    state_->changes.pop_front();
    return true;
}

}  // namespace bb::platform::sdl2::nx
