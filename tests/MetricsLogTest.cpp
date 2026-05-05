#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
  std::cout << "[test] metrics and COLRv1 trace log\n";
  const std::vector<std::string> args = {
      "--verify-metrics",
      "--trace-colrv1",
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
      "👨‍👩‍👧‍👦👨‍👩‍👧‍👦",
      "0",
      "128",
  };
  return glyphrenderer::runCli(args);
}
