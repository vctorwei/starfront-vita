# Copyright (c) 2026 vctorwei. MIT; upstream sources retain their own licenses.
set(SF_DEP_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/vita-headers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vitaGL/source
    ${CMAKE_CURRENT_SOURCE_DIR}/vitaShaRK/source
    ${CMAKE_CURRENT_SOURCE_DIR}/SceShaccCgExt/include
    ${CMAKE_CURRENT_SOURCE_DIR}/math-neon/source
    ${CMAKE_CURRENT_SOURCE_DIR}/polarssl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/zlib)
set(SF_DEP_INCLUDE ${SF_DEP_INCLUDE} PARENT_SCOPE)

file(GLOB VGL_C CONFIGURE_DEPENDS vitaGL/source/*.c vitaGL/source/utils/*.c vitaGL/source/utils/preprocessor/*.c)
file(GLOB VGL_CPP CONFIGURE_DEPENDS vitaGL/source/*.cpp vitaGL/source/utils/*.cpp vitaGL/source/utils/preprocessor/*.cpp)
add_library(sf_vitaGL STATIC ${VGL_C} ${VGL_CPP})
target_include_directories(sf_vitaGL PRIVATE ${SF_DEP_INCLUDE})
target_compile_options(sf_vitaGL PRIVATE -g -O3 -ffast-math -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -mfp16-format=ieee -Wno-incompatible-pointer-types -Wno-stringop-overflow $<$<COMPILE_LANGUAGE:CXX>:-fexceptions> $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++11> $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings>)
target_compile_definitions(sf_vitaGL PRIVATE HAVE_SOFTFP_ABI SKIP_SPLASHSCREEN VGL_GIT_HASH="dca4b9d")

file(GLOB SHARK_C CONFIGURE_DEPENDS vitaShaRK/source/*.c vitaShaRK/source/*.S)
add_library(sf_vitashark STATIC ${SHARK_C})
target_include_directories(sf_vitashark PRIVATE ${SF_DEP_INCLUDE})
add_library(sf_shacc STATIC SceShaccCgExt/src/shacccg_ext.c SceShaccCgExt/src/shacccg_ext_oop.cpp)
target_include_directories(sf_shacc PRIVATE ${SF_DEP_INCLUDE})
target_compile_options(sf_shacc PRIVATE -fno-optimize-sibling-calls $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions> $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti> $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++11> $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)
file(GLOB MATH_C CONFIGURE_DEPENDS math-neon/source/*.c)
add_library(sf_mathneon STATIC ${MATH_C})
foreach(lib sf_vitashark sf_shacc sf_mathneon)
  target_compile_options(${lib} PRIVATE -g -O2 -ffast-math -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -ftree-vectorize)
endforeach()

# Only the AES/Base64 modules used by this loader are linked. Full upstream
# PolarSSL source is included, and no network/SSL service is exposed by this target.
add_library(sf_polarssl STATIC polarssl/library/aes.c polarssl/library/base64.c polarssl/library/aesni.c polarssl/library/padlock.c)
target_include_directories(sf_polarssl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/polarssl/include)
target_compile_options(sf_polarssl PRIVATE -O2 -mfloat-abi=softfp)
set(ZLIB_C adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)
list(TRANSFORM ZLIB_C PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/zlib/")
add_library(sf_z STATIC ${ZLIB_C})
target_include_directories(sf_z PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/zlib)
target_compile_options(sf_z PRIVATE -O2 -mfloat-abi=softfp)
include(${CMAKE_CURRENT_SOURCE_DIR}/pthread-sources.cmake)
add_library(sf_pthread STATIC ${SF_PTHREAD_C})
target_include_directories(sf_pthread PRIVATE pthread-embedded/platform/vita pthread-embedded)
target_compile_options(sf_pthread PRIVATE -O3 -fno-strict-aliasing -mfloat-abi=softfp)
