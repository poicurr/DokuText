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

  std::cout << "ALL TextPrepareAllowGlyphAppendTest PASSED" << std::endl;
  return 0;
}
