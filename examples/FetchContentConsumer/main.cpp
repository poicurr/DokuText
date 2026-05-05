#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>

int main() {
  glyphrenderer::TextSystem textSystem;

  glyphrenderer::FontFallbackChain chain;
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  chain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };

  const glyphrenderer::FontChainId fontChainId =
      textSystem.registerFontFallbackChain(chain);

  glyphrenderer::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 128;
  glyphrenderer::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  glyphrenderer::TextSeedRequest seedRequest;
  seedRequest.texts = {"A🍣"};
  const glyphrenderer::TextAtlasSeedResult seedResult =
      textSystem.seedAtlas(atlas, seedRequest);

  const glyphrenderer::PreparedTextLine prepared =
      textSystem.prepareLine(atlas, "A🍣");

  std::cout << "seedPages=" << atlas.pages().size()
            << " uploadPlans=" << seedResult.atlasUploadPlan.size()
            << " glyphs=" << prepared.submission.glyphs.size() << '\n';
  return prepared.submission.glyphs.empty() ? 1 : 0;
}
