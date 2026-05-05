function(add_glyphrenderer_test target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE GlyphRenderer::GlyphRenderer)
  target_compile_features(${target_name} PRIVATE cxx_std_20)

  if(WIN32)
    target_compile_definitions(
      ${target_name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
    )
  endif()
endfunction()

add_glyphrenderer_test(GlyphRendererTestSmoke SmokeCliTest.cpp)
add_glyphrenderer_test(GlyphRendererTestRenderTextApi RenderTextApiTest.cpp)
add_glyphrenderer_test(GlyphRendererTestMetricsLog MetricsLogTest.cpp)
add_glyphrenderer_test(GlyphRendererTestScanLog ScanLogTest.cpp)
add_glyphrenderer_test(
  GlyphRendererTestFallbackDiagnostics
  FallbackDiagnosticsTest.cpp
)
add_glyphrenderer_test(
  GlyphRendererTestShapeLayoutAtlas
  ShapeLayoutAtlasTest.cpp
)
add_glyphrenderer_test(
  GlyphRendererTestMultiLineTextApi
  MultiLineTextApiTest.cpp
)
add_glyphrenderer_test(GlyphRendererTestAtlasCacheApi AtlasCacheApiTest.cpp)
add_glyphrenderer_test(GlyphRendererTestVisualPng VisualPngScenarioTest.cpp)
add_glyphrenderer_test(GlyphRendererTestBakedAtlasPng BakedAtlasPngTest.cpp)
add_glyphrenderer_test(GlyphRendererTestEmptyFallback EmptyFallbackTest.cpp)
add_glyphrenderer_test(
  GlyphRendererTestIntegrationAdapter
  IntegrationAdapterTest.cpp
)
add_glyphrenderer_test(
  GlyphRendererTestTextSystemFacade
  TextSystemFacadeTest.cpp
)
add_glyphrenderer_test(GlyphRendererTestBakedAtlasApi BakedAtlasApiTest.cpp)

target_include_directories(
  GlyphRendererTestVisualPng PRIVATE ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(
  GlyphRendererTestBakedAtlasPng PRIVATE ${CMAKE_SOURCE_DIR}/src
)
