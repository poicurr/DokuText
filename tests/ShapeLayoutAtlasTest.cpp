#include <DokuText/DokuText.hpp>

#include <iostream>

#include "TestUtils.hpp"

int main() {
  doku_test::skipIfTestFontsMissing();

  std::cout << "[test] shape layout atlas pipeline\n";

  doku::text::DokuTextContext context;
  doku::text::FontFallbackChain chain;
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  chain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  const doku::text::FontChainId fontChainId =
      context.registerFontFallbackChain(chain);
  doku::text::DokuText renderer(context);
  doku::text::GlyphAtlasCache atlasCache({});

  doku::text::TextRequest request;
  request.text = "Aあ🍣";
  request.fontChainId = fontChainId;
  request.pixelSize = 128;
  request.verifyMetrics = true;

  const doku::text::TextLineLayout layout = renderer.layoutLine(request);
  const doku::text::TextDrawRun drawRun =
      renderer.prepareLine(request, atlasCache);

  std::cout << "layoutGlyphs=" << layout.glyphs.size()
            << " atlasEntries=" << atlasCache.entries().size()
            << " drawGlyphs=" << drawRun.glyphs.size() << '\n';
  std::cout << "lineMetrics ascent=" << layout.metrics.ascent
            << " descent=" << layout.metrics.descent
            << " baseline=" << layout.metrics.baseline
            << " lineWidth=" << layout.metrics.lineWidth
            << " atlasPages=" << drawRun.summary.atlasPageCount << '\n';
  const std::vector<doku::text::GlyphAtlasEntry> atlasEntries =
      atlasCache.entries();
  if (!layout.glyphs.empty() && !atlasEntries.empty() && !drawRun.glyphs.empty()) {
    std::cout << "handleMatch="
              << (layout.glyphs.front().handle.value ==
                  atlasEntries.front().handle.value)
              << " atlasUvMaxX=" << atlasEntries.front().uvRect.maxX
              << " drawPage=" << drawRun.glyphs.front().atlasPage << '\n';
  }
  return layout.glyphs.empty() ? 1 : 0;
}
