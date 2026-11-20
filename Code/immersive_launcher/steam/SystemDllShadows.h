#pragma once

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

namespace steam
{
/**
 * @brief The System32 copies of the DLLs that aForeignDir would shadow once it is a DLL search directory.
 *
 * Directories added with AddDllDirectory are searched before System32, so a third-party file in the Steam folder (a
 * dwmapi.dll proxy, say) would be bound to every SKSE plugin that imports that name. Windows hands an already loaded
 * module to later imports of the same name, so loading these System32 copies first keeps the normal search order for
 * them. Names the game or launcher folders provide are left alone: those are proxies the player installed on purpose.
 */
inline std::vector<std::filesystem::path> SystemDllShadows(const std::filesystem::path& aForeignDir, const std::filesystem::path& aSystemDir,
                                                           std::initializer_list<std::filesystem::path> aOwnDirs)
{
    namespace fs = std::filesystem;

    std::vector<fs::path> shadows;
    std::error_code listError, fileError;
    for (fs::directory_iterator it(aForeignDir, listError), end; !listError && it != end; it.increment(listError))
    {
        // Wide, a name the code page can't represent would make string() throw
        std::wstring extension = it->path().extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        if (extension != L".dll" || !it->is_regular_file(fileError))
            continue;

        const fs::path cName = it->path().filename();
        if (!fs::is_regular_file(aSystemDir / cName, fileError))
            continue;

        const bool cOwnCopy = std::any_of(aOwnDirs.begin(), aOwnDirs.end(), [&](const fs::path& aDir) { return fs::exists(aDir / cName, fileError); });
        if (!cOwnCopy)
            shadows.push_back(aSystemDir / cName);
    }
    return shadows;
}
} // namespace steam
