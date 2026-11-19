#include <Windows.h>
#include <ShlObj.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <TiltedCore/Filesystem.hpp>

#include "../../client/Systems/LoadOrderRules.h"

namespace
{
std::vector<std::string> ReadLines(const std::filesystem::path& acPath)
{
    std::vector<std::string> lines;
    std::ifstream file(acPath);
    for (std::string line; std::getline(file, line);)
        lines.push_back(line);
    return lines;
}
} // namespace

// A mod pack ships LoadOrder.txt next to the launcher. Installs without one are left alone.
// Needs to run after CoreStubsInit, so that an instance with a folder of its own gets its own Plugins.txt.
void ApplyLoadOrder()
{
    const auto cRequired = ReadLines(TiltedPhoques::GetPath() / "LoadOrder.txt");
    if (cRequired.empty())
        return;

    char localAppData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData)))
        return;

    const auto cFolder = std::filesystem::path(localAppData) / "Skyrim Special Edition";
    std::error_code error;
    std::filesystem::create_directories(cFolder, error);

    const auto cPlugins = cFolder / "Plugins.txt";
    const auto cExisting = ReadLines(cPlugins);
    const auto cMerged = LoadOrderRules::Merge(cRequired, cExisting);
    if (cMerged == cExisting)
        return;

    std::ofstream file(cPlugins, std::ios::binary | std::ios::trunc);
    for (const auto& cLine : cMerged)
        file << cLine << '\n';
}
