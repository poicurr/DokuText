function(add_dokutext_test target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE DokuText::DokuText)
  target_compile_features(${target_name} PRIVATE cxx_std_20)

  if(WIN32)
    target_compile_definitions(
      ${target_name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
    )
  endif()
endfunction()

add_dokutext_test(DokuTextTestSmoke SmokeCliTest.cpp)
add_dokutext_test(DokuTextTestRenderTextApi RenderTextApiTest.cpp)
add_dokutext_test(DokuTextTestMetricsLog MetricsLogTest.cpp)
add_dokutext_test(DokuTextTestScanLog ScanLogTest.cpp)
add_dokutext_test(
  DokuTextTestFallbackDiagnostics
  FallbackDiagnosticsTest.cpp
)
add_dokutext_test(
  DokuTextTestShapeLayoutAtlas
  ShapeLayoutAtlasTest.cpp
)
add_dokutext_test(
  DokuTextTestMultiLineTextApi
  MultiLineTextApiTest.cpp
)
add_dokutext_test(DokuTextTestAtlasCacheApi AtlasCacheApiTest.cpp)
add_dokutext_test(DokuTextTestVisualPng VisualPngScenarioTest.cpp)
add_dokutext_test(DokuTextTestBakedAtlasPng BakedAtlasPngTest.cpp)
add_dokutext_test(DokuTextTestEmptyFallback EmptyFallbackTest.cpp)
add_dokutext_test(
  DokuTextTestIntegrationAdapter
  IntegrationAdapterTest.cpp
)
add_dokutext_test(
  DokuTextTestTextSystemFacade
  TextSystemFacadeTest.cpp
)
add_dokutext_test(DokuTextTestBakedAtlasApi BakedAtlasApiTest.cpp)
add_dokutext_test(DokuTextPublicHeaderSmoke DokuTextPublicHeaderSmoke.cpp)

target_include_directories(
  DokuTextTestVisualPng PRIVATE ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(
  DokuTextTestBakedAtlasPng PRIVATE ${CMAKE_SOURCE_DIR}/src
)
