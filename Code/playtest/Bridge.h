#pragma once
#include <Windows.h>
struct IDXGISwapChain;

// Versioned C ABI: the optional DLL owns IPC/input; the client owns game interfaces.
struct PlaytestApi
{
    unsigned Version;
    void (*Tick)();
    bool (*Active)();
    bool (*State)(bool, unsigned long, void*);
    bool (*Data)(bool, unsigned long, void*, unsigned long*, unsigned long);
    void (*Capture)(IDXGISwapChain*);
};
using PlaytestHost = void (*)(const char*, char*, unsigned);
using StartPlaytest = const PlaytestApi* (*)(PlaytestHost, const wchar_t*);
