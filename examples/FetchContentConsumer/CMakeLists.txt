cmake_minimum_required(VERSION 3.20)

project(GlyphRendererFetchContentConsumer LANGUAGES CXX)

include(FetchContent)

set(GLYPH_RENDERER_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(GLYPH_RENDERER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLYPH_RENDERER_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  GlyphRenderer
  SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..
)

FetchContent_MakeAvailable(GlyphRenderer)

add_executable(FetchContentConsumer
  main.cpp
)
target_link_libraries(FetchContentConsumer PRIVATE GlyphRenderer::GlyphRenderer)
target_compile_features(FetchContentConsumer PRIVATE cxx_std_20)
