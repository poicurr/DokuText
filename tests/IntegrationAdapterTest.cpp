#include "GlyphRenderer/GlyphRenderer.hpp"

#include <iostream>

int main() {
  std::cout << "[test] integration adapter api\n";

  glyphrenderer::TextSystem textSystem;
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
      textSystem.registerFontFallbackChain(chain);

  glyphrenderer::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 64;
  atlasOptions.atlasContext.pageWidth = 512;
  atlasOptions.atlasContext.pageHeight = 512;
  atlasOptions.atlasContext.padding = 2;
  glyphrenderer::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  glyphrenderer::TextSeedRequest seedRequest;
  seedRequest.presets = {glyphrenderer::TextSeedPreset::Ascii};
  const glyphrenderer::TextAtlasSeedResult seedResult =
      textSystem.seedAtlas(atlas, seedRequest);
  std::cout << "seed.codePoints=" << seedResult.codePoints.size()
            << " seed.pages=" << atlas.pages().size()
            << " uploadPlans=" << seedResult.atlasUploadPlan.size() << '\n';

  const glyphrenderer::PreparedTextLine line =
      textSystem.prepareLine(atlas, "A🍣");
  const glyphrenderer::TextCompatibilityReport layoutCompatibility =
      glyphrenderer::analyzeTextCompatibility(line.layout);
  const glyphrenderer::TextCompatibilityReport compatibility =
      glyphrenderer::analyzeTextCompatibility(line.drawRun);
  glyphrenderer::CompatibilityConstraints constraints;
  constraints.allowColorGlyphs = false;
  constraints.allowMultiplePages = false;
  constraints.maxPages = 1;
  const glyphrenderer::TextCompatibilityReport constrainedCompatibility =
      glyphrenderer::analyzeTextCompatibility(line.drawRun, constraints);
  const glyphrenderer::TextDrawSubmission submission =
      glyphrenderer::buildTextDrawSubmission(line.drawRun);
  std::cout << "compat.alphaOnly=" << compatibility.alphaOnlyCompatible
            << " requiresColor=" << compatibility.requiresColorGlyph
            << " pages=" << compatibility.pagesUsed.size()
            << " materials=" << compatibility.materialKindsUsed.size()
            << " missingGlyph=" << compatibility.hasMissingGlyph
            << " atlasOverflow=" << compatibility.hasAtlasOverflow << '\n';
  std::cout << "layoutCompat.alphaOnly="
            << layoutCompatibility.alphaOnlyCompatible
            << " constrainedDiagnostics="
            << constrainedCompatibility.diagnostics.size()
            << " uploadPlanPages=" << line.atlasUploadPlan.size() << '\n';
  std::cout << "submission.glyphs=" << submission.glyphs.size()
            << " drawPages=" << submission.compatibility.pagesUsed.size()
            << " firstMaterial="
            << (submission.glyphs.empty()
                    ? "None"
                    : glyphrenderer::toString(
                          submission.glyphs.front().materialKind))
            << '\n';

  if (!atlas.pages().empty()) {
    const glyphrenderer::BitmapImage normalized =
        glyphrenderer::normalizeAlphaMaskToWhite(atlas.pages().front().image);
    std::cout << "normalizedAlpha=" << normalized.width << "x"
              << normalized.height << " uvOrigin="
              << glyphrenderer::toString(glyphrenderer::atlasUvOrigin())
              << '\n';
  }

  glyphrenderer::TextPrepareOptions paragraphOptions;
  paragraphOptions.layoutOptions.wrappingWidth = 128;
  const glyphrenderer::PreparedTextParagraph paragraph =
      textSystem.prepareText(atlas, "Aあ\n🍣", paragraphOptions);
  const glyphrenderer::TextCompatibilityReport paragraphCompatibility =
      glyphrenderer::analyzeTextCompatibility(paragraph.drawRun);
  std::cout << "paragraph.lines=" << paragraph.drawRun.lines.size()
            << " paragraph.pages=" << paragraphCompatibility.pagesUsed.size()
            << " paragraph.requiresColor="
            << paragraphCompatibility.requiresColorGlyph << '\n';

  const bool ok =
      !seedResult.codePoints.empty() && !seedResult.atlasUploadPlan.empty() &&
      layoutCompatibility.requiresColorGlyph &&
      !compatibility.alphaOnlyCompatible && compatibility.requiresColorGlyph &&
      !constrainedCompatibility.diagnostics.empty() &&
      !line.atlasUploadPlan.empty() && !submission.glyphs.empty() &&
      !paragraph.drawRun.lines.empty() &&
      paragraphCompatibility.requiresColorGlyph;
  return ok ? 0 : 1;
}
