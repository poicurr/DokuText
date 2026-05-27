#include <DokuText/DokuText.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <algorithm>

#include "TestUtils.hpp"

namespace {

void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(1);
    }
}

void assert_eq(std::size_t actual, std::size_t expected, const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAILED: " << message << " (expected " << expected << ", actual " << actual << ")" << std::endl;
        std::exit(1);
    }
}

} // namespace

int main() {
    doku_test::skipIfTestFontsMissing();

    std::cout << "[test] TextProbeTest" << std::endl;

    doku::text::TextSystem system;
    doku::text::FontFallbackChain chain;
    chain.textFonts = {
        "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
    };
    chain.emojiFonts = {
        "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
    };
    const doku::text::FontChainId fontChainId = system.registerFontFallbackChain(chain);

    doku::text::TextAtlasOptions options;
    options.fontChainId = fontChainId;
    options.pixelSize = 64;
    doku::text::TextAtlas atlas = system.createAtlas(options);

    // 1. Empty atlas + ASCII text
    {
        std::cout << "Test 1: Empty atlas + ASCII text" << std::endl;
        std::size_t pageCountBefore = atlas.pages().size();
        std::size_t entryCountBefore = atlas.entries().size();

        doku::text::TextProbeResult result = system.probeText(atlas, "Hello");

        assert_true(result.missingGlyphCount > 0, "Expected missing glyphs in empty atlas");
        assert_true(result.wouldAppendGlyphs, "Expected wouldAppendGlyphs to be true");
        assert_true(!result.allGlyphsAvailable, "Expected allGlyphsAvailable to be false");
        assert_eq(result.alreadyPresentGlyphCount, 0, "Expected 0 already present glyphs");
        assert_eq(result.requestedCodepointCount, 5, "Expected 5 requested codepoints");
        assert_eq(result.resolvedGlyphCount, 5, "Expected 5 resolved glyphs");
        assert_eq(result.missingGlyphs.size(), result.missingGlyphCount, "missingGlyphs vector size mismatch");

        // Non-mutation check
        assert_eq(atlas.pages().size(), pageCountBefore, "Page count changed after probeText");
        assert_eq(atlas.entries().size(), entryCountBefore, "Entry count changed after probeText");
    }

    // 2. seedAtlas("Hello") then probeText("Hello")
    {
        std::cout << "Test 2: seedAtlas(\"Hello\") then probeText(\"Hello\")" << std::endl;
        doku::text::TextSeedRequest seedRequest;
        seedRequest.texts = {"Hello"};
        system.seedAtlas(atlas, seedRequest);

        std::size_t pageCountBefore = atlas.pages().size();
        std::size_t entryCountBefore = atlas.entries().size();

        doku::text::TextProbeResult result = system.probeText(atlas, "Hello");

        assert_eq(result.missingGlyphCount, 0, "Expected 0 missing glyphs after seeding");
        assert_true(result.allGlyphsAvailable, "Expected allGlyphsAvailable to be true");
        assert_true(!result.wouldAppendGlyphs, "Expected wouldAppendGlyphs to be false");
        assert_true(result.alreadyPresentGlyphCount > 0, "Expected already present glyphs");
        assert_eq(result.missingGlyphs.size(), 0, "Expected empty missingGlyphs vector");

        // Non-mutation check
        assert_eq(atlas.pages().size(), pageCountBefore, "Page count changed after probeText");
        assert_eq(atlas.entries().size(), entryCountBefore, "Entry count changed after probeText");
    }

    // 3. Partial atlas
    {
        std::cout << "Test 3: Partial atlas" << std::endl;
        // Current atlas has "Hello". Probing "Hello World".
        std::size_t pageCountBefore = atlas.pages().size();
        std::size_t entryCountBefore = atlas.entries().size();

        doku::text::TextProbeResult result = system.probeText(atlas, "Hello World");

        assert_true(result.alreadyPresentGlyphCount > 0, "Expected some present glyphs");
        assert_true(result.missingGlyphCount > 0, "Expected some missing glyphs ('W', 'o', 'r', 'l', 'd', ' ')");
        assert_true(!result.allGlyphsAvailable, "Expected allGlyphsAvailable to be false");
        assert_true(result.wouldAppendGlyphs, "Expected wouldAppendGlyphs to be true");

        // Non-mutation check
        assert_eq(atlas.pages().size(), pageCountBefore, "Page count changed after probeText");
        assert_eq(atlas.entries().size(), entryCountBefore, "Entry count changed after probeText");
    }

    // 4. Emoji / ColorBitmap glyph
    {
        std::cout << "Test 4: Emoji / ColorBitmap glyph" << std::endl;
        doku::text::TextProbeResult result1 = system.probeText(atlas, "🍣");

        assert_true(result1.missingGlyphCount > 0, "Expected sushi emoji to be missing");
        bool hasColor = false;
        for (const auto& mg : result1.missingGlyphs) {
            if (mg.isColorGlyph || mg.materialKind == doku::text::MaterialKind::ColorBitmap) {
                hasColor = true;
                break;
            }
        }
        assert_true(hasColor, "Expected missing glyph to be detected as color glyph");

        doku::text::TextSeedRequest seedRequest;
        seedRequest.texts = {"🍣"};
        system.seedAtlas(atlas, seedRequest);

        doku::text::TextProbeResult result2 = system.probeText(atlas, "🍣");
        assert_eq(result2.missingGlyphCount, 0, "Expected sushi emoji to be present after seeding");
        assert_true(result2.allGlyphsAvailable, "Expected allGlyphsAvailable to be true for sushi");
    }

    std::cout << "PASSED" << std::endl;
    return 0;
}
