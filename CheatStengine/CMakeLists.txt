project(CheatStengine)

include(vendor/zasm.cmake)
include(vendor/icon_font_cpp_headers.cmake)
include(vendor/json.cmake)
include(vendor/httplib.cmake)
#include(vendor/imgui_hex_editor.cmake)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

if (WIN32)
    add_definitions(-DNOMINMAX)
    # cpp-httplib pulls in winsock2.h; define this so <Windows.h> doesn't drag in
    # the legacy winsock.h first and cause sockaddr/type redefinition errors.
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    # Needed so the DX11 context header (guarded by PLATFORM_WINDOWS) is visible
    # here; the icon cache uploads process icons through the live D3D11 device.
    add_definitions(-DPLATFORM_WINDOWS)
endif ()

target_include_directories(${PROJECT_NAME} PUBLIC
        src
        ${ICON_FONT_CPP_HEADERS}
)
target_link_libraries(${PROJECT_NAME} PUBLIC
        Engine
        zasm
        nlohmann_json::nlohmann_json
        httplib::httplib
        Dbghelp
        Shell32
        d3d11
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/Resources
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources
)