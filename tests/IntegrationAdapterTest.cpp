#include <DokuText/DokuText.hpp>

#include <iostream>

#include "TestUtils.hpp"

int main() {
  doku_test::skipIfTestFontsMissing();

  std::cout << "[test] integration adapter api\n";

  doku::text::TextSystem textSystem;
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
      textSystem.registerFontFallbackChain(chain);

  doku::text::TextAtlasOptions atlasOptions;
  atlasOptions.fontChainId = fontChainId;
  atlasOptions.pixelSize = 64;
  atlasOptions.atlasContext.pageWidth = 512;
  atlasOptions.atlasContext.pageHeight = 512;
  atlasOptions.atlasContext.padding = 2;
  doku::text::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

  doku::text::TextSeedRequest seedRequest;
  seedRequest.presets = {doku::text::TextSeedPreset::Ascii};
  const doku::text::TextAtlasSeedResult seedResult =
      textSystem.seedAtlas(atlas, seedRequest);
  std::cout << "seed.codePoints=" << seedResult.codePoints.size()
            << " seed.pages=" << atlas.pages().size()
            << " uploadPlans=" << seedResult.atlasUploadPlan.size() << '\n';

  const doku::text::PreparedTextLine line =
      textSystem.prepareLine(atlas, "A🍣");
  const doku::text::TextCompatibilityReport layoutCompatibility =
      doku::text::analyzeTextCompatibility(line.layout);
  const doku::text::TextCompatibilityReport compatibility =
      doku::text::analyzeTextCompatibility(line.drawRun);
  doku::text::CompatibilityConstraints constraints;
  constraints.allowColorGlyphs = false;
  constraints.allowMultiplePages = false;
  constraints.maxPages = 1;
  const doku::text::TextCompatibilityReport constrainedCompatibility =
      doku::text::analyzeTextCompatibility(line.drawRun, constraints);
  const doku::text::TextDrawSubmission submission =
      doku::text::buildTextDrawSubmission(line.drawRun);
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
                    : doku::text::toString(
                          submission.glyphs.front().materialKind))
            << '\n';

  if (!atlas.pages().empty()) {
    const doku::text::BitmapImage normalized =
        doku::text::normalizeAlphaMaskToWhite(atlas.pages().front().image);
    std::cout << "normalizedAlpha=" << normalized.width << "x"
              << normalized.height << " uvOrigin="
              << doku::text::toString(doku::text::atlasUvOrigin())
              << '\n';
  }

  doku::text::TextPrepareOptions paragraphOptions;
  paragraphOptions.layoutOptions.wrappingWidth = 128;
  const doku::text::PreparedTextParagraph paragraph =
      textSystem.prepareText(atlas, "Aあ\n🍣", paragraphOptions);
  const doku::text::TextCompatibilityReport paragraphCompatibility =
      doku::text::analyzeTextCompatibility(paragraph.drawRun);
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
