cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain")
  else()
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/.tools/vitasdk/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain")
  endif()
endif()
project(starfront_vita C CXX ASM)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(CMAKE_C_STANDARD 11)
add_subdirectory(deps)
option(STARFRONT_AUTOSTART "Enable isolated emulator title and mouse-touch input" OFF)
option(STARFRONT_FILE_LOG "Write diagnostic port.log (emulator builds only by default)" ${STARFRONT_AUTOSTART})
option(STARFRONT_GL_TRACE "Trace original GL calls without changing their results" OFF)
option(STARFRONT_DISPLAY_TEST "Run isolated CPU/vitaGL display diagnostics" OFF)
set(STARFRONT_NATIVE_HEIGHT "600" CACHE STRING "Logical 1024-wide Android layout height (600 or 640)")
if(NOT STARFRONT_NATIVE_HEIGHT MATCHES "^(600|640)$")
  message(FATAL_ERROR "Use a reviewed 1024x600 or 1024x640 logical layout")
endif()
set(STARFRONT_VITAGL_LIBRARY "" CACHE FILEPATH "Optional isolated emulator VitaGL library")
if(STARFRONT_VITAGL_LIBRARY)
  if(NOT STARFRONT_AUTOSTART)
    message(FATAL_ERROR "Alternate VitaGL library requires the isolated emulator build")
  endif()
  if(NOT EXISTS "${STARFRONT_VITAGL_LIBRARY}")
    message(FATAL_ERROR "Alternate VitaGL library not found")
  endif()
  set(STARFRONT_VITAGL_LINK "${STARFRONT_VITAGL_LIBRARY}")
else()
  set(STARFRONT_VITAGL_LINK sf_vitaGL)
endif()
add_executable(starfront
  src/main.c src/log.c src/paths.c src/bridges.c src/fd_bridge.c src/signal_bridge.c src/network_bridge.c src/audio_track.c src/drm_preferences.c src/graphics_trace.c src/graphics_compat.c src/display_geometry.c src/emulator_touch.c src/display_test.c src/jni.c src/imports.c
  vendor/so-loader/so_util.c vendor/debugscreen/debugScreen.c)
target_include_directories(starfront PRIVATE src vendor/so-loader vendor/debugscreen ${SF_DEP_INCLUDE})
target_compile_options(starfront PRIVATE -O2 -g -Wall -Wextra -Wno-unused-parameter -mfloat-abi=softfp -D_GNU_SOURCE)
target_compile_definitions(starfront PRIVATE SF_NATIVE_H=${STARFRONT_NATIVE_HEIGHT}
  STARFRONT_FILE_LOG=$<BOOL:${STARFRONT_FILE_LOG}>)
if(STARFRONT_AUTOSTART)
  target_compile_definitions(starfront PRIVATE STARFRONT_AUTOSTART=1)
endif()
if(STARFRONT_GL_TRACE)
  target_compile_definitions(starfront PRIVATE STARFRONT_GL_TRACE=1)
endif()
if(STARFRONT_DISPLAY_TEST)
  if(NOT STARFRONT_AUTOSTART)
    message(FATAL_ERROR "Display diagnostics require the isolated emulator build")
  endif()
  target_compile_definitions(starfront PRIVATE STARFRONT_DISPLAY_TEST=1)
endif()
target_link_options(starfront PRIVATE -Wl,-q -Wl,-Map=${CMAKE_BINARY_DIR}/starfront.map)
target_link_libraries(starfront
  -Wl,--whole-archive sf_pthread -Wl,--no-whole-archive
  ${STARFRONT_VITAGL_LINK} sf_vitashark sf_shacc stdc++ m sf_mathneon sf_z sf_polarssl
  taihen_stub kubridge_stub_weak
  SceAppMgr_stub SceAppUtil_stub SceAudio_stub SceCtrl_stub
  SceCommonDialog_stub SceDisplay_stub SceKernelDmacMgr_stub SceGxm_stub
  SceNet_stub SceNetCtl_stub SceRtc_stub
  SceShaccCg_stub SceSysmodule_stub ScePower_stub SceTouch_stub SceVshBridge_stub)
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12 -s CONTENT_ID=HB0001-SFHP00001_00-0000000000000000")
# The softfp SDK's old mksfoex injects deprecated fields and VERSION=00.00.
# Use the metadata schema of the released Gun Bros VPK without changing the SDK.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(VITA_MKSFOEX "${Python3_EXECUTABLE};${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_sfo.py")
vita_create_self(eboot.bin starfront UNSAFE)
set(STARFRONT_APP_NAME "Starfront Test")
if(STARFRONT_AUTOSTART)
  set(STARFRONT_APP_NAME "Starfront Emulator Test")
endif()
file(GLOB SF_LICENSE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/*.txt")
set(SF_LICENSE_ARGS)
foreach(SF_LICENSE_FILE IN LISTS SF_LICENSE_FILES)
  get_filename_component(SF_LICENSE_NAME "${SF_LICENSE_FILE}" NAME)
  list(APPEND SF_LICENSE_ARGS FILE "${SF_LICENSE_FILE}" "licenses/${SF_LICENSE_NAME}")
endforeach()
vita_create_vpk(Starfront-test.vpk SFHP00001 eboot.bin
  VERSION 00.06 NAME "${STARFRONT_APP_NAME}"
  ${SF_LICENSE_ARGS}
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING" COPYING
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md" LICENSE.md
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY.md" THIRD_PARTY.md
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/DISTRIBUTION.txt" DISTRIBUTION.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/docs/BUILD.md" docs/BUILD.md
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/ARTWORK.md" assets/ARTWORK.md
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/sce_sys/icon0.png" sce_sys/icon0.png
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/sce_sys/pic0.png" sce_sys/pic0.png
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/sce_sys/livearea/contents/bg-r2.png" sce_sys/livearea/contents/bg-r2.png
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/sce_sys/livearea/contents/startup-r3.png" sce_sys/livearea/contents/startup-r3.png
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/sce_sys/livearea/contents/template.xml" sce_sys/livearea/contents/template.xml
)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Starfront-test.vpk_param.sfo"
  APPEND DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_sfo.py")
