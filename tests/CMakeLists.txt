function(add_dokutext_test target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE DokuText::DokuText)
  target_compile_features(${target_name} PRIVATE cxx_std_20)
  add_test(NAME ${target_name} COMMAND ${target_name})
  set_tests_properties(
    ${target_name} PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

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
  DokuTextTestColrV1GradientRender
  ColrV1GradientRenderTest.cpp
)
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
if(DOKU_TEXT_ENABLE_PNG_IO)
  add_dokutext_test(DokuTextTestVisualPng VisualPngScenarioTest.cpp)
  add_dokutext_test(DokuTextTestBakedAtlasPng BakedAtlasPngTest.cpp)
  target_include_directories(
    DokuTextTestVisualPng PRIVATE ${CMAKE_SOURCE_DIR}/src
  )
  target_include_directories(
    DokuTextTestBakedAtlasPng PRIVATE ${CMAKE_SOURCE_DIR}/src
  )
endif()

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
add_dokutext_test(
  DokuTextTestDynamicAtlasSeedIdempotency
  DynamicAtlasSeedIdempotencyTest.cpp
)
add_dokutext_test(DokuTextPublicHeaderSmokeHighLevel DokuTextPublicHeaderSmokeHighLevel.cpp)
add_dokutext_test(DokuTextPublicHeaderSmokeLowLevel DokuTextPublicHeaderSmokeLowLevel.cpp)
add_dokutext_test(DokuTextPublicHeaderSmokeGeometry DokuTextPublicHeaderSmokeGeometry.cpp)
add_dokutext_test(DokuTextTestPageInfo TextAtlasPageInfoTest.cpp)
add_dokutext_test(DokuTextTestProbeText TextProbeTest.cpp)
add_dokutext_test(DokuTextPublicHeaderSmoke DokuTextPublicHeaderSmoke.cpp)
add_dokutext_test(DokuTextTestPrepareAllowGlyphAppend TextPrepareAllowGlyphAppendTest.cpp)
add_dokutext_test(DokuTextTestTextInputApi TextInputApiTest.cpp)

target_include_directories(
  DokuTextTestColrV1GradientRender PRIVATE ${CMAKE_SOURCE_DIR}/src
)
