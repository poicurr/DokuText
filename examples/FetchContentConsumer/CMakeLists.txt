cmake_minimum_required(VERSION 3.20)

project(DokuTextFetchContentConsumer LANGUAGES CXX)

include(FetchContent)

set(GLYPH_RENDERER_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(GLYPH_RENDERER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLYPH_RENDERER_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  DokuText
  SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..
)

FetchContent_MakeAvailable(DokuText)

add_executable(FetchContentConsumer
  main.cpp
)
target_link_libraries(FetchContentConsumer PRIVATE DokuText::DokuText)
target_compile_features(FetchContentConsumer PRIVATE cxx_std_20)
