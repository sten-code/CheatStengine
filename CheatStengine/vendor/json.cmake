set(NLOHMANN_JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/json)

if (NOT EXISTS ${NLOHMANN_JSON_DIR})
    include(FetchContent)
    FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
            SOURCE_DIR ${NLOHMANN_JSON_DIR}
    )
    FetchContent_MakeAvailable(nlohmann_json)
else ()
    add_subdirectory(${NLOHMANN_JSON_DIR})
endif ()
