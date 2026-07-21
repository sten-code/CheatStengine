project(CheatStengineDriver LANGUAGES CXX)

include(vendor/FindWDK.cmake)

find_package(WDK REQUIRED)

set(DRIVER_WINVER 0x0A00)
set(DRIVER_NTDDI_VERSION 0x0A000008)

if (MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif ()

wdk_add_driver(${PROJECT_NAME}
        WINVER ${DRIVER_WINVER}
        NTDDI_VERSION ${DRIVER_NTDDI_VERSION}
        src/CheatStengineDriver/Control.cpp
        src/CheatStengineDriver/Control.h
        src/CheatStengineDriver/Device.cpp
        src/CheatStengineDriver/Device.h
        src/CheatStengineDriver/Driver.cpp
        src/CheatStengineDriver/Driver.h
        src/CheatStengineDriver/Mapper.cpp
        src/CheatStengineDriver/Mapper.h
        src/CheatStengineDriver/Memory.cpp
        src/CheatStengineDriver/Memory.h
        src/CheatStengineDriver/Pnp.cpp
        src/CheatStengineDriver/Pnp.h
        src/CheatStengineDriver/Process.cpp
        src/CheatStengineDriver/Process.h
        Driver.inf
        ../Shared/Protocol.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Shared
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        _KMODE
        WINVER=${DRIVER_WINVER}
)

target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /GR-
        /EHs-c-
)

target_link_libraries(${PROJECT_NAME} WDK::WDMSEC)

get_target_property(DRIVER_LINK_FLAGS ${PROJECT_NAME} LINK_FLAGS)
string(REPLACE [=[/DRIVER ]=] [=[/DRIVER:WDM ]=] DRIVER_LINK_FLAGS ${DRIVER_LINK_FLAGS})
string(REPLACE [=[/MERGE:_TEXT=.text;_PAGE=PAGE ]=] "" DRIVER_LINK_FLAGS ${DRIVER_LINK_FLAGS})
string(REPLACE [=[/MERGE:_TEXT=.text_PAGE=PAGE ]=] "" DRIVER_LINK_FLAGS ${DRIVER_LINK_FLAGS})
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS ${DRIVER_LINK_FLAGS})
set_source_files_properties(Driver.inf PROPERTIES HEADER_FILE_ONLY TRUE)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Driver.inf
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/Driver.inf
)
