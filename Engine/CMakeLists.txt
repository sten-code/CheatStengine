project(Engine LANGUAGES CXX)

include(vendor/imgui.cmake)
include(vendor/spdlog.cmake)

add_library(${PROJECT_NAME}
        src/Engine/Core/Application.cpp
        src/Engine/Core/Layers/Layer.cpp
        src/Engine/Core/Layers/LayerStack.cpp
        src/Engine/Core/Log.cpp
        src/Engine/Core/Window.cpp
        src/Engine/Renderer/GraphicsContext.cpp
        src/Platform/DX11/DX11Context.cpp
        src/Platform/Win32/Win32Window.cpp
)

target_compile_definitions(imgui PUBLIC IMGUI_USE_WCHAR32)
target_compile_definitions(${PROJECT_NAME} PUBLIC ENGINE_IMGUI)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC PLATFORM_WINDOWS NOMINMAX)
elseif (UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC PLATFORM_LINUX)
elseif (APPLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC PLATFORM_MACOS)
endif ()

target_include_directories(${PROJECT_NAME} PUBLIC src)
target_link_libraries(${PROJECT_NAME} PUBLIC imgui spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE
        d3d11
        d3dcompiler
        Dwmapi
        Shlwapi
)
