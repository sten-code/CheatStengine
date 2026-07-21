# Matches the version required by the top-level CMakeLists.txt. Present here too so
# this file is self-consistent and never emits a "No cmake_minimum_required" warning.
cmake_minimum_required(VERSION 3.30)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    get_filename_component(_cs_repo_root "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
    message(FATAL_ERROR
            "CheatStengine is a subproject and cannot be configured on its own.\n"
            "It links the 'Engine' target and requires C++20, both set up by the "
            "top-level CMakeLists.txt. Configure from the repository root instead:\n"
            "    cmake -S \"${_cs_repo_root}\" -B \"${_cs_repo_root}/out/build/x64-Release\" -G Ninja -DCMAKE_BUILD_TYPE=Release\n"
            "    cmake --build \"${_cs_repo_root}/out/build/x64-Release\"\n"
            "See the 'Compiling from Source' section of the README.")
endif ()

project(CheatStengine)

include(vendor/zasm.cmake)
include(vendor/icon_font_cpp_headers.cmake)
include(vendor/json.cmake)
include(vendor/httplib.cmake)
#include(vendor/imgui_hex_editor.cmake)

add_executable(${PROJECT_NAME}
        src/CheatStengine/AddressEvaluator/Ast.cpp
        src/CheatStengine/AddressEvaluator/Evaluator.cpp
        src/CheatStengine/AddressEvaluator/Lexer.cpp
        src/CheatStengine/AddressEvaluator/Parser.cpp
        src/CheatStengine/Assembly/Assembler.cpp
        src/CheatStengine/Assembly/Formatter.cpp
        src/CheatStengine/main.cpp
        src/CheatStengine/MainLayer.cpp
        src/CheatStengine/Panes/DebugPane.cpp
        src/CheatStengine/Panes/DisassemblyPane.cpp
        src/CheatStengine/Panes/MemoryScannerPane.cpp
        src/CheatStengine/Panes/MemoryViewerPane.cpp
        src/CheatStengine/Panes/ModulesPane.cpp
        src/CheatStengine/Panes/PatternScannerPane.cpp
        src/CheatStengine/Panes/PEViewer.cpp
        src/CheatStengine/Panes/StructDissectPane.cpp
        src/CheatStengine/Panes/WatchPane.cpp
        src/CheatStengine/Process/Device.cpp
        src/CheatStengine/Process/KernelProcess.cpp
        src/CheatStengine/Process/Monitor.cpp
        src/CheatStengine/Process/Process.cpp
        src/CheatStengine/Process/WinAPIProcess.cpp
        src/CheatStengine/Server/AuthManager.cpp
        src/CheatStengine/Server/JobManager.cpp
        src/CheatStengine/Server/McpInstaller.cpp
        src/CheatStengine/Server/McpServer.cpp
        src/CheatStengine/Server/SessionManager.cpp
        src/CheatStengine/Server/ToolRegistry.cpp
        src/CheatStengine/Settings/ComboSettings.cpp
        src/CheatStengine/Settings/KeybindSetting.cpp
        src/CheatStengine/Settings/KernelSettings.cpp
        src/CheatStengine/Settings/SettingsCategory.cpp
        src/CheatStengine/Settings/SettingsManager.cpp
        src/CheatStengine/Settings/SliderSetting.cpp
        src/CheatStengine/Settings/ToggleSetting.cpp
        src/CheatStengine/Tools/MemoryScanner.cpp
        src/CheatStengine/Tools/PatternGenerator.cpp
        src/CheatStengine/Tools/PatternScanner.cpp
        src/CheatStengine/Tools/RTTI.cpp
        src/CheatStengine/Tools/StructDissect.cpp
        src/CheatStengine/UI/ImGui/EditableLabel.cpp
        src/CheatStengine/UI/ImGui/Fonts.cpp
        src/CheatStengine/UI/ImGui/IconCache.cpp
        src/CheatStengine/UI/ImGui/ImGuiHex.cpp
        src/CheatStengine/UI/ImGui/Menu.cpp
        src/CheatStengine/UI/MenuBar.cpp
        src/CheatStengine/UI/TitleBar.cpp
        src/CheatStengine/Utils.cpp
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            PLATFORM_WINDOWS
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            NTDDI_VERSION=0x0A000008
    )
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE
        src
        ${CMAKE_CURRENT_SOURCE_DIR}/../Shared
        ${ICON_FONT_CPP_HEADERS}
)
target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_link_libraries(${PROJECT_NAME} PRIVATE
        Engine
        zasm
        nlohmann_json::nlohmann_json
        httplib::httplib
        Dbghelp
        Shell32
        Cfgmgr32
        d3d11
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/Resources
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources
)
