#include <DokuText/DokuText.hpp>

#include <iostream>

int main() {
  std::cout << "[test] fallback diagnostics\n";

  doku::text::DokuTextContext context;
  doku::text::DokuText renderer(context);

  doku::text::TextLayoutContext layoutContext;
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

  const doku::text::TextShapeResult shape =
      renderer.shapeLine(layoutContext, "A🫨");
  std::cout << "clusters=" << shape.clusters.size()
            << " diagnostics=" << shape.diagnostics.size() << '\n';
  for (std::size_t i = 0; i < shape.clusters.size(); ++i) {
    const doku::text::ShapedCluster &cluster = shape.clusters[i];
    std::cout << "cluster[" << i << "] fallback=" << cluster.fallbackApplied
              << " font=" << cluster.fontPath
              << " glyphs=" << cluster.glyphs.size() << '\n';
  }
  for (const doku::text::Diagnostic &diagnostic : shape.diagnostics) {
    std::cout << "diagnostic severity="
              << doku::text::toString(diagnostic.severity)
              << " code=" << doku::text::toString(diagnostic.code)
              << " clusterText=" << diagnostic.clusterText
              << " message=" << diagnostic.message
              << " font=" << diagnostic.fontPath << '\n';
  }
  for (const doku::text::FallbackTraceEntry &entry : shape.fallbackTrace) {
    std::cout << "trace cluster=" << entry.clusterIndex
              << " role=" << doku::text::toString(entry.requestedRole)
              << " text=" << entry.clusterText
              << " fallback=" << entry.fallbackApplied
              << " attempted=" << entry.attemptedFonts.size()
              << " selected=" << entry.selectedFontPath << '\n';
  }
  return shape.clusters.empty() ? 1 : 0;
}
