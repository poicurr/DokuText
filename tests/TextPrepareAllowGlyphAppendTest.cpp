#include <DokuText/DokuText.hpp>
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace doku::text;

void test_prepareLine_false_empty_atlas() {
    std::cout << "Running test_prepareLine_false_empty_atlas..." << std::endl;
    TextSystem system;
    TextAtlasOptions atlasOptions;
    atlasOptions.pixelSize = 32;
    // Note: font paths are from build context memory
    atlasOptions.fontChainId = system.registerFontFallbackChain({
        {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"},
        {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"},
        {}
    });

    TextAtlas atlas(atlasOptions);
    assert(atlas.pages().size() == 0);
    assert(atlas.entries().size() == 0);

    TextPrepareOptions options;
    options.allowGlyphAppend = false;

    PreparedTextLine prepared = system.prepareLine(atlas, "Hello", options);

    if (atlas.pages().size() != 0 || atlas.entries().size() != 0) {
        std::cerr << "FAILED at after prepareLine allowGlyphAppend=false: pages "
                  << atlas.pages().size() << ", entries " << atlas.entries().size() << std::endl;
        exit(1);
    }

    TextProbeResult probe = system.probeText(atlas, "Hello");
    if (probe.allGlyphsAvailable) {
        std::cerr << "FAILED: probe should report missing glyphs on empty atlas" << std::endl;
        exit(1);
    }
    std::cout << "test_prepareLine_false_empty_atlas passed." << std::endl;
}

void test_prepareText_false_empty_atlas() {
    std::cout << "Running test_prepareText_false_empty_atlas..." << std::endl;
    TextSystem system;
    TextAtlasOptions atlasOptions;
    atlasOptions.pixelSize = 32;
    atlasOptions.fontChainId = system.registerFontFallbackChain({
        {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"},
        {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"},
        {}
    });

    TextAtlas atlas(atlasOptions);
    TextPrepareOptions options;
    options.allowGlyphAppend = false;

    PreparedTextParagraph prepared = system.prepareText(atlas, "Multi\nLine", options);

    if (atlas.pages().size() != 0 || atlas.entries().size() != 0) {
        std::cerr << "FAILED at after prepareText allowGlyphAppend=false: pages "
                  << atlas.pages().size() << ", entries " << atlas.entries().size() << std::endl;
        exit(1);
    }
    std::cout << "test_prepareText_false_empty_atlas passed." << std::endl;
}

void test_prepareLine_true_empty_atlas() {
    std::cout << "Running test_prepareLine_true_empty_atlas..." << std::endl;
    TextSystem system;
    TextAtlasOptions atlasOptions;
    atlasOptions.pixelSize = 32;
    atlasOptions.fontChainId = system.registerFontFallbackChain({
        {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"},
        {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"},
        {}
    });

    TextAtlas atlas(atlasOptions);
    TextPrepareOptions options;
    options.allowGlyphAppend = true;

    PreparedTextLine prepared = system.prepareLine(atlas, "Hello", options);

    if (atlas.entries().size() == 0) {
        std::cerr << "FAILED: allowGlyphAppend=true should append glyphs" << std::endl;
        exit(1);
    }
    if (atlas.pages().size() == 0) {
        std::cerr << "FAILED: allowGlyphAppend=true should create pages" << std::endl;
        exit(1);
    }

    TextProbeResult probe = system.probeText(atlas, "Hello");
    if (!probe.allGlyphsAvailable) {
        std::cerr << "FAILED: probe should report allGlyphsAvailable after mutating prepare" << std::endl;
        exit(1);
    }
    std::cout << "test_prepareLine_true_empty_atlas passed." << std::endl;
}

void test_default_options_preserve_behavior() {
    std::cout << "Running test_default_options_preserve_behavior..." << std::endl;
    TextSystem system;
    TextAtlasOptions atlasOptions;
    atlasOptions.pixelSize = 32;
    atlasOptions.fontChainId = system.registerFontFallbackChain({
        {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"},
        {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"},
        {}
    });

    TextAtlas atlas(atlasOptions);
    // Call without options
    system.prepareLine(atlas, "Default");

    if (atlas.entries().size() == 0) {
        std::cerr << "FAILED: default options should allow append" << std::endl;
        exit(1);
    }
    std::cout << "test_default_options_preserve_behavior passed." << std::endl;
}

void test_seeded_atlas_false() {
    std::cout << "Running test_seeded_atlas_false..." << std::endl;
    TextSystem system;
    TextAtlasOptions atlasOptions;
    atlasOptions.pixelSize = 32;
    atlasOptions.fontChainId = system.registerFontFallbackChain({
        {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"},
        {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"},
        {}
    });

    TextAtlas atlas(atlasOptions);
    TextSeedRequest seed;
    seed.texts = {"Hello"};
    system.seedAtlas(atlas, seed);

    std::size_t entriesBefore = atlas.entries().size();
    std::size_t pagesBefore = atlas.pages().size();

    TextPrepareOptions options;
    options.allowGlyphAppend = false;
    system.prepareLine(atlas, "Hello", options);

    if (atlas.entries().size() != entriesBefore || atlas.pages().size() != pagesBefore) {
        std::cerr << "FAILED: seeded atlas mutated with allowGlyphAppend=false" << std::endl;
        exit(1);
    }
    std::cout << "test_seeded_atlas_false passed." << std::endl;
}

void test_emoji_false() {
    std::cout << "Running test_emoji_false..." << std::endl;
    TextSystem system;
    TextAtlasOptions atlasOptions;
    atlasOptions.pixelSize = 32;
    atlasOptions.fontChainId = system.registerFontFallbackChain({
        {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"},
        {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"},
        {}
    });

    TextAtlas atlas(atlasOptions);
    TextPrepareOptions options;
    options.allowGlyphAppend = false;

    // Sushi emoji
    system.prepareLine(atlas, "🍣", options);

    if (atlas.pages().size() != 0 || atlas.entries().size() != 0) {
        std::cerr << "FAILED: emoji prepareLine allowGlyphAppend=false mutated atlas" << std::endl;
        exit(1);
    }
    std::cout << "test_emoji_false passed." << std::endl;
}

int main() {
    try {
        test_prepareLine_false_empty_atlas();
        test_prepareText_false_empty_atlas();
        test_prepareLine_true_empty_atlas();
        test_default_options_preserve_behavior();
        test_seeded_atlas_false();
        test_emoji_false();
        std::cout << "ALL TextPrepareAllowGlyphAppendTest PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
