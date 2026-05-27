#include <DokuText/DokuText.hpp>

#include <iostream>

#include "TestUtils.hpp"

int main() {
  doku_test::skipIfTestFontsMissing();

  std::cout << "[test] text system facade\n";

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

  doku::text::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 96;
  atlasOptions.atlasContext.pageWidth = 512;
  atlasOptions.atlasContext.pageHeight = 512;
  atlasOptions.atlasContext.padding = 2;
  doku::text::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  doku::text::TextSeedRequest seedRequest;
  seedRequest.presets = {doku::text::TextSeedPreset::Ascii,
                         doku::text::TextSeedPreset::NerdFont};
  seedRequest.customRanges = {{0x3042U, 0x3044U}};
  seedRequest.texts = {"🍣"};
  const doku::text::TextAtlasSeedResult seedResult =
      textSystem.seedAtlas(atlas, seedRequest);

  const doku::text::PreparedTextLine line =
      textSystem.prepareLine(atlas, "Aあ🍣");
  doku::text::TextPrepareOptions paragraphOptions;
  paragraphOptions.layoutOptions.wrappingWidth = 240;
  const doku::text::PreparedTextParagraph paragraph =
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
