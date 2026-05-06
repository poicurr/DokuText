cmake_minimum_required(VERSION 3.20)
project(DokuText VERSION 0.1.0 LANGUAGES C CXX)

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

option(DOKU_TEXT_BUILD_CLI "Build DokuText CLI executable" ON)
option(DOKU_TEXT_BUILD_EXAMPLES "Build DokuText examples" ON)
option(DOKU_TEXT_BUILD_TESTS "Build DokuText tests" ON)

set(DOKU_TEXT_ENABLE_PNG_IO_DEFAULT OFF)
if (DOKU_TEXT_BUILD_CLI OR DOKU_TEXT_BUILD_EXAMPLES OR DOKU_TEXT_BUILD_TESTS)
  set(DOKU_TEXT_ENABLE_PNG_IO_DEFAULT ON)
endif()
option(DOKU_TEXT_ENABLE_PNG_IO
  "Enable PNG output support for CLI, examples, and tests"
  ${DOKU_TEXT_ENABLE_PNG_IO_DEFAULT}
)

if ((DOKU_TEXT_BUILD_CLI OR DOKU_TEXT_BUILD_EXAMPLES OR DOKU_TEXT_BUILD_TESTS) AND
    NOT DOKU_TEXT_ENABLE_PNG_IO)
  message(FATAL_ERROR
    "DOKU_TEXT_ENABLE_PNG_IO must be ON when CLI, examples, or tests are enabled")
endif()

find_package(freetype CONFIG REQUIRED)
find_package(harfbuzz REQUIRED)
if (DOKU_TEXT_ENABLE_PNG_IO)
  find_package(PNG REQUIRED)
endif()

add_library(DokuText
  src/main.cpp
  src/core/ir/Bitmap.cpp
  src/adapter/render/ColrV0Renderer.cpp
  src/adapter/render/ColrV1Renderer.cpp
)
if (DOKU_TEXT_ENABLE_PNG_IO)
  target_sources(DokuText PRIVATE
    src/adapter/io/PngWriter.cpp
  )
endif()
add_library(DokuText::DokuText ALIAS DokuText)

target_include_directories(DokuText
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    src
)
target_link_libraries(DokuText PUBLIC
  Freetype::Freetype
  harfbuzz::harfbuzz
)
if (DOKU_TEXT_ENABLE_PNG_IO)
  target_link_libraries(DokuText PUBLIC PNG::PNG)
  target_compile_definitions(DokuText PRIVATE DOKU_TEXT_ENABLE_PNG_IO=1)
else()
  target_compile_definitions(DokuText PRIVATE DOKU_TEXT_ENABLE_PNG_IO=0)
endif()
target_compile_features(DokuText PUBLIC cxx_std_20)
if (WIN32)
  target_compile_definitions(DokuText PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if (WIN32)
  else()
    target_compile_options(DokuText PRIVATE -stdlib=libstdc++)
    target_link_options(DokuText PRIVATE -stdlib=libstdc++ -lc++abi)
  endif()
endif()

if (DOKU_TEXT_BUILD_CLI)
  add_executable(app
    src/cli_main.cpp
  )
  target_link_libraries(app PRIVATE DokuText::DokuText)
  target_compile_features(app PRIVATE cxx_std_20)
  if (WIN32)
    target_compile_definitions(app PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()
endif()

if (DOKU_TEXT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if (DOKU_TEXT_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(TARGETS DokuText EXPORT DokuTextTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT DokuTextTargets
  FILE DokuTextTargets.cmake
  NAMESPACE DokuText::
  DESTINATION lib/cmake/DokuText
)
