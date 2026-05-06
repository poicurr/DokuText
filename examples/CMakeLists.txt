function(add_glyphrenderer_example target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE GlyphRenderer::GlyphRenderer)
  target_compile_features(${target_name} PRIVATE cxx_std_20)

  if(WIN32)
    target_compile_definitions(
      ${target_name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
    )
  endif()
endfunction()

add_glyphrenderer_example(GlyphRendererExampleBasic BasicEmbedding.cpp)
add_glyphrenderer_example(
  GlyphRendererExampleMetricsLog
  MetricsLogEmbedding.cpp
)
add_glyphrenderer_example(
  GlyphRendererExampleEngineSession
  EngineSessionEmbedding.cpp
)
add_glyphrenderer_example(
  GlyphRendererExampleRendererContract
  RendererIntegrationContract.cpp
)
add_glyphrenderer_example(
  GlyphRendererExampleStaticAtlas
  StaticAtlasEmbedding.cpp
)
add_glyphrenderer_example(DokuTextExampleEmbedding DokuTextEmbedding.cpp)
