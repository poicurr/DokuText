#include <DokuText/DokuText.hpp>

#include <iostream>

int main() {
  doku::text::TextSystem textSystem;

  doku::text::FontFallbackChain chain;
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  chain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };

  const doku::text::FontChainId fontChainId =
      textSystem.registerFontFallbackChain(chain);

  doku::text::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 128;
  doku::text::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  doku::text::TextSeedRequest seedRequest;
  seedRequest.texts = {"A🍣"};
  const doku::text::TextAtlasSeedResult seedResult =
      textSystem.seedAtlas(atlas, seedRequest);

  const doku::text::PreparedTextLine prepared =
      textSystem.prepareLine(atlas, "A🍣");

  std::cout << "seedPages=" << atlas.pages().size()
            << " uploadPlans=" << seedResult.atlasUploadPlan.size()
            << " glyphs=" << prepared.submission.glyphs.size() << '\n';
  return prepared.submission.glyphs.empty() ? 1 : 0;
}
