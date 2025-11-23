project(SampleApp)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC src)
target_link_libraries(${PROJECT_NAME} PUBLIC
        Engine
)