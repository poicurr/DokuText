#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>

int main() {
  std::cout << "[test] atlas cache api\n";

  glyphrenderer::GlyphRendererContext context;
  glyphrenderer::FontFallbackChain chain;
  chain.textFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  chain.emojiFonts = {
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
  };
  chain.symbolFonts = {
      "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
  };
  const glyphrenderer::FontChainId fontChainId =
      context.registerFontFallbackChain(chain);

  glyphrenderer::GlyphRenderer renderer(context);
  glyphrenderer::TextLayoutContext layoutContext;
  layoutContext.fontChainId = fontChainId;
  layoutContext.pixelSize = 96;
  layoutContext.paragraphOptions.wrappingWidth = 256;
  layoutContext.paragraphOptions.horizontalAlign =
      glyphrenderer::HorizontalAlign::Center;

  const glyphrenderer::TextLineLayout layout =
      renderer.layoutLine(layoutContext, "Aあ🍣");

  glyphrenderer::AtlasBuildContext atlasContext;
  atlasContext.pageWidth = 256;
  atlasContext.pageHeight = 256;
  atlasContext.padding = 2;
  glyphrenderer::GlyphAtlasCache cache(atlasContext);

  const glyphrenderer::GlyphAtlasCacheUpdate firstUpdate =
      cache.ensureGlyphs(layout);
  const std::vector<glyphrenderer::AtlasPageUploadPlan> firstUploadPlan =
      glyphrenderer::buildAtlasPageUploadPlan(cache, firstUpdate);
  const glyphrenderer::GlyphAtlasCacheUpdate secondUpdate =
      cache.ensureGlyphs(layout);
  const std::vector<glyphrenderer::AtlasPageUploadPlan> secondUploadPlan =
      glyphrenderer::buildAtlasPageUploadPlan(cache, secondUpdate);
  const glyphrenderer::TextDrawRun drawRun =
      renderer.buildDrawRun(layout, cache);
  const glyphrenderer::GlyphHandle firstHandle =
      drawRun.glyphs.empty() ? glyphrenderer::GlyphHandle{}
                             : drawRun.glyphs.front().handle;
  const glyphrenderer::GlyphAtlasCacheUpdate evictUpdate =
      firstHandle.value == 0 ? glyphrenderer::GlyphAtlasCacheUpdate{}
                             : cache.evict({firstHandle});
  const glyphrenderer::GlyphAtlasCacheUpdate compactUpdate = cache.compact();

  std::cout << "first.newPages=" << firstUpdate.newPages.size()
            << " first.updatedEntries=" << firstUpdate.updatedEntries.size()
            << " first.dirtyRects=" << firstUpdate.dirtyRects.size()
            << " first.uploadPlans=" << firstUploadPlan.size() << '\n';
  std::cout << "second.newPages=" << secondUpdate.newPages.size()
            << " second.updatedEntries=" << secondUpdate.updatedEntries.size()
            << " second.uploadPlans=" << secondUploadPlan.size()
            << " cache.pages=" << cache.pages().size()
            << " cache.entries=" << cache.entries().size() << '\n';
  std::cout << "evict.evicted=" << evictUpdate.evictedHandles.size()
            << " evict.entries=" << cache.entries().size()
            << " compact.pages=" << compactUpdate.newPages.size()
            << " compact.entries=" << compactUpdate.updatedEntries.size()
            << '\n';
  if (!drawRun.glyphs.empty()) {
    const glyphrenderer::PositionedGlyph &glyph = drawRun.glyphs.front();
    std::cout << "first.page=" << glyph.atlasPage
              << " pageHandle=" << glyph.atlasPageHandle.value
              << " material=" << glyphrenderer::toString(glyph.materialKind)
              << " blend=" << glyphrenderer::toString(glyph.blendHint) << '\n';
  }
  return firstUpdate.updatedEntries.empty() || firstUploadPlan.empty() ? 1 : 0;
}
