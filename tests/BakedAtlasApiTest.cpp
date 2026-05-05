#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>

int main() {
  std::cout << "[test] baked atlas api\n";
  const glyphrenderer::TextSeedPresetInfo japaneseInfo =
      glyphrenderer::describeTextSeedPreset(
          glyphrenderer::TextSeedPreset::Japanese);
  const glyphrenderer::TextSeedPresetInfo emojiInfo =
      glyphrenderer::describeTextSeedPreset(
          glyphrenderer::TextSeedPreset::Emoji);
  std::cout << "preset.Japanese.codePoints="
            << japaneseInfo.estimatedCodePointCount
            << " pages64=" << japaneseInfo.typicalAtlasPagesAt64Px
            << " preset.Emoji.codePoints=" << emojiInfo.estimatedCodePointCount
            << " pages64=" << emojiInfo.typicalAtlasPagesAt64Px << '\n';

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
  request.text = "Aあ\n🍣";
  request.fontChainId = fontChainId;
  request.pixelSize = 96;
  const glyphrenderer::PreparedTextParagraph prepared =
      textSystem.layoutTextParagraph(bakedAtlas, request);

  std::cout << "pages=" << bakedAtlas.pages().size()
            << " entries=" << bakedAtlas.entries().size()
            << " uploadPlan=" << bakedAtlas.uploadPlan().size()
            << " revision=" << bakedAtlas.revision() << '\n';
  std::cout << "bake.seed=" << bakedAtlas.bakeStats().seedCodePointCount
            << " processed="
            << bakedAtlas.bakeStats().processedSeedCodePointCount
            << " resolved=" << bakedAtlas.bakeStats().resolvedGlyphCount
            << " missing=" << bakedAtlas.bakeStats().missingGlyphCount
            << " elapsedMs=" << bakedAtlas.bakeStats().elapsedMilliseconds
            << '\n';
  std::cout << "uvOrigin=" << glyphrenderer::toString(bakedAtlas.uvOrigin())
            << " page0PixelFormat="
            << glyphrenderer::toString(bakedAtlas.pixelFormatForPage(0))
            << '\n';
  std::cout << "prepared.glyphs=" << prepared.submission.glyphs.size()
            << " runtimeUploadPlan=" << prepared.atlasUploadPlan.size()
            << " missingGlyphs=" << prepared.missingGlyphs.size() << '\n';

  const bool ok =
      japaneseInfo.estimatedCodePointCount >
          emojiInfo.estimatedCodePointCount &&
      !bakedAtlas.pages().empty() && !bakedAtlas.entries().empty() &&
      !bakedAtlas.uploadPlan().empty() && !prepared.submission.glyphs.empty() &&
      prepared.atlasUploadPlan.empty() && !prepared.missingGlyphs.empty() &&
      bakedAtlas.bakeStats().resolvedGlyphCount == bakedAtlas.entries().size();
  return ok ? 0 : 1;
}
