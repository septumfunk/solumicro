cmake_minimum_required(VERSION 3.28)

project(solumicro C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CCSD "${CMAKE_CURRENT_SOURCE_DIR}")
set(SOLUS_SOURCE
    ${CCSD}/src/game.c
    ${CCSD}/src/asset.c

    ${CCSD}/src/api/api.c
    ${CCSD}/src/api/collision.c
    ${CCSD}/src/api/graphics.c
    ${CCSD}/src/api/kb_mouse.c
    ${CCSD}/src/api/ctrl.c
    ${CCSD}/src/api/sound.c
    ${CCSD}/src/api/state.c
)

# Fetch Dependencies
include(FetchContent)
FetchContent_Declare(
        solus
        GIT_REPOSITORY https://github.com/solus-lang/solus.git
        GIT_TAG next
)
FetchContent_MakeAvailable(solus)

# Targets
if (VITA)
    add_subdirectory("targets/vita")
else()
    add_subdirectory("targets/desktop")
endif()