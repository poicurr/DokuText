#include <DokuText/DokuText.hpp>
#include "adapter/io/PngWriter.hpp"
#include "core/ir/Bitmap.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct VisualScenario {
  std::string name;
  std::string text;
  int wrappingWidth = 0;
};

[[nodiscard]] Bitmap toBitmap(const doku::text::BitmapImage &image) {
  Bitmap bitmap;
  bitmap.resize(image.width, image.height);
  bitmap.rgba() = image.rgba;
  return bitmap;
}

[[nodiscard]] Bitmap
composeParagraph(const doku::text::MultiLineDrawRun &drawRun) {
  constexpr int PADDING = 8;

  int minX = 0;
  int minY = 0;
  int maxX = 1;
  int maxY = 1;
  bool hasVisibleGlyph = false;

  for (const doku::text::PositionedGlyph &glyph : drawRun.glyphs) {
    const doku::text::BitmapImage &image = glyph.glyph.image;
    if (image.width <= 0 || image.height <= 0) {
      continue;
    }

    const int glyphMinX = glyph.originX;
    const int glyphMinY = glyph.originY;
    const int glyphMaxX = glyph.originX + image.width;
    const int glyphMaxY = glyph.originY + image.height;
    if (!hasVisibleGlyph) {
      minX = glyphMinX;
      minY = glyphMinY;
      maxX = glyphMaxX;
      maxY = glyphMaxY;
      hasVisibleGlyph = true;
      continue;
    }
    minX = std::min(minX, glyphMinX);
    minY = std::min(minY, glyphMinY);
    maxX = std::max(maxX, glyphMaxX);
    maxY = std::max(maxY, glyphMaxY);
  }

  Bitmap canvas;
  const int width = std::max(maxX - minX + PADDING * 2, 1);
  const int height = std::max(maxY - minY + PADDING * 2, 1);
  canvas.resize(width, height);
  canvas.clear(0, 0, 0, 0);
  for (const doku::text::PositionedGlyph &glyph : drawRun.glyphs) {
    const doku::text::BitmapImage &image = glyph.glyph.image;
    if (image.width <= 0 || image.height <= 0) {
      continue;
    }
    const Bitmap bitmap = toBitmap(image);
    canvas.blit(bitmap, glyph.originX - minX + PADDING,
                glyph.originY - minY + PADDING);
  }
  return canvas;
}

[[nodiscard]] Bitmap composeGrid(const std::vector<Bitmap> &bitmaps,
                                 int columns, int padding) {
  if (bitmaps.empty()) {
    Bitmap empty;
    empty.resize(1, 1);
    empty.clear(0, 0, 0, 0);
    return empty;
  }

  int tileWidth = 1;
  int tileHeight = 1;
  for (const Bitmap &bitmap : bitmaps) {
    tileWidth = std::max(tileWidth, bitmap.width());
    tileHeight = std::max(tileHeight, bitmap.height());
  }

  const int rows = static_cast<int>((bitmaps.size() + columns - 1) / columns);
  const int width = columns * tileWidth + (columns + 1) * padding;
  const int height = rows * tileHeight + (rows + 1) * padding;

  Bitmap canvas;
  canvas.resize(width, height);
  canvas.clear(24, 24, 24, 255);

  for (std::size_t index = 0; index < bitmaps.size(); ++index) {
    const int column = static_cast<int>(index % columns);
    const int row = static_cast<int>(index / columns);
    const int originX = padding + column * (tileWidth + padding);
    const int originY = padding + row * (tileHeight + padding);
    canvas.blit(bitmaps[index], originX, originY);
  }
  return canvas;
}

} // namespace

int main() {
  std::cout << "[test] visual png scenario\n";

  doku::text::DokuTextContext context;
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
      context.registerFontFallbackChain(chain);

  doku::text::DokuText renderer(context);
  doku::text::AtlasBuildContext atlasContext;
  atlasContext.pageWidth = 512;
  atlasContext.pageHeight = 512;
  atlasContext.padding = 2;
  doku::text::GlyphAtlasCache atlasCache(atlasContext);

  const std::vector<VisualScenario> scenarios = {
      {
          .name = "text_no_wrap",
          .text = "Alpha あいう\nNerd  Symbols",
          .wrappingWidth = 1024,
      },
      {
          .name = "emoji_no_wrap",
          .text = "Alpha あいう 🍣🙂\nNerd  Mix 🚀",
          .wrappingWidth = 1024,
      },
      {
          .name = "text_wrap",
          .text = "Alpha beta gamma あいうえお\nNerd  terminal glyphs",
          .wrappingWidth = 260,
      },
      {
          .name = "emoji_wrap",
          .text = "Alpha beta 🍣🙂😎\nNerd  rocket 🚀 sushi 🍜",
          .wrappingWidth = 260,
      },
  };

  const std::filesystem::path outputDir =
      std::filesystem::path("test_outputs") / "stb_replacement";
  std::filesystem::create_directories(outputDir);

  std::vector<Bitmap> composedScenarios;
  composedScenarios.reserve(scenarios.size());
  std::size_t totalGlyphCount = 0;

  for (const VisualScenario &scenario : scenarios) {
    doku::text::TextRequest request;
    request.text = scenario.text;
    request.fontChainId = fontChainId;
    request.pixelSize = 96;
    request.layoutOptions.wrappingWidth = scenario.wrappingWidth;
    request.layoutOptions.lineSpacingMultiplier = 1.15F;

    const doku::text::MultiLineDrawRun drawRun =
        renderer.prepareText(request, atlasCache);
    const Bitmap composed = composeParagraph(drawRun);
    composedScenarios.push_back(composed);
    totalGlyphCount += drawRun.glyphs.size();

    const std::filesystem::path textPath =
        outputDir / ("paragraph_" + scenario.name + ".png");
    if (!writePng(textPath.string(), composed)) {
      std::cerr << "failed to write paragraph png: " << textPath.string()
                << '\n';
      return 1;
    }

    std::cout << "scenario=" << scenario.name
              << " wrappingWidth=" << scenario.wrappingWidth
              << " drawGlyphs=" << drawRun.glyphs.size()
              << " lines=" << drawRun.lines.size() << '\n';
  }

  for (std::size_t pageIndex = 0; pageIndex < atlasCache.pages().size();
       ++pageIndex) {
    const std::filesystem::path pngPath =
        outputDir / ("atlas_page_" + std::to_string(pageIndex) + ".png");
    if (!writePng(pngPath.string(),
                  toBitmap(atlasCache.pages()[pageIndex].image))) {
      std::cerr << "failed to write atlas png: " << pngPath.string() << '\n';
      return 1;
    }
  }

  const Bitmap overview = composeGrid(composedScenarios, 2, 16);
  const std::filesystem::path overviewPath = outputDir / "paragraph_grid.png";
  if (!writePng(overviewPath.string(), overview)) {
    std::cerr << "failed to write paragraph grid png: " << overviewPath.string()
              << '\n';
    return 1;
  }

  std::cout << "atlasPages=" << atlasCache.pages().size()
            << " totalDrawGlyphs=" << totalGlyphCount
            << " gridOrder=text_no_wrap,emoji_no_wrap,text_wrap,emoji_wrap"
            << " outputDir=" << outputDir.string() << '\n';
  return totalGlyphCount == 0 ? 1 : 0;
}
