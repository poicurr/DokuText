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

# Mark a test as requiring the large external test fonts.
# The test must call doku_test::skipIfTestFontsMissing() early in main().
function(add_dokutext_test_with_fonts target_name source_file)
  add_dokutext_test(${target_name} ${source_file})
  set_tests_properties(${target_name} PROPERTIES SKIP_RETURN_CODE 77)
  target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/tests)
endfunction()

add_dokutext_test_with_fonts(DokuTextTestRenderTextApi RenderTextApiTest.cpp)
add_dokutext_test_with_fonts(
  DokuTextTestColrV1GradientRender
  ColrV1GradientRenderTest.cpp
)
add_dokutext_test_with_fonts(
  DokuTextTestFallbackDiagnostics
  FallbackDiagnosticsTest.cpp
)
add_dokutext_test_with_fonts(
  DokuTextTestShapeLayoutAtlas
  ShapeLayoutAtlasTest.cpp
)
add_dokutext_test_with_fonts(
  DokuTextTestMultiLineTextApi
  MultiLineTextApiTest.cpp
)
add_dokutext_test_with_fonts(DokuTextTestAtlasCacheApi AtlasCacheApiTest.cpp)
if(DOKU_TEXT_ENABLE_PNG_IO)
  add_dokutext_test_with_fonts(DokuTextTestVisualPng VisualPngScenarioTest.cpp)
  add_dokutext_test_with_fonts(DokuTextTestBakedAtlasPng BakedAtlasPngTest.cpp)
  target_include_directories(
    DokuTextTestVisualPng PRIVATE ${CMAKE_SOURCE_DIR}/src
  )
  target_include_directories(
    DokuTextTestBakedAtlasPng PRIVATE ${CMAKE_SOURCE_DIR}/src
  )
endif()

add_dokutext_test(DokuTextTestEmptyFallback EmptyFallbackTest.cpp)
add_dokutext_test_with_fonts(
  DokuTextTestIntegrationAdapter
  IntegrationAdapterTest.cpp
)
add_dokutext_test_with_fonts(
  DokuTextTestTextSystemFacade
  TextSystemFacadeTest.cpp
)
add_dokutext_test_with_fonts(DokuTextTestBakedAtlasApi BakedAtlasApiTest.cpp)
add_dokutext_test_with_fonts(
  DokuTextTestDynamicAtlasSeedIdempotency
  DynamicAtlasSeedIdempotencyTest.cpp
)
add_dokutext_test(DokuTextPublicHeaderSmokeHighLevel DokuTextPublicHeaderSmokeHighLevel.cpp)
add_dokutext_test(DokuTextPublicHeaderSmokeLowLevel DokuTextPublicHeaderSmokeLowLevel.cpp)
add_dokutext_test(DokuTextPublicHeaderSmokeGeometry DokuTextPublicHeaderSmokeGeometry.cpp)
add_dokutext_test_with_fonts(DokuTextTestPageInfo TextAtlasPageInfoTest.cpp)
add_dokutext_test_with_fonts(DokuTextTestProbeText TextProbeTest.cpp)
add_dokutext_test(DokuTextPublicHeaderSmoke DokuTextPublicHeaderSmoke.cpp)
add_dokutext_test_with_fonts(DokuTextTestPrepareAllowGlyphAppend TextPrepareAllowGlyphAppendTest.cpp)
add_dokutext_test_with_fonts(DokuTextTestTextInputApi TextInputApiTest.cpp)

target_include_directories(
  DokuTextTestColrV1GradientRender PRIVATE ${CMAKE_SOURCE_DIR}/src
)
