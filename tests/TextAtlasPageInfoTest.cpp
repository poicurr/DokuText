#include <DokuText/DokuText.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <stdexcept>
#include <algorithm>

using namespace doku::text;

void test_page_info_alpha_mask() {
    std::cout << "[test] AlphaMask page info\n";

    TextSystem system;
    FontFallbackChain chain;
    chain.textFonts = {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"};
    const FontChainId fontChainId = system.registerFontFallbackChain(chain);

    TextAtlasOptions options;
    options.fontChainId = fontChainId;
    options.pixelSize = 32;
    TextAtlas atlas = system.createAtlas(options);

    TextSeedRequest seedRequest;
    seedRequest.texts = {"ASCII Text"};
    system.seedAtlas(atlas, seedRequest);

    assert(!atlas.pages().empty());

    for (std::size_t i = 0; i < atlas.pages().size(); ++i) {
        TextAtlasPageInfo info = atlas.pageInfo(i);
        assert(info.pageIndex == i);
        assert(info.page.value == atlas.pages()[i].handle.value);
        assert(info.materialKind == MaterialKind::AlphaMask);
        assert(info.pixelFormat == AtlasPixelFormat::AlphaMaskStraightRgba);
        assert(info.samplerKind == SamplerKind::LinearClamp);

        // Test handle-based query
        TextAtlasPageInfo infoByHandle = atlas.pageInfo(info.page);
        assert(infoByHandle.pageIndex == info.pageIndex);
        assert(infoByHandle.materialKind == info.materialKind);
    }
}

void test_page_info_color_bitmap() {
    std::cout << "[test] ColorBitmap page info\n";

    TextSystem system;
    FontFallbackChain chain;
    chain.emojiFonts = {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"};
    const FontChainId fontChainId = system.registerFontFallbackChain(chain);

    TextAtlasOptions options;
    options.fontChainId = fontChainId;
    options.pixelSize = 64;
    TextAtlas atlas = system.createAtlas(options);

    TextSeedRequest seedRequest;
    seedRequest.texts = {"🍣🍺"};
    system.seedAtlas(atlas, seedRequest);

    assert(!atlas.pages().empty());

    bool foundColorBitmap = false;
    for (std::size_t i = 0; i < atlas.pages().size(); ++i) {
        TextAtlasPageInfo info = atlas.pageInfo(i);
        if (info.materialKind == MaterialKind::ColorBitmap) {
            foundColorBitmap = true;
            assert(info.pixelFormat == AtlasPixelFormat::ColorStraightRgba);
        }
    }
    assert(foundColorBitmap);
}

void test_no_mixed_material_page() {
    std::cout << "[test] No mixed material page\n";

    TextSystem system;
    FontFallbackChain chain;
    chain.textFonts = {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"};
    chain.emojiFonts = {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"};
    const FontChainId fontChainId = system.registerFontFallbackChain(chain);

    TextAtlasOptions options;
    options.fontChainId = fontChainId;
    options.pixelSize = 64;
    TextAtlas atlas = system.createAtlas(options);

    TextSeedRequest seedRequest;
    seedRequest.texts = {"Hello 🍣"};
    system.seedAtlas(atlas, seedRequest);

    auto entries = atlas.entries();
    assert(!entries.empty());

    for (std::size_t i = 0; i < atlas.pages().size(); ++i) {
        MaterialKind pageMaterial = atlas.materialKindForPage(i);

        // Verify all entries in this page match the page material
        for (const auto& entry : entries) {
            if (static_cast<std::size_t>(entry.pageIndex) == i) {
                assert(entry.materialKind == pageMaterial);
            }
        }
    }
}

void test_invalid_queries() {
    std::cout << "[test] Invalid queries (out of range)\n";

    TextSystem system;
    TextAtlasOptions options;
    TextAtlas atlas = system.createAtlas(options);

    try {
        atlas.pageInfo(0);
        assert(false && "Should have thrown out_of_range");
    } catch (const std::out_of_range&) {
        // Expected
    }

    try {
        atlas.pageInfo(GlyphAtlasPageHandle{999});
        assert(false && "Should have thrown out_of_range");
    } catch (const std::out_of_range&) {
        // Expected
    }
}

void test_const_query_non_mutation() {
    std::cout << "[test] Const query non-mutation\n";

    TextSystem system;
    FontFallbackChain chain;
    chain.textFonts = {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"};
    const FontChainId fontChainId = system.registerFontFallbackChain(chain);

    TextAtlasOptions options;
    options.fontChainId = fontChainId;
    TextAtlas atlas = system.createAtlas(options);

    TextSeedRequest seedRequest;
    seedRequest.texts = {"A"};
    system.seedAtlas(atlas, seedRequest);

    std::size_t pageCountBefore = atlas.pages().size();
    std::size_t entryCountBefore = atlas.entries().size();

    // Perform queries
    (void)atlas.pageInfo(0);
    (void)atlas.materialKindForPage(0);
    (void)atlas.pixelFormatForPage(0);
    (void)atlas.samplerKindForPage(0);

    assert(atlas.pages().size() == pageCountBefore);
    assert(atlas.entries().size() == entryCountBefore);
}

void test_upload_plan_mapping() {
    std::cout << "[test] Upload plan mapping\n";

    TextSystem system;
    FontFallbackChain chain;
    chain.textFonts = {"fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf"};
    chain.emojiFonts = {"fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf"};
    const FontChainId fontChainId = system.registerFontFallbackChain(chain);

    TextAtlasOptions options;
    options.fontChainId = fontChainId;
    TextAtlas atlas = system.createAtlas(options);

    TextSeedRequest seedRequest;
    seedRequest.texts = {"A 🍣"};
    TextAtlasSeedResult result = system.seedAtlas(atlas, seedRequest);

    assert(!result.atlasUploadPlan.empty());

    for (const auto& plan : result.atlasUploadPlan) {
        TextAtlasPageInfo info = atlas.pageInfo(plan.pageHandle);
        assert(info.pageIndex == static_cast<std::uint32_t>(plan.pageIndex));

        if (info.materialKind == MaterialKind::AlphaMask) {
            assert(info.pixelFormat == AtlasPixelFormat::AlphaMaskStraightRgba);
        } else {
            assert(info.materialKind == MaterialKind::ColorBitmap);
            assert(info.pixelFormat == AtlasPixelFormat::ColorStraightRgba);
        }
        assert(info.samplerKind == SamplerKind::LinearClamp);
    }
}

int main() {
    try {
        test_page_info_alpha_mask();
        test_page_info_color_bitmap();
        test_no_mixed_material_page();
        test_invalid_queries();
        test_const_query_non_mutation();
        test_upload_plan_mapping();

        std::cout << "ALL PASSED\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAILED with exception: " << ex.what() << "\n";
        return 1;
    }
}
