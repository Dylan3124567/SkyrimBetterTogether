#include <windows.h>
#include <OverlayProc.hpp>
#include "ParentProcess.h"
#include "ProcessHandler.h"

#include <cwchar>
#include <thread>

// The browser's GPU process draws the overlay, so it asks hybrid-graphics drivers for the same high-performance GPU the
// game process does (immersive_launcher/Main.cpp) instead of the integrated one (issue 464)
extern "C"
{
    __declspec(dllexport) int NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Without the sandbox the browser's child processes are in no job object, so nothing ends them when the game is killed
// or crashes (issue 148). The game passes its id to every child it starts as --pid (TiltedUI OverlayBrowserProcessHandler).
static void ExitWithParent()
{
    const auto pArgs = CefCommandLine::CreateCommandLine();
    pArgs->InitFromString(GetCommandLineW());
    const DWORD cParentId = std::wcstoul(pArgs->GetSwitchValue("pid").ToWString().c_str(), nullptr, 10);
    if (cParentId == 0)
        return;

    bool gone = false;
    const HANDLE hParent = OpenParentProcess(cParentId, gone);
    if (gone)
        TerminateProcess(GetCurrentProcess(), 0);
    if (!hParent)
        return;

    // TerminateProcess rather than exit(): static destructors would race the browser threads still running
    std::thread([hParent] {
        WaitForSingleObject(hParent, INFINITE);
        TerminateProcess(GetCurrentProcess(), 0);
    }).detach();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    ExitWithParent();
    return TiltedPhoques::UIMain(lpCmdLine, hInstance, []() { return new ProcessHandler; });
}
