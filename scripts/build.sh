#!/bin/sh
set -eu
: "${VITASDK:?Set VITASDK to your softfp VitaSDK installation}"
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSTARFRONT_NATIVE_HEIGHT=600 \
  -DSTARFRONT_AUTOSTART=OFF -DSTARFRONT_FILE_LOG=OFF \
  -DSTARFRONT_GL_TRACE=OFF -DSTARFRONT_DISPLAY_TEST=OFF
cmake --build build --parallel "${SF_BUILD_JOBS:-4}"
