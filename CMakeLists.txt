cmake_minimum_required(VERSION 3.20)
project(GlyphRenderer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
  message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_link_options(-fuse-ld=lld)
  elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_link_options(-fuse-ld=lld)
  elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC") # clang-cl
      add_compile_options(/std:c++20 /EHsc /W4)
    else() # MinGW clang++
      add_compile_options(-Wall -Wextra -Wpedantic)
      add_link_options(-fuse-ld=lld)
    endif()
  endif()
endif()

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

option(GLYPH_RENDERER_BUILD_CLI "Build GlyphRenderer CLI executable" ON)
option(GLYPH_RENDERER_BUILD_EXAMPLES "Build GlyphRenderer examples" ON)
option(GLYPH_RENDERER_BUILD_TESTS "Build GlyphRenderer tests" ON)

set(GLYPH_RENDERER_ENABLE_PNG_IO_DEFAULT OFF)
if (GLYPH_RENDERER_BUILD_CLI OR GLYPH_RENDERER_BUILD_EXAMPLES OR GLYPH_RENDERER_BUILD_TESTS)
  set(GLYPH_RENDERER_ENABLE_PNG_IO_DEFAULT ON)
endif()
option(GLYPH_RENDERER_ENABLE_PNG_IO
  "Enable PNG output support for CLI, examples, and tests"
  ${GLYPH_RENDERER_ENABLE_PNG_IO_DEFAULT}
)

if ((GLYPH_RENDERER_BUILD_CLI OR GLYPH_RENDERER_BUILD_EXAMPLES OR GLYPH_RENDERER_BUILD_TESTS) AND
    NOT GLYPH_RENDERER_ENABLE_PNG_IO)
  message(FATAL_ERROR
    "GLYPH_RENDERER_ENABLE_PNG_IO must be ON when CLI, examples, or tests are enabled")
endif()

find_package(Freetype CONFIG REQUIRED)
find_package(harfbuzz CONFIG REQUIRED)
if (GLYPH_RENDERER_ENABLE_PNG_IO)
  find_package(PNG REQUIRED)
endif()

add_library(GlyphRenderer
  src/main.cpp
  src/core/ir/Bitmap.cpp
  src/adapter/render/ColrV0Renderer.cpp
  src/adapter/render/ColrV1Renderer.cpp
)
if (GLYPH_RENDERER_ENABLE_PNG_IO)
  target_sources(GlyphRenderer PRIVATE
    src/adapter/io/PngWriter.cpp
  )
endif()
add_library(GlyphRenderer::GlyphRenderer ALIAS GlyphRenderer)
add_library(DokuText::DokuText ALIAS GlyphRenderer)

target_include_directories(GlyphRenderer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    src
)
target_link_libraries(GlyphRenderer PUBLIC
  Freetype::Freetype
  harfbuzz::harfbuzz
)
if (GLYPH_RENDERER_ENABLE_PNG_IO)
  target_link_libraries(GlyphRenderer PUBLIC PNG::PNG)
  target_compile_definitions(GlyphRenderer PRIVATE GLYPH_RENDERER_ENABLE_PNG_IO=1)
else()
  target_compile_definitions(GlyphRenderer PRIVATE GLYPH_RENDERER_ENABLE_PNG_IO=0)
endif()
target_compile_features(GlyphRenderer PUBLIC cxx_std_20)
if (WIN32)
  target_compile_definitions(GlyphRenderer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if (WIN32)
  else()
    target_compile_options(GlyphRenderer PRIVATE -stdlib=libstdc++)
    target_link_options(GlyphRenderer PRIVATE -stdlib=libstdc++ -lc++abi)
  endif()
endif()

if (GLYPH_RENDERER_BUILD_CLI)
  add_executable(app
    src/cli_main.cpp
  )
  target_link_libraries(app PRIVATE GlyphRenderer::GlyphRenderer)
  target_compile_features(app PRIVATE cxx_std_20)
  if (WIN32)
    target_compile_definitions(app PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()
endif()

if (GLYPH_RENDERER_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if (GLYPH_RENDERER_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(TARGETS GlyphRenderer EXPORT GlyphRendererTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT GlyphRendererTargets
  FILE GlyphRendererTargets.cmake
  NAMESPACE GlyphRenderer::
  DESTINATION lib/cmake/GlyphRenderer
)
