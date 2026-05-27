#include <DokuText/DokuText.hpp>

#include <iostream>

#include "TestUtils.hpp"

int main() {
  doku_test::skipIfTestFontsMissing();

  std::cout << "[test] baked atlas api\n";
  const doku::text::TextSeedPresetInfo japaneseInfo =
      doku::text::describeTextSeedPreset(
          doku::text::TextSeedPreset::Japanese);
  const doku::text::TextSeedPresetInfo emojiInfo =
      doku::text::describeTextSeedPreset(
          doku::text::TextSeedPreset::Emoji);
  std::cout << "preset.Japanese.codePoints="
            << japaneseInfo.estimatedCodePointCount
            << " pages64=" << japaneseInfo.typicalAtlasPagesAt64Px
            << " preset.Emoji.codePoints=" << emojiInfo.estimatedCodePointCount
            << " pages64=" << emojiInfo.typicalAtlasPagesAt64Px << '\n';

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

  doku::text::TextAtlasBakeRequest bakeRequest;
  bakeRequest.fontChainId = fontChainId;
  bakeRequest.pixelSize = 96;
  bakeRequest.atlasContext.pageWidth = 512;
  bakeRequest.atlasContext.pageHeight = 512;
  bakeRequest.atlasContext.padding = 2;
  bakeRequest.seedPresets = {
      doku::text::TextSeedPreset::Ascii,
      doku::text::TextSeedPreset::CjkSymbolsPunctuation,
  };
  bakeRequest.bakeBudget.maxSeedCodePoints = 256;
  bakeRequest.bakeBudget.maxPages = 4;
  bakeRequest.seedTexts = {"Aあ"};

  const doku::text::BakedTextAtlas bakedAtlas =
      textSystem.bakeTextAtlas(bakeRequest);

  doku::text::TextRequest request;
  request.text = "Aあ\n🍣";
  request.fontChainId = fontChainId;
  request.pixelSize = 96;
  const doku::text::PreparedTextParagraph prepared =
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
  std::cout << "uvOrigin=" << doku::text::toString(bakedAtlas.uvOrigin())
            << " page0PixelFormat="
            << doku::text::toString(bakedAtlas.pixelFormatForPage(0))
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
