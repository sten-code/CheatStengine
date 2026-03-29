project(CheatStengineDriver)

include(vendor/FindWDK.cmake)

find_package(WDK REQUIRED)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)
wdk_add_driver(${PROJECT_NAME}
        NTDDI_VERSION 0x0A000010
        ${SOURCE_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE _KMODE)
