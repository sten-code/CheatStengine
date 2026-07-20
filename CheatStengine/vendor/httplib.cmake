set(CPP_HTTPLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cpp-httplib)

if (NOT EXISTS ${CPP_HTTPLIB_DIR})
    include(FetchContent)
    FetchContent_Declare(
            httplib
            GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
            GIT_TAG v0.18.3
            SOURCE_DIR ${CPP_HTTPLIB_DIR}
    )
    FetchContent_MakeAvailable(httplib)
else ()
    add_subdirectory(${CPP_HTTPLIB_DIR})
endif ()
