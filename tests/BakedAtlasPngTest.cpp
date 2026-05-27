#include <DokuText/DokuText.hpp>
#include "adapter/io/PngWriter.hpp"
#include "core/ir/Bitmap.hpp"

#include <filesystem>
#include <iostream>

#include "TestUtils.hpp"

namespace {

[[nodiscard]] Bitmap toBitmap(const doku::text::BitmapImage &image) {
  Bitmap bitmap;
  bitmap.resize(image.width, image.height);
  bitmap.rgba() = image.rgba;
  return bitmap;
}

} // namespace

int main() {
  doku_test::skipIfTestFontsMissing();

  std::cout << "[test] baked atlas png\n";

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
  bakeRequest.atlasContext.pageWidth = 1024;
  bakeRequest.atlasContext.pageHeight = 1024;
  bakeRequest.atlasContext.padding = 2;
  bakeRequest.seedPresets = {
      doku::text::TextSeedPreset::BasicLatin,
      doku::text::TextSeedPreset::CjkSymbolsPunctuation,
      doku::text::TextSeedPreset::Hiragana,
      doku::text::TextSeedPreset::Katakana,
      doku::text::TextSeedPreset::EmojiSymbols,
  };
  bakeRequest.customRanges = {
      {0x65E5U, 0x65E5U},
      {0x672CU, 0x672CU},
      {0x8A9EU, 0x8A9EU},
  };
  bakeRequest.bakeBudget.maxPages = 8;
  bakeRequest.bakeBudget.maxBakeMilliseconds = 500.0;
  bakeRequest.seedTexts = {
      "Hello UI",
      "あいうえおアイウエオ日本語",
      "🍣🙂🚀🎉",
  };

  const doku::text::BakedTextAtlas bakedAtlas =
      textSystem.bakeTextAtlas(bakeRequest);

  const std::filesystem::path outputDir =
      std::filesystem::path("test_outputs") / "baked_static_atlas_subset";
  std::filesystem::create_directories(outputDir);

  for (std::size_t pageIndex = 0; pageIndex < bakedAtlas.pages().size();
       ++pageIndex) {
    const std::filesystem::path pngPath =
        outputDir / ("baked_atlas_page_" + std::to_string(pageIndex) + ".png");
    if (!writePng(pngPath.string(),
                  toBitmap(bakedAtlas.pageImages().at(pageIndex)))) {
      std::cerr << "failed to write atlas png: " << pngPath.string() << '\n';
      return 1;
    }

    std::cout << "page=" << pageIndex << " pixelFormat="
              << doku::text::toString(
                     bakedAtlas.pixelFormatForPage(pageIndex))
              << " materials=";
    for (doku::text::MaterialKind kind :
         bakedAtlas.materialKindsForPage(pageIndex)) {
      std::cout << doku::text::toString(kind) << ',';
    }
    std::cout << '\n';
  }

  std::cout << "revision=" << bakedAtlas.revision()
            << " pages=" << bakedAtlas.pages().size()
            << " entries=" << bakedAtlas.entries().size()
            << " resolved=" << bakedAtlas.bakeStats().resolvedGlyphCount
            << " elapsedMs=" << bakedAtlas.bakeStats().elapsedMilliseconds
            << " outputDir=" << outputDir.string() << '\n';
  return bakedAtlas.pages().empty() ? 1 : 0;
}
