#include <DokuText/DokuText.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

doku::text::TextAtlas makeAtlas(doku::text::TextSystem &textSystem) {
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

  doku::text::TextAtlasOptions options;
  options.fontChainId = fontChainId;
  options.pixelSize = 96;
  options.atlasContext.pageWidth = 1024;
  options.atlasContext.pageHeight = 1024;
  options.atlasContext.padding = 2;
  return textSystem.createAtlas(options);
}

void requireOffsets(const doku::text::PreparedTextLine &line,
                    const std::vector<std::size_t> &expected) {
  const std::span<const doku::text::TextCaretBoundary> boundaries =
      doku::text::caretBoundaries(line);
  require(boundaries.size() == expected.size(),
          "unexpected caret boundary count");
  for (std::size_t index = 0; index < expected.size(); ++index) {
    require(boundaries[index].byteOffset == expected[index],
            "unexpected caret byte offset");
  }
}

void requireMonotonicCaretX(const doku::text::PreparedTextLine &line) {
  const std::span<const doku::text::TextCaretBoundary> boundaries =
      doku::text::caretBoundaries(line);
  for (std::size_t index = 1; index < boundaries.size(); ++index) {
    require(boundaries[index - 1U].x <= boundaries[index].x,
            "caret x must be monotonic");
  }
}

} // namespace

int main() {
  std::cout << "[test] text input caret/hit-test api\n";

  doku::text::TextSystem textSystem;
  doku::text::TextAtlas atlas = makeAtlas(textSystem);

  const doku::text::PreparedTextLine ascii =
      textSystem.prepareLine(atlas, "abc");
  requireOffsets(ascii, {0U, 1U, 2U, 3U});
  requireMonotonicCaretX(ascii);
  require(doku::text::hitTestTextLine(ascii, -100.0F).byteOffset == 0U,
          "left hit-test should clamp to start");
  require(doku::text::hitTestTextLine(ascii, 100000.0F).byteOffset == 3U,
          "right hit-test should clamp to end");
  require(doku::text::caretXForByteOffset(ascii, 0U) <
              doku::text::caretXForByteOffset(ascii, 1U),
          "ASCII caret x should increase");

  const doku::text::PreparedTextLine japanese =
      textSystem.prepareLine(atlas, "祇園");
  requireOffsets(japanese, {0U, 3U, 6U});
  requireMonotonicCaretX(japanese);
  require(doku::text::caretForByteOffset(japanese, 1U).byteOffset == 0U,
          "Japanese internal byte offset should clamp to valid boundary");
  require(doku::text::hitTestTextLine(japanese, -100.0F).byteOffset == 0U,
          "Japanese hit-test start clamp failed");
  require(doku::text::hitTestTextLine(japanese, 100000.0F).byteOffset == 6U,
          "Japanese hit-test end clamp failed");

  const doku::text::PreparedTextLine mixed =
      textSystem.prepareLine(atlas, "A祇B");
  requireOffsets(mixed, {0U, 1U, 4U, 5U});
  requireMonotonicCaretX(mixed);

  const doku::text::PreparedTextLine emoji =
      textSystem.prepareLine(atlas, "A🍣B");
  requireOffsets(emoji, {0U, 1U, 5U, 6U});
  requireMonotonicCaretX(emoji);
  require(doku::text::caretForByteOffset(emoji, 2U).byteOffset == 1U,
          "emoji internal byte offset should clamp before cluster");
  const auto emojiBoundaries = doku::text::caretBoundaries(emoji);
  const float emojiLeft = emojiBoundaries[1].x;
  const float emojiRight = emojiBoundaries[2].x;
  require(doku::text::hitTestTextLine(
              emoji, emojiLeft + (emojiRight - emojiLeft) * 0.25F)
                  .byteOffset == 1U,
          "emoji hit-test before midpoint should return leading boundary");
  require(doku::text::hitTestTextLine(
              emoji, emojiLeft + (emojiRight - emojiLeft) * 0.75F)
                  .byteOffset == 5U,
          "emoji hit-test after midpoint should return trailing boundary");

  const doku::text::PreparedTextLine zwj =
      textSystem.prepareLine(atlas, "👨‍👩‍👧");
  require(doku::text::caretBoundaries(zwj).size() == 2U,
          "ZWJ sequence should be one caret cluster when supported");

  const doku::text::PreparedTextLine empty = textSystem.prepareLine(atlas, "");
  require(doku::text::caretBoundaries(empty).size() >= 1U,
          "empty line should expose one caret boundary");
  require(doku::text::caretBoundaries(empty)[0].byteOffset == 0U,
          "empty caret byte offset should be zero");
  require(doku::text::caretBoundaries(empty)[0].x == 0.0F,
          "empty caret x should be zero");
  require(doku::text::hitTestTextLine(empty, 50.0F).byteOffset == 0U,
          "empty hit-test should return zero");

  const std::vector<doku::text::TextSelectionRect> selection =
      doku::text::selectionRectsForByteRange(ascii, 1U, 3U);
  require(selection.size() == 1U, "ASCII selection should return one rect");
  require(selection.front().width > 0.0F, "selection width should be positive");
  require(std::fabs(selection.front().x -
                    doku::text::caretXForByteOffset(ascii, 1U)) < 0.001F,
          "selection x should match begin caret");
  require(std::fabs(selection.front().x + selection.front().width -
                    doku::text::caretXForByteOffset(ascii, 3U)) < 0.001F,
          "selection right should match end caret");

  return 0;
}
