#include "DokuText/DokuText.hpp"

#include <iostream>
#include <type_traits>

template <typename T1, typename T2>
void assert_same() {
  static_assert(std::is_same_v<T1, T2>, "Namespace alias mismatch");
}

int main() {
  std::cout << "[test] Verifying doku::text aliases...\n";

  assert_same<doku::text::TextSystem, doku::text::TextSystem>();
  assert_same<doku::text::TextAtlas, doku::text::TextAtlas>();
  assert_same<doku::text::BakedTextAtlas, doku::text::BakedTextAtlas>();
  assert_same<doku::text::PreparedTextLine, doku::text::PreparedTextLine>();
  assert_same<doku::text::PreparedTextParagraph, doku::text::PreparedTextParagraph>();
  assert_same<doku::text::FontFallbackChain, doku::text::FontFallbackChain>();
  assert_same<doku::text::FontChainId, doku::text::FontChainId>();
  assert_same<doku::text::TextRequest, doku::text::TextRequest>();
  assert_same<doku::text::TextSeedRequest, doku::text::TextSeedRequest>();
  assert_same<doku::text::TextSeedPreset, doku::text::TextSeedPreset>();
  assert_same<doku::text::GlyphAtlasCacheUpdate, doku::text::GlyphAtlasCacheUpdate>();
  assert_same<doku::text::AtlasPageUploadPlan, doku::text::AtlasPageUploadPlan>();
  assert_same<doku::text::TextCompatibilityReport, doku::text::TextCompatibilityReport>();
  assert_same<doku::text::TextAtlasOptions, doku::text::TextAtlasOptions>();
  assert_same<doku::text::TextAtlasSeedResult, doku::text::TextAtlasSeedResult>();
  assert_same<doku::text::TextPrepareOptions, doku::text::TextPrepareOptions>();
  assert_same<doku::text::AtlasBuildContext, doku::text::AtlasBuildContext>();

  std::cout << "All aliases verified successfully.\n";
  return 0;
}
