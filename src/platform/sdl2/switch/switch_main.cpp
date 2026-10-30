// Process entry point of Switch builds, in place of sdl2_main.cpp: the
// system's services the game needs before SDL starts, then bbMain
// (switch-port.md, 5).

#include <arpa/inet.h>  // struct in_addr, for nxlink's host address
#include <switch.h>
#include <unistd.h>

#include <string>

#include "bb/platform/entry.hpp"
#include "switch_system.hpp"

int main(int argc, char* argv[]) {
    // The RomFS holds the runtime pack: the one appended to the .nro, or the
    // installed title's.
    romfsInit();

    // Started by nxlink (development): the game's standard output and error,
    // where the log's warnings go, on the PC's terminal.
    bool linked = false;
    if (__nxlink_host.s_addr != 0 && R_SUCCEEDED(socketInitializeDefault())) {
        linked = nxlinkStdio() >= 0;
        if (!linked) {
            socketExit();
        }
    }

    // Closing the game from the HOME menu then reaches it as a quit request
    // (SDL_QUIT), so that it saves its settings and hi-scores.
    appletLockExit();

    // The settings, hi-scores and log live there, whatever started the game:
    // an installed title has no directory of its own, a .nro has its folder.
    const std::string preferences = bb::platform::sdl2::nx::preferencesDirectory();
    chdir(preferences.c_str());

    const int status = bbMain(argc, argv);

    appletUnlockExit();
    if (linked) {
        socketExit();
    }
    romfsExit();
    return status;
}
