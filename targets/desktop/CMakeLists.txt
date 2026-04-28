add_executable(${PROJECT_NAME}
    ${SOLUS_SOURCE}
    ${CCSD}/src/platforms/desktop.c
)
target_compile_definitions(${PROJECT_NAME} PUBLIC _CRT_SECURE_NO_WARNINGS)

if (MSVC) # Good luck if you're on MSVC
    add_compile_options(/W4 /WX)
elseif(UNIX)
    add_compile_options(
        -Wall -Wextra -Werror
        -Wpedantic -Wconversion -Wuninitialized
        -Wsign-conversion -Wnull-dereference
        $<$<CONFIG:Release>:-O3>
    )
endif()

# Dependencies
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    solus
    SDL2::SDL2-static
    SDL2_image::SDL2_image-static
    SDL2_mixer::SDL2_mixer-static
)

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

add_custom_target(stage ALL
    DEPENDS ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/targets"
        "${CMAKE_SOURCE_DIR}/build/out"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/build/compile_commands.json"
    COMMENT "Staging binaries and compile_commands.json"
)
