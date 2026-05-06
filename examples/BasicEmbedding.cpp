#include <DokuText/DokuText.hpp>

#include <iostream>

int main() {
  doku::text::TextSystem textSystem;
  doku::text::FontFallbackChain chain;
  chain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  const doku::text::FontChainId fontChainId =
      textSystem.registerFontFallbackChain(chain);

  doku::text::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 128;
  doku::text::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  doku::text::TextSeedRequest seedRequest;
  seedRequest.presets = {doku::text::TextSeedPreset::Ascii};
  seedRequest.texts = {"Aあ🍣"};
  const doku::text::TextAtlasSeedResult seedResult =
      textSystem.seedAtlas(atlas, seedRequest);

  const doku::text::PreparedTextLine line =
      textSystem.prepareLine(atlas, "Aあ🍣👨‍👩‍👧‍👦");
  std::cout << "[example] high-level text system line glyphCount="
            << line.layout.glyphs.size() << '\n';
  std::cout << "  seed codePoints=" << seedResult.codePoints.size()
            << " uploadPlans=" << seedResult.atlasUploadPlan.size()
            << " pages=" << atlas.pages().size() << '\n';
  std::cout << "  lineMetrics ascent=" << line.layout.metrics.ascent
            << " descent=" << line.layout.metrics.descent
            << " lineGap=" << line.layout.metrics.lineGap
            << " baseline=" << line.layout.metrics.baseline
            << " lineWidth=" << line.layout.metrics.lineWidth << '\n';
  std::cout << "  compatibility alphaOnly="
            << line.compatibility.alphaOnlyCompatible
            << " requiresColor=" << line.compatibility.requiresColorGlyph
            << " atlasPages=" << line.compatibility.pagesUsed.size() << '\n';
  for (std::size_t i = 0; i < line.layout.glyphs.size(); ++i) {
    const doku::text::GlyphBitmap &glyph = line.layout.glyphs[i].glyph;
    std::cout << "  glyph[" << i << "] advance=" << glyph.advance
              << " bearingX=" << glyph.bearingX
              << " bearingY=" << glyph.bearingY << " size=" << glyph.image.width
              << "x" << glyph.image.height << " font=" << glyph.fontPath
              << '\n';
  }

  doku::text::TextPrepareOptions paragraphOptions;
  paragraphOptions.layoutOptions.wrappingWidth = 512;
  const doku::text::PreparedTextParagraph paragraph =
      textSystem.prepareText(atlas, "Aあ\n🍣", paragraphOptions);
  std::cout << "  paragraph lines=" << paragraph.layout.lines.size()
            << " drawLines=" << paragraph.drawRun.lines.size()
            << " flatGlyphs=" << paragraph.drawRun.glyphs.size()
            << " lineAdvance=" << paragraph.drawRun.lineAdvance << '\n';
  return 0;
}
