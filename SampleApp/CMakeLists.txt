project(SampleApp)

add_executable(${PROJECT_NAME}
        src/SampleApp/main.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC src)
target_link_libraries(${PROJECT_NAME} PUBLIC
        Engine
)
