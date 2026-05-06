#include <DokuText/DokuText.hpp>

#include <iostream>

int main() {
  std::cout << "[test] atlas cache api\n";

  doku::text::DokuTextContext context;
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
      context.registerFontFallbackChain(chain);

  doku::text::DokuText renderer(context);
  doku::text::TextLayoutContext layoutContext;
  layoutContext.fontChainId = fontChainId;
  layoutContext.pixelSize = 96;
  layoutContext.paragraphOptions.wrappingWidth = 256;
  layoutContext.paragraphOptions.horizontalAlign =
      doku::text::HorizontalAlign::Center;

  const doku::text::TextLineLayout layout =
      renderer.layoutLine(layoutContext, "Aあ🍣");

  doku::text::AtlasBuildContext atlasContext;
  atlasContext.pageWidth = 256;
  atlasContext.pageHeight = 256;
  atlasContext.padding = 2;
  doku::text::GlyphAtlasCache cache(atlasContext);

  const doku::text::GlyphAtlasCacheUpdate firstUpdate =
      cache.ensureGlyphs(layout);
  const std::vector<doku::text::AtlasPageUploadPlan> firstUploadPlan =
      doku::text::buildAtlasPageUploadPlan(cache, firstUpdate);
  const doku::text::GlyphAtlasCacheUpdate secondUpdate =
      cache.ensureGlyphs(layout);
  const std::vector<doku::text::AtlasPageUploadPlan> secondUploadPlan =
      doku::text::buildAtlasPageUploadPlan(cache, secondUpdate);
  const doku::text::TextDrawRun drawRun =
      renderer.buildDrawRun(layout, cache);
  const doku::text::GlyphHandle firstHandle =
      drawRun.glyphs.empty() ? doku::text::GlyphHandle{}
                             : drawRun.glyphs.front().handle;
  const doku::text::GlyphAtlasCacheUpdate evictUpdate =
      firstHandle.value == 0 ? doku::text::GlyphAtlasCacheUpdate{}
                             : cache.evict({firstHandle});
  const doku::text::GlyphAtlasCacheUpdate compactUpdate = cache.compact();

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
    const doku::text::PositionedGlyph &glyph = drawRun.glyphs.front();
    std::cout << "first.page=" << glyph.atlasPage
              << " pageHandle=" << glyph.atlasPageHandle.value
              << " material=" << doku::text::toString(glyph.materialKind)
              << " blend=" << doku::text::toString(glyph.blendHint) << '\n';
  }
  return firstUpdate.updatedEntries.empty() || firstUploadPlan.empty() ? 1 : 0;
}
