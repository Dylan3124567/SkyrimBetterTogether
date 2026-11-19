
#include "SteamLoader.h"
#include "TargetConfig.h"

#include "Utils/Registry.h"
#include <cstdio>

namespace steam
{
constexpr wchar_t kSteamDllName[] = L"steamclient64.dll";

void Load(const fs::path& aGameDir)
{
    auto appId = std::to_wstring(CurrentTarget.steamAppId);
    SetEnvironmentVariableW(L"SteamAppId", appId.c_str());

    auto path = aGameDir / "steam_appid.txt";

    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"w");
    if (f)
    {
        fprintf(f, "%d", CurrentTarget.steamAppId);
        fclose(f);
    }

    fs::path steamPath = Registry::ReadString<wchar_t>(HKEY_CURRENT_USER, LR"(SOFTWARE\Valve\Steam)", L"SteamPath");

    // wait what
    if (!fs::exists(steamPath))
        return;

    // Pin the game's own Steam API before the search path can resolve a stray copy from the Steam
    // folder; such a file shadows the game one and fails with "Unable to locate the original Valve API".
    const auto gameApi = aGameDir / L"steam_api64.dll";
    if (fs::exists(gameApi))
        LoadLibraryW(gameApi.c_str());

    AddDllDirectory(steamPath.c_str());

    fs::path clientPath = steamPath / kSteamDllName;
    LoadLibraryW(clientPath.c_str());
}
} // namespace steam
