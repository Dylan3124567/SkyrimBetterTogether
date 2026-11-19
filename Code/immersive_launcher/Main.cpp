
#include "Utils/Error.h"
#include "launcher.h"
#include <Windows.h>
#include <combaseapi.h>
#include <base/threading/ThreadUtils.h>

#include "utils/ComUtils.h"
#include "script_extender/SEMemoryBlock.h"

#include <TiltedCore/Platform.hpp>

#include <filesystem>
#include <string>

extern void CoreStubsInit();
extern void ApplyLoadOrder();

extern "C"
{
    __declspec(dllexport) int NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

auto kSystemPreloadDlls = {
    L"\\dinput8.dll",   // < Skyrim early init hook
    L"\\dsound.dll",    // < breaks DSound init in game code
    L"\\nvspcap64.dll", // < Nvidia overlay, needs to be loaded before d3d11 device creation
                        // < X360CE v3 is buggy with COM hooks
    L"\\xinput9_1_0.dll", L"\\xinput1_1.dll", L"\\xinput1_2.dll", L"\\xinput1_3.dll", L"\\xinput1_4.dll", L"\\version.dll"};

// Asks BetterTogetherUpdater, shipped beside the launcher, whether a newer release is out. It shows the player the update
// box, and returns 2 once they chose to update: it then waits for this process to exit, installs and starts the game again.
static bool StartedUpdate(int argc, char** argv)
{
    wchar_t launcher[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, launcher, MAX_PATH))
        return false;

    const std::filesystem::path cLauncher = launcher;
    const std::filesystem::path cUpdater = cLauncher.parent_path() / L"BetterTogetherUpdater.exe";
    wchar_t flag[8]{};
    // Tests drive the game through the playtest bridge, and an update box would wait for a click nobody makes
    if (!std::filesystem::exists(cUpdater) || GetEnvironmentVariableW(L"STR_PLAYTEST", flag, 8) || GetEnvironmentVariableW(L"STR_NO_UPDATE", flag, 8))
        return false;

    // The launcher lives in Data\SkyrimTogetherReborn, unless --exePath points at another game
    std::filesystem::path game = cLauncher.parent_path().parent_path().parent_path();
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (_stricmp(argv[i], "--exePath") == 0)
            game = std::filesystem::path(argv[i + 1]).parent_path();
    }

    // Everything after the program name, to start the game again the same way
    std::wstring arguments = GetCommandLineW();
    const bool cQuoted = !arguments.empty() && arguments[0] == L'"';
    const size_t cEnd = cQuoted ? arguments.find(L'"', 1) : arguments.find(L' ');
    arguments = cEnd == std::wstring::npos ? L"" : arguments.substr(cEnd + 1);
    arguments.erase(0, arguments.find_first_not_of(L' '));
    std::wstring escaped;
    for (const wchar_t c : arguments)
        escaped += c == L'"' ? std::wstring(L"\\\"") : std::wstring(1, c);

    std::wstring command = L"\"" + cUpdater.wstring() + L"\" --check --game \"" + game.wstring() + L"\" --launcher-pid " + std::to_wstring(GetCurrentProcessId()) +
                           L" --relaunch \"" + cLauncher.wstring() + L"\" --relaunch-args \"" + escaped + L"\"";

    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(cUpdater.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, cUpdater.parent_path().c_str(), &startup, &process))
        return false;

    // Gives up on the network within seconds by itself, the rest is the player reading the box
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode == 2;
}

static void PreloadSystemDlls()
{
    auto loadSystemDll = [](auto dll)
    {
        wchar_t systemPath[512];
        GetSystemDirectoryW(systemPath, _countof(systemPath));

        wcscat_s(systemPath, dll);

        LoadLibraryW(systemPath);
    };

    for (auto dll : kSystemPreloadDlls)
        loadSystemDll(dll);
}

int main(int argc, char** argv)
{
    Base::SetCurrentThreadName("MainLauncherThread");

    // memory block for Script Extender reserved as early as we can
    script_extender::SEMemoryBlock b;
    if (!b.Good())
    {
        Die(L"Failed to pre-reserve script extender zone.\nAsk Force!");
        return -1;
    }

    if (StartedUpdate(argc, argv))
        return 0;

    PreloadSystemDlls();
    CoreStubsInit();
    ApplyLoadOrder();

    ComScope cs;
    TP_UNUSED(cs);

    auto ret = launcher::StartUp(argc, argv);

    return ret;
}
