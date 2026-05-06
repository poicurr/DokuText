function(add_dokutext_example target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE DokuText::DokuText)
  target_compile_features(${target_name} PRIVATE cxx_std_20)

  if(WIN32)
    target_compile_definitions(
      ${target_name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
    )
  endif()
endfunction()

add_dokutext_example(DokuTextExampleBasic BasicEmbedding.cpp)
add_dokutext_example(
  DokuTextExampleMetricsLog
  MetricsLogEmbedding.cpp
)
add_dokutext_example(
  DokuTextExampleEngineSession
  EngineSessionEmbedding.cpp
)
add_dokutext_example(
  DokuTextExampleRendererContract
  RendererIntegrationContract.cpp
)
add_dokutext_example(
  DokuTextExampleStaticAtlas
  StaticAtlasEmbedding.cpp
)
add_dokutext_example(DokuTextExampleEmbedding DokuTextEmbedding.cpp)
