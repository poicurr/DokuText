#include "adapter/render/ColrV1Renderer.hpp"

#include <ft2build.h>
#include FT_COLOR_H
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

namespace {
struct FreeTypeLibrary {
  FT_Library library = nullptr;

  FreeTypeLibrary() {
    if (FT_Init_FreeType(&library) != 0) {
      throw std::runtime_error("FT_Init_FreeType failed");
    }
  }

  ~FreeTypeLibrary() {
    if (library != nullptr) {
      FT_Done_FreeType(library);
    }
  }
};

struct FreeTypeFace {
  FT_Face face = nullptr;

  FreeTypeFace(FT_Library library, const char *path) {
    if (FT_New_Face(library, path, 0, &face) != 0) {
      throw std::runtime_error("FT_New_Face failed");
    }
  }

  ~FreeTypeFace() {
    if (face != nullptr) {
      FT_Done_Face(face);
    }
  }
};

[[nodiscard]] ColorPalette loadDefaultPalette(FT_Face face) {
  ColorPalette palette;
  FT_Palette_Data paletteData{};
  if (FT_Palette_Data_Get(face, &paletteData) != 0 ||
      paletteData.num_palette_entries == 0) {
    return palette;
  }

  FT_Color *colors = nullptr;
  if (FT_Palette_Select(face, 0, &colors) != 0 || colors == nullptr) {
    return palette;
  }

  palette.m_colors.reserve(paletteData.num_palette_entries);
  for (FT_UShort i = 0; i < paletteData.num_palette_entries; ++i) {
    const FT_Color &color = colors[i];
    palette.m_colors.push_back(
        {color.red, color.green, color.blue, color.alpha});
  }
  return palette;
}

[[nodiscard]] std::uint32_t makeRgbKey(const std::vector<std::uint8_t> &rgba,
                                       std::size_t index) {
  return (static_cast<std::uint32_t>(rgba[index + 0]) << 16U) |
         (static_cast<std::uint32_t>(rgba[index + 1]) << 8U) |
         static_cast<std::uint32_t>(rgba[index + 2]);
}

[[nodiscard]] std::size_t countOpaqueBlackPixelsInRegion(const Bitmap &image,
                                                         std::uint32_t minX,
                                                         std::uint32_t minY,
                                                         std::uint32_t maxX,
                                                         std::uint32_t maxY) {
  const auto &rgba = image.rgba();
  const std::uint32_t width = static_cast<std::uint32_t>(image.width());
  const std::uint32_t height = static_cast<std::uint32_t>(image.height());
  const std::uint32_t clippedMaxX = std::min(maxX, width);
  const std::uint32_t clippedMaxY = std::min(maxY, height);
  std::size_t blackPixels = 0;
  for (std::uint32_t y = minY; y < clippedMaxY; ++y) {
    for (std::uint32_t x = minX; x < clippedMaxX; ++x) {
      const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4U;
      if (rgba[index + 3] >= 240U && rgba[index + 0] <= 32U &&
          rgba[index + 1] <= 32U && rgba[index + 2] <= 32U) {
        ++blackPixels;
      }
    }
  }
  return blackPixels;
}
} // namespace

int main() {
  constexpr auto FONT_PATH =
      "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf";
  constexpr std::uint32_t GRADIENT_HEAVY_GLYPH_ID = 53;
  constexpr std::uint32_t FAMILY_ZWJ_GLYPH_ID = 360;
  constexpr std::uint32_t NESTED_COMPOSITE_GLYPH_ID = 1790;
  constexpr std::uint32_t HIGH_PAINT_COUNT_GLYPH_ID = 1852;

  FreeTypeLibrary freetype;
  FreeTypeFace font(freetype.library, FONT_PATH);
  if (FT_Set_Pixel_Sizes(font.face, 0, 128) != 0) {
    throw std::runtime_error("FT_Set_Pixel_Sizes failed");
  }

  ColrV1RenderStats stats;
  const GlyphSurface surface =
      renderColrV1Glyph(font.face, GRADIENT_HEAVY_GLYPH_ID,
                        loadDefaultPalette(font.face), {}, &stats);

  const auto &rgba = surface.m_image.rgba();
  std::unordered_set<std::uint32_t> opaqueRgbColors;
  std::size_t opaquePixels = 0;
  for (std::size_t index = 0; index + 3 < rgba.size(); index += 4U) {
    if (rgba[index + 3] < 240U) {
      continue;
    }
    ++opaquePixels;
    opaqueRgbColors.insert(makeRgbKey(rgba, index));
  }

  std::cout << "[test] COLRv1 gradient render glyph=" << GRADIENT_HEAVY_GLYPH_ID
            << " size=" << surface.m_image.width() << "x"
            << surface.m_image.height() << " gradients=" << stats.m_gradients
            << " opaquePixels=" << opaquePixels
            << " opaqueRgbColors=" << opaqueRgbColors.size() << '\n';

  if (stats.m_gradients == 0) {
    std::cerr << "expected a COLRv1 glyph with gradients\n";
    return 1;
  }
  if (opaquePixels < 1000) {
    std::cerr << "expected a substantial rendered glyph body\n";
    return 1;
  }
  if (opaqueRgbColors.size() < 64) {
    std::cerr << "expected gradients to produce varied opaque RGB colors\n";
    return 1;
  }

  ColrV1RenderStats familyStats;
  const GlyphSurface familySurface =
      renderColrV1Glyph(font.face, FAMILY_ZWJ_GLYPH_ID,
                        loadDefaultPalette(font.face), {}, &familyStats);
  const std::size_t topLeftHeadPixels =
      countOpaqueBlackPixelsInRegion(familySurface.m_image, 24, 24, 64, 64);
  std::cout << "[test] COLRv1 family ZWJ glyph=" << FAMILY_ZWJ_GLYPH_ID
            << " size=" << familySurface.m_image.width() << "x"
            << familySurface.m_image.height()
            << " transforms=" << familyStats.m_transformPushes
            << " topLeftHeadPixels=" << topLeftHeadPixels << '\n';
  if (familyStats.m_transformPushes < 8 || topLeftHeadPixels < 300) {
    std::cerr << "expected family ZWJ transformed components to render\n";
    return 1;
  }

  ColrV1RenderStats compositeStats;
  const GlyphSurface compositeSurface =
      renderColrV1Glyph(font.face, NESTED_COMPOSITE_GLYPH_ID,
                        loadDefaultPalette(font.face), {}, &compositeStats);
  std::cout << "[test] COLRv1 nested composite glyph="
            << NESTED_COMPOSITE_GLYPH_ID
            << " size=" << compositeSurface.m_image.width() << "x"
            << compositeSurface.m_image.height()
            << " composites=" << compositeStats.m_composites
            << " groupPushes=" << compositeStats.m_groupPushes << '\n';
  if (compositeStats.m_composites == 0 || compositeStats.m_groupPushes < 2) {
    std::cerr << "expected nested COLRv1 composite groups\n";
    return 1;
  }

  ColrV1RenderStats stressStats;
  const GlyphSurface stressSurface =
      renderColrV1Glyph(font.face, HIGH_PAINT_COUNT_GLYPH_ID,
                        loadDefaultPalette(font.face), {}, &stressStats);
  std::cout << "[test] COLRv1 high paint count glyph="
            << HIGH_PAINT_COUNT_GLYPH_ID
            << " size=" << stressSurface.m_image.width() << "x"
            << stressSurface.m_image.height()
            << " paints=" << stressStats.m_totalPaints
            << " composites=" << stressStats.m_composites
            << " transforms=" << stressStats.m_transformPushes
            << " groupPushes=" << stressStats.m_groupPushes << '\n';
  if (stressStats.m_totalPaints < 6000 || stressStats.m_composites < 4 ||
      stressStats.m_transformPushes < 64 || stressStats.m_groupPushes < 8) {
    std::cerr << "expected a high paint count COLRv1 stress glyph\n";
    return 1;
  }
  return 0;
}
