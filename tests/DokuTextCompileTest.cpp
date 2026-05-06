#include "DokuText/DokuText.hpp"

#include <iostream>
#include <type_traits>

template <typename T1, typename T2>
void assert_same() {
  static_assert(std::is_same_v<T1, T2>, "Namespace alias mismatch");
}

int main() {
  std::cout << "[test] Verifying doku::text aliases...\n";

  assert_same<doku::text::TextSystem, glyphrenderer::TextSystem>();
  assert_same<doku::text::TextAtlas, glyphrenderer::TextAtlas>();
  assert_same<doku::text::BakedTextAtlas, glyphrenderer::BakedTextAtlas>();
  assert_same<doku::text::PreparedTextLine, glyphrenderer::PreparedTextLine>();
  assert_same<doku::text::PreparedTextParagraph, glyphrenderer::PreparedTextParagraph>();
  assert_same<doku::text::FontFallbackChain, glyphrenderer::FontFallbackChain>();
  assert_same<doku::text::FontChainId, glyphrenderer::FontChainId>();
  assert_same<doku::text::TextRequest, glyphrenderer::TextRequest>();
  assert_same<doku::text::TextSeedRequest, glyphrenderer::TextSeedRequest>();
  assert_same<doku::text::TextSeedPreset, glyphrenderer::TextSeedPreset>();
  assert_same<doku::text::GlyphAtlasCacheUpdate, glyphrenderer::GlyphAtlasCacheUpdate>();
  assert_same<doku::text::AtlasPageUploadPlan, glyphrenderer::AtlasPageUploadPlan>();
  assert_same<doku::text::TextCompatibilityReport, glyphrenderer::TextCompatibilityReport>();
  assert_same<doku::text::TextAtlasOptions, glyphrenderer::TextAtlasOptions>();
  assert_same<doku::text::TextAtlasSeedResult, glyphrenderer::TextAtlasSeedResult>();
  assert_same<doku::text::TextPrepareOptions, glyphrenderer::TextPrepareOptions>();
  assert_same<doku::text::AtlasBuildContext, glyphrenderer::AtlasBuildContext>();

  std::cout << "All aliases verified successfully.\n";
  return 0;
}
