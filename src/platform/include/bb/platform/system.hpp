#pragma once

#include <cstdint>
#include <string>

namespace bb::platform {

// Operating-system services that are not files, input, audio or video.
class System {
public:
    virtual ~System() = default;

    // Shows a fatal error to the user in a message box, where there is a
    // screen (not in headless runs). The caller logs the message.
    virtual void showError(const std::string& title, const std::string& message) = 0;

    // Same for a problem the program can continue after.
    virtual void showWarning(const std::string& title, const std::string& message) = 0;

    // The process's peak resident memory in bytes, 0 if unknown (for the
    // benchmark line of headless runs). On the Switch, the memory in use.
    virtual std::uint64_t peakMemoryBytes() const = 0;

    // The language the user chose for the system, as two lower-case
    // letters ("fr", "en"), "" when unknown: the game's language on its
    // first run.
    virtual std::string languageCode() const = 0;
};

}  // namespace bb::platform
