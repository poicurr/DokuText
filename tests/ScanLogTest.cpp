#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
  std::cout << "[test] filtered COLRv1 scan log\n";
  const std::vector<std::string> args = {
      "--scan-colrv1",
      "--scan-colrv1-summary-only",
      "--scan-colrv1-limit",
      "80",
      "--scan-colrv1-min-gradients",
      "8",
      "--scan-colrv1-min-transforms",
      "20",
      "--scan-colrv1-top",
      "5",
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  return glyphrenderer::runCli(args);
}
