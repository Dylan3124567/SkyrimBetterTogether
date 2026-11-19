
#include <BSGraphics/BSGraphicsRenderer.h>
#include <Playtest.h>

struct BSInputDeviceManager;

void (*BSInputDeviceManager_PollInputDevices)(BSInputDeviceManager*, float) = nullptr;

void Hook_BSInputDeviceManager_PollInputDevices(BSInputDeviceManager* inputDeviceMgr, float afDelta)
{
    Playtest::Tick();
    if (Playtest::Baseline())
    {
        POINTER_SKYRIMSE(uint32_t, alwaysActive, 380768);
        *alwaysActive = 1;
    }
    if (!Playtest::Active() && (!BSGraphics::GetMainWindow() || !BSGraphics::GetMainWindow()->IsForeground()))
        return;

    Playtest::BeginInput(afDelta);
    BSInputDeviceManager_PollInputDevices(inputDeviceMgr, afDelta);
}

void Playtest::InstallInputHooks()
{
    const VersionDbPtr<void> pollInputDevices(68617);

    BSInputDeviceManager_PollInputDevices = static_cast<decltype(BSInputDeviceManager_PollInputDevices)>(pollInputDevices.GetPtr());

    TP_HOOK_IMMEDIATE(&BSInputDeviceManager_PollInputDevices, &Hook_BSInputDeviceManager_PollInputDevices);
}

static TiltedPhoques::Initializer s_initInputDeviceManager(Playtest::InstallInputHooks);
