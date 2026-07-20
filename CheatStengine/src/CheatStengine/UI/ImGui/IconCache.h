#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

struct ID3D11Device;
struct ID3D11ShaderResourceView;
struct HICON__;
using HICON = HICON__*;

// Turns Windows shell icons into ImGui-renderable textures and caches them.
//
// The process/window selector wants a small icon in front of every row. Pulling
// and uploading an icon is comparatively expensive (shell query + a GPU texture
// per icon), so results are cached: process icons by pid, file icons by path.
// Textures live for the lifetime of the cache and are released together in
// Shutdown(), which the owner calls before the D3D11 device goes away.
//
// Everything here is best-effort: if an icon can't be extracted the lookup
// returns 0 and the caller simply draws nothing (or a shared fallback glyph),
// never an error.
class IconCache {
public:
    IconCache() = default;
    ~IconCache();

    // Binds the cache to the live D3D11 device. Must be called once, on the main
    // thread, before any Get* call. Passing nullptr disables texture creation so
    // the selector still works headless (every lookup just returns 0).
    void Initialize(ID3D11Device* device);

    // Releases every cached texture. Safe to call more than once; the destructor
    // calls it too.
    void Shutdown();

    // Icon for a running process, resolved from its executable path and keyed by
    // pid. Returns an ImTextureID-compatible handle (the SRV pointer as a
    // uint64_t) or 0 when unavailable.
    [[nodiscard]] uint64_t GetProcessIcon(uint32_t pid);

    // Icon for the window's owning process, keyed by pid so it shares the process
    // cache. hwnd is only used to find that pid.
    [[nodiscard]] uint64_t GetWindowIcon(void* hwnd);

    // A neutral fallback (the generic application icon) for rows whose real icon
    // couldn't be extracted. Cached once.
    [[nodiscard]] uint64_t GetFallbackIcon();

private:
    // Extracts the shell icon for a file path and uploads it as a texture.
    // Returns 0 on any failure.
    [[nodiscard]] uint64_t CreateTextureFromPath(const std::wstring& path);

    // Rasterizes an HICON into BGRA pixels and uploads a texture. Takes ownership
    // of nothing; the caller still destroys the HICON.
    [[nodiscard]] uint64_t CreateTextureFromIcon(HICON icon);

    // Resolves a pid to its executable path, or an empty string on failure.
    [[nodiscard]] static std::wstring GetProcessPath(uint32_t pid);

    ID3D11Device* m_Device = nullptr;

    std::unordered_map<uint32_t, uint64_t> m_ProcessIcons;
    uint64_t m_FallbackIcon = 0;
};
