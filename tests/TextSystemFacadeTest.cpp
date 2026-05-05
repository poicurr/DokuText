#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>

int main() {
  std::cout << "[test] text system facade\n";

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

  glyphrenderer::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 96;
  atlasOptions.atlasContext.pageWidth = 512;
  atlasOptions.atlasContext.pageHeight = 512;
  atlasOptions.atlasContext.padding = 2;
  glyphrenderer::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  glyphrenderer::TextSeedRequest seedRequest;
  seedRequest.presets = {glyphrenderer::TextSeedPreset::Ascii,
                         glyphrenderer::TextSeedPreset::NerdFont};
  seedRequest.customRanges = {{0x3042U, 0x3044U}};
  seedRequest.texts = {"🍣"};
  const glyphrenderer::TextAtlasSeedResult seedResult =
      textSystem.seedAtlas(atlas, seedRequest);

  const glyphrenderer::PreparedTextLine line =
      textSystem.prepareLine(atlas, "Aあ🍣");
  glyphrenderer::TextPrepareOptions paragraphOptions;
  paragraphOptions.layoutOptions.wrappingWidth = 240;
  const glyphrenderer::PreparedTextParagraph paragraph =
      textSystem.prepareText(atlas, "Aあ🍣\n", paragraphOptions);

  std::cout << "seed.codePoints=" << seedResult.codePoints.size()
            << " seed.uploadPlans=" << seedResult.atlasUploadPlan.size()
            << " atlas.pages=" << atlas.pages().size() << '\n';
  std::cout << "line.glyphs=" << line.drawRun.glyphs.size()
            << " line.pages=" << line.compatibility.pagesUsed.size()
            << " line.requiresColor=" << line.compatibility.requiresColorGlyph
            << '\n';
  std::cout << "paragraph.lines=" << paragraph.drawRun.lines.size()
            << " paragraph.uploadPlans=" << paragraph.atlasUploadPlan.size()
            << " paragraph.materials="
            << paragraph.submission.compatibility.materialKindsUsed.size()
            << '\n';

  const bool ok =
      !seedResult.codePoints.empty() && !seedResult.atlasUploadPlan.empty() &&
      !line.drawRun.glyphs.empty() && !paragraph.drawRun.lines.empty();
  return ok ? 0 : 1;
}
