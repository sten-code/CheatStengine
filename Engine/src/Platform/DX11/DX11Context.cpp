#ifdef PLATFORM_WINDOWS
#include "DX11Context.h"
#include <Engine/Core/Log.h>
#include <Engine/Core/Window.h>

#ifdef ENGINE_IMGUI
#include <imgui_impl_DX11.h>
#endif

DX11Context::~DX11Context()
{
    Shutdown();
}

void DX11Context::Initialize()
{
    HWND hWnd = static_cast<HWND>(m_Window.GetNativeHandle());

    INFO("Creating swapchain");

    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    constexpr D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT res = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_SwapChain,
        &m_Device, &featureLevel, &m_DeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_SwapChain, &m_Device,
            &featureLevel, &m_DeviceContext);

    ASSERT(res == S_OK, "Failed to create DirectX 10 device and swap chain");

    CreateRenderTarget();
}

void DX11Context::SwapBuffers()
{
    // Present with vsync (1) and handle window occlusion
    HRESULT result = m_SwapChain->Present(m_VSync, 0);

    // Check if the window is occluded (minimized, etc.)
    m_SwapChainOccluded = (result == DXGI_STATUS_OCCLUDED);
}

void DX11Context::Shutdown()
{
#ifdef ENGINE_IMGUI
    if (m_ImGuiInitialized) {
        ::ImGui_ImplDX11_Shutdown();
        m_ImGuiInitialized = false;
    }
#endif

    CleanupRenderTarget();
    if (m_SwapChain) {
        m_SwapChain->Release();
        m_SwapChain = nullptr;
    }
    if (m_Device) {
        m_Device->Release();
        m_Device = nullptr;
    }
}

#ifdef ENGINE_IMGUI
void DX11Context::ImGuiBegin()
{
    ::ImGui_ImplDX11_NewFrame();
}

void DX11Context::ImGuiEnd()
{
    constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_DeviceContext->OMSetRenderTargets(1, &m_MainRenderTargetView, nullptr);
    m_DeviceContext->ClearRenderTargetView(m_MainRenderTargetView, clearColor);
    ::ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DX11Context::InitializeImGui()
{
    ::ImGui_ImplDX11_Init(m_Device, m_DeviceContext);
    m_ImGuiInitialized = true;
}
#endif

void DX11Context::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0 || !m_Device || !m_SwapChain) {
        return;
    }

    CleanupRenderTarget();

    // Preserve the existing swap chain format
    HRESULT res = m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    ASSERT(SUCCEEDED(res), "Failed to resize DX11 swapchain buffers");

    CreateRenderTarget();
}

void DX11Context::CreateRenderTarget()
{
    // INFO("Creating render target");
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        HRESULT res = m_Device->CreateRenderTargetView(backBuffer, nullptr, &m_MainRenderTargetView);
        ASSERT(SUCCEEDED(res), "Failed to create render target view");
        backBuffer->Release();
    }
}

void DX11Context::CleanupRenderTarget()
{
    if (m_MainRenderTargetView) {
        m_MainRenderTargetView->Release();
        m_MainRenderTargetView = nullptr;
    }
}

#endif