#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>

int main() {
  std::cout << "[test] fallback diagnostics\n";

  glyphrenderer::GlyphRendererContext context;
  glyphrenderer::GlyphRenderer renderer(context);

  glyphrenderer::TextLayoutContext layoutContext;
  layoutContext.fallbackChain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  layoutContext.fallbackChain.emojiFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  layoutContext.fallbackChain.symbolFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  layoutContext.pixelSize = 96;

  const glyphrenderer::TextShapeResult shape =
      renderer.shapeLine(layoutContext, "A🫨");
  std::cout << "clusters=" << shape.clusters.size()
            << " diagnostics=" << shape.diagnostics.size() << '\n';
  for (std::size_t i = 0; i < shape.clusters.size(); ++i) {
    const glyphrenderer::ShapedCluster &cluster = shape.clusters[i];
    std::cout << "cluster[" << i << "] fallback=" << cluster.fallbackApplied
              << " font=" << cluster.fontPath
              << " glyphs=" << cluster.glyphs.size() << '\n';
  }
  for (const glyphrenderer::Diagnostic &diagnostic : shape.diagnostics) {
    std::cout << "diagnostic severity="
              << glyphrenderer::toString(diagnostic.severity)
              << " code=" << glyphrenderer::toString(diagnostic.code)
              << " clusterText=" << diagnostic.clusterText
              << " message=" << diagnostic.message
              << " font=" << diagnostic.fontPath << '\n';
  }
  for (const glyphrenderer::FallbackTraceEntry &entry : shape.fallbackTrace) {
    std::cout << "trace cluster=" << entry.clusterIndex
              << " role=" << glyphrenderer::toString(entry.requestedRole)
              << " text=" << entry.clusterText
              << " fallback=" << entry.fallbackApplied
              << " attempted=" << entry.attemptedFonts.size()
              << " selected=" << entry.selectedFontPath << '\n';
  }
  return shape.clusters.empty() ? 1 : 0;
}
