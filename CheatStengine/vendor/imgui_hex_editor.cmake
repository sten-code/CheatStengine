set(IMGUI_HEX_EDITOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui_hex_editor)

if (NOT EXISTS ${IMGUI_HEX_EDITOR_DIR})
    include(FetchContent)
    FetchContent_Declare(
            imgui_hex_editor
            GIT_REPOSITORY https://github.com/Teselka/imgui_hex_editor.git
            GIT_TAG main
            SOURCE_DIR ${IMGUI_HEX_EDITOR_DIR}
    )
    FetchContent_MakeAvailable(imgui_hex_editor)
endif ()

add_library(imgui_hex_editor STATIC
        ${IMGUI_HEX_EDITOR_DIR}/imgui_hex.cpp
)
target_link_libraries(imgui_hex_editor PRIVATE
        imgui
)
target_include_directories(imgui_hex_editor PUBLIC
        ${IMGUI_HEX_EDITOR_DIR}
)