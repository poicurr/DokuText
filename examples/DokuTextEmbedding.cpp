#include "DokuText/DokuText.hpp"

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
  const auto fontChainId = textSystem.registerFontFallbackChain(chain);

  doku::text::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 128;
  doku::text::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  doku::text::TextSeedRequest seedRequest;
  seedRequest.presets = {doku::text::TextSeedPreset::Ascii};
  seedRequest.texts = {"DokuText"};
  const doku::text::TextAtlasSeedResult seedResult =
      textSystem.seedAtlas(atlas, seedRequest);

  const doku::text::PreparedTextLine line =
      textSystem.prepareLine(atlas, "DokuText Compatibility Test 🍣");

  std::cout << "[example] DokuText embedding using doku::text namespace\n";
  std::cout << "  glyphCount=" << line.layout.glyphs.size() << '\n';
  std::cout << "  atlas pages=" << atlas.pages().size() << '\n';
  std::cout << "  uploadPlans=" << seedResult.atlasUploadPlan.size() << '\n';

  return 0;
}
