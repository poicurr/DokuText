#include <DokuText/DokuText.hpp>

#include <iostream>

#include "TestUtils.hpp"

int main() {
  doku_test::skipIfTestFontsMissing();

  std::cout << "[test] multi-line text api\n";

  doku::text::DokuTextContext context;
  doku::text::FontFallbackChain chain;
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  chain.emojiFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
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

  const std::string text = "Aあ🍣\n🙂";
  doku::text::TextRequest request;
  request.text = text;
  request.fontChainId = fontChainId;
  request.pixelSize = 96;
  request.layoutOptions.wrappingWidth = 220;
  request.layoutOptions.horizontalAlign =
      doku::text::HorizontalAlign::Center;
  request.layoutOptions.lineSpacingMultiplier = 1.2F;

  const doku::text::MultiLineLayout layout = renderer.layoutText(request);
  const doku::text::MultiLineDrawRun draw =
      renderer.prepareText(request, atlasCache);

  std::cout << "lines=" << layout.lines.size()
            << " drawLines=" << draw.lines.size()
            << " lineOrigins=" << draw.lineOriginsY.size()
            << " flatGlyphs=" << draw.glyphs.size() << '\n';
  std::cout << "lineAdvance=" << draw.lineAdvance
            << " atlasPages=" << draw.summary.atlasPageCount
            << " usedFonts=" << draw.summary.usedFonts.size() << '\n';
  std::cout << "lineSummaries=" << draw.lineSummaries.size()
            << " fallbackTrace=" << draw.fallbackTrace.size()
            << " lineTexts=" << draw.lineTexts.size()
            << " lineOverflow=" << draw.diagnosticsSummary.lineOverflowCount
            << '\n';
  if (draw.lines.size() >= 2 && draw.lineOriginsY.size() >= 2) {
    std::cout << "secondLineOriginY=" << draw.lineOriginsY[1]
              << " secondLineGlyphs=" << draw.lines[1].glyphs.size() << '\n';
    if (!draw.lines[1].glyphs.empty()) {
      std::cout << "secondLineGlyphOriginY="
                << draw.lines[1].glyphs.front().originY
                << " secondLineIndex=" << draw.lines[1].glyphs.front().lineIndex
                << '\n';
    }
  }
  if (!draw.lineTexts.empty()) {
    std::cout << "firstLineText=" << draw.lineTexts.front() << '\n';
  }
  if (draw.lineTexts.size() >= 2) {
    std::cout << "wrappedSecondText=" << draw.lineTexts[1] << '\n';
  }
  std::cout << "paragraphLines=" << draw.lines.size()
            << " paragraphFlatGlyphs=" << draw.glyphs.size()
            << " atlasCachePages=" << atlasCache.pages().size() << '\n';
  if (!draw.diagnostics.empty()) {
    std::cout << "paragraphFirstDiagnosticLine="
              << draw.diagnostics.front().lineIndex << " code="
              << doku::text::toString(
                     draw.diagnostics.front().code)
              << '\n';
  }
  return draw.lines.empty() ? 1 : 0;
}
