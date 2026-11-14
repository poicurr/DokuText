#include <DokuText/DokuText.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "TestUtils.hpp"

using namespace doku::text;

namespace {

constexpr const char *TEXT_FONT =
    "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf";
constexpr const char *EMOJI_FONT =
    "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf";

[[noreturn]] void fail(const std::string &message) {
  std::cerr << "FAILED: " << message << std::endl;
  std::exit(1);
}

void check(bool condition, const std::string &message) {
  if (!condition) {
    fail(message);
  }
}

struct AtlasSnapshot {
  std::size_t pageCount = 0;
  std::size_t entryCount = 0;
};

AtlasSnapshot snapshot(const TextAtlas &atlas) {
  return AtlasSnapshot{
      atlas.pages().size(),
      atlas.entries().size(),
  };
}

void checkUnchanged(const AtlasSnapshot &before, const TextAtlas &atlas,
                    const std::string &stage) {
  const AtlasSnapshot after = snapshot(atlas);
  if (after.pageCount != before.pageCount ||
      after.entryCount != before.entryCount) {
    std::cerr << "FAILED at " << stage << ": pages " << before.pageCount
              << " -> " << after.pageCount << ", entries "
              << before.entryCount << " -> " << after.entryCount
              << std::endl;
    std::exit(1);
  }
}

FontChainId registerDefaultChain(TextSystem &system) {
  FontFallbackChain chain;
  chain.textFonts = {TEXT_FONT};
  chain.emojiFonts = {EMOJI_FONT};
  return system.registerFontFallbackChain(chain);
}

TextAtlasOptions makeAtlasOptions(FontChainId fontChainId,
                                  unsigned int pixelSize = 32) {
  TextAtlasOptions options;
  options.fontChainId = fontChainId;
  options.pixelSize = pixelSize;
  return options;
}

TextRequest makeRequest(const TextAtlasOptions &atlasOptions,
                        std::string text) {
  TextRequest request;
  request.text = std::move(text);
  request.fontChainId = atlasOptions.fontChainId;
  request.pixelSize = atlasOptions.pixelSize;
  request.paletteIndex = atlasOptions.paletteIndex;
  request.layoutOptions = {};
  return request;
}

bool hasMissingColorBitmapGlyph(const TextProbeResult &probe) {
  for (const MissingAtlasGlyph &missing : probe.missingGlyphs) {
    if (missing.isColorGlyph ||
        missing.materialKind == MaterialKind::ColorBitmap) {
      return true;
    }
  }
  return false;
}

void test_append_plan_from_non_mutating_layout() {
  std::cout << "Running test_append_plan_from_non_mutating_layout..."
            << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  TextPrepareOptions options;
  options.layoutOptions.horizontalAlign = HorizontalAlign::Left;
  const TextAtlasAppendPlan plan =
      system.buildTextAtlasAppendPlan(atlas, "Hello", options);

  check(plan.estimatedGlyphCount > 0,
        "append plan should report missing glyph candidates");
  check(!plan.candidates.empty(),
        "append plan should return concrete missing glyph candidates");
  check(plan.estimatedColorGlyphCount == 0,
        "ASCII append plan should not require color glyphs");
  check(!plan.budgetExceeded, "append plan should not exceed default budget");
  check(atlas.entries().empty(),
        "append plan should not mutate atlas");
}

void test_append_plan_budget_gate() {
  std::cout << "Running test_append_plan_budget_gate..." << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  TextAtlasAppendBudget budget;
  budget.maxGlyphs = 1U;
  const TextAtlasAppendPlan plan =
      system.buildTextAtlasAppendPlan(atlas, "Hello", {}, budget);

  check(plan.budgetExceeded,
        "append plan should flag budget exceeded when maxGlyphs is too small");
  check(plan.estimatedUploadBytes == 0 && plan.candidates.size() == 4,
        "glyph-count rejection should retain candidates and skip bitmap work");
  const auto measured = system.buildTextAtlasAppendPlan(atlas, "Hello");
  check(measured.estimatedUploadBytes > 0, "append plan lost upload bytes");
  budget = {};
  budget.maxAtlasUploadBytes = measured.estimatedUploadBytes - 1;
  check(system.buildTextAtlasAppendPlan(atlas, "Hello", {}, budget)
            .budgetExceeded,
        "append plan should enforce the upload-byte limit");
}

void test_append_plan_color_glyph_candidates() {
  std::cout << "Running test_append_plan_color_glyph_candidates..."
            << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId, 64);
  TextAtlas atlas(atlasOptions);

  const TextAtlasAppendPlan plan = system.buildTextAtlasAppendPlan(atlas, "🍣");
  check(plan.estimatedColorGlyphCount > 0,
        "emoji append plan should include color glyph candidates");
}

void test_revisions_change_only_on_mutation() {
  std::cout << "Running test_revisions_change_only_on_mutation..."
            << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  check(atlas.glyphSetRevision() == 0,
        "initial glyphSetRevision should be 0");
  check(atlas.atlasTextureRevision() == 0,
        "initial atlasTextureRevision should be 0");

  TextPrepareOptions noAppend;
  noAppend.allowGlyphAppend = false;
  (void)system.prepareLine(atlas, "Hello", noAppend);
  check(atlas.glyphSetRevision() == 0,
        "non-mutating prepareLine should not change glyphSetRevision");
  check(atlas.atlasTextureRevision() == 0,
        "non-mutating prepareLine should not change atlasTextureRevision");

  TextPrepareOptions yesAppend;
  yesAppend.allowGlyphAppend = true;
  (void)system.prepareLine(atlas, "Hello", yesAppend);
  check(atlas.glyphSetRevision() > 0,
        "mutating prepareLine should advance glyphSetRevision");
  check(atlas.atlasTextureRevision() > 0,
        "mutating prepareLine should advance atlasTextureRevision");

  const std::uint64_t glyphRevision = atlas.glyphSetRevision();
  const std::uint64_t textureRevision = atlas.atlasTextureRevision();
  (void)system.prepareLine(atlas, "Hello", yesAppend);
  check(atlas.glyphSetRevision() == glyphRevision,
        "existing-glyph prepareLine should not advance glyphSetRevision");
  check(atlas.atlasTextureRevision() == textureRevision,
        "existing-glyph prepareLine should not advance atlasTextureRevision");
}

void test_prepareLine_false_empty_atlas_string_view() {
  std::cout << "Running test_prepareLine_false_empty_atlas_string_view..."
            << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  check(atlas.pages().empty(), "fresh atlas should have no pages");
  check(atlas.entries().empty(), "fresh atlas should have no entries");

  TextPrepareOptions options;
  options.allowGlyphAppend = false;

  const AtlasSnapshot before = snapshot(atlas);
  (void)system.prepareLine(atlas, "Hello", options);
  checkUnchanged(before, atlas,
                 "after prepareLine(string_view, allowGlyphAppend=false)");

  TextProbeResult probe = system.probeText(atlas, "Hello");
  check(!probe.allGlyphsAvailable,
        "probe should report missing glyphs on empty atlas");
  check(probe.missingGlyphCount > 0,
        "probe should report missingGlyphCount > 0 on empty atlas");
}

void test_prepareText_false_empty_atlas_string_view() {
  std::cout << "Running test_prepareText_false_empty_atlas_string_view..."
            << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  TextPrepareOptions options;
  options.allowGlyphAppend = false;

  const AtlasSnapshot before = snapshot(atlas);
  (void)system.prepareText(atlas, "Multi\nLine", options);
  checkUnchanged(before, atlas,
                 "after prepareText(string_view, allowGlyphAppend=false)");

  TextProbeResult probe = system.probeText(atlas, "Multi\nLine");
  check(!probe.allGlyphsAvailable,
        "probe should report missing glyphs after non-mutating prepareText");
  check(probe.missingGlyphCount > 0,
        "probe should report missingGlyphCount > 0 after non-mutating prepareText");
}

void test_prepareLine_false_empty_atlas_text_request() {
  std::cout << "Running test_prepareLine_false_empty_atlas_text_request..."
            << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  TextRequest request = makeRequest(atlasOptions, "Hello");

  TextPrepareOptions options;
  options.allowGlyphAppend = false;

  const AtlasSnapshot before = snapshot(atlas);
  (void)system.prepareLine(atlas, request, options);
  checkUnchanged(before, atlas,
                 "after prepareLine(TextRequest, allowGlyphAppend=false)");

  TextProbeResult probe = system.probeText(atlas, request);
  check(!probe.allGlyphsAvailable,
        "probe should report missing glyphs after TextRequest non-mutating prepareLine");
  check(probe.missingGlyphCount > 0,
        "probe should report missingGlyphCount > 0 after TextRequest non-mutating prepareLine");
}

void test_prepareText_false_empty_atlas_text_request() {
  std::cout << "Running test_prepareText_false_empty_atlas_text_request..."
            << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  TextRequest request = makeRequest(atlasOptions, "Multi\nLine");

  TextPrepareOptions options;
  options.allowGlyphAppend = false;

  const AtlasSnapshot before = snapshot(atlas);
  (void)system.prepareText(atlas, request, options);
  checkUnchanged(before, atlas,
                 "after prepareText(TextRequest, allowGlyphAppend=false)");

  TextProbeResult probe = system.probeText(atlas, request);
  check(!probe.allGlyphsAvailable,
        "probe should report missing glyphs after TextRequest non-mutating prepareText");
  check(probe.missingGlyphCount > 0,
        "probe should report missingGlyphCount > 0 after TextRequest non-mutating prepareText");
}

void test_prepareLine_true_empty_atlas() {
  std::cout << "Running test_prepareLine_true_empty_atlas..." << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  TextPrepareOptions options;
  options.allowGlyphAppend = true;

  (void)system.prepareLine(atlas, "Hello", options);

  check(!atlas.entries().empty(),
        "allowGlyphAppend=true should append glyph entries");
  check(!atlas.pages().empty(),
        "allowGlyphAppend=true should create atlas pages");

  TextProbeResult probe = system.probeText(atlas, "Hello");
  check(probe.allGlyphsAvailable,
        "probe should report allGlyphsAvailable after mutating prepareLine");
  check(probe.missingGlyphCount == 0,
        "probe should report missingGlyphCount == 0 after mutating prepareLine");
}

void test_default_options_preserve_behavior() {
  std::cout << "Running test_default_options_preserve_behavior..." << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  (void)system.prepareLine(atlas, "Default");

  check(!atlas.entries().empty(),
        "default TextPrepareOptions should allow glyph append");
  check(!atlas.pages().empty(),
        "default TextPrepareOptions should allow page creation");
}

void test_seeded_atlas_false_string_view() {
  std::cout << "Running test_seeded_atlas_false_string_view..." << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  TextSeedRequest seed;
  seed.texts = {"Hello"};
  (void)system.seedAtlas(atlas, seed);

  TextPrepareOptions options;
  options.allowGlyphAppend = false;

  const AtlasSnapshot before = snapshot(atlas);
  (void)system.prepareLine(atlas, "Hello", options);
  checkUnchanged(before, atlas,
                 "after seeded prepareLine(string_view, allowGlyphAppend=false)");

  TextProbeResult probe = system.probeText(atlas, "Hello");
  check(probe.allGlyphsAvailable,
        "seeded atlas should already contain glyphs for Hello");
  check(probe.missingGlyphCount == 0,
        "seeded atlas should have missingGlyphCount == 0 for Hello");
}

void test_seeded_atlas_false_text_request() {
  std::cout << "Running test_seeded_atlas_false_text_request..." << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId);
  TextAtlas atlas(atlasOptions);

  TextSeedRequest seed;
  seed.texts = {"Hello"};
  (void)system.seedAtlas(atlas, seed);

  TextRequest request = makeRequest(atlasOptions, "Hello");

  TextPrepareOptions options;
  options.allowGlyphAppend = false;

  const AtlasSnapshot before = snapshot(atlas);
  (void)system.prepareLine(atlas, request, options);
  checkUnchanged(before, atlas,
                 "after seeded prepareLine(TextRequest, allowGlyphAppend=false)");

  TextProbeResult probe = system.probeText(atlas, request);
  check(probe.allGlyphsAvailable,
        "seeded atlas should already contain glyphs for TextRequest Hello");
  check(probe.missingGlyphCount == 0,
        "seeded atlas should have missingGlyphCount == 0 for TextRequest Hello");
}

void test_emoji_false_string_view() {
  std::cout << "Running test_emoji_false_string_view..." << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId, 64);
  TextAtlas atlas(atlasOptions);

  TextPrepareOptions options;
  options.allowGlyphAppend = false;

  const AtlasSnapshot before = snapshot(atlas);
  (void)system.prepareLine(atlas, "🍣", options);
  checkUnchanged(before, atlas,
                 "after emoji prepareLine(string_view, allowGlyphAppend=false)");

  TextProbeResult probe = system.probeText(atlas, "🍣");
  check(!probe.allGlyphsAvailable,
        "emoji should still be missing after non-mutating prepareLine");
  check(probe.missingGlyphCount > 0,
        "emoji probe should report missing glyphs after non-mutating prepareLine");
  check(hasMissingColorBitmapGlyph(probe),
        "emoji probe should report missing ColorBitmap/color glyph");
}

void test_emoji_false_text_request() {
  std::cout << "Running test_emoji_false_text_request..." << std::endl;

  TextSystem system;
  const FontChainId fontChainId = registerDefaultChain(system);
  TextAtlasOptions atlasOptions = makeAtlasOptions(fontChainId, 64);
  TextAtlas atlas(atlasOptions);

  TextRequest request = makeRequest(atlasOptions, "🍣");

  TextPrepareOptions options;
  options.allowGlyphAppend = false;

  const AtlasSnapshot before = snapshot(atlas);
  (void)system.prepareLine(atlas, request, options);
  checkUnchanged(before, atlas,
                 "after emoji prepareLine(TextRequest, allowGlyphAppend=false)");

  TextProbeResult probe = system.probeText(atlas, request);
  check(!probe.allGlyphsAvailable,
        "emoji TextRequest should still be missing after non-mutating prepareLine");
  check(probe.missingGlyphCount > 0,
        "emoji TextRequest probe should report missing glyphs");
  check(hasMissingColorBitmapGlyph(probe),
        "emoji TextRequest probe should report missing ColorBitmap/color glyph");
}

bool sameRect(const Rect &a, const Rect &b) {
  return a.minX == b.minX && a.minY == b.minY && a.maxX == b.maxX &&
         a.maxY == b.maxY;
}

void comparePlacement(const std::vector<PositionedGlyph> &prepared,
                      const std::vector<PositionedGlyph> &reference,
                      bool compareAtlas) {
  check(prepared.size() == reference.size(), "glyph count differs");
  for (std::size_t i = 0; i < prepared.size(); ++i) {
    const PositionedGlyph &a = prepared[i];
    const PositionedGlyph &b = reference[i];
    check(a.glyph.image.rgba.empty(), "high-level glyph owns pixels");
    check(!b.glyph.image.rgba.empty(), "low-level glyph has no pixels");
    check(a.handle.value == b.handle.value && a.glyphId == b.glyphId &&
              a.lineIndex == b.lineIndex && a.clusterIndex == b.clusterIndex &&
              a.originX == b.originX && a.originY == b.originY &&
              a.glyph.image.width == b.glyph.image.width &&
              a.glyph.image.height == b.glyph.image.height &&
              a.glyph.bearingX == b.glyph.bearingX &&
              a.glyph.bearingY == b.glyph.bearingY &&
              a.glyph.advance == b.glyph.advance &&
              a.isColorGlyph == b.isColorGlyph,
          "glyph placement or raster metrics differ");
    if (compareAtlas) {
      check(a.atlasPage == b.atlasPage &&
                a.atlasPageHandle.value == b.atlasPageHandle.value &&
                sameRect(a.atlasRect, b.atlasRect) &&
                a.uvRect.minX == b.uvRect.minX &&
                a.uvRect.minY == b.uvRect.minY &&
                a.uvRect.maxX == b.uvRect.maxX &&
                a.uvRect.maxY == b.uvRect.maxY &&
                a.materialKind == b.materialKind &&
                a.samplerKind == b.samplerKind && a.blendHint == b.blendHint,
            "atlas placement or draw attributes differ");
    }
  }
}

void test_lightweight_preparation() {
  TextSystem system;
  DokuTextContext context;
  DokuText renderer(context);
  FontFallbackChain chain;
  chain.textFonts = {TEXT_FONT};
  chain.emojiFonts = {EMOJI_FONT};
  const FontChainId id = system.registerFontFallbackChain(chain);
  const FontChainId referenceId = context.registerFontFallbackChain(chain);

  TextAtlas atlas(makeAtlasOptions(id, 64));
  TextRequest request = makeRequest(atlas.options(), "A🍣 AA\n\n🍣AA A");
  request.layoutOptions.wrappingWidth = 160;
  request.layoutOptions.horizontalAlign = HorizontalAlign::Right;
  request.layoutOptions.baselinePolicy = BaselinePolicy::Tight;
  TextRequest referenceRequest = request;
  referenceRequest.fontChainId = referenceId;
  GlyphAtlasCache referenceAtlas(atlas.options().atlasContext);
  const MultiLineLayout layout = renderer.layoutText(referenceRequest);
  const MultiLineDrawRun draw = renderer.buildDrawText(layout, referenceAtlas);

  const auto cold = system.prepareText(atlas, request);
  check(!cold.atlasUpdate.newPages.empty(), "cold prepare created no pages");
  for (const auto &page : cold.atlasUpdate.newPages) {
    check(page.image.width > 0 && page.image.height > 0 &&
              page.image.rgba.empty(),
          "high-level new-page update contains pixels or lost dimensions");
  }
  TextPrepareOptions readOnly;
  readOnly.allowGlyphAppend = false;
  const auto warm = system.prepareText(atlas, request, readOnly);
  for (const auto *prepared : {&cold, &warm}) {
    check(prepared->glyphSetRevision == atlas.glyphSetRevision() &&
              prepared->atlasTextureRevision == atlas.atlasTextureRevision(),
          "dynamic revisions do not match");
    check(prepared->drawRun.atlases.empty(),
          "high-level result copied atlas pages");
    check(prepared->layout.lineOriginsY == layout.lineOriginsY &&
              prepared->layout.lineAdvance == layout.lineAdvance &&
              sameRect(prepared->layout.boundingBox, layout.boundingBox),
          "paragraph geometry differs");
    comparePlacement(prepared->layout.glyphs, layout.glyphs, false);
    comparePlacement(prepared->drawRun.glyphs, draw.glyphs, true);
    check(prepared->layout.lines.size() == layout.lines.size(),
          "wrapped line count differs");
    for (std::size_t i = 0; i < layout.lines.size(); ++i) {
      comparePlacement(prepared->layout.lines[i].glyphs, layout.lines[i].glyphs,
                       false);
      comparePlacement(prepared->drawRun.lines[i].glyphs, draw.lines[i].glyphs,
                       true);
    }
    check(prepared->compatibility.requiresColorGlyph &&
              prepared->submission.compatibility.requiresColorGlyph,
          "color compatibility changed after omitting pixels");
  }
  check(warm.atlasUpdate.updatedEntries.empty() && warm.atlasUploadPlan.empty(),
        "warm prepare generated upload work");
  check(!draw.atlases.empty() &&
            draw.atlases.front().pages.size() == atlas.pages().size(),
        "owning low-level snapshot missing");
  for (std::size_t i = 0; i < atlas.pages().size(); ++i) {
    check(draw.atlases.front().pages[i].image.rgba ==
              atlas.pages()[i].image.rgba,
          "dynamic atlas texels differ");
  }

  TextAtlasBakeRequest bakeRequest;
  bakeRequest.fontChainId = id;
  bakeRequest.pixelSize = request.pixelSize;
  bakeRequest.seedTexts = {request.text};
  const auto baked = system.bakeTextAtlas(bakeRequest);
  const auto bakedText = system.layoutTextParagraph(baked, request);
  check(bakedText.bakedAtlasRevision == baked.revision() &&
            bakedText.drawRun.atlases.empty() &&
            bakedText.missingGlyphs.empty(),
        "baked paragraph does not reference the baked atlas");
  comparePlacement(bakedText.layout.glyphs, layout.glyphs, false);
  comparePlacement(bakedText.drawRun.glyphs, draw.glyphs, false);
  for (std::size_t i = 0; i < layout.lines.size(); ++i) {
    comparePlacement(bakedText.layout.lines[i].glyphs, layout.lines[i].glyphs,
                     false);
    comparePlacement(bakedText.drawRun.lines[i].glyphs, draw.lines[i].glyphs,
                     false);
  }
  request.text = "A🍣";
  referenceRequest.text = request.text;
  const auto line = system.prepareLine(atlas, request, readOnly);
  const auto bakedLine = system.layoutTextLine(baked, request);
  const auto referenceLine = renderer.layoutLine(referenceRequest);
  check(line.glyphSetRevision == warm.glyphSetRevision &&
            bakedLine.bakedAtlasRevision == baked.revision(),
        "line revision does not match");
  for (const auto *prepared : {&line, &bakedLine}) {
    comparePlacement(prepared->layout.glyphs, referenceLine.glyphs, false);
    comparePlacement(prepared->drawRun.glyphs, referenceLine.glyphs, false);
    check(prepared->drawRun.glyphs.front().atlasPage >= 0,
          "line does not reference a page");
  }

  request.text = "Z";
  const auto missing = system.prepareLine(atlas, request, readOnly);
  check(missing.missingGlyphs == std::vector<std::string>{"Z"} &&
            missing.caretBoundaries.size() == 2 &&
            missing.submission.glyphs.size() == 1 &&
            !missing.submission.glyphs.front().visible,
        "read-only miss lost caret geometry or was submitted as visible");

  referenceRequest.text = "Z";
  const auto directAppend =
      atlas.cache().ensureGlyphs(renderer.layoutLine(referenceRequest));
  check(!directAppend.updatedEntries.empty() &&
            atlas.glyphSetRevision() > warm.glyphSetRevision &&
            atlas.atlasTextureRevision() > warm.atlasTextureRevision,
        "direct cache append bypassed revisions");
  const auto beforeCompact = system.prepareLine(atlas, request, readOnly);
  (void)atlas.cache().compact();
  check(atlas.glyphSetRevision() == beforeCompact.glyphSetRevision &&
            atlas.atlasTextureRevision() > beforeCompact.atlasTextureRevision,
        "direct cache compaction failed to invalidate old UVs");
  const auto beforeEvict = system.prepareLine(atlas, request, readOnly);
  (void)atlas.cache().evict({beforeEvict.drawRun.glyphs.front().handle});
  check(atlas.glyphSetRevision() > beforeEvict.glyphSetRevision &&
            atlas.atlasTextureRevision() > beforeEvict.atlasTextureRevision &&
            !system.prepareLine(atlas, request, readOnly).missingGlyphs.empty(),
        "direct cache eviction did not invalidate the prepared result");
}

void test_composed_cluster_identity() {
  TextSystem system;
  FontFallbackChain chain;
  chain.textFonts = {TEXT_FONT};
  const FontChainId font = system.registerFontFallbackChain(chain);
  TextAtlas atlas(makeAtlasOptions(font));
  const std::string text = "A A\u200DB";
  const auto plan = system.buildTextAtlasAppendPlan(atlas, text);
  check(plan.candidates.size() == 3 &&
            plan.candidates.front().glyphId == plan.candidates.back().glyphId &&
            plan.candidates.front().handle.value !=
                plan.candidates.back().handle.value,
        "a composed cluster needs its own atlas entry");
  const auto inserted = system.prepareLine(atlas, text);
  check(inserted.atlasUpdate.updatedEntries.size() == 3,
        "append plan and prepared cluster handles disagree");
  const auto probe = system.probeText(atlas, text);
  check(probe.allGlyphsAvailable && probe.resolvedGlyphCount == 3,
        "probing a registered composition reported an atlas miss");
  const auto reused = system.prepareLine(atlas, text);
  check(reused.atlasUpdate.updatedEntries.empty() &&
            reused.layout.glyphs.back().glyph.image.width >
                reused.layout.glyphs.front().glyph.image.width,
        "reuse substituted the first component image for its composition");
}

void test_wrapped_diagnostic_distribution() {
  DokuTextContext context;
  DokuText renderer(context);
  TextLayoutContext options;
  options.fallbackChain.textFonts = {TEXT_FONT};
  options.fallbackChain.emojiFonts = {EMOJI_FONT};
  options.paragraphOptions.wrappingWidth = 1;
  const auto shape = renderer.shapeText(options, "A"
                                                 "\xF4\x8F\xBF\xBF"
                                                 "AB");
  check(shape.lines.size() == 3 && shape.fallbackTrace.size() == 3,
        "wrapped line or fallback trace count changed");
  check(shape.fallbackTrace[1].lineIndex == 1 &&
            shape.fallbackTrace[1].clusterIndex == 0 &&
            shape.lines[1].clusters.front().beginByteOffset == 5,
        "wrapping lost line-local trace indices or physical-line byte offsets");
  check(shape.lines[0].diagnostics.size() == 1 &&
            shape.lines[0].diagnostics.front().code ==
                DiagnosticCode::MissingGlyphCluster &&
            shape.lines[0].diagnostics.front().clusterIndex == 1,
        "wrapping lost or misplaced a missing cluster diagnostic");
}

} // namespace

int main() {
  doku_test::skipIfTestFontsMissing();

  test_prepareLine_false_empty_atlas_string_view();
  test_prepareText_false_empty_atlas_string_view();
  test_prepareLine_false_empty_atlas_text_request();
  test_prepareText_false_empty_atlas_text_request();
  test_prepareLine_true_empty_atlas();
  test_default_options_preserve_behavior();
  test_append_plan_from_non_mutating_layout();
  test_append_plan_budget_gate();
  test_append_plan_color_glyph_candidates();
  test_revisions_change_only_on_mutation();
  test_seeded_atlas_false_string_view();
  test_seeded_atlas_false_text_request();
  test_emoji_false_string_view();
  test_emoji_false_text_request();
  test_lightweight_preparation();
  test_composed_cluster_identity();
  test_wrapped_diagnostic_distribution();

  std::cout << "ALL TextPrepareAllowGlyphAppendTest PASSED" << std::endl;
  return 0;
}
