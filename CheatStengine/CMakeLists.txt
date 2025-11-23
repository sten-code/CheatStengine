project(CheatStengine)

include(vendor/zasm.cmake)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

if (WIN32)
    add_definitions(-DNOMINMAX)
endif ()

target_include_directories(${PROJECT_NAME} PUBLIC src)
target_link_libraries(${PROJECT_NAME} PUBLIC
        Engine
        zasm
        Dbghelp
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/Resources
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources
)