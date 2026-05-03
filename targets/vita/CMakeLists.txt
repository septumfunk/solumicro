include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "solumicro")
set(VITA_TITLEID  "SFNK00001")
set(VITA_VERSION  "01.00")

set(VITA_MACROS
    __vita__
    POSIX_COMPAT
    VITA_APP_NAME="${VITA_APP_NAME}"
    VITA_TITLEID="${VITA_TITLEID}"
)

add_executable(${PROJECT_NAME}
    ${SOLUS_SOURCE}
    ${CCSD}/src/platforms/vita.c
)
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Werror
    -Wpedantic -Wconversion -Wuninitialized
    -Wsign-conversion
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-Wno-error=null-dereference>
    $<$<CONFIG:Release>:-Wno-error=maybe-uninitialized>
)
target_compile_options(solus PRIVATE
    $<$<CONFIG:Release>:-Wno-error=null-dereference>
    $<$<CONFIG:Release>:-Wno-error=maybe-uninitialized>
)
target_compile_definitions(solus PUBLIC ${VITA_MACROS})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${VITA_MACROS})

# Dependencies
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
  solus

  SDL2_image
  SDL2_mixer
  SDL2

  png jpeg z
  webpdemux webp

  mpg123
  vorbisfile vorbis ogg
  opusfile opus
  modplug xmp

  pthread stdc++ m

  SceGxm_stub
  SceDisplay_stub
  SceAudio_stub
  SceAudioIn_stub
  SceTouch_stub
  SceMotion_stub
  SceCommonDialog_stub
  SceIme_stub
  SceHid_stub
  SceCtrl_stub
  SceNet_stub
  SceNetCtl_stub
  debugnet
)

# Vita
set(VPK_INCLUDE_FILES)
set(VPK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/targets/vita/include")
file(GLOB_RECURSE INCLUDE_FILES
    CONFIGURE_DEPENDS
    "${VPK_INCLUDE_DIR}/*"
)

foreach(INCLUDE_FILE ${INCLUDE_FILES})
    file(RELATIVE_PATH REL_INCLUDE_FILE
        ${VPK_INCLUDE_DIR}
        "${INCLUDE_FILE}"
    )
    list(APPEND VPK_INCLUDE_FILES
        FILE "${INCLUDE_FILE}" "${REL_INCLUDE_FILE}"
    )
    message(STATUS "VPK INCLUDE:\n   ${REL_INCLUDE_FILE} -> ux0:/app/${VITA_TITLEID}/${REL_INCLUDE_FILE}")
endforeach()

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE assets/icon0.png sce_sys/icon0.png
  FILE assets/pic0.png sce_sys/pic0.png
  FILE assets/bg.png sce_sys/livearea/contents/bg.png
  FILE assets/startup.png sce_sys/livearea/contents/startup.png
  FILE assets/template.xml sce_sys/livearea/contents/template.xml

  ${VPK_INCLUDE_FILES}
)

add_custom_target(stage ALL
    DEPENDS ${PROJECT_NAME}.vpk
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/targets"
        "${CMAKE_SOURCE_DIR}/build/out"
    COMMENT "Copied binaries"
)