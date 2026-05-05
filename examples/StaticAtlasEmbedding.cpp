#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>

int main() {
  glyphrenderer::TextSystem textSystem;
  glyphrenderer::FontFallbackChain chain;
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  chain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  chain.symbolFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  const glyphrenderer::FontChainId fontChainId =
      textSystem.registerFontFallbackChain(chain);

  glyphrenderer::TextAtlasBakeRequest bakeRequest;
  bakeRequest.fontChainId = fontChainId;
  bakeRequest.pixelSize = 96;
  bakeRequest.atlasContext.pageWidth = 512;
  bakeRequest.atlasContext.pageHeight = 512;
  bakeRequest.atlasContext.padding = 2;
  bakeRequest.seedPresets = {
      glyphrenderer::TextSeedPreset::Ascii,
      glyphrenderer::TextSeedPreset::CjkSymbolsPunctuation,
  };
  bakeRequest.bakeBudget.maxSeedCodePoints = 256;
  bakeRequest.bakeBudget.maxPages = 4;
  bakeRequest.seedTexts = {"Aあ"};
  const glyphrenderer::BakedTextAtlas bakedAtlas =
      textSystem.bakeTextAtlas(bakeRequest);

  glyphrenderer::TextRequest request;
  request.text = "Aあ\nMissing 🍣";
  request.fontChainId = fontChainId;
  request.pixelSize = 96;
  const glyphrenderer::PreparedTextParagraph prepared =
      textSystem.layoutTextParagraph(bakedAtlas, request);

  std::cout << "[example] static atlas\n";
  std::cout << "revision=" << bakedAtlas.revision()
            << " pages=" << bakedAtlas.pages().size()
            << " uploadPlan=" << bakedAtlas.uploadPlan().size() << '\n';
  std::cout << "bake.seedCodePoints="
            << bakedAtlas.bakeStats().seedCodePointCount << " processed="
            << bakedAtlas.bakeStats().processedSeedCodePointCount
            << " resolved=" << bakedAtlas.bakeStats().resolvedGlyphCount
            << " elapsedMs=" << bakedAtlas.bakeStats().elapsedMilliseconds
            << '\n';
  std::cout << "prepared.glyphs=" << prepared.submission.glyphs.size()
            << " missingGlyphs=" << prepared.missingGlyphs.size()
            << " runtimeUploadPlan=" << prepared.atlasUploadPlan.size() << '\n';
  std::cout << "compat.pages=" << prepared.compatibility.pagesUsed.size()
            << " requiresColor=" << prepared.compatibility.requiresColorGlyph
            << " hasMissing=" << prepared.compatibility.hasMissingGlyph << '\n';
  return prepared.submission.glyphs.empty() ? 1 : 0;
}
