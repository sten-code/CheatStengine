#include "IconCache.h"

#include <Engine/Core/Log.h>

#include <Windows.h>

#include <d3d11.h>
#include <shellapi.h>

#include <psapi.h>
#include <vector>

#pragma comment(lib, "Shell32.lib")

IconCache::~IconCache()
{
    Shutdown();
}

void IconCache::Initialize(ID3D11Device* device)
{
    m_Device = device;
}

void IconCache::Shutdown()
{
    for (auto& [pid, texture] : m_ProcessIcons) {
        if (texture) {
            reinterpret_cast<ID3D11ShaderResourceView*>(texture)->Release();
        }
    }
    m_ProcessIcons.clear();

    if (m_FallbackIcon) {
        reinterpret_cast<ID3D11ShaderResourceView*>(m_FallbackIcon)->Release();
        m_FallbackIcon = 0;
    }
}

uint64_t IconCache::GetProcessIcon(uint32_t pid)
{
    if (!m_Device) {
        return 0;
    }

    if (auto it = m_ProcessIcons.find(pid); it != m_ProcessIcons.end()) {
        return it->second;
    }

    std::wstring path = GetProcessPath(pid);
    uint64_t texture = path.empty() ? 0 : CreateTextureFromPath(path);

    m_ProcessIcons.emplace(pid, texture);
    return texture;
}

uint64_t IconCache::GetWindowIcon(void* hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(static_cast<HWND>(hwnd), &pid);
    if (pid == 0) {
        return 0;
    }
    return GetProcessIcon(pid);
}

uint64_t IconCache::GetFallbackIcon()
{
    if (!m_Device || m_FallbackIcon) {
        return m_FallbackIcon;
    }

    SHFILEINFOW info {};
    if (SHGetFileInfoW(L"app.exe", FILE_ATTRIBUTE_NORMAL, &info, sizeof(info),
            SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)
        && info.hIcon) {
        m_FallbackIcon = CreateTextureFromIcon(info.hIcon);
        DestroyIcon(info.hIcon);
    }
    return m_FallbackIcon;
}

uint64_t IconCache::CreateTextureFromPath(const std::wstring& path)
{
    SHFILEINFOW info {};
    if (!SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_SMALLICON)
        || !info.hIcon) {
        return 0;
    }

    uint64_t texture = CreateTextureFromIcon(info.hIcon);
    DestroyIcon(info.hIcon);
    return texture;
}

uint64_t IconCache::CreateTextureFromIcon(HICON icon)
{
    ICONINFO iconInfo {};
    if (!GetIconInfo(icon, &iconInfo)) {
        return 0;
    }

    BITMAP bmp {};
    if (GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp) == 0) {
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        return 0;
    }

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;

    BITMAPINFO bmi {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; 
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);

    HDC screenDc = GetDC(nullptr);
    int scanlines = GetDIBits(screenDc, iconInfo.hbmColor, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

    if (scanlines == 0) {
        ReleaseDC(nullptr, screenDc);
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        return 0;
    }

    // Swap B and R (GDI gives BGRA, D3D wants RGBA) while checking whether the
    // colour bitmap carried any real alpha.
    bool hasAlpha = false;
    for (uint32_t& pixel : pixels) {
        if (pixel & 0xFF000000) {
            hasAlpha = true;
        }
        uint32_t b = (pixel & 0x000000FF);
        uint32_t r = (pixel & 0x00FF0000) >> 16;
        pixel = (pixel & 0xFF00FF00) | (b << 16) | r;
    }

    // Legacy icons store no per-pixel alpha; transparency lives in the AND mask
    // (a set bit means transparent). Without this, such icons decode to alpha 0
    // everywhere and render fully invisible. Rebuild alpha from the mask.
    if (!hasAlpha) {
        std::vector<uint32_t> mask(static_cast<size_t>(width) * height);
        if (GetDIBits(screenDc, iconInfo.hbmMask, 0, height, mask.data(), &bmi, DIB_RGB_COLORS) != 0) {
            for (size_t i = 0; i < pixels.size(); ++i) {
                bool transparent = (mask[i] & 0x00FFFFFF) != 0;
                pixels[i] = transparent ? (pixels[i] & 0x00FFFFFF) : (pixels[i] | 0xFF000000);
            }
        } else {
            // Mask unreadable: make the icon opaque rather than invisible.
            for (uint32_t& pixel : pixels) {
                pixel |= 0xFF000000;
            }
        }
    }

    ReleaseDC(nullptr, screenDc);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);

    D3D11_TEXTURE2D_DESC desc {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data {};
    data.pSysMem = pixels.data();
    data.SysMemPitch = width * 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(m_Device->CreateTexture2D(&desc, &data, &tex)) || !tex) {
        return 0;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = m_Device->CreateShaderResourceView(tex, &srvDesc, &srv);
    tex->Release(); 

    if (FAILED(hr) || !srv) {
        return 0;
    }

    return reinterpret_cast<uint64_t>(srv);
}

std::wstring IconCache::GetProcessPath(uint32_t pid)
{
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!handle) {
        return {};
    }

    wchar_t buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    std::wstring path;
    if (QueryFullProcessImageNameW(handle, 0, buffer, &size)) {
        path.assign(buffer, size);
    }

    CloseHandle(handle);
    return path;
}
