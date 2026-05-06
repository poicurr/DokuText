#include <DokuText/DokuText.hpp>
#include <type_traits>

int main() {
  // Verify primary API types exist in doku::text namespace
  [[maybe_unused]] doku::text::TextSystem system;
  [[maybe_unused]] doku::text::TextAtlas* atlas = nullptr;
  [[maybe_unused]] doku::text::BakedTextAtlas* bakedAtlas = nullptr;
  [[maybe_unused]] doku::text::FontFallbackChain chain;
  [[maybe_unused]] doku::text::TextRequest request;
  [[maybe_unused]] doku::text::PreparedTextLine line;
  [[maybe_unused]] doku::text::PreparedTextParagraph paragraph;
  [[maybe_unused]] doku::text::MaterialKind material = doku::text::MaterialKind::AlphaMask;
  [[maybe_unused]] doku::text::PositionedDrawGlyph drawGlyph;
  [[maybe_unused]] doku::text::AtlasPageUploadPlan uploadPlan;

  return 0;
}
