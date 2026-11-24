cmake_minimum_required(VERSION 3.25)
project(platformmaker VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.20
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_Declare(SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE)

FetchContent_MakeAvailable(SDL3)

# Same workaround as the card game: SDL3 from FetchContent is a subproject and
# has no SDL3::Headers target, which SDL3_ttf checks before find_package().
if(NOT TARGET SDL3::Headers)
  add_library(SDL3::Headers INTERFACE IMPORTED)
  set_target_properties(SDL3::Headers PROPERTIES
    INTERFACE_LINK_LIBRARIES SDL3::SDL3)
endif()

FetchContent_MakeAvailable(SDL3_ttf nlohmann_json)

add_executable(platformmaker
  src/main.cpp
  src/level.cpp
  src/world.cpp
  src/share.cpp
  src/replay.cpp
  src/renderer.cpp)

target_include_directories(platformmaker PRIVATE src third_party)
target_compile_definitions(platformmaker PRIVATE
  PM_SOURCE_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_link_libraries(platformmaker PRIVATE
  SDL3::SDL3
  SDL3_ttf::SDL3_ttf
  nlohmann_json::nlohmann_json)

add_custom_command(TARGET platformmaker POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/assets"
          "$<TARGET_FILE_DIR:platformmaker>/assets"
  COMMENT "Copying assets")
