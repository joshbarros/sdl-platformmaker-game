# Install script for directory: D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/platformmaker")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/harfbuzz" TYPE FILE FILES
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-aat-layout.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-aat.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-blob.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-buffer.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-common.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-cplusplus.hh"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-deprecated.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-draw.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-face.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-font.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-map.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-color.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-deprecated.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-font.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-layout.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-math.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-meta.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-metrics.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-name.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-shape.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot-var.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ot.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-paint.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-set.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-shape-plan.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-shape.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-style.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-unicode.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-version.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-ft.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-gdi.h"
    "D:/dev/cardrogue/build/_deps/sdl3_ttf-src/external/harfbuzz/src/hb-uniscribe.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/dev/sdl-platformmaker-game/build-test/_deps/sdl3_ttf-build/external/harfbuzz/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
