// malid3d - d3d11.dll entry points.
//
// Milestone 0: this DLL does not render yet. When a game asks for a D3D11
// device we probe the Vulkan driver, write everything to malid3d.log and
// report "unsupported", so the game exits cleanly with a D3D11 error.

#include <windows.h>

#include "log.h"
#include "version.h"
#include "vk_probe.h"

using malid3d::mlog;

namespace {

// DXGI_ERROR_UNSUPPORTED
const HRESULT kUnsupported = static_cast<HRESULT>(0x887A0004L);

void log_request(const char* fn, int driver_type, UINT flags, const int* levels, UINT count) {
    mlog("%s: driverType=%d flags=0x%x featureLevels=%u", fn, driver_type, flags, count);
    for (UINT i = 0; levels && i < count && i < 16; i++)
        mlog("  requested feature level 0x%04x", static_cast<unsigned>(levels[i]));
}

}  // namespace

extern "C" {

HRESULT WINAPI D3D11CreateDevice(void* adapter, int driver_type, HMODULE software, UINT flags,
                                 const int* feature_levels, UINT feature_level_count,
                                 UINT sdk_version, void** out_device, int* out_feature_level,
                                 void** out_context) {
    (void)adapter;
    (void)software;
    (void)sdk_version;
    log_request("D3D11CreateDevice", driver_type, flags, feature_levels, feature_level_count);

    if (out_device) *out_device = nullptr;
    if (out_context) *out_context = nullptr;
    if (out_feature_level) *out_feature_level = 0;

    malid3d::run_vulkan_probe();
    mlog("D3D11CreateDevice: rendering is not implemented yet, returning DXGI_ERROR_UNSUPPORTED");
    return kUnsupported;
}

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(void* adapter, int driver_type, HMODULE software,
                                             UINT flags, const int* feature_levels,
                                             UINT feature_level_count, UINT sdk_version,
                                             const void* swap_chain_desc, void** out_swap_chain,
                                             void** out_device, int* out_feature_level,
                                             void** out_context) {
    (void)adapter;
    (void)software;
    (void)sdk_version;
    (void)swap_chain_desc;
    log_request("D3D11CreateDeviceAndSwapChain", driver_type, flags, feature_levels,
                feature_level_count);

    if (out_swap_chain) *out_swap_chain = nullptr;
    if (out_device) *out_device = nullptr;
    if (out_context) *out_context = nullptr;
    if (out_feature_level) *out_feature_level = 0;

    malid3d::run_vulkan_probe();
    mlog("D3D11CreateDeviceAndSwapChain: rendering is not implemented yet, returning "
        "DXGI_ERROR_UNSUPPORTED");
    return kUnsupported;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        char exe[MAX_PATH] = "?";
        GetModuleFileNameA(nullptr, exe, sizeof(exe));
        mlog("malid3d %s (%s, %d-bit) loaded", MALID3D_VERSION, MALID3D_BUILD,
            static_cast<int>(sizeof(void*) * 8));
        mlog("process: %s", exe);
    }
    return TRUE;
}

}  // extern "C"
