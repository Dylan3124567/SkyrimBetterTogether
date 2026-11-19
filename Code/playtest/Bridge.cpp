#include "Bridge.h"
#include <dinput.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <sddl.h>
#include <array>
#include <atomic>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

namespace
{
using Microsoft::WRL::ComPtr;
HANDLE pipe = INVALID_HANDLE_VALUE;
PlaytestHost host{};
std::filesystem::path folder;
bool connected{}, replied{};
ULONGLONG connectionStarted{};
std::atomic<bool> active{}, capture{};
std::atomic<unsigned> frames{}, captureError{}, statePolls{}, dataPolls{};
std::atomic<unsigned> keyboardDataSize{}, mouseDataSize{}, keyboardStateSize{}, mouseStateSize{};
std::mutex mutex;
std::array<BYTE, 256> keys{};
DIMOUSESTATE2 mouse{};
std::deque<DIDEVICEOBJECTDATA> keyboardEvents, mouseEvents;
ULONGLONG deadline{}, lease{}, stopping{};
DWORD sequence{};

void Event(std::deque<DIDEVICEOBJECTDATA>& queue, DWORD offset, DWORD value)
{
    queue.push_back({offset, value, GetTickCount(), ++sequence, 0});
}

void Buttons(const std::array<BYTE, 256>& next, unsigned buttons)
{
    for (unsigned i = 0; i < keys.size(); ++i)
        if (keys[i] != next[i])
            Event(keyboardEvents, i, next[i]);
    keys = next;
    for (unsigned i = 0; i < 8; ++i)
    {
        const BYTE value = (buttons & (1u << i)) ? 0x80 : 0;
        if (mouse.rgbButtons[i] != value)
            Event(mouseEvents, DIMOFS_BUTTON0 + i, value);
        mouse.rgbButtons[i] = value;
    }
}

void Release()
{
    Buttons({}, 0);
    mouse.lX = mouse.lY = mouse.lZ = 0;
    deadline = 0;
}

bool Active() { return active.load(); }

bool State(bool keyboard, DWORD size, void* output)
{
    (keyboard ? keyboardStateSize : mouseStateSize) = size;
    if (!active || !output)
        return false;
    std::lock_guard lock(mutex);
    if (keyboard && size == keys.size())
        memcpy(output, keys.data(), size);
    else if (!keyboard && (size == sizeof(DIMOUSESTATE) || size == sizeof(DIMOUSESTATE2)))
    {
        memcpy(output, &mouse, size);
        mouse.lX = mouse.lY = mouse.lZ = 0;
        mouseEvents.clear();
    }
    else
        return false;
    ++statePolls;
    return true;
}

bool Data(bool keyboard, DWORD size, void* output, DWORD* count, DWORD flags)
{
    (keyboard ? keyboardDataSize : mouseDataSize) = size;
    if (!active || !count || size != sizeof(DIDEVICEOBJECTDATA))
        return false;
    std::lock_guard lock(mutex);
    auto& events = keyboard ? keyboardEvents : mouseEvents;
    const DWORD amount = std::min(*count, static_cast<DWORD>(events.size()));
    if (output)
        std::copy_n(events.begin(), amount, static_cast<DIDEVICEOBJECTDATA*>(output));
    if (!(flags & DIGDD_PEEK))
    {
        events.erase(events.begin(), events.begin() + amount);
        if (!keyboard)
            mouse.lX = mouse.lY = mouse.lZ = 0;
    }
    *count = amount;
    ++dataPolls;
    return true;
}

std::string Command(const char* text)
{
    std::istringstream in(text);
    std::string command, extra;
    in >> command;
    if (command == "status")
    {
        char state[4096] = "{}";
        host("status", state, sizeof(state));
        return "{\"active\":" + std::string(active ? "true" : "false") + ",\"frames\":" + std::to_string(frames) +
            ",\"capture_error\":" + std::to_string(captureError) + ",\"state_polls\":" + std::to_string(statePolls) +
            ",\"data_polls\":" + std::to_string(dataPolls) + ",\"device_sizes\":[" + std::to_string(keyboardDataSize) + "," + std::to_string(mouseDataSize) + "," + std::to_string(keyboardStateSize) + "," + std::to_string(mouseStateSize) + "],\"game\":" + state + "}";
    }
    if (command == "capture" && !(in >> extra))
    {
        capture = true;
        return "{\"pending\":true}";
    }
    if ((command == "claim" || command == "stop") && !(in >> extra))
    {
        std::lock_guard lock(mutex);
        if (command == "stop")
        {
            Release();
            stopping = GetTickCount64() + 250; // Deliver key/button-up events before returning to physical input.
        }
        else
        {
            if (!active) { Release(); keyboardEvents.clear(); mouseEvents.clear(); }
            active = true;
            stopping = 0;
        }
        lease = GetTickCount64() + 30000;
        return "{\"ok\":true}";
    }
    if (!active || stopping)
        return "{\"error\":\"claim control first\"}";
    if (command == "input")
    {
        int ms, x, y, z, buttons, key;
        if (!(in >> ms >> x >> y >> z >> buttons) || ms < 1 || ms > 5000 || x < -4096 || x > 4096 || y < -4096 || y > 4096 || z < -1200 || z > 1200 || buttons < 0 || buttons > 255)
            return "{\"error\":\"input: ms(1..5000) dx dy wheel buttons scancodes...\"}";
        std::array<BYTE, 256> next{};
        unsigned count = 0;
        while (in >> key)
        {
            if (key < 1 || key > 255 || ++count > 16)
                return "{\"error\":\"invalid scancode list\"}";
            next[key] = 0x80;
        }
        if (!in.eof())
            return "{\"error\":\"invalid input token\"}";
        std::lock_guard lock(mutex);
        if (keyboardEvents.size() + mouseEvents.size() > 512)
            return "{\"error\":\"input queue full; stop or wait for game\"}";
        Buttons(next, buttons);
        mouse.lX += x;
        mouse.lY += y;
        mouse.lZ += z;
        if (x) Event(mouseEvents, DIMOFS_X, static_cast<DWORD>(x));
        if (y) Event(mouseEvents, DIMOFS_Y, static_cast<DWORD>(y));
        if (z) Event(mouseEvents, DIMOFS_Z, static_cast<DWORD>(z));
        deadline = GetTickCount64() + ms;
        lease = GetTickCount64() + 30000;
        return "{\"ok\":true}";
    }
    char response[65536] = "{\"error\":\"unknown command\"}";
    host(text, response, sizeof(response));
    if (std::string(response) == "{\"ok\":true}")
    {
        std::lock_guard lock(mutex);
        lease = GetTickCount64() + 30000;
    }
    return response;
}

void Tick()
{
    static std::mutex tickMutex;
    std::unique_lock tickLock(tickMutex, std::try_to_lock);
    if (!tickLock.owns_lock()) return;
    {
        std::lock_guard lock(mutex);
        const auto now = GetTickCount64();
        if (deadline && now >= deadline)
            Release();
        if (active && !stopping && (now >= lease || (GetAsyncKeyState(VK_F12) & 0x8000)))
        {
            Release();
            stopping = now + 250;
        }
        if (stopping && now >= stopping)
        {
            active = false;
            stopping = 0;
        }
    }
    if (pipe == INVALID_HANDLE_VALUE)
        return;
    if (connected && GetTickCount64() - connectionStarted > 1000)
    {
        DisconnectNamedPipe(pipe);
        connected = false;
    }
    if (!connected)
    {
        connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
        if (!connected)
        {
            if (GetLastError() == ERROR_NO_DATA)
                DisconnectNamedPipe(pipe);
            return;
        }
        replied = false;
        connectionStarted = GetTickCount64();
    }
    char request[2048]{};
    DWORD read = 0;
    if (!ReadFile(pipe, request, sizeof(request) - 1, &read, nullptr))
    {
        const DWORD error = GetLastError();
        if (error != ERROR_NO_DATA)
        {
            DisconnectNamedPipe(pipe);
            connected = false;
        }
        return;
    }
    if (!read || replied)
        return;
    const std::string response = Command(request);
    DWORD written = 0;
    WriteFile(pipe, response.data(), static_cast<DWORD>(response.size()), &written, nullptr);
    replied = true;
}

void Capture(IDXGISwapChain* swap)
{
    if (!capture.exchange(false) || !swap)
        return;
    ComPtr<ID3D11Texture2D> texture, staging;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HRESULT result = swap->GetBuffer(0, IID_PPV_ARGS(&texture));
    if (FAILED(result)) { captureError = result; return; }
    texture->GetDevice(&device);
    device->GetImmediateContext(&context);
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    const bool rgba = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (desc.SampleDesc.Count != 1 || (!rgba && desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))
    { captureError = ERROR_NOT_SUPPORTED; return; }
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    result = device->CreateTexture2D(&desc, nullptr, &staging);
    if (FAILED(result)) { captureError = result; return; }
    context->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    result = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(result)) { captureError = result; return; }
    BITMAPFILEHEADER header{0x4D42, 54 + desc.Width * desc.Height * 4, 0, 0, 54};
    BITMAPINFOHEADER info{40, static_cast<LONG>(desc.Width), -static_cast<LONG>(desc.Height), 1, 32, BI_RGB};
    std::ofstream file(folder / L"frame.tmp", std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(&info), sizeof(info));
    std::vector<char> row(desc.Width * 4);
    for (unsigned y = 0; y < desc.Height; ++y)
    {
        memcpy(row.data(), static_cast<char*>(mapped.pData) + y * mapped.RowPitch, row.size());
        if (rgba)
            for (unsigned x = 0; x < row.size(); x += 4)
                std::swap(row[x], row[x + 2]);
        file.write(row.data(), row.size());
    }
    context->Unmap(staging.Get(), 0);
    file.close();
    if (!file || !MoveFileExW((folder / L"frame.tmp").c_str(), (folder / L"frame.bmp").c_str(), MOVEFILE_REPLACE_EXISTING))
    { captureError = ERROR_WRITE_FAULT; return; }
    captureError = 0;
    ++frames;
}
}

extern "C" __declspec(dllexport) const PlaytestApi* StartBridge(PlaytestHost callback, const wchar_t* root)
{
    if (!callback || pipe != INVALID_HANDLE_VALUE)
        return nullptr;
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return nullptr;
    DWORD size{};
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> user(size);
    const bool gotUser = GetTokenInformation(token, TokenUser, user.data(), size, &size) != 0;
    CloseHandle(token);
    if (!gotUser) return nullptr;
    LPWSTR sid{};
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, &sid)) return nullptr;
    const std::wstring acl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR security{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1, &security, nullptr)) return nullptr;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), security, FALSE};
    const std::wstring name = L"\\\\.\\pipe\\SkyrimPlaytest-" + std::to_wstring(GetCurrentProcessId());
    pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 65536, 2048, 0, &attributes);
    LocalFree(security);
    if (pipe == INVALID_HANDLE_VALUE) return nullptr;
    folder = std::filesystem::path(root) / L"playtest" / std::to_wstring(GetCurrentProcessId());
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    if (error) { CloseHandle(pipe); pipe = INVALID_HANDLE_VALUE; return nullptr; }
    host = callback;
    static const PlaytestApi api{1, Tick, Active, State, Data, Capture};
    return &api;
}
