#include <DokuText/DokuText.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cassert>

int main() {
    std::cout << "[test] DynamicAtlasSeedIdempotencyTest\n";

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

    doku::text::TextSeedRequest seedRequest;
    seedRequest.texts = {"Hello"};

    // First call
    std::cout << "First seedAtlas call...\n";
    doku::text::TextAtlasSeedResult result1 = system.seedAtlas(atlas, seedRequest);

    std::cout << "  insertedGlyphCount: " << result1.insertedGlyphCount << "\n";
    std::cout << "  alreadyPresentGlyphCount: " << result1.alreadyPresentGlyphCount << "\n";
    std::cout << "  atlasChanged: " << (result1.atlasChanged ? "true" : "false") << "\n";

    if (result1.insertedGlyphCount == 0) {
        std::cerr << "FAILED: Expected insertedGlyphCount > 0 on first call\n";
        return 1;
    }
    if (!result1.atlasChanged) {
        std::cerr << "FAILED: Expected atlasChanged == true on first call\n";
        return 1;
    }
    if (result1.atlasUploadPlan.empty()) {
        std::cerr << "FAILED: Expected atlasUploadPlan to be non-empty on first call\n";
        return 1;
    }

    const std::size_t pageCountAfterFirst = atlas.pages().size();

    // Second call with same request
    std::cout << "Second seedAtlas call (identical)...\n";
    doku::text::TextAtlasSeedResult result2 = system.seedAtlas(atlas, seedRequest);

    std::cout << "  insertedGlyphCount: " << result2.insertedGlyphCount << "\n";
    std::cout << "  alreadyPresentGlyphCount: " << result2.alreadyPresentGlyphCount << "\n";
    std::cout << "  atlasChanged: " << (result2.atlasChanged ? "true" : "false") << "\n";

    if (result2.insertedGlyphCount != 0) {
        std::cerr << "FAILED: Expected insertedGlyphCount == 0 on second call\n";
        return 1;
    }
    if (result2.alreadyPresentGlyphCount == 0) {
        std::cerr << "FAILED: Expected alreadyPresentGlyphCount > 0 on second call\n";
        return 1;
    }
    if (result2.atlasChanged) {
        std::cerr << "FAILED: Expected atlasChanged == false on second call\n";
        return 1;
    }
    if (!result2.atlasUploadPlan.empty()) {
        std::cerr << "FAILED: Expected atlasUploadPlan to be empty on second call\n";
        return 1;
    }
    if (!result2.atlasUpdate.newPages.empty()) {
        std::cerr << "FAILED: Expected newPages to be empty on second call\n";
        return 1;
    }
    if (!result2.atlasUpdate.updatedEntries.empty()) {
        std::cerr << "FAILED: Expected updatedEntries to be empty on second call\n";
        return 1;
    }
    if (!result2.atlasUpdate.dirtyRects.empty()) {
        std::cerr << "FAILED: Expected dirtyRects to be empty on second call\n";
        return 1;
    }

    if (atlas.pages().size() != pageCountAfterFirst) {
        std::cerr << "FAILED: Page count changed on second call\n";
        return 1;
    }

    std::cout << "PASSED\n";
    return 0;
}
