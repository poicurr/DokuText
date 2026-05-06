#include <DokuText/DokuText.hpp>

#include <iostream>

int main() {
  doku::text::DokuTextContext context;
  doku::text::FontFallbackChain registeredChain;
  registeredChain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  registeredChain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  registeredChain.symbolFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  const doku::text::FontChainId fontChainId =
      context.registerFontFallbackChain(registeredChain);
  doku::text::DokuText renderer(context);

  doku::text::TextLayoutContext layoutContext;
  layoutContext.fontChainId = fontChainId;
  layoutContext.pixelSize = 96;
  layoutContext.verifyMetrics = true;
  layoutContext.paragraphOptions.wrappingWidth = 512;
  layoutContext.paragraphOptions.horizontalAlign =
      doku::text::HorizontalAlign::Center;
  layoutContext.paragraphOptions.lineSpacingMultiplier = 1.1F;

  const std::string text = "Aあ\n🍣";
  const doku::text::TextShapeResult shape =
      renderer.shapeLine(layoutContext, "Aあ🍣");
  const doku::text::TextRasterResult raster =
      renderer.rasterizeLine(layoutContext, shape);
  const doku::text::TextLineLayout layout = renderer.layoutLine(raster);
  const doku::text::MultiLineLayout multiLine =
      renderer.layoutText(layoutContext, text);
  const doku::text::MultiLineShapeResult multiShape =
      renderer.shapeText(layoutContext, text);
  const doku::text::MultiLineRasterResult multiRaster =
      renderer.rasterizeText(layoutContext, multiShape);

  doku::text::AtlasBuildContext atlasContext;
  atlasContext.pageWidth = 512;
  atlasContext.pageHeight = 512;
  atlasContext.padding = 2;
  doku::text::GlyphAtlasCache atlasCache(atlasContext);
  const doku::text::AtlasBuildResult atlas =
      renderer.buildAtlas(layout, atlasContext);
  const doku::text::TextDrawRun drawRun =
      renderer.buildDrawRun(layout, atlas);
  const doku::text::GlyphAtlasCacheUpdate atlasUpdate =
      atlasCache.ensureGlyphs(layout);
  const std::vector<doku::text::AtlasPageUploadPlan> atlasUploadPlan =
      doku::text::buildAtlasPageUploadPlan(atlasCache, atlasUpdate);
  const doku::text::TextDrawRun cachedDrawRun =
      renderer.buildDrawRun(layout, atlasCache);
  const doku::text::MultiLineDrawRun multiDraw =
      renderer.buildDrawText(layoutContext, text, atlasContext);
  const doku::text::MultiLineDrawRun cachedMultiDraw =
      renderer.buildDrawText(multiLine, atlasCache);
  const doku::text::TextCompatibilityReport layoutCompatibility =
      doku::text::analyzeTextCompatibility(layout);
  const doku::text::TextCompatibilityReport compatibility =
      doku::text::analyzeTextCompatibility(cachedDrawRun);
  doku::text::CompatibilityConstraints alphaOnlyConstraints;
  alphaOnlyConstraints.allowColorGlyphs = false;
  alphaOnlyConstraints.allowMultiplePages = false;
  alphaOnlyConstraints.maxPages = 1;
  const doku::text::TextCompatibilityReport constrainedCompatibility =
      doku::text::analyzeTextCompatibility(cachedDrawRun,
                                              alphaOnlyConstraints);
  const doku::text::TextDrawSubmission submission =
      doku::text::buildTextDrawSubmission(cachedMultiDraw);

  std::cout << "[example] engine session pipeline\n";
  std::cout << "shape.clusters=" << shape.clusters.size()
            << " diagnostics=" << shape.diagnostics.size()
            << " fallbackCount=" << shape.summary.fallbackCount << '\n';
  for (std::size_t i = 0; i < shape.clusters.size(); ++i) {
    const doku::text::ShapedCluster &cluster = shape.clusters[i];
    std::cout << "  cluster[" << i << "] glyphs=" << cluster.glyphs.size()
              << " fallback=" << cluster.fallbackApplied
              << " font=" << cluster.fontPath << '\n';
  }
  for (const doku::text::FallbackTraceEntry &entry : shape.fallbackTrace) {
    std::cout << "  trace cluster=" << entry.clusterIndex
              << " text=" << entry.clusterText
              << " attempted=" << entry.attemptedFonts.size()
              << " selected=" << entry.selectedFontPath << '\n';
  }

  std::cout << "layout.glyphs=" << layout.glyphs.size()
            << " baseline=" << layout.metrics.baseline
            << " lineWidth=" << layout.metrics.lineWidth
            << " colorGlyphs=" << layout.summary.colorGlyphCount << '\n';
  std::cout << "raster.glyphs=" << raster.glyphs.size()
            << " raster.diagnostics=" << raster.diagnostics.size()
            << " usedFonts=" << raster.summary.usedFonts.size() << '\n';
  if (!layout.glyphs.empty()) {
    const doku::text::PositionedGlyph &glyph = layout.glyphs.front();
    std::cout << "first.layout handle=" << glyph.handle.value
              << " glyphId=" << glyph.glyphId
              << " clusterIndex=" << glyph.clusterIndex << '\n';
  }
  std::cout << "atlas.pages=" << atlas.pages.size()
            << " atlas.entries=" << atlas.entries.size()
            << " atlas.diagnostics=" << atlas.diagnostics.size() << '\n';
  if (!atlas.entries.empty()) {
    const doku::text::GlyphAtlasEntry &entry = atlas.entries.front();
    std::cout << "first.atlas handle=" << entry.handle.value
              << " page=" << entry.pageIndex << " uv=(" << entry.uvRect.minX
              << "," << entry.uvRect.minY << ")-(" << entry.uvRect.maxX << ","
              << entry.uvRect.maxY << ")\n";
  }
  std::cout << "drawRun.glyphs=" << drawRun.glyphs.size()
            << " drawRun.diagnostics=" << drawRun.diagnostics.size()
            << " atlasPages=" << drawRun.summary.atlasPageCount << '\n';
  std::cout << "atlasCache.newPages=" << atlasUpdate.newPages.size()
            << " atlasCache.updatedEntries="
            << atlasUpdate.updatedEntries.size()
            << " atlasCache.dirtyRects=" << atlasUpdate.dirtyRects.size()
            << " cachePages=" << atlasCache.pages().size()
            << " uploadPlans=" << atlasUploadPlan.size() << '\n';
  if (!drawRun.glyphs.empty()) {
    const doku::text::PositionedGlyph &glyph = drawRun.glyphs.front();
    std::cout << "first.draw page=" << glyph.atlasPage << " uv=("
              << glyph.uvRect.minX << "," << glyph.uvRect.minY << ")-("
              << glyph.uvRect.maxX << "," << glyph.uvRect.maxY << ")\n";
  }
  if (!cachedDrawRun.glyphs.empty()) {
    const doku::text::PositionedGlyph &glyph = cachedDrawRun.glyphs.front();
    std::cout << "first.cached pageHandle=" << glyph.atlasPageHandle.value
              << " material=" << doku::text::toString(glyph.materialKind)
              << " blend=" << doku::text::toString(glyph.blendHint) << '\n';
  }
  std::cout << "layoutCompatibility alphaOnly="
            << layoutCompatibility.alphaOnlyCompatible
            << " requiresColor=" << layoutCompatibility.requiresColorGlyph
            << '\n';
  std::cout << "compatibility alphaOnly=" << compatibility.alphaOnlyCompatible
            << " singlePage=" << compatibility.singlePageCompatible
            << " requiresColor=" << compatibility.requiresColorGlyph
            << " pages=" << compatibility.pagesUsed.size()
            << " fallbackFonts=" << compatibility.fallbackFonts.size() << '\n';
  std::cout << "constrainedCompatibility diagnostics="
            << constrainedCompatibility.diagnostics.size()
            << " singlePage=" << constrainedCompatibility.singlePageCompatible
            << '\n';
  std::cout << "submission glyphs=" << submission.glyphs.size() << " materials="
            << submission.compatibility.materialKindsUsed.size()
            << " boundingMaxY=" << submission.boundingBox.maxY << '\n';
  std::cout << "multi.lines=" << multiLine.lines.size()
            << " multi.shapeLines=" << multiShape.lines.size()
            << " multi.rasterLines=" << multiRaster.lines.size()
            << " multi.lineAdvance=" << multiLine.lineAdvance
            << " multiDraw.lines=" << multiDraw.lines.size()
            << " multiDraw.glyphs=" << multiDraw.glyphs.size() << '\n';
  std::cout << "multi.lineSummaries=" << multiDraw.lineSummaries.size()
            << " multi.fallbackTrace=" << multiDraw.fallbackTrace.size()
            << " multi.lineTexts=" << multiDraw.lineTexts.size()
            << " cachedMultiDraw=" << cachedMultiDraw.glyphs.size() << '\n';
  if (multiDraw.lines.size() >= 2 && !multiDraw.lines[1].glyphs.empty()) {
    const doku::text::PositionedGlyph &glyph =
        multiDraw.lines[1].glyphs.front();
    std::cout << "multi.second lineIndex=" << glyph.lineIndex
              << " originY=" << glyph.originY << '\n';
  }
  if (!multiDraw.fallbackTrace.empty()) {
    std::cout << "multi.first traceRole="
              << doku::text::toString(
                     multiDraw.fallbackTrace.front().requestedRole)
              << " lineIndex=" << multiDraw.fallbackTrace.front().lineIndex
              << '\n';
  }
  return layout.glyphs.empty() ? 1 : 0;
}
