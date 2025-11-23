include(FetchContent)

FetchContent_Declare(
        zasm
        GIT_REPOSITORY https://github.com/zyantific/zasm.git
        GIT_TAG master
)
FetchContent_MakeAvailable(zasm)
