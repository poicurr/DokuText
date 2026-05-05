#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
  std::cout << "[test] smoke cli emoji cluster render\n";
  const std::vector<std::string> args = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
      "👨‍👩‍👧‍👦",
      "0",
      "128",
  };
  return glyphrenderer::runCli(args);
}
