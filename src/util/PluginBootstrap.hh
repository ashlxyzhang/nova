#pragma once
#ifndef PLUGIN_BOOTSTRAP_HH
#define PLUGIN_BOOTSTRAP_HH

#include "util/pch.hh"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace plugin_bootstrap
{
namespace detail
{
inline std::filesystem::path executable_path()
{
#if defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    return std::filesystem::weakly_canonical(buf);
#elif defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return {};
    return std::filesystem::path(std::wstring(buf, len));
#else
    std::error_code ec;
    auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path{} : p;
#endif
}

inline void set_env(const char *name, const std::string &value)
{
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}
} // namespace detail

/**
 * @brief Point Metavision HAL at the plugin directory bundled with the
 *        application so distributable builds can discover cameras with no
 *        setup. Honors an existing MV_HAL_PLUGIN_PATH so developers can
 *        override at runtime. Must be called before any Metavision SDK use.
 */
inline void setup_metavision_plugin_path()
{
    if (std::getenv("MV_HAL_PLUGIN_PATH")) return;

    auto exe_path = detail::executable_path();
    if (exe_path.empty()) return;

#if defined(__APPLE__)
    // .app layout: Contents/MacOS/NOVA -> Contents/Frameworks/metavision/hal/plugins
    auto plugin_dir = exe_path.parent_path().parent_path() / "Frameworks" / "metavision" / "hal" / "plugins";
#else
    // Tarball/zip layout: <root>/bin/NOVA -> <root>/lib/metavision/hal/plugins
    auto plugin_dir = exe_path.parent_path().parent_path() / "lib" / "metavision" / "hal" / "plugins";
#endif

    std::error_code ec;
    if (!std::filesystem::exists(plugin_dir, ec)) return;

    detail::set_env("MV_HAL_PLUGIN_PATH", plugin_dir.string());
}

/**
 * @brief On macOS, point the Vulkan loader at the MoltenVK ICD bundled inside
 *        the .app so end users without the Vulkan SDK can still run NOVA. No-op
 *        on other platforms (system GPU drivers provide the Vulkan ICD). Must
 *        be called before any Vulkan call (including SDL_GPU init).
 */
inline void setup_vulkan_icd_path()
{
#if defined(__APPLE__)
    if (std::getenv("VK_ICD_FILENAMES") || std::getenv("VK_DRIVER_FILES")) return;

    auto exe_path = detail::executable_path();
    if (exe_path.empty()) return;

    // .app layout: Contents/MacOS/NOVA -> Contents/Resources/vulkan/icd.d/MoltenVK_icd.json
    auto icd_path = exe_path.parent_path().parent_path()
                  / "Resources" / "vulkan" / "icd.d" / "MoltenVK_icd.json";

    std::error_code ec;
    if (!std::filesystem::exists(icd_path, ec)) return;

    auto p = icd_path.string();
    detail::set_env("VK_ICD_FILENAMES", p);  // legacy name
    detail::set_env("VK_DRIVER_FILES", p);   // current name
#endif
}
} // namespace plugin_bootstrap

#endif // PLUGIN_BOOTSTRAP_HH
