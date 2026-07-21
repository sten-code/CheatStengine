#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

struct ID3D11Device;
struct ID3D11ShaderResourceView;
struct HICON__;
using HICON = HICON__*;

class IconCache {
public:
    IconCache() = default;
    ~IconCache();

    void Initialize(ID3D11Device* device);
    void Shutdown();

    [[nodiscard]] uint64_t GetProcessIcon(uint32_t pid);
    [[nodiscard]] uint64_t GetWindowIcon(void* hwnd);
    [[nodiscard]] uint64_t GetFallbackIcon();

private:
    [[nodiscard]] uint64_t CreateTextureFromPath(const std::wstring& path);
    [[nodiscard]] uint64_t CreateTextureFromIcon(HICON icon);
    [[nodiscard]] static std::wstring GetProcessPath(uint32_t pid);

    ID3D11Device* m_Device = nullptr;

    std::unordered_map<uint32_t, uint64_t> m_ProcessIcons;
    uint64_t m_FallbackIcon = 0;
};
