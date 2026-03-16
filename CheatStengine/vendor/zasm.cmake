set(ZASM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/zasm)

if (NOT EXISTS ${ZASM_DIR})
    include(FetchContent)
    FetchContent_Declare(
            zasm
            GIT_REPOSITORY https://github.com/zyantific/zasm.git
            GIT_TAG master
            SOURCE_DIR ${ZASM_DIR}
    )
    FetchContent_MakeAvailable(zasm)
else ()
    add_subdirectory(${ZASM_DIR})
endif ()