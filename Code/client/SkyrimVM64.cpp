#include <TiltedOnlinePCH.h>
#include "TiltedOnlineApp.h"
#include <Misc/GameVM.h>
#include <Services/HelgenService.h>
#include <Interface/UI.h>

extern std::unique_ptr<TiltedOnlineApp> g_appInstance;

struct Main;

TP_THIS_FUNCTION(TVMUpdate, int, GameVM, float);
TP_THIS_FUNCTION(TMainLoop, short, Main);
TP_THIS_FUNCTION(TVMDestructor, uintptr_t, void);

static TVMUpdate* VMUpdate = nullptr;
static TMainLoop* MainLoop = nullptr;
static TVMDestructor* VMDestructor = nullptr;
static thread_local bool s_modUpdatedThisFrame{};

int TP_MAKE_THISCALL(HookVMUpdate, GameVM, float a2)
{
    if (apThis->inactive == 0)
    {
        s_modUpdatedThisFrame = true;
        g_appInstance->Update();
    }

    return TiltedPhoques::ThisCall(VMUpdate, apThis, a2);
}

short TP_MAKE_THISCALL(HookMainLoop, Main)
{
    s_modUpdatedThisFrame = false;
    const short result = TiltedPhoques::ThisCall(MainLoop, apThis);
    // Unpaused RaceSex Menu can skip the VM tick that normally pumps networking and readiness.
    // Use the existing main-loop hook only for those frames, without updating services twice.
    auto* ui = UI::Get();
    if (!s_modUpdatedThisFrame && HelgenService::HoldScene(0xCD68E) && ui &&
        ui->GetMenuOpen(BSFixedString("RaceSex Menu")) && !ui->GetMenuOpen(BSFixedString("Loading Menu")) &&
        GameVM::Get() && GameVM::Get()->inactive == 0)
        g_appInstance->Update();
    return result;
}

uintptr_t TP_MAKE_THISCALL(HookVMDestructor, void)
{
    TP_EMPTY_HOOK_PLACEHOLDER

    return TiltedPhoques::ThisCall(VMDestructor, apThis);
}

static TiltedPhoques::Initializer s_mainHooks(
    []()
    {
        POINTER_SKYRIMSE(TMainLoop, cMainLoop, 36564);
        POINTER_SKYRIMSE(TVMUpdate, cVMUpdate, 53926);
        POINTER_SKYRIMSE(TVMDestructor, cVMDestructor, 40412);

        VMUpdate = cVMUpdate.Get();
        MainLoop = cMainLoop.Get();
        VMDestructor = cVMDestructor.Get();

        TP_HOOK(&VMUpdate, HookVMUpdate);
        TP_HOOK(&MainLoop, HookMainLoop);
        TP_HOOK(&VMDestructor, HookVMDestructor);
    });

