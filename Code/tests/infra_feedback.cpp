#include <catch2/catch.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../immersive_launcher/steam/SystemDllShadows.h"

namespace fs = std::filesystem;

namespace
{
struct TempTree
{
    fs::path root = fs::temp_directory_path() / ("st_infra_feedback_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));

    TempTree()
    {
        std::error_code error;
        fs::remove_all(root, error);
        fs::create_directories(root);
    }
    ~TempTree()
    {
        std::error_code error;
        fs::remove_all(root, error);
    }

    void Touch(const fs::path& aRelative) const
    {
        const fs::path cPath = root / aRelative;
        fs::create_directories(cPath.parent_path());
        std::ofstream file(cPath);
        file << "x";
    }
};
} // namespace

TEST_CASE("Steam folder DLLs that shadow System32 DLLs get their System32 copy pinned", "[infra.launcher]")
{
    TempTree tree;
    tree.Touch("steam/dwmapi.dll");        // third-party proxy (issue 922)
    tree.Touch("steam/steamclient64.dll"); // Valve's own, no System32 namesake
    tree.Touch("steam/dxgi.dll");          // also in the game folder: a proxy the player installed there
    tree.Touch("steam/WINMM.DLL");
    tree.Touch("steam/notes.txt");
    fs::create_directories(tree.root / "steam/folder.dll");
    tree.Touch("system/dwmapi.dll");
    tree.Touch("system/dxgi.dll");
    tree.Touch("system/WINMM.DLL");
    tree.Touch("system/notes.txt");
    tree.Touch("system/folder.dll");
    tree.Touch("game/dxgi.dll");
    fs::create_directories(tree.root / "launcher");

    auto pinned = steam::SystemDllShadows(tree.root / "steam", tree.root / "system", {tree.root / "game", tree.root / "launcher"});
    std::vector<std::string> names;
    for (const auto& cPath : pinned)
    {
        REQUIRE(cPath.parent_path() == tree.root / "system");
        names.push_back(cPath.filename().string());
    }
    std::sort(names.begin(), names.end());

    REQUIRE(names == std::vector<std::string>{"WINMM.DLL", "dwmapi.dll"});
}

TEST_CASE("A missing Steam folder pins nothing", "[infra.launcher]")
{
    TempTree tree;
    tree.Touch("system/dwmapi.dll");

    REQUIRE(steam::SystemDllShadows(tree.root / "nope", tree.root / "system", {}).empty());
}

#ifdef _WIN32
#include "../tp_process/ParentProcess.h"

TEST_CASE("Browser subprocesses only follow a parent that started before them", "[infra.ui]")
{
    bool gone = true;
    const HANDLE hSelf = OpenParentProcess(GetCurrentProcessId(), gone);
    REQUIRE(hSelf != nullptr);
    REQUIRE_FALSE(gone);
    CloseHandle(hSelf);

    // A process started after this one stands for a parent whose id was reused after it exited. Process start times
    // move in clock ticks, so let one pass first.
    Sleep(50);
    wchar_t systemDir[MAX_PATH]{};
    REQUIRE(GetSystemDirectoryW(systemDir, MAX_PATH) > 0);
    std::wstring command = L"\"" + (fs::path(systemDir) / L"cmd.exe").wstring() + L"\" /c exit";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    REQUIRE(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process));

    const HANDLE hYounger = OpenParentProcess(process.dwProcessId, gone);
    TerminateProcess(process.hProcess, 0);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    REQUIRE(hYounger == nullptr);
    REQUIRE(gone);

    // Process id 0 never names a parent that can be opened
    REQUIRE(OpenParentProcess(0, gone) == nullptr);
    REQUIRE(gone);
}
#endif
