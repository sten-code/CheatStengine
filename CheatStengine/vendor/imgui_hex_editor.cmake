include(FetchContent)

FetchContent_Declare(
        imgui_hex_editor
        GIT_REPOSITORY https://github.com/Teselka/imgui_hex_editor.git
        GIT_TAG main
)
FetchContent_MakeAvailable(imgui_hex_editor)

add_library(imgui_hex_editor STATIC
        ${imgui_hex_editor_SOURCE_DIR}/imgui_hex.cpp
)
target_link_libraries(imgui_hex_editor PRIVATE
        imgui
)
target_include_directories(imgui_hex_editor PUBLIC
        ${imgui_hex_editor_SOURCE_DIR}
)
