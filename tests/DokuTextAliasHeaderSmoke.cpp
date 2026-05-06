#include <DokuText/DokuText.hpp>

#include <type_traits>

int main() {
  static_assert(std::is_same_v<doku::text::TextSystem,
                               glyphrenderer::TextSystem>);
  static_assert(std::is_same_v<doku::text::TextAtlas,
                               glyphrenderer::TextAtlas>);
  static_assert(std::is_same_v<doku::text::BakedTextAtlas,
                               glyphrenderer::BakedTextAtlas>);
  static_assert(std::is_same_v<doku::text::FontFallbackChain,
                               glyphrenderer::FontFallbackChain>);
  static_assert(std::is_same_v<doku::text::TextRequest,
                               glyphrenderer::TextRequest>);
  static_assert(std::is_same_v<doku::text::PreparedTextParagraph,
                               glyphrenderer::PreparedTextParagraph>);

  static_assert(std::is_same_v<doku::text::MaterialKind,
                               glyphrenderer::MaterialKind>);
  static_assert(std::is_same_v<doku::text::SamplerKind,
                               glyphrenderer::SamplerKind>);
  static_assert(std::is_same_v<doku::text::BlendHint,
                               glyphrenderer::BlendHint>);
  static_assert(std::is_same_v<doku::text::TextDrawRun,
                               glyphrenderer::TextDrawRun>);
  static_assert(std::is_same_v<doku::text::MultiLineDrawRun,
                               glyphrenderer::MultiLineDrawRun>);
  static_assert(std::is_same_v<doku::text::PositionedDrawGlyph,
                               glyphrenderer::PositionedDrawGlyph>);
  static_assert(std::is_same_v<doku::text::AtlasPageUploadPlan,
                               glyphrenderer::AtlasPageUploadPlan>);
  static_assert(std::is_same_v<doku::text::GlyphAtlasCacheUpdate,
                               glyphrenderer::GlyphAtlasCacheUpdate>);

  return 0;
}
