#include <cstdio>
#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <mutex>

namespace cursor_debug
{
static std::atomic_int s_lines{0};
static std::mutex s_lock;

static void Write(const char* aLine)
{
    const int index = s_lines.fetch_add(1);
    if (index >= 200)
        return;

    std::lock_guard _(s_lock);

    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0)
        return;

    std::filesystem::path file(path);
    file = file.parent_path() / L"cursor-debug.txt";

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, file.c_str(), index == 0 ? L"w" : L"a") != 0 || !fp)
        return;

    fprintf(fp, "%s\n", aLine);
    fclose(fp);
}
} // namespace cursor_debug
