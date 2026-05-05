#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>

int main() {
  std::cout << "[test] stb replacement line api\n";

  glyphrenderer::GlyphRendererContext context;
  glyphrenderer::FontFallbackChain chain;
  chain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  const glyphrenderer::FontChainId fontChainId =
      context.registerFontFallbackChain(chain);

  glyphrenderer::GlyphRenderer renderer(context);
  glyphrenderer::GlyphAtlasCache atlasCache({});
  glyphrenderer::TextRequest request;
  request.text = "Aあ🍣";
  request.fontChainId = fontChainId;
  request.pixelSize = 128;
  request.verifyMetrics = true;

  const glyphrenderer::TextLineLayout layout = renderer.layoutLine(request);
  const glyphrenderer::TextDrawRun drawRun =
      renderer.prepareLine(request, atlasCache);
  std::cout << "glyphCount=" << layout.glyphs.size()
            << " atlasPages=" << drawRun.summary.atlasPageCount << '\n';
  std::cout << "summary fallbackCount=" << drawRun.summary.fallbackCount
            << " usedFonts=" << drawRun.summary.usedFonts.size()
            << " colorSummary=" << drawRun.diagnosticsSummary.colorGlyphCount
            << '\n';
  std::cout << "lineMetrics ascent=" << layout.metrics.ascent
            << " descent=" << layout.metrics.descent
            << " lineGap=" << layout.metrics.lineGap
            << " baseline=" << layout.metrics.baseline
            << " lineWidth=" << layout.metrics.lineWidth << '\n';
  for (std::size_t i = 0; i < layout.glyphs.size(); ++i) {
    const glyphrenderer::GlyphBitmap &glyph = layout.glyphs[i].glyph;
    std::cout << "glyph[" << i << "] advance=" << glyph.advance
              << " bearingX=" << glyph.bearingX
              << " bearingY=" << glyph.bearingY << " size=" << glyph.image.width
              << "x" << glyph.image.height << '\n';
  }
  if (!drawRun.glyphs.empty()) {
    std::cout << "firstHandle=" << drawRun.glyphs.front().handle.value
              << " page=" << drawRun.glyphs.front().atlasPage
              << " material="
              << glyphrenderer::toString(drawRun.glyphs.front().materialKind)
              << '\n';
  }
  return layout.glyphs.empty() ? 1 : 0;
}
