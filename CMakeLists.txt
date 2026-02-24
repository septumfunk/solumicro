cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

project(solumicro C)
add_executable(${PROJECT_NAME}
    src/game.c
    src/script.c
    src/asset.c
)
target_compile_definitions(${PROJECT_NAME} PUBLIC _CRT_SECURE_NO_WARNINGS)

# Sanitizer
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
if (ENABLE_SANITIZERS AND NOT MSVC)
    message(STATUS "Sanitizers enabled")
    set(SANITIZER_FLAGS
        -fsanitize=address
        -fsanitize=undefined
        -fno-omit-frame-pointer
        -g
    )
    target_compile_options(${PROJECT_NAME} PUBLIC ${SANITIZER_FLAGS})
    target_link_options(${PROJECT_NAME} PUBLIC ${SANITIZER_FLAGS})
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_SANITIZERS)
    set(SANITIZER_COMPILE_FLAGS ${SANITIZER_FLAGS})
    set(SANITIZER_DEFINE USE_SANITIZERS)
endif()

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
if (MSVC) # Good luck if you're on MSVC
    add_compile_options(/W4 /WX)
elseif(UNIX)
    add_compile_options(
        -Wall -Wextra -Werror
        -Wpedantic -Wconversion -Wuninitialized
        -Wsign-conversion -Wnull-dereference
    )
endif()
target_compile_options(${PROJECT_NAME} PUBLIC ${COMPILE_OPTIONS})

# Fetch Dependencies
include(FetchContent)
FetchContent_Declare(
        solus
        GIT_REPOSITORY https://github.com/solus-lang/solus.git
        GIT_TAG next
)
FetchContent_MakeAvailable(solus)
find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE solus glfw raylib)
