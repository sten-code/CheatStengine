set(FINDWDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/FindWDK)

if (NOT EXISTS ${FINDWDK_DIR})
    include(FetchContent)
    FetchContent_Declare(
            FindWDK
            GIT_REPOSITORY https://github.com/SergiusTheBest/FindWDK.git
            GIT_TAG master
            SOURCE_DIR ${FINDWDK_DIR}
    )
    FetchContent_MakeAvailable(FindWDK)
else ()
    add_subdirectory(${FINDWDK_DIR})
endif ()

list(APPEND CMAKE_MODULE_PATH "${FINDWDK_DIR}/cmake")