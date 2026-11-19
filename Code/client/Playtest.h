#pragma once
struct IDXGISwapChain;
namespace Playtest
{
void Initialize();
bool Baseline();
void InstallInputHooks();
void InstallRenderHooks();
void Tick();
bool Active();
void Capture(IDXGISwapChain*);
void PollMouse(float);
void BeginInput(float);
bool Cursor(float& x, float& y, bool set = false);
}
