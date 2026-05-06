#include <DokuText/DokuText.hpp>

#include <iostream>

int main() {
  doku::text::TextSystem textSystem;
  doku::text::FontFallbackChain chain;
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  chain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  chain.symbolFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  const doku::text::FontChainId fontChainId =
      textSystem.registerFontFallbackChain(chain);

  doku::text::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 96;
  atlasOptions.atlasContext.pageWidth = 512;
  atlasOptions.atlasContext.pageHeight = 512;
  atlasOptions.atlasContext.padding = 2;
  doku::text::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  doku::text::TextSeedRequest seedRequest;
  seedRequest.presets = {doku::text::TextSeedPreset::Ascii};
  seedRequest.texts = {"あ🍣"};
  const doku::text::TextAtlasSeedResult seed =
      textSystem.seedAtlas(atlas, seedRequest);

  doku::text::TextPrepareOptions prepareOptions;
  prepareOptions.layoutOptions.wrappingWidth = 320;
  const doku::text::PreparedTextParagraph prepared =
      textSystem.prepareText(atlas, "UI あ\nEmoji 🍣", prepareOptions);

  std::size_t alphaGlyphCount = 0;
  std::size_t colorGlyphCount = 0;
  for (const doku::text::PositionedDrawGlyph &glyph :
       prepared.submission.glyphs) {
    if (glyph.materialKind == doku::text::MaterialKind::AlphaMask) {
      ++alphaGlyphCount;
    } else {
      ++colorGlyphCount;
    }
  }

  std::cout << "[example] renderer integration contract\n";
  std::cout
      << "uvOrigin=" << doku::text::toString(doku::text::atlasUvOrigin())
      << " alphaPixelFormat="
      << doku::text::toString(doku::text::atlasPixelFormatForMaterial(
             doku::text::MaterialKind::AlphaMask))
      << " alphaTint="
      << doku::text::materialSupportsVertexTint(
             doku::text::MaterialKind::AlphaMask)
      << " colorPixelFormat="
      << doku::text::toString(doku::text::atlasPixelFormatForMaterial(
             doku::text::MaterialKind::ColorBitmap))
      << " colorTint="
      << doku::text::materialSupportsVertexTint(
             doku::text::MaterialKind::ColorBitmap)
      << '\n';
  std::cout << "seed.pages=" << atlas.pages().size()
            << " seed.uploadPlans=" << seed.atlasUploadPlan.size()
            << " prepared.uploadPlans=" << prepared.atlasUploadPlan.size()
            << '\n';
  std::cout << "compat.pages=" << prepared.compatibility.pagesUsed.size()
            << " requiresColor=" << prepared.compatibility.requiresColorGlyph
            << " alphaOnly=" << prepared.compatibility.alphaOnlyCompatible
            << " materialKinds="
            << prepared.compatibility.materialKindsUsed.size() << '\n';
  std::cout << "submission.glyphs=" << prepared.submission.glyphs.size()
            << " alphaGlyphs=" << alphaGlyphCount
            << " colorGlyphs=" << colorGlyphCount << '\n';

  if (!atlas.pages().empty()) {
    const doku::text::BitmapImage normalized =
        doku::text::normalizeAlphaMaskToWhite(atlas.pages().front().image);
    std::cout << "normalizedAlphaPreview=" << normalized.width << "x"
              << normalized.height << '\n';
  }

  return prepared.submission.glyphs.empty() ? 1 : 0;
}
