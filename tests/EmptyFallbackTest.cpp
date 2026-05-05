#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>
#include <stdexcept>

int main() {
  glyphrenderer::GlyphRendererContext context;
  glyphrenderer::GlyphRenderer renderer(context);
  glyphrenderer::TextLayoutContext layoutContext;

  try {
    renderer.shapeLine(layoutContext, "Hello");
    std::cerr << "Error: shapeLine should have thrown std::invalid_argument\n";
    return 1;
  } catch (const std::invalid_argument &e) {
    std::cout << "Caught expected exception: " << e.what() << '\n';
  }

  glyphrenderer::TextRequest request;
  request.text = "Hello";

  try {
    renderer.layoutLine(request);
    std::cerr << "Error: layoutLine(TextRequest) should have thrown "
                 "std::invalid_argument\n";
    return 1;
  } catch (const std::invalid_argument &e) {
    std::cout << "Caught expected exception from TextRequest: " << e.what()
              << '\n';
  }

  return 0;
}
