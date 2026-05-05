#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
  const std::vector<std::string> args = {
      "--verify-metrics",
      "--trace-colrv1",
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
      "👨‍👩‍👧‍👦👨‍👩‍👧‍👦",
      "0",
      "128",
  };

  std::cout << "[example] runCli metrics and trace logging\n";
  return glyphrenderer::runCli(args);
}
