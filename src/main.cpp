#include "GlyphRenderer/GlyphRenderer.hpp"
#include "adapter/render/ColrV0Renderer.hpp"
#include "adapter/render/ColrV1Renderer.hpp"

#if GLYPH_RENDERER_ENABLE_PNG_IO
#include "adapter/io/PngWriter.hpp"
#endif

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_COLOR_H
#include FT_GLYPH_H

#include <hb-ft.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace {
struct FreeTypeLibraryDeleter {
  void operator()(FT_Library library) const {
    if (library != nullptr) {
      FT_Done_FreeType(library);
    }
  }
};

struct FreeTypeFaceDeleter {
  void operator()(FT_Face face) const {
    if (face != nullptr) {
      FT_Done_Face(face);
    }
  }
};

struct HbFaceDeleter {
  void operator()(hb_face_t *face) const {
    if (face != nullptr) {
      hb_face_destroy(face);
    }
  }
};

struct HbFontDeleter {
  void operator()(hb_font_t *font) const {
    if (font != nullptr) {
      hb_font_destroy(font);
    }
  }
};

struct HbBufferDeleter {
  void operator()(hb_buffer_t *buffer) const {
    if (buffer != nullptr) {
      hb_buffer_destroy(buffer);
    }
  }
};

using FreeTypeLibraryHandle =
    std::unique_ptr<std::remove_pointer_t<FT_Library>, FreeTypeLibraryDeleter>;
using FreeTypeFaceHandle =
    std::unique_ptr<std::remove_pointer_t<FT_Face>, FreeTypeFaceDeleter>;
using HbFaceHandle = std::unique_ptr<hb_face_t, HbFaceDeleter>;
using HbFontHandle = std::unique_ptr<hb_font_t, HbFontDeleter>;
using HbBufferHandle = std::unique_ptr<hb_buffer_t, HbBufferDeleter>;

struct FontContext {
  std::string m_path;
  FreeTypeFaceHandle m_face;
  HbFaceHandle m_hbFace;
  HbFontHandle m_hbFont;
};

struct RenderTarget {
  FontContext *m_font = nullptr;
  hb_codepoint_t m_glyphId = 0;
};

struct TextCluster {
  std::vector<std::uint32_t> m_codePoints;
};

struct ShapedGlyph {
  hb_codepoint_t m_glyphId = 0;
  int m_xAdvance = 0;
  int m_yAdvance = 0;
  int m_xOffset = 0;
  int m_yOffset = 0;
};

struct ClusterRenderResult {
  GlyphSurface m_surface;
  std::vector<ShapedGlyph> m_shapedGlyphs;
  ColrV1RenderStats m_colrV1Stats;
  std::size_t m_colrV1GlyphCount = 0;
};

struct GlyphCacheKey {
  hb_codepoint_t m_glyphId = 0;
  unsigned int m_paletteIndex = 0;

  bool operator==(const GlyphCacheKey &other) const {
    return m_glyphId == other.m_glyphId &&
           m_paletteIndex == other.m_paletteIndex;
  }
};

struct GlyphCacheKeyHasher {
  std::size_t operator()(const GlyphCacheKey &key) const {
    return (static_cast<std::size_t>(key.m_glyphId) << 1U) ^
           static_cast<std::size_t>(key.m_paletteIndex);
  }
};

struct CachedGlyphRender {
  GlyphSurface m_surface;
  ColrV1RenderStats m_colrV1Stats;
};

using GlyphRenderCache =
    std::unordered_map<GlyphCacheKey, CachedGlyphRender, GlyphCacheKeyHasher>;

struct ClusterCacheKey {
  std::vector<std::uint32_t> m_codePoints;
  unsigned int m_paletteIndex = 0;

  bool operator==(const ClusterCacheKey &other) const {
    return m_paletteIndex == other.m_paletteIndex &&
           m_codePoints == other.m_codePoints;
  }
};

struct ClusterCacheKeyHasher {
  std::size_t operator()(const ClusterCacheKey &key) const {
    std::size_t hash = static_cast<std::size_t>(key.m_paletteIndex);
    for (std::uint32_t codePoint : key.m_codePoints) {
      hash ^= static_cast<std::size_t>(codePoint) + 0x9e3779b9U + (hash << 6U) +
              (hash >> 2U);
    }
    return hash;
  }
};

using ClusterRenderCache =
    std::unordered_map<ClusterCacheKey, ClusterRenderResult,
                       ClusterCacheKeyHasher>;

struct CacheStats {
  std::size_t m_glyphHits = 0;
  std::size_t m_glyphMisses = 0;
  std::size_t m_clusterHits = 0;
  std::size_t m_clusterMisses = 0;
};

struct SessionColrV1Stats {
  ColrV1RenderStats m_renderedStats;
  ColrV1RenderStats m_logicalStats;
  std::size_t m_renderedGlyphCount = 0;
  std::size_t m_logicalGlyphCount = 0;
  std::size_t m_renderedClusterCount = 0;
  std::size_t m_logicalClusterCount = 0;
};

struct ColrV1ScanEntry {
  hb_codepoint_t m_glyphId = 0;
  ColrV1RenderStats m_stats;
  int m_width = 0;
  int m_height = 0;
  int m_advance = 0;
};

struct ColrV1ScanFilters {
  unsigned int m_minPaints = 0;
  unsigned int m_minGradients = 0;
  unsigned int m_minComposites = 0;
  unsigned int m_minTransforms = 0;
  bool m_summaryOnly = false;
};

void accumulateColrV1Stats(ColrV1RenderStats &dst,
                           const ColrV1RenderStats &src) {
  dst.m_totalPaints += src.m_totalPaints;
  dst.m_layerPaints += src.m_layerPaints;
  dst.m_glyphPaints += src.m_glyphPaints;
  dst.m_colrGlyphPaints += src.m_colrGlyphPaints;
  dst.m_composites += src.m_composites;
  dst.m_gradients += src.m_gradients;
  dst.m_clipPushes += src.m_clipPushes;
  dst.m_transformPushes += src.m_transformPushes;
  dst.m_groupPushes += src.m_groupPushes;
}

[[nodiscard]] unsigned int clampPaletteIndex(hb_face_t *hbFace,
                                             unsigned int paletteIndex) {
  const unsigned int paletteCount = hb_ot_color_palette_get_count(hbFace);
  if (paletteCount == 0U) {
    return 0U;
  }
  if (paletteIndex >= paletteCount) {
    std::cerr << "警告: パレットインデックス " << paletteIndex
              << " は範囲外のため 0 に切り替えます\n";
    return 0U;
  }
  return paletteIndex;
}

[[nodiscard]] ColorPalette loadPalette(FT_Face face, hb_face_t *hbFace,
                                       unsigned int paletteIndex) {
  ColorPalette palette;
  unsigned int colorCount = 0U;
  hb_ot_color_palette_get_colors(hbFace, paletteIndex, 0U, &colorCount,
                                 nullptr);
  if (colorCount > 0U) {
    std::vector<hb_color_t> hbColors(colorCount);
    hb_ot_color_palette_get_colors(hbFace, paletteIndex, 0U, &colorCount,
                                   hbColors.data());
    palette.m_colors.reserve(colorCount);
    for (const hb_color_t color : hbColors) {
      palette.m_colors.push_back(
          {static_cast<uint8_t>(hb_color_get_red(color)),
           static_cast<uint8_t>(hb_color_get_green(color)),
           static_cast<uint8_t>(hb_color_get_blue(color)),
           static_cast<uint8_t>(hb_color_get_alpha(color))});
    }
    const auto paletteFlags =
        hb_ot_color_palette_get_flags(hbFace, paletteIndex);
    if ((paletteFlags & HB_OT_COLOR_PALETTE_FLAG_USABLE_WITH_DARK_BACKGROUND) !=
        0U) {
      palette.m_foregroundColor = {255U, 255U, 255U, 255U};
    } else {
      palette.m_foregroundColor = {0U, 0U, 0U, 255U};
    }
    return palette;
  }

  if (face != nullptr) {
    FT_Palette_Data paletteData{};
    if (FT_Palette_Data_Get(face, &paletteData) == 0 &&
        paletteData.num_palette_entries > 0) {
      FT_Color *ftColors = nullptr;
      FT_UShort paletteCount = paletteData.num_palettes;
      FT_UShort selectedIndex = 0U;
      if (paletteCount > 0U) {
        selectedIndex = static_cast<FT_UShort>(std::min(
            paletteIndex, static_cast<unsigned int>(paletteCount - 1U)));
      }
      if (FT_Palette_Select(face, selectedIndex, &ftColors) == 0 &&
          ftColors != nullptr) {
        palette.m_colors.reserve(paletteData.num_palette_entries);
        for (FT_UShort i = 0; i < paletteData.num_palette_entries; ++i) {
          const FT_Color &entry = ftColors[i];
          palette.m_colors.push_back(
              {entry.red, entry.green, entry.blue, entry.alpha});
        }
        if (paletteData.palette_flags != nullptr &&
            selectedIndex < paletteCount &&
            (paletteData.palette_flags[selectedIndex] &
             FT_PALETTE_FOR_DARK_BACKGROUND) != 0) {
          palette.m_foregroundColor = {255U, 255U, 255U, 255U};
        } else {
          palette.m_foregroundColor = {0U, 0U, 0U, 255U};
        }
      }
    }
  }

  return palette;
}

[[nodiscard]] bool fileExists(const std::string &path) {
  std::error_code error;
  return std::filesystem::is_regular_file(std::filesystem::path(path), error);
}

[[nodiscard]] std::string
resolveExistingPath(std::initializer_list<std::string> candidates) {
  for (const std::string &candidate : candidates) {
    if (fileExists(candidate)) {
      return candidate;
    }
  }
  std::ostringstream stream;
  bool first = true;
  for (const std::string &candidate : candidates) {
    if (!first) {
      stream << ", ";
    }
    stream << candidate;
    first = false;
  }
  throw std::runtime_error("フォントが見つかりません: " + stream.str());
}

[[nodiscard]] std::vector<std::uint32_t> decodeUtf8(const std::string &text) {
  std::vector<std::uint32_t> codePoints;
  const auto *bytes = reinterpret_cast<const unsigned char *>(text.data());
  std::size_t index = 0;
  while (index < text.size()) {
    const unsigned char lead = bytes[index];
    std::uint32_t codePoint = 0;
    std::size_t sequenceLength = 0;
    if ((lead & 0x80U) == 0U) {
      codePoint = lead;
      sequenceLength = 1;
    } else if ((lead & 0xE0U) == 0xC0U) {
      codePoint = lead & 0x1FU;
      sequenceLength = 2;
    } else if ((lead & 0xF0U) == 0xE0U) {
      codePoint = lead & 0x0FU;
      sequenceLength = 3;
    } else if ((lead & 0xF8U) == 0xF0U) {
      codePoint = lead & 0x07U;
      sequenceLength = 4;
    } else {
      throw std::runtime_error("UTF-8のデコードに失敗しました");
    }
    if (index + sequenceLength > text.size()) {
      throw std::runtime_error("UTF-8が途中で途切れています");
    }
    for (std::size_t i = 1; i < sequenceLength; ++i) {
      const unsigned char trail = bytes[index + i];
      if ((trail & 0xC0U) != 0x80U) {
        throw std::runtime_error("UTF-8の継続バイトが不正です");
      }
      codePoint = (codePoint << 6U) | (trail & 0x3FU);
    }
    codePoints.push_back(codePoint);
    index += sequenceLength;
  }
  return codePoints;
}

[[nodiscard]] std::string encodeUtf8(std::uint32_t codePoint) {
  std::string text;
  if (codePoint <= 0x7FU) {
    text.push_back(static_cast<char>(codePoint));
  } else if (codePoint <= 0x7FFU) {
    text.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
    text.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
  } else if (codePoint <= 0xFFFFU) {
    text.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
    text.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
    text.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
  } else {
    text.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
    text.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
    text.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
    text.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
  }
  return text;
}

[[nodiscard]] std::string
encodeUtf8(const std::vector<std::uint32_t> &codePoints) {
  std::string text;
  for (std::uint32_t codePoint : codePoints) {
    text += encodeUtf8(codePoint);
  }
  return text;
}

[[nodiscard]] std::string describeCluster(const TextCluster &cluster) {
  std::ostringstream stream;
  bool first = true;
  for (std::uint32_t codePoint : cluster.m_codePoints) {
    if (!first) {
      stream << ' ';
    }
    stream << "U+" << std::uppercase << std::hex << codePoint;
    first = false;
  }
  return stream.str();
}

[[nodiscard]] bool isInRange(std::uint32_t codePoint, std::uint32_t first,
                             std::uint32_t last) {
  return first <= codePoint && codePoint <= last;
}

[[nodiscard]] bool isEmojiCodePoint(std::uint32_t codePoint) {
  return isInRange(codePoint, 0x1F300U, 0x1FAFFU) ||
         isInRange(codePoint, 0x2600U, 0x27BFU) ||
         isInRange(codePoint, 0xFE00U, 0xFE0FU) ||
         isInRange(codePoint, 0x1F1E6U, 0x1F1FFU) ||
         isInRange(codePoint, 0x1F900U, 0x1F9FFU);
}

[[nodiscard]] bool isVariationSelector(std::uint32_t codePoint) {
  return isInRange(codePoint, 0xFE00U, 0xFE0FU) ||
         isInRange(codePoint, 0xE0100U, 0xE01EFU);
}

[[nodiscard]] bool isEmojiModifier(std::uint32_t codePoint) {
  return isInRange(codePoint, 0x1F3FBU, 0x1F3FFU);
}

[[nodiscard]] bool isRegionalIndicator(std::uint32_t codePoint) {
  return isInRange(codePoint, 0x1F1E6U, 0x1F1FFU);
}

[[nodiscard]] bool isTagCodePoint(std::uint32_t codePoint) {
  return isInRange(codePoint, 0xE0020U, 0xE007FU);
}

[[nodiscard]] bool isEmojiJoiner(std::uint32_t codePoint) {
  return codePoint == 0x200DU;
}

[[nodiscard]] bool isKeycapCombiner(std::uint32_t codePoint) {
  return codePoint == 0x20E3U;
}

[[nodiscard]] bool shouldUseEmojiFont(const TextCluster &cluster) {
  for (std::uint32_t codePoint : cluster.m_codePoints) {
    if (isEmojiCodePoint(codePoint) || isVariationSelector(codePoint) ||
        isEmojiModifier(codePoint) || isRegionalIndicator(codePoint) ||
        isEmojiJoiner(codePoint) || isKeycapCombiner(codePoint) ||
        isTagCodePoint(codePoint)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] FontContext makeFontContext(FT_Library library,
                                          const std::string &fontPath,
                                          unsigned int pixelSize) {
  FT_Face faceRaw = nullptr;
  if (FT_New_Face(library, fontPath.c_str(), 0, &faceRaw) != 0) {
    throw std::runtime_error("フォント読み込みに失敗しました: " + fontPath);
  }
  FreeTypeFaceHandle face(faceRaw);
  if (FT_Set_Pixel_Sizes(face.get(), 0, pixelSize) != 0) {
    throw std::runtime_error("ピクセルサイズ設定に失敗しました: " + fontPath);
  }

  hb_face_t *hbFaceRaw = hb_ft_face_create_referenced(face.get());
  if (hbFaceRaw == nullptr) {
    throw std::runtime_error("hb_face_tの生成に失敗しました: " + fontPath);
  }
  HbFaceHandle hbFace(hbFaceRaw);

  hb_font_t *hbFontRaw = hb_ft_font_create_referenced(face.get());
  if (hbFontRaw == nullptr) {
    throw std::runtime_error("hb_font_tの生成に失敗しました: " + fontPath);
  }
  HbFontHandle hbFont(hbFontRaw);

  return FontContext{fontPath, std::move(face), std::move(hbFace),
                     std::move(hbFont)};
}

[[nodiscard]] bool hasColrV1RootPaint(FontContext &font,
                                      hb_codepoint_t glyphId) {
  if (!hb_ot_color_has_paint(font.m_hbFace.get()) ||
      !hb_ot_color_glyph_has_paint(font.m_hbFace.get(), glyphId)) {
    return false;
  }
  FT_OpaquePaint opaquePaint{};
  return FT_Get_Color_Glyph_Paint(font.m_face.get(), glyphId,
                                  FT_COLOR_NO_ROOT_TRANSFORM,
                                  &opaquePaint) != 0;
}

[[nodiscard]] std::vector<TextCluster>
clusterText(const std::vector<std::uint32_t> &codePoints) {
  std::vector<TextCluster> clusters;
  for (std::size_t i = 0; i < codePoints.size();) {
    TextCluster cluster;
    cluster.m_codePoints.push_back(codePoints[i++]);

    if (isRegionalIndicator(cluster.m_codePoints.front()) &&
        i < codePoints.size() && isRegionalIndicator(codePoints[i])) {
      cluster.m_codePoints.push_back(codePoints[i++]);
    }

    while (i < codePoints.size()) {
      const std::uint32_t codePoint = codePoints[i];
      if (isVariationSelector(codePoint) || isEmojiModifier(codePoint) ||
          isKeycapCombiner(codePoint) || isTagCodePoint(codePoint)) {
        cluster.m_codePoints.push_back(codePoint);
        ++i;
        continue;
      }
      if (isEmojiJoiner(codePoint) && i + 1U < codePoints.size()) {
        cluster.m_codePoints.push_back(codePoint);
        cluster.m_codePoints.push_back(codePoints[i + 1U]);
        i += 2U;
        continue;
      }
      break;
    }

    clusters.push_back(std::move(cluster));
  }
  return clusters;
}

[[nodiscard]] std::vector<ShapedGlyph>
shapeTextCluster(FontContext &font, const TextCluster &cluster) {
  hb_buffer_t *hbBufferRaw = hb_buffer_create();
  if (hbBufferRaw == nullptr) {
    throw std::runtime_error("hb_buffer_tの生成に失敗しました");
  }
  HbBufferHandle hbBuffer(hbBufferRaw);
  const std::string text = encodeUtf8(cluster.m_codePoints);
  hb_buffer_add_utf8(hbBuffer.get(), text.c_str(),
                     static_cast<int>(text.size()), 0, -1);
  hb_buffer_guess_segment_properties(hbBuffer.get());
  hb_shape(font.m_hbFont.get(), hbBuffer.get(), nullptr, 0);

  const unsigned int glyphCount = hb_buffer_get_length(hbBuffer.get());
  if (glyphCount == 0U) {
    throw std::runtime_error("描画対象のグリフが得られませんでした");
  }
  const hb_glyph_info_t *glyphInfos =
      hb_buffer_get_glyph_infos(hbBuffer.get(), nullptr);
  const hb_glyph_position_t *glyphPositions =
      hb_buffer_get_glyph_positions(hbBuffer.get(), nullptr);
  std::vector<ShapedGlyph> shapedGlyphs;
  shapedGlyphs.reserve(glyphCount);
  const auto round26dot6 = [](hb_position_t value) {
    return static_cast<int>(std::lround(static_cast<double>(value) / 64.0));
  };
  for (unsigned int i = 0; i < glyphCount; ++i) {
    shapedGlyphs.push_back({glyphInfos[i].codepoint,
                            round26dot6(glyphPositions[i].x_advance),
                            round26dot6(glyphPositions[i].y_advance),
                            round26dot6(glyphPositions[i].x_offset),
                            round26dot6(glyphPositions[i].y_offset)});
  }
  return shapedGlyphs;
}

[[nodiscard]] GlyphSurface renderMonochromeGlyph(FT_Face face,
                                                 hb_codepoint_t glyphId) {
  if (FT_Load_Glyph(face, glyphId, FT_LOAD_DEFAULT) != 0) {
    throw std::runtime_error("FT_Load_Glyph に失敗しました");
  }
  if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
    throw std::runtime_error("FT_Render_Glyph に失敗しました");
  }

  const FT_GlyphSlot slot = face->glyph;
  GlyphSurface surface;
  const int width = static_cast<int>(slot->bitmap.width);
  const int height = static_cast<int>(slot->bitmap.rows);
  surface.m_image.resize(width, height);
  surface.m_image.clear(0, 0, 0, 0);
  surface.m_bearingX = slot->bitmap_left;
  surface.m_bearingY = slot->bitmap_top;
  surface.m_advance = static_cast<int>(
      std::lround(static_cast<double>(slot->advance.x) / 64.0));

  auto &rgba = surface.m_image.rgba();
  for (int y = 0; y < height; ++y) {
    const auto *srcRow =
        slot->bitmap.buffer + static_cast<std::size_t>(y) * slot->bitmap.pitch;
    for (int x = 0; x < width; ++x) {
      const uint8_t alpha = srcRow[x];
      const std::size_t pixelIndex =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x)) *
          4U;
      rgba[pixelIndex + 0U] = 255U;
      rgba[pixelIndex + 1U] = 255U;
      rgba[pixelIndex + 2U] = 255U;
      rgba[pixelIndex + 3U] = alpha;
    }
  }
  return surface;
}

[[nodiscard]] GlyphSurface renderBitmapSlot(const FT_GlyphSlot slot) {
  GlyphSurface surface;
  const int width = static_cast<int>(slot->bitmap.width);
  const int height = static_cast<int>(slot->bitmap.rows);
  surface.m_image.resize(width, height);
  surface.m_image.clear(0U, 0U, 0U, 0U);
  surface.m_bearingX = slot->bitmap_left;
  surface.m_bearingY = slot->bitmap_top;
  surface.m_advance = static_cast<int>(
      std::lround(static_cast<double>(slot->advance.x) / 64.0));

  auto &rgba = surface.m_image.rgba();
  const FT_Bitmap &bitmap = slot->bitmap;
  for (int y = 0; y < height; ++y) {
    const auto *srcRow =
        bitmap.buffer + static_cast<std::size_t>(y) * bitmap.pitch;
    for (int x = 0; x < width; ++x) {
      const std::size_t pixelIndex =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x)) *
          4U;
      switch (bitmap.pixel_mode) {
        case FT_PIXEL_MODE_BGRA: {
          const std::size_t srcIndex = static_cast<std::size_t>(x) * 4U;
          rgba[pixelIndex + 0U] = srcRow[srcIndex + 2U];
          rgba[pixelIndex + 1U] = srcRow[srcIndex + 1U];
          rgba[pixelIndex + 2U] = srcRow[srcIndex + 0U];
          rgba[pixelIndex + 3U] = srcRow[srcIndex + 3U];
          break;
        }
        case FT_PIXEL_MODE_GRAY: {
          const uint8_t alpha = srcRow[x];
          rgba[pixelIndex + 0U] = 255U;
          rgba[pixelIndex + 1U] = 255U;
          rgba[pixelIndex + 2U] = 255U;
          rgba[pixelIndex + 3U] = alpha;
          break;
        }
        case FT_PIXEL_MODE_MONO: {
          const uint8_t alpha = (srcRow[static_cast<std::size_t>(x) / 8U] &
                                 (0x80U >> (static_cast<unsigned int>(x) & 7U)))
                                    ? 255U
                                    : 0U;
          rgba[pixelIndex + 0U] = 255U;
          rgba[pixelIndex + 1U] = 255U;
          rgba[pixelIndex + 2U] = 255U;
          rgba[pixelIndex + 3U] = alpha;
          break;
        }
        default:
          throw std::runtime_error("未対応のFT_Bitmap pixel_modeです");
      }
    }
  }
  return surface;
}

[[nodiscard]] GlyphSurface renderEmbeddedGlyph(FT_Face face,
                                               hb_codepoint_t glyphId) {
  if (FT_Load_Glyph(face, glyphId, FT_LOAD_COLOR | FT_LOAD_DEFAULT) != 0) {
    throw std::runtime_error("FT_Load_Glyph(FT_LOAD_COLOR) に失敗しました");
  }
  const FT_GlyphSlot slot = face->glyph;
  if (slot->format == FT_GLYPH_FORMAT_BITMAP) {
    return renderBitmapSlot(slot);
  }
  if (slot->format == FT_GLYPH_FORMAT_SVG) {
    throw std::runtime_error(
        "SVG glyph は未対応です。SVG レンダラの追加が必要です");
  }
  if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
    throw std::runtime_error("FT_Render_Glyph に失敗しました");
  }
  return renderBitmapSlot(slot);
}

[[nodiscard]] GlyphSurface
renderSingleGlyph(FontContext &font, hb_codepoint_t glyphId,
                  unsigned int paletteIndex, const ColrV1RenderOptions &options,
                  ColrV1RenderStats *stats = nullptr) {
  FT_Face face = font.m_face.get();
  hb_face_t *hbFace = font.m_hbFace.get();
  unsigned int layerCount = 0;
  hb_ot_color_glyph_get_layers(hbFace, glyphId, 0, &layerCount, nullptr);
  if (layerCount == 0 && FT_HAS_COLOR(face)) {
    FT_LayerIterator iterator;
    iterator.p = nullptr;
    FT_UInt layerGlyph = 0;
    FT_UInt layerColor = 0;
    while (FT_Get_Color_Glyph_Layer(face, glyphId, &layerGlyph, &layerColor,
                                    &iterator)) {
      ++layerCount;
    }
  }
  const bool glyphHasV1 = hasColrV1RootPaint(font, glyphId);
  const unsigned int effectivePaletteIndex =
      clampPaletteIndex(hbFace, paletteIndex);
  ColorPalette palette = loadPalette(face, hbFace, effectivePaletteIndex);
  if (layerCount > 0) {
    if (stats != nullptr) {
      *stats = {};
    }
    return renderColrV0Glyph(face, hbFace, glyphId, palette);
  }
  if (glyphHasV1) {
    return renderColrV1Glyph(face, static_cast<std::uint32_t>(glyphId), palette,
                             options, stats);
  }
  if (stats != nullptr) {
    *stats = {};
  }
  if (FT_HAS_COLOR(face)) {
    try {
      return renderEmbeddedGlyph(face, glyphId);
    } catch (const std::exception &ex) {
      if (std::string_view(ex.what()).find("SVG glyph") !=
          std::string_view::npos) {
        throw;
      }
    }
  }
  return renderMonochromeGlyph(face, glyphId);
}

[[nodiscard]] ClusterRenderResult
renderGlyphCluster(FontContext &font, const TextCluster &cluster,
                   const std::vector<ShapedGlyph> &shapedGlyphs,
                   unsigned int paletteIndex, GlyphRenderCache &glyphCache,
                   CacheStats &cacheStats, const ColrV1RenderOptions &options) {
  if (shapedGlyphs.empty()) {
    throw std::runtime_error("クラスタのシェーピング結果が空です");
  }
  ClusterRenderResult result;

  struct PositionedGlyph {
    GlyphSurface m_surface;
    int m_left = 0;
    int m_top = 0;
  };

  std::vector<PositionedGlyph> positionedGlyphs;
  positionedGlyphs.reserve(shapedGlyphs.size());

  int penX = 0;
  int penY = 0;
  bool hasBounds = false;
  int minX = 0;
  int maxX = 0;
  int minY = 0;
  int maxY = 0;

  for (const ShapedGlyph &glyph : shapedGlyphs) {
    const GlyphCacheKey cacheKey{glyph.m_glyphId, paletteIndex};
    auto cacheIt = glyphCache.find(cacheKey);
    if (cacheIt == glyphCache.end()) {
      ++cacheStats.m_glyphMisses;
      CachedGlyphRender cachedGlyph;
      ColrV1RenderStats glyphStats;
      try {
        cachedGlyph.m_surface = renderSingleGlyph(
            font, glyph.m_glyphId, paletteIndex, options, &glyphStats);
      } catch (const std::exception &ex) {
        throw std::runtime_error(
            "glyph render failure glyph=" + std::to_string(glyph.m_glyphId) +
            " cluster=[" + describeCluster(cluster) + "] reason=" + ex.what());
      }
      cachedGlyph.m_colrV1Stats = glyphStats;
      cacheIt = glyphCache.emplace(cacheKey, std::move(cachedGlyph)).first;
    } else {
      ++cacheStats.m_glyphHits;
    }
    if (cacheIt->second.m_colrV1Stats.m_totalPaints > 0) {
      accumulateColrV1Stats(result.m_colrV1Stats,
                            cacheIt->second.m_colrV1Stats);
      ++result.m_colrV1GlyphCount;
    }
    PositionedGlyph positioned;
    positioned.m_left =
        penX + glyph.m_xOffset + cacheIt->second.m_surface.m_bearingX;
    positioned.m_top =
        penY + glyph.m_yOffset + cacheIt->second.m_surface.m_bearingY;
    positioned.m_surface = cacheIt->second.m_surface;

    if (positioned.m_surface.m_image.width() > 0 &&
        positioned.m_surface.m_image.height() > 0) {
      const int left = positioned.m_left;
      const int right = left + positioned.m_surface.m_image.width();
      const int top = positioned.m_top;
      const int bottom = top - positioned.m_surface.m_image.height();
      if (!hasBounds) {
        minX = left;
        maxX = right;
        minY = bottom;
        maxY = top;
        hasBounds = true;
      } else {
        minX = std::min(minX, left);
        maxX = std::max(maxX, right);
        minY = std::min(minY, bottom);
        maxY = std::max(maxY, top);
      }
    }

    penX += glyph.m_xAdvance;
    penY += glyph.m_yAdvance;
    positionedGlyphs.push_back(std::move(positioned));
  }
  result.m_shapedGlyphs = shapedGlyphs;
  GlyphSurface &clusterSurface = result.m_surface;
  clusterSurface.m_advance = penX;
  clusterSurface.m_bearingX = hasBounds ? minX : 0;
  clusterSurface.m_bearingY = hasBounds ? maxY : 0;

  if (!hasBounds) {
    clusterSurface.m_image.resize(1, 1);
    clusterSurface.m_image.clear(0U, 0U, 0U, 0U);
    return result;
  }

  clusterSurface.m_image.resize(maxX - minX, maxY - minY);
  clusterSurface.m_image.clear(0U, 0U, 0U, 0U);
  for (const PositionedGlyph &glyph : positionedGlyphs) {
    if (glyph.m_surface.m_image.width() <= 0 ||
        glyph.m_surface.m_image.height() <= 0) {
      continue;
    }
    const int offsetX = glyph.m_left - minX;
    const int offsetY = maxY - glyph.m_top;
    clusterSurface.m_image.blit(glyph.m_surface.m_image, offsetX, offsetY);
  }

  return result;
}

[[nodiscard]] ClusterRenderResult
renderGlyphCluster(FontContext &font, const TextCluster &cluster,
                   unsigned int paletteIndex, GlyphRenderCache &glyphCache,
                   CacheStats &cacheStats, const ColrV1RenderOptions &options) {
  const std::vector<ShapedGlyph> shapedGlyphs = shapeTextCluster(font, cluster);
  return renderGlyphCluster(font, cluster, shapedGlyphs, paletteIndex,
                            glyphCache, cacheStats, options);
}

[[nodiscard]] unsigned int parseUnsigned(const char *value, const char *label) {
  if (value == nullptr) {
    throw std::invalid_argument(std::string(label) + " が未指定です");
  }
  const unsigned long parsed = std::stoul(value);
  if (parsed > std::numeric_limits<unsigned int>::max()) {
    throw std::out_of_range(std::string(label) + " が大きすぎます");
  }
  return static_cast<unsigned int>(parsed);
}

[[nodiscard]] std::string makeOutputName(unsigned int index,
                                         const TextCluster &cluster) {
  std::ostringstream stream;
  stream << "glyph_" << index;
  for (std::uint32_t codePoint : cluster.m_codePoints) {
    stream << "_u" << std::uppercase << std::hex << codePoint;
  }
  stream << ".png";
  return stream.str();
}

[[nodiscard]] std::string makeGlyphIdOutputName(hb_codepoint_t glyphId) {
  std::ostringstream stream;
  stream << "glyph_id_" << glyphId << ".png";
  return stream.str();
}

[[nodiscard]] bool verifyClusterMetrics(const ClusterRenderResult &result,
                                        const TextCluster &cluster) {
  int expectedAdvance = 0;
  int expectedYAdvance = 0;
  for (const ShapedGlyph &glyph : result.m_shapedGlyphs) {
    expectedAdvance += glyph.m_xAdvance;
    expectedYAdvance += glyph.m_yAdvance;
  }
  bool ok = true;
  if (result.m_surface.m_advance != expectedAdvance) {
    std::cerr << "cluster advance不一致"
              << " expected=" << expectedAdvance
              << " actual=" << result.m_surface.m_advance << " cluster=";
    for (std::uint32_t codePoint : cluster.m_codePoints) {
      std::cerr << " U+" << std::uppercase << std::hex << codePoint;
    }
    std::cerr << std::dec << '\n';
    ok = false;
  }
  if (expectedYAdvance != 0) {
    std::cerr << "警告: yAdvance を含むクラスタは未検証です"
              << " expectedYAdvance=" << expectedYAdvance << '\n';
  }
  return ok;
}

void printColrV1ScanSummary(hb_codepoint_t glyphId,
                            const ColrV1RenderStats &stats,
                            const GlyphSurface &surface, std::ostream &stream) {
  stream << "glyph " << glyphId << " paints=" << stats.m_totalPaints
         << " layers=" << stats.m_layerPaints
         << " glyphPaints=" << stats.m_glyphPaints
         << " colrGlyphPaints=" << stats.m_colrGlyphPaints
         << " composites=" << stats.m_composites
         << " gradients=" << stats.m_gradients
         << " clipPushes=" << stats.m_clipPushes
         << " transformPushes=" << stats.m_transformPushes
         << " groupPushes=" << stats.m_groupPushes
         << " size=" << surface.m_image.width() << "x"
         << surface.m_image.height() << " advance=" << surface.m_advance
         << '\n';
}

[[nodiscard]] bool matchesScanFilters(const ColrV1RenderStats &stats,
                                      const ColrV1ScanFilters &filters) {
  return stats.m_totalPaints >= filters.m_minPaints &&
         stats.m_gradients >= filters.m_minGradients &&
         stats.m_composites >= filters.m_minComposites &&
         stats.m_transformPushes >= filters.m_minTransforms;
}

template <typename Metric>
void printTopScanEntries(const char *label,
                         const std::vector<ColrV1ScanEntry> &entries,
                         std::size_t limit, Metric metric,
                         std::ostream &stream) {
  std::vector<ColrV1ScanEntry> sorted = entries;
  std::sort(sorted.begin(), sorted.end(),
            [&](const ColrV1ScanEntry &lhs, const ColrV1ScanEntry &rhs) {
              const std::size_t lhsValue = metric(lhs);
              const std::size_t rhsValue = metric(rhs);
              if (lhsValue != rhsValue) {
                return lhsValue > rhsValue;
              }
              return lhs.m_glyphId < rhs.m_glyphId;
            });
  stream << "[scan-top] " << label << '\n';
  std::size_t printed = 0;
  for (const ColrV1ScanEntry &entry : sorted) {
    const std::size_t value = metric(entry);
    if (value == 0) {
      continue;
    }
    stream << "glyph " << entry.m_glyphId << " value=" << value
           << " paints=" << entry.m_stats.m_totalPaints
           << " gradients=" << entry.m_stats.m_gradients
           << " composites=" << entry.m_stats.m_composites
           << " transforms=" << entry.m_stats.m_transformPushes
           << " size=" << entry.m_width << "x" << entry.m_height << '\n';
    ++printed;
    if (printed >= limit) {
      break;
    }
  }
}

void traceColrV1Stats(const ColrV1RenderStats &stats, std::size_t glyphCount,
                      std::ostream &stream) {
  stream << "[COLRv1-summary] glyphs=" << glyphCount
         << " paints=" << stats.m_totalPaints
         << " layers=" << stats.m_layerPaints
         << " glyphPaints=" << stats.m_glyphPaints
         << " colrGlyphPaints=" << stats.m_colrGlyphPaints
         << " composites=" << stats.m_composites
         << " gradients=" << stats.m_gradients
         << " clipPushes=" << stats.m_clipPushes
         << " transformPushes=" << stats.m_transformPushes
         << " groupPushes=" << stats.m_groupPushes << '\n';
}

void traceClusterLayout(const TextCluster &cluster,
                        const ClusterRenderResult &result,
                        std::ostream &stream) {
  stream << "[cluster] codepoints=";
  for (std::uint32_t codePoint : cluster.m_codePoints) {
    stream << " U+" << std::uppercase << std::hex << codePoint;
  }
  stream << std::dec << '\n';
  for (std::size_t i = 0; i < result.m_shapedGlyphs.size(); ++i) {
    const ShapedGlyph &glyph = result.m_shapedGlyphs[i];
    stream << "[cluster] glyph[" << i << "]"
           << " id=" << glyph.m_glyphId << " xAdvance=" << glyph.m_xAdvance
           << " yAdvance=" << glyph.m_yAdvance << " xOffset=" << glyph.m_xOffset
           << " yOffset=" << glyph.m_yOffset << '\n';
  }
  if (result.m_colrV1GlyphCount > 0) {
    traceColrV1Stats(result.m_colrV1Stats, result.m_colrV1GlyphCount, stream);
  }
}

void traceCacheStats(const char *label, const GlyphRenderCache &glyphCache,
                     const ClusterRenderCache &clusterCache,
                     const CacheStats &stats, std::ostream &stream) {
  stream << "[cache] " << label << " glyphEntries=" << glyphCache.size()
         << " glyphHits=" << stats.m_glyphHits
         << " glyphMisses=" << stats.m_glyphMisses
         << " clusterEntries=" << clusterCache.size()
         << " clusterHits=" << stats.m_clusterHits
         << " clusterMisses=" << stats.m_clusterMisses << '\n';
}

void traceSessionColrV1Stats(const char *label, const ColrV1RenderStats &stats,
                             std::size_t glyphCount, std::ostream &stream) {
  if (glyphCount == 0 || stats.m_totalPaints == 0) {
    stream << "[COLRv1-session] " << label << " glyphs=0 paints=0\n";
    return;
  }
  stream << "[COLRv1-session] " << label << " glyphs=" << glyphCount
         << " paints=" << stats.m_totalPaints
         << " layers=" << stats.m_layerPaints
         << " glyphPaints=" << stats.m_glyphPaints
         << " colrGlyphPaints=" << stats.m_colrGlyphPaints
         << " composites=" << stats.m_composites
         << " gradients=" << stats.m_gradients
         << " clipPushes=" << stats.m_clipPushes
         << " transformPushes=" << stats.m_transformPushes
         << " groupPushes=" << stats.m_groupPushes << '\n';
}

void traceSessionColrV1Stats(const char *label, const SessionColrV1Stats &stats,
                             std::ostream &stream) {
  stream << "[COLRv1-session] " << label
         << " logicalClusters=" << stats.m_logicalClusterCount
         << " renderedClusters=" << stats.m_renderedClusterCount
         << " logicalGlyphs=" << stats.m_logicalGlyphCount
         << " renderedGlyphs=" << stats.m_renderedGlyphCount << '\n';
  traceSessionColrV1Stats((std::string(label) + "/logical").c_str(),
                          stats.m_logicalStats, stats.m_logicalGlyphCount,
                          stream);
  traceSessionColrV1Stats((std::string(label) + "/rendered").c_str(),
                          stats.m_renderedStats, stats.m_renderedGlyphCount,
                          stream);
}

} // namespace

namespace glyphrenderer {

namespace {

struct TempRenderedCluster {
  ClusterRenderResult renderResult;
  TextCluster cluster;
  FontContext *font = nullptr;
  std::size_t clusterIndex = 0;
  int penX = 0;
  bool isColorGlyph = false;
  bool fallbackApplied = false;
};

struct ResolvedShapedCluster {
  TextCluster cluster;
  std::vector<ShapedGlyph> shapedGlyphs;
  FontContext *font = nullptr;
  FontRole requestedRole = FontRole::Text;
  std::vector<std::string> attemptedFonts;
  int penX = 0;
  bool fallbackApplied = false;
  bool isColorGlyph = false;
};

[[nodiscard]] int toPixels(FT_Pos value) {
  return static_cast<int>(std::lround(static_cast<double>(value) / 64.0));
}

[[nodiscard]] LineMetrics
buildLineMetrics(const std::vector<TempRenderedCluster> &clusters,
                 const std::vector<const FontContext *> &usedFonts,
                 unsigned int pixelSize, int lineWidth) {
  LineMetrics metrics;
  metrics.pixelSize = pixelSize;
  for (const FontContext *font : usedFonts) {
    metrics.ascent = std::max(metrics.ascent,
                              toPixels(font->m_face->size->metrics.ascender));
    metrics.descent = std::max(
        metrics.descent, -toPixels(font->m_face->size->metrics.descender));
    const int lineHeight = toPixels(font->m_face->size->metrics.height);
    metrics.lineGap =
        std::max(metrics.lineGap,
                 std::max(lineHeight - metrics.ascent - metrics.descent, 0));
  }
  metrics.baseline = metrics.ascent;
  metrics.lineWidth = lineWidth;

  bool hasBounds = false;
  for (const TempRenderedCluster &cluster : clusters) {
    const GlyphSurface &surface = cluster.renderResult.m_surface;
    if (surface.m_image.width() <= 0 || surface.m_image.height() <= 0) {
      continue;
    }
    const int minX = cluster.penX + surface.m_bearingX;
    const int maxX = minX + surface.m_image.width();
    const int maxY = surface.m_bearingY;
    const int minY = maxY - surface.m_image.height();
    if (!hasBounds) {
      metrics.boundingBox = {minX, minY, maxX, maxY};
      hasBounds = true;
    } else {
      metrics.boundingBox.minX = std::min(metrics.boundingBox.minX, minX);
      metrics.boundingBox.minY = std::min(metrics.boundingBox.minY, minY);
      metrics.boundingBox.maxX = std::max(metrics.boundingBox.maxX, maxX);
      metrics.boundingBox.maxY = std::max(metrics.boundingBox.maxY, maxY);
    }
  }
  return metrics;
}

[[nodiscard]] bool glyphUsesColorRendering(FontContext &font,
                                           hb_codepoint_t glyphId) {
  unsigned int layerCount = 0;
  hb_ot_color_glyph_get_layers(font.m_hbFace.get(), glyphId, 0, &layerCount,
                               nullptr);
  return layerCount > 0 || hasColrV1RootPaint(font, glyphId) ||
         FT_HAS_COLOR(font.m_face.get());
}

[[nodiscard]] bool
clusterUsesColorRendering(FontContext &font,
                          const std::vector<ShapedGlyph> &shapedGlyphs) {
  for (const ShapedGlyph &glyph : shapedGlyphs) {
    if (glyphUsesColorRendering(font, glyph.m_glyphId)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] int computeShapedAdvance(const std::vector<ShapedGlyph> &glyphs) {
  int advance = 0;
  for (const ShapedGlyph &glyph : glyphs) {
    advance += glyph.m_xAdvance;
  }
  return advance;
}

[[nodiscard]] BitmapImage toBitmapImage(const Bitmap &bitmap) {
  BitmapImage image;
  image.width = bitmap.width();
  image.height = bitmap.height();
  image.rgba = bitmap.rgba();
  return image;
}

[[nodiscard]] std::uint64_t fnv1aAppend(std::uint64_t hash,
                                        std::string_view text) {
  constexpr std::uint64_t OFFSET_BASIS = 1469598103934665603ULL;
  constexpr std::uint64_t PRIME = 1099511628211ULL;
  if (hash == 0) {
    hash = OFFSET_BASIS;
  }
  for (const unsigned char ch : text) {
    hash ^= static_cast<std::uint64_t>(ch);
    hash *= PRIME;
  }
  return hash;
}

[[nodiscard]] std::uint64_t fnv1aAppend(std::uint64_t hash,
                                        std::uint64_t value) {
  constexpr std::uint64_t PRIME = 1099511628211ULL;
  if (hash == 0) {
    hash = 1469598103934665603ULL;
  }
  for (unsigned int i = 0; i < 8U; ++i) {
    const std::uint8_t byte =
        static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU);
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= PRIME;
  }
  return hash;
}

[[nodiscard]] GlyphHandle
makeGlyphHandle(std::string_view fontPath, std::uint32_t glyphId,
                unsigned int pixelSize, unsigned int paletteIndex,
                std::size_t clusterIndex, std::size_t glyphIndexInCluster,
                bool isColorGlyph) {
  std::uint64_t hash = 0;
  hash = fnv1aAppend(hash, fontPath);
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(glyphId));
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(pixelSize));
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(paletteIndex));
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(clusterIndex));
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(glyphIndexInCluster));
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(isColorGlyph ? 1U : 0U));
  return GlyphHandle{hash};
}

[[nodiscard]] MaterialKind pickMaterialKind(bool isColorGlyph) {
  return isColorGlyph ? MaterialKind::ColorBitmap : MaterialKind::AlphaMask;
}

[[nodiscard]] SamplerKind pickSamplerKind(bool /* isColorGlyph */) {
  return SamplerKind::LinearClamp;
}

[[nodiscard]] BlendHint pickBlendHint(bool isColorGlyph) {
  return isColorGlyph ? BlendHint::PremultipliedAlpha : BlendHint::Coverage;
}

[[nodiscard]] GlyphBitmap toGlyphBitmap(const GlyphSurface &surface,
                                        const TextCluster &cluster,
                                        const FontContext &font) {
  GlyphBitmap glyph;
  glyph.image = toBitmapImage(surface.m_image);
  glyph.bearingX = surface.m_bearingX;
  glyph.bearingY = surface.m_bearingY;
  glyph.advance = surface.m_advance;
  glyph.codePoints = cluster.m_codePoints;
  glyph.fontPath = font.m_path;
  return glyph;
}

[[nodiscard]] FloatRect toNormalizedUvRect(const Rect &pixelRect, int pageWidth,
                                           int pageHeight) {
  FloatRect uvRect;
  if (pageWidth <= 0 || pageHeight <= 0) {
    return uvRect;
  }
  uvRect.minX =
      static_cast<float>(pixelRect.minX) / static_cast<float>(pageWidth);
  uvRect.minY =
      static_cast<float>(pixelRect.minY) / static_cast<float>(pageHeight);
  uvRect.maxX =
      static_cast<float>(pixelRect.maxX) / static_cast<float>(pageWidth);
  uvRect.maxY =
      static_cast<float>(pixelRect.maxY) / static_cast<float>(pageHeight);
  return uvRect;
}

void blitBitmapImage(BitmapImage &dst, const BitmapImage &src, int offsetX,
                     int offsetY) {
  for (int y = 0; y < src.height; ++y) {
    const int dstY = offsetY + y;
    if (dstY < 0 || dstY >= dst.height) {
      continue;
    }
    for (int x = 0; x < src.width; ++x) {
      const int dstX = offsetX + x;
      if (dstX < 0 || dstX >= dst.width) {
        continue;
      }
      const std::size_t srcIndex =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(src.width) +
           static_cast<std::size_t>(x)) *
          4U;
      const std::size_t dstIndex = (static_cast<std::size_t>(dstY) *
                                        static_cast<std::size_t>(dst.width) +
                                    static_cast<std::size_t>(dstX)) *
                                   4U;
      dst.rgba[dstIndex + 0U] = src.rgba[srcIndex + 0U];
      dst.rgba[dstIndex + 1U] = src.rgba[srcIndex + 1U];
      dst.rgba[dstIndex + 2U] = src.rgba[srcIndex + 2U];
      dst.rgba[dstIndex + 3U] = src.rgba[srcIndex + 3U];
    }
  }
}

[[nodiscard]] bool writeOutputPng(const std::string &path,
                                  const Bitmap &bitmap) {
#if GLYPH_RENDERER_ENABLE_PNG_IO
  return writePng(path, bitmap);
#else
  (void)path;
  (void)bitmap;
  return false;
#endif
}

[[nodiscard]] FontFallbackChain
makeEffectiveFallbackChain(const FontFallbackChain &chain) {
  FontFallbackChain effective;
  for (const std::string &path : chain.textFonts) {
    effective.textFonts.push_back(resolveExistingPath({path}));
  }
  for (const std::string &path : chain.emojiFonts) {
    effective.emojiFonts.push_back(resolveExistingPath({path}));
  }
  for (const std::string &path : chain.symbolFonts) {
    effective.symbolFonts.push_back(resolveExistingPath({path}));
  }
  return effective;
}

[[nodiscard]] DiagnosticsSummary
buildDiagnosticsSummary(const std::vector<Diagnostic> &diagnostics,
                        const LineSummary &summary) {
  DiagnosticsSummary result;
  result.fallbackAppliedCount = summary.fallbackCount;
  result.colorGlyphCount = summary.colorGlyphCount;
  for (const Diagnostic &diagnostic : diagnostics) {
    if (diagnostic.code == DiagnosticCode::MissingGlyphCluster) {
      result.missingGlyphClusters.push_back(diagnostic.clusterText);
    }
    if (diagnostic.code == DiagnosticCode::AtlasGlyphTooLarge) {
      ++result.atlasOverflowCount;
    }
    if (diagnostic.code == DiagnosticCode::LineOverflow) {
      ++result.lineOverflowCount;
    }
  }
  return result;
}

[[nodiscard]] bool isFallbackChainEmpty(const FontFallbackChain &chain) {
  return chain.textFonts.empty() && chain.emojiFonts.empty() &&
         chain.symbolFonts.empty();
}

[[nodiscard]] std::string
selectMetricsFontPath(const FontFallbackChain &chain) {
  if (!chain.textFonts.empty()) {
    return chain.textFonts.front();
  }
  if (!chain.emojiFonts.empty()) {
    return chain.emojiFonts.front();
  }
  if (!chain.symbolFonts.empty()) {
    return chain.symbolFonts.front();
  }
  throw std::invalid_argument(
      "GlyphRenderer: FontFallbackChain is empty. Provide at least one font "
      "path in textFonts, emojiFonts, or symbolFonts.");
}

void appendLineOverflowDiagnostic(std::vector<Diagnostic> &diagnostics,
                                  const std::string &lineText,
                                  std::size_t lineIndex, int lineWidth,
                                  int wrappingWidth) {
  if (wrappingWidth <= 0 || lineWidth <= wrappingWidth) {
    return;
  }
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::Warning;
  diagnostic.code = DiagnosticCode::LineOverflow;
  diagnostic.lineIndex = lineIndex;
  diagnostic.message = "line width exceeds wrapping width";
  diagnostic.clusterText = lineText;
  diagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] FontRole pickRequestedRole(const TextCluster &cluster) {
  return shouldUseEmojiFont(cluster) ? FontRole::Emoji : FontRole::Text;
}

void appendUniqueFont(std::vector<std::string> &fonts,
                      const std::string &fontPath) {
  if (std::find(fonts.begin(), fonts.end(), fontPath) == fonts.end()) {
    fonts.push_back(fontPath);
  }
}

[[nodiscard]] LineSummary summarizeShape(const TextShapeResult &shape) {
  LineSummary summary;
  summary.clusterCount = shape.clusters.size();
  for (const ShapedCluster &cluster : shape.clusters) {
    appendUniqueFont(summary.usedFonts, cluster.fontPath);
    if (cluster.fallbackApplied) {
      ++summary.fallbackCount;
    }
    for (const ShapedGlyphInfo &glyph : cluster.glyphs) {
      ++summary.glyphCount;
      if (glyph.isColorGlyph) {
        ++summary.colorGlyphCount;
      }
    }
  }
  return summary;
}

[[nodiscard]] int computeClusterAdvance(const ShapedCluster &cluster) {
  int advance = 0;
  for (const ShapedGlyphInfo &glyph : cluster.glyphs) {
    advance += glyph.xAdvance;
  }
  return advance;
}

[[nodiscard]] std::string
joinClusterText(const std::vector<ShapedCluster> &clusters) {
  std::string text;
  for (const ShapedCluster &cluster : clusters) {
    text += cluster.text;
  }
  return text;
}

[[nodiscard]] TextShapeResult sliceShapeResult(const TextShapeResult &source,
                                               std::size_t beginCluster,
                                               std::size_t endCluster) {
  TextShapeResult result;
  if (beginCluster >= endCluster || endCluster > source.clusters.size()) {
    result.metrics = source.metrics;
    result.metrics.lineWidth = 0;
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  result.metrics = source.metrics;
  result.clusters.reserve(endCluster - beginCluster);
  for (std::size_t clusterIndex = beginCluster; clusterIndex < endCluster;
       ++clusterIndex) {
    ShapedCluster cluster = source.clusters[clusterIndex];
    for (std::size_t glyphIndex = 0; glyphIndex < cluster.glyphs.size();
         ++glyphIndex) {
      cluster.glyphs[glyphIndex].clusterIndex = clusterIndex - beginCluster;
      cluster.glyphs[glyphIndex].glyphIndexInCluster = glyphIndex;
    }
    result.metrics.lineWidth += computeClusterAdvance(cluster);
    result.clusters.push_back(std::move(cluster));
  }

  for (const FallbackTraceEntry &trace : source.fallbackTrace) {
    if (trace.clusterIndex < beginCluster || trace.clusterIndex >= endCluster) {
      continue;
    }
    FallbackTraceEntry adjusted = trace;
    adjusted.clusterIndex -= beginCluster;
    result.fallbackTrace.push_back(std::move(adjusted));
  }

  for (const Diagnostic &diagnostic : source.diagnostics) {
    if (diagnostic.code == DiagnosticCode::LineOverflow &&
        diagnostic.clusterText == joinClusterText(result.clusters)) {
      result.diagnostics.push_back(diagnostic);
      continue;
    }
    if (diagnostic.clusterIndex < beginCluster ||
        diagnostic.clusterIndex >= endCluster) {
      continue;
    }
    Diagnostic adjusted = diagnostic;
    adjusted.clusterIndex -= beginCluster;
    result.diagnostics.push_back(std::move(adjusted));
  }

  result.summary = summarizeShape(result);
  result.diagnosticsSummary =
      buildDiagnosticsSummary(result.diagnostics, result.summary);
  return result;
}

[[nodiscard]] std::vector<TextShapeResult>
wrapShapeResult(const TextShapeResult &lineShape, int wrappingWidth) {
  if (wrappingWidth <= 0 || lineShape.clusters.empty()) {
    return {lineShape};
  }

  std::vector<TextShapeResult> wrapped;
  std::size_t lineBegin = 0;
  int currentWidth = 0;
  for (std::size_t clusterIndex = 0; clusterIndex < lineShape.clusters.size();
       ++clusterIndex) {
    const int clusterWidth =
        computeClusterAdvance(lineShape.clusters[clusterIndex]);
    const bool shouldWrap =
        clusterIndex > lineBegin && currentWidth + clusterWidth > wrappingWidth;
    if (shouldWrap) {
      wrapped.push_back(sliceShapeResult(lineShape, lineBegin, clusterIndex));
      lineBegin = clusterIndex;
      currentWidth = 0;
    }
    currentWidth += clusterWidth;
  }

  if (lineBegin < lineShape.clusters.size()) {
    wrapped.push_back(
        sliceShapeResult(lineShape, lineBegin, lineShape.clusters.size()));
  }
  return wrapped;
}

[[nodiscard]] std::vector<FallbackTraceEntry>
makeFallbackTrace(const std::vector<ResolvedShapedCluster> &clusters) {
  std::vector<FallbackTraceEntry> trace;
  trace.reserve(clusters.size());
  for (std::size_t clusterIndex = 0; clusterIndex < clusters.size();
       ++clusterIndex) {
    const ResolvedShapedCluster &cluster = clusters[clusterIndex];
    FallbackTraceEntry entry;
    entry.lineIndex = 0;
    entry.clusterIndex = clusterIndex;
    entry.requestedRole = cluster.requestedRole;
    entry.fallbackApplied = cluster.fallbackApplied;
    entry.resolved = cluster.font != nullptr;
    entry.isColorGlyph = cluster.isColorGlyph;
    entry.clusterText = encodeUtf8(cluster.cluster.m_codePoints);
    entry.codePoints = cluster.cluster.m_codePoints;
    entry.selectedFontPath =
        cluster.font != nullptr ? cluster.font->m_path : "";
    entry.attemptedFonts = cluster.attemptedFonts;
    trace.push_back(std::move(entry));
  }
  return trace;
}

[[nodiscard]] LineSummary summarizeRaster(const TextRasterResult &raster) {
  LineSummary summary;
  summary.clusterCount = raster.glyphs.size();
  summary.glyphCount = raster.glyphs.size();
  for (const RasterizedGlyph &glyph : raster.glyphs) {
    appendUniqueFont(summary.usedFonts, glyph.glyph.fontPath);
    if (glyph.isColorGlyph) {
      ++summary.colorGlyphCount;
    }
    if (glyph.fallbackApplied) {
      ++summary.fallbackCount;
    }
  }
  return summary;
}

[[nodiscard]] LineSummary summarizeLayout(const TextLineLayout &layout) {
  LineSummary summary;
  summary.clusterCount = layout.glyphs.size();
  summary.glyphCount = layout.glyphs.size();
  for (const PositionedGlyph &glyph : layout.glyphs) {
    appendUniqueFont(summary.usedFonts, glyph.glyph.fontPath);
    if (glyph.isColorGlyph) {
      ++summary.colorGlyphCount;
    }
    if (glyph.fallbackApplied) {
      ++summary.fallbackCount;
    }
  }
  return summary;
}

void mergeLineSummary(LineSummary &dst, const LineSummary &src) {
  dst.clusterCount += src.clusterCount;
  dst.glyphCount += src.glyphCount;
  dst.colorGlyphCount += src.colorGlyphCount;
  dst.fallbackCount += src.fallbackCount;
  dst.atlasPageCount += src.atlasPageCount;
  for (const std::string &font : src.usedFonts) {
    appendUniqueFont(dst.usedFonts, font);
  }
}

[[nodiscard]] std::vector<std::string> splitLines(std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = text.find('\n', start);
    if (end == std::string_view::npos) {
      lines.emplace_back(text.substr(start));
      break;
    }
    lines.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  if (lines.empty()) {
    lines.emplace_back();
  }
  return lines;
}

void assignLineIndex(std::vector<Diagnostic> &diagnostics,
                     std::size_t lineIndex) {
  for (Diagnostic &diagnostic : diagnostics) {
    diagnostic.lineIndex = lineIndex;
  }
}

[[nodiscard]] TextLayoutContext
makeTextLayoutContext(const FontFallbackChain &fallbackChain,
                      FontChainId fontChainId, unsigned int paletteIndex,
                      unsigned int pixelSize, bool verifyMetrics,
                      const ParagraphLayoutOptions &paragraphOptions = {}) {
  TextLayoutContext layoutContext;
  layoutContext.fallbackChain = fallbackChain;
  layoutContext.fontChainId = fontChainId;
  layoutContext.paletteIndex = paletteIndex;
  layoutContext.pixelSize = pixelSize;
  layoutContext.verifyMetrics = verifyMetrics;
  layoutContext.paragraphOptions = paragraphOptions;
  return layoutContext;
}

[[nodiscard]] TextLayoutContext
makeTextLayoutContext(const TextRequest &request) {
  return makeTextLayoutContext({}, request.fontChainId, request.paletteIndex,
                               request.pixelSize, request.verifyMetrics,
                               request.layoutOptions);
}

[[nodiscard]] AtlasBuildResult
buildAtlasLayout(const TextLineLayout &layout,
                 const AtlasBuildContext &context) {
  if (context.pageWidth <= 0 || context.pageHeight <= 0) {
    throw std::invalid_argument("atlas page size が不正です");
  }
  if (context.padding < 0) {
    throw std::invalid_argument("atlas padding が不正です");
  }

  AtlasBuildResult result;
  result.summary = layout.summary;
  auto makePage = [&]() {
    GlyphAtlasPage page;
    page.handle = GlyphAtlasPageHandle{
        static_cast<std::uint32_t>(result.pages.size() + 1U)};
    page.image.width = context.pageWidth;
    page.image.height = context.pageHeight;
    page.image.rgba.assign(static_cast<std::size_t>(context.pageWidth) *
                               static_cast<std::size_t>(context.pageHeight) *
                               4U,
                           0U);
    result.pages.push_back(std::move(page));
  };
  makePage();

  int currentPage = 0;
  int penX = context.padding;
  int penY = context.padding;
  int rowHeight = 0;

  for (std::size_t i = 0; i < layout.glyphs.size(); ++i) {
    const PositionedGlyph &glyph = layout.glyphs[i];
    const BitmapImage &image = glyph.glyph.image;
    if (image.width <= 0 || image.height <= 0) {
      continue;
    }
    if (image.width + context.padding * 2 > context.pageWidth ||
        image.height + context.padding * 2 > context.pageHeight) {
      Diagnostic diagnostic;
      diagnostic.severity = DiagnosticSeverity::Error;
      diagnostic.code = DiagnosticCode::AtlasGlyphTooLarge;
      diagnostic.message = "glyph does not fit atlas page";
      diagnostic.clusterText = encodeUtf8(glyph.glyph.codePoints);
      diagnostic.codePoints = glyph.glyph.codePoints;
      diagnostic.fontPath = glyph.glyph.fontPath;
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }
    if (penX + image.width + context.padding > context.pageWidth) {
      penX = context.padding;
      penY += rowHeight + context.padding;
      rowHeight = 0;
    }
    if (penY + image.height + context.padding > context.pageHeight) {
      makePage();
      ++currentPage;
      penX = context.padding;
      penY = context.padding;
      rowHeight = 0;
    }

    blitBitmapImage(result.pages[currentPage].image, image, penX, penY);

    GlyphAtlasEntry entry;
    entry.handle = glyph.handle;
    entry.glyphId = glyph.glyphId;
    entry.lineIndex = glyph.lineIndex;
    entry.clusterIndex = glyph.clusterIndex;
    entry.glyphIndexInCluster = glyph.glyphIndexInCluster;
    entry.glyphIndex = i;
    entry.pageIndex = currentPage;
    entry.pageHandle = result.pages[currentPage].handle;
    entry.pixelRect = {penX, penY, penX + image.width, penY + image.height};
    entry.uvRect = toNormalizedUvRect(entry.pixelRect, context.pageWidth,
                                      context.pageHeight);
    entry.isColorGlyph = glyph.isColorGlyph;
    entry.materialKind = glyph.materialKind;
    entry.samplerKind = glyph.samplerKind;
    entry.blendHint = glyph.blendHint;
    result.entries.push_back(std::move(entry));

    penX += image.width + context.padding;
    rowHeight = std::max(rowHeight, image.height);
  }

  result.summary.atlasPageCount = result.pages.size();
  result.diagnosticsSummary =
      buildDiagnosticsSummary(result.diagnostics, result.summary);
  return result;
}

} // namespace

class GlyphRendererContext::Impl {
public:
  Impl() {
    FT_Library libraryRaw = nullptr;
    if (FT_Init_FreeType(&libraryRaw) != 0) {
      throw std::runtime_error("FreeTypeの初期化に失敗しました");
    }
    m_library.reset(libraryRaw);
  }

  FontContext &getFont(const std::string &path, unsigned int pixelSize) {
    const std::string key = path + "#" + std::to_string(pixelSize);
    auto it = m_fontCache.find(key);
    if (it == m_fontCache.end()) {
      auto font = std::make_unique<FontContext>(
          makeFontContext(m_library.get(), path, pixelSize));
      it = m_fontCache.emplace(key, std::move(font)).first;
    }
    return *it->second;
  }

  FontChainId registerFontFallbackChain(const FontFallbackChain &chain) {
    const FontFallbackChain effective = makeEffectiveFallbackChain(chain);
    const FontChainId id{m_nextFontChainId++};
    m_fontChains.emplace(id.value, effective);
    return id;
  }

  FontFallbackChain resolveFontFallbackChain(FontChainId fontChainId) const {
    if (fontChainId.value == 0) {
      return {};
    }
    const auto it = m_fontChains.find(fontChainId.value);
    if (it == m_fontChains.end()) {
      throw std::invalid_argument("未知の FontChainId が指定されました");
    }
    return it->second;
  }

private:
  FreeTypeLibraryHandle m_library;
  std::unordered_map<std::string, std::unique_ptr<FontContext>> m_fontCache;
  std::unordered_map<std::uint32_t, FontFallbackChain> m_fontChains;
  std::uint32_t m_nextFontChainId = 1;
};

class GlyphRenderer::Impl {
public:
  explicit Impl(GlyphRendererContext::Impl &context) : m_context(context) {}

  TextShapeResult shapeLine(const TextLayoutContext &layoutContext,
                            std::string_view text) const {
    if (text.empty()) {
      throw std::invalid_argument("text が空です");
    }
    TextShapeResult result;
    const std::vector<ResolvedShapedCluster> shapedClusters =
        shapeClusters(layoutContext, text, result.diagnostics, result.metrics);
    result.clusters.reserve(shapedClusters.size());
    for (std::size_t clusterIndex = 0; clusterIndex < shapedClusters.size();
         ++clusterIndex) {
      const ResolvedShapedCluster &cluster = shapedClusters[clusterIndex];
      ShapedCluster shapedCluster;
      shapedCluster.text = encodeUtf8(cluster.cluster.m_codePoints);
      shapedCluster.codePoints = cluster.cluster.m_codePoints;
      shapedCluster.fontPath = cluster.font->m_path;
      shapedCluster.requestedRole = cluster.requestedRole;
      shapedCluster.fallbackApplied = cluster.fallbackApplied;
      shapedCluster.glyphs.reserve(cluster.shapedGlyphs.size());
      for (std::size_t glyphIndex = 0; glyphIndex < cluster.shapedGlyphs.size();
           ++glyphIndex) {
        const ShapedGlyph &glyph = cluster.shapedGlyphs[glyphIndex];
        shapedCluster.glyphs.push_back(
            {makeGlyphHandle(cluster.font->m_path, glyph.m_glyphId,
                             layoutContext.pixelSize,
                             layoutContext.paletteIndex, clusterIndex,
                             glyphIndex, cluster.isColorGlyph),
             glyph.m_glyphId, clusterIndex, glyphIndex, glyph.m_xAdvance,
             glyph.m_yAdvance, glyph.m_xOffset, glyph.m_yOffset,
             cluster.isColorGlyph});
      }
      result.clusters.push_back(std::move(shapedCluster));
    }
    result.summary = summarizeShape(result);
    result.fallbackTrace = makeFallbackTrace(shapedClusters);
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  MultiLineShapeResult shapeText(const TextLayoutContext &layoutContext,
                                 std::string_view text) const {
    MultiLineShapeResult result;
    const std::vector<std::string> inputLines = splitLines(text);
    result.lineSummaries.reserve(inputLines.size());

    for (const std::string &inputLine : inputLines) {
      const TextShapeResult physicalLine =
          inputLine.empty() ? makeEmptyLineShape(layoutContext)
                            : shapeLine(layoutContext, inputLine);
      const std::vector<TextShapeResult> wrappedLines =
          inputLine.empty()
              ? std::vector<TextShapeResult>{physicalLine}
              : wrapShapeResult(physicalLine,
                                layoutContext.paragraphOptions.wrappingWidth);
      for (TextShapeResult wrappedLine : wrappedLines) {
        const std::size_t lineIndex = result.lines.size();
        const std::string lineText = joinClusterText(wrappedLine.clusters);
        appendLineOverflowDiagnostic(
            result.diagnostics, lineText, lineIndex,
            wrappedLine.metrics.lineWidth,
            layoutContext.paragraphOptions.wrappingWidth);
        assignLineIndex(wrappedLine.diagnostics, lineIndex);
        result.lineTexts.push_back(lineText);
        result.lineSummaries.push_back(wrappedLine.summary);
        mergeLineSummary(result.summary, wrappedLine.summary);
        result.diagnostics.insert(result.diagnostics.end(),
                                  wrappedLine.diagnostics.begin(),
                                  wrappedLine.diagnostics.end());
        for (const FallbackTraceEntry &lineTrace : wrappedLine.fallbackTrace) {
          FallbackTraceEntry trace = lineTrace;
          trace.lineIndex = lineIndex;
          result.fallbackTrace.push_back(std::move(trace));
        }
        result.lines.push_back(std::move(wrappedLine));
      }
    }
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  TextRasterResult rasterizeLine(const TextLayoutContext &layoutContext,
                                 const TextShapeResult &shape) const {
    GlyphRenderCache glyphCache;
    CacheStats cacheStats;
    ColrV1RenderOptions options;
    std::vector<TempRenderedCluster> tempClusters;
    std::vector<const FontContext *> usedFonts;

    TextRasterResult result;
    result.diagnostics = shape.diagnostics;
    result.fallbackTrace = shape.fallbackTrace;

    int penX = 0;
    for (std::size_t clusterIndex = 0; clusterIndex < shape.clusters.size();
         ++clusterIndex) {
      const ShapedCluster &cluster = shape.clusters[clusterIndex];
      FontContext &font = m_context.getFont(
          resolveExistingPath({cluster.fontPath}), layoutContext.pixelSize);
      TextCluster textCluster;
      textCluster.m_codePoints = cluster.codePoints;

      std::vector<ShapedGlyph> shapedGlyphs;
      shapedGlyphs.reserve(cluster.glyphs.size());
      for (const ShapedGlyphInfo &glyph : cluster.glyphs) {
        shapedGlyphs.push_back({glyph.glyphId, glyph.xAdvance, glyph.yAdvance,
                                glyph.xOffset, glyph.yOffset});
      }

      ClusterRenderResult clusterResult;
      try {
        clusterResult = renderGlyphCluster(font, textCluster, shapedGlyphs,
                                           layoutContext.paletteIndex,
                                           glyphCache, cacheStats, options);
      } catch (const std::exception &exception) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Warning;
        diagnostic.code = DiagnosticCode::MissingGlyphCluster;
        diagnostic.clusterIndex = clusterIndex;
        diagnostic.message =
            std::string("glyph cluster could not be rasterized: ") +
            exception.what();
        diagnostic.clusterText = encodeUtf8(textCluster.m_codePoints);
        diagnostic.codePoints = textCluster.m_codePoints;
        diagnostic.fontPath = font.m_path;
        result.diagnostics.push_back(std::move(diagnostic));
        penX += computeShapedAdvance(shapedGlyphs);
        continue;
      }
      if (layoutContext.verifyMetrics &&
          !verifyClusterMetrics(clusterResult, textCluster)) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Warning;
        diagnostic.code = DiagnosticCode::MetricsVerificationFailed;
        diagnostic.message = "metrics verification failed";
        diagnostic.clusterText = encodeUtf8(textCluster.m_codePoints);
        diagnostic.codePoints = textCluster.m_codePoints;
        diagnostic.fontPath = font.m_path;
        result.diagnostics.push_back(std::move(diagnostic));
      }

      tempClusters.push_back(
          {std::move(clusterResult), textCluster, &font, clusterIndex, penX,
           cluster.glyphs.empty() ? false : cluster.glyphs.front().isColorGlyph,
           cluster.fallbackApplied});
      usedFonts.push_back(&font);
      penX += computeShapedAdvance(shapedGlyphs);
    }

    result.metrics = buildLineMetrics(
        tempClusters, usedFonts, layoutContext.pixelSize,
        shape.metrics.lineWidth > 0 ? shape.metrics.lineWidth : penX);

    result.glyphs.reserve(tempClusters.size());
    for (const TempRenderedCluster &cluster : tempClusters) {
      RasterizedGlyph glyph;
      glyph.handle =
          cluster.renderResult.m_shapedGlyphs.empty()
              ? GlyphHandle{}
              : makeGlyphHandle(
                    cluster.font->m_path,
                    cluster.renderResult.m_shapedGlyphs.front().m_glyphId,
                    layoutContext.pixelSize, layoutContext.paletteIndex,
                    cluster.clusterIndex, 0U, cluster.isColorGlyph);
      glyph.glyphId =
          cluster.renderResult.m_shapedGlyphs.empty()
              ? 0
              : cluster.renderResult.m_shapedGlyphs.front().m_glyphId;
      glyph.lineIndex = 0;
      glyph.clusterIndex = cluster.clusterIndex;
      glyph.glyphIndexInCluster = 0;
      glyph.glyph = toGlyphBitmap(cluster.renderResult.m_surface,
                                  cluster.cluster, *cluster.font);
      glyph.penX = cluster.penX;
      glyph.penY = 0;
      glyph.isColorGlyph = cluster.isColorGlyph;
      glyph.fallbackApplied = cluster.fallbackApplied;
      result.glyphs.push_back(std::move(glyph));
    }
    result.summary = summarizeRaster(result);
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  MultiLineRasterResult rasterizeText(const TextLayoutContext &layoutContext,
                                      const MultiLineShapeResult &shape) const {
    MultiLineRasterResult result;
    result.lineTexts = shape.lineTexts;
    result.lineSummaries = shape.lineSummaries;
    result.summary = shape.summary;
    result.fallbackTrace = shape.fallbackTrace;
    result.diagnostics = shape.diagnostics;
    result.lines.reserve(shape.lines.size());

    int currentOriginY = 0;
    bool hasBounds = false;
    for (std::size_t lineIndex = 0; lineIndex < shape.lines.size();
         ++lineIndex) {
      const TextShapeResult &lineShape = shape.lines[lineIndex];
      TextRasterResult lineRaster =
          lineShape.clusters.empty() && shape.lineTexts[lineIndex].empty()
              ? makeEmptyLineRaster(layoutContext)
              : rasterizeLine(layoutContext, lineShape);
      assignLineIndex(lineRaster.diagnostics, lineIndex);
      result.lineOriginsY.push_back(currentOriginY);
      result.diagnostics.insert(result.diagnostics.end(),
                                lineRaster.diagnostics.begin(),
                                lineRaster.diagnostics.end());

      if (lineRaster.metrics.boundingBox.minX != 0 ||
          lineRaster.metrics.boundingBox.minY != 0 ||
          lineRaster.metrics.boundingBox.maxX != 0 ||
          lineRaster.metrics.boundingBox.maxY != 0) {
        Rect shifted = lineRaster.metrics.boundingBox;
        shifted.minY += currentOriginY;
        shifted.maxY += currentOriginY;
        if (!hasBounds) {
          result.boundingBox = shifted;
          hasBounds = true;
        } else {
          result.boundingBox.minX =
              std::min(result.boundingBox.minX, shifted.minX);
          result.boundingBox.minY =
              std::min(result.boundingBox.minY, shifted.minY);
          result.boundingBox.maxX =
              std::max(result.boundingBox.maxX, shifted.maxX);
          result.boundingBox.maxY =
              std::max(result.boundingBox.maxY, shifted.maxY);
        }
      }

      const int lineHeight =
          computeLineAdvance(layoutContext, lineRaster.metrics);
      result.lineAdvance = std::max(result.lineAdvance, lineHeight);

      for (const RasterizedGlyph &lineGlyph : lineRaster.glyphs) {
        RasterizedGlyph glyph = lineGlyph;
        glyph.lineIndex = lineIndex;
        glyph.penY += currentOriginY;
        result.glyphs.push_back(std::move(glyph));
      }

      currentOriginY += lineHeight;
      result.lines.push_back(std::move(lineRaster));
    }
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  TextLineLayout layoutLine(const TextRasterResult &raster) const {
    TextLineLayout result;
    result.metrics = raster.metrics;
    result.summary = raster.summary;
    result.fallbackTrace = raster.fallbackTrace;
    result.diagnostics = raster.diagnostics;
    result.glyphs.reserve(raster.glyphs.size());
    for (const RasterizedGlyph &rasterGlyph : raster.glyphs) {
      PositionedGlyph glyph;
      glyph.handle = rasterGlyph.handle;
      glyph.glyphId = rasterGlyph.glyphId;
      glyph.lineIndex = rasterGlyph.lineIndex;
      glyph.clusterIndex = rasterGlyph.clusterIndex;
      glyph.glyphIndexInCluster = rasterGlyph.glyphIndexInCluster;
      glyph.glyph = rasterGlyph.glyph;
      glyph.originX = rasterGlyph.penX + glyph.glyph.bearingX;
      glyph.originY = raster.metrics.baseline - glyph.glyph.bearingY;
      glyph.isColorGlyph = rasterGlyph.isColorGlyph;
      glyph.fallbackApplied = rasterGlyph.fallbackApplied;
      glyph.atlasRect = {0, 0, glyph.glyph.image.width,
                         glyph.glyph.image.height};
      glyph.materialKind = pickMaterialKind(glyph.isColorGlyph);
      glyph.samplerKind = pickSamplerKind(glyph.isColorGlyph);
      glyph.blendHint = pickBlendHint(glyph.isColorGlyph);
      result.glyphs.push_back(std::move(glyph));
    }
    result.summary = summarizeLayout(result);
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  TextLineLayout layoutLine(const TextLayoutContext &layoutContext,
                            std::string_view text) const {
    if (text.empty()) {
      throw std::invalid_argument("text が空です");
    }
    const TextShapeResult shape = shapeLine(layoutContext, text);
    const TextRasterResult raster = rasterizeLine(layoutContext, shape);
    return layoutLine(raster);
  }

  MultiLineLayout layoutText(const TextLayoutContext &layoutContext,
                             std::string_view text) const {
    const MultiLineShapeResult shape = shapeText(layoutContext, text);
    const MultiLineRasterResult raster = rasterizeText(layoutContext, shape);
    MultiLineLayout result;
    result.lineTexts = raster.lineTexts;
    result.lineOriginsY = raster.lineOriginsY;
    result.lineSummaries = raster.lineSummaries;
    result.fallbackTrace = raster.fallbackTrace;
    result.summary = raster.summary;
    result.diagnostics = raster.diagnostics;
    result.boundingBox = raster.boundingBox;
    result.lineAdvance = raster.lineAdvance;
    result.lines.reserve(raster.lines.size());
    result.glyphs.reserve(raster.glyphs.size());

    for (std::size_t lineIndex = 0; lineIndex < raster.lines.size();
         ++lineIndex) {
      TextLineLayout lineLayout = layoutLine(raster.lines[lineIndex]);
      assignLineIndex(lineLayout.diagnostics, lineIndex);
      const int alignOffset =
          computeHorizontalAlignOffset(layoutContext, lineLayout.metrics);
      if (alignOffset != 0) {
        for (PositionedGlyph &glyph : lineLayout.glyphs) {
          glyph.originX += alignOffset;
        }
      }
      result.lines.push_back(std::move(lineLayout));
    }
    for (const RasterizedGlyph &rasterGlyph : raster.glyphs) {
      PositionedGlyph glyph;
      glyph.handle = rasterGlyph.handle;
      glyph.glyphId = rasterGlyph.glyphId;
      glyph.lineIndex = rasterGlyph.lineIndex;
      glyph.clusterIndex = rasterGlyph.clusterIndex;
      glyph.glyphIndexInCluster = rasterGlyph.glyphIndexInCluster;
      glyph.glyph = rasterGlyph.glyph;
      glyph.originX = rasterGlyph.penX + glyph.glyph.bearingX;
      glyph.originY = result.lines[glyph.lineIndex].metrics.baseline -
                      glyph.glyph.bearingY + rasterGlyph.penY;
      glyph.isColorGlyph = rasterGlyph.isColorGlyph;
      glyph.fallbackApplied = rasterGlyph.fallbackApplied;
      glyph.atlasRect = {0, 0, glyph.glyph.image.width,
                         glyph.glyph.image.height};
      glyph.materialKind = pickMaterialKind(glyph.isColorGlyph);
      glyph.samplerKind = pickSamplerKind(glyph.isColorGlyph);
      glyph.blendHint = pickBlendHint(glyph.isColorGlyph);
      const int alignOffset = computeHorizontalAlignOffset(
          layoutContext, result.lines[glyph.lineIndex].metrics);
      glyph.originX += alignOffset;
      result.glyphs.push_back(std::move(glyph));
    }
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  TextDrawRun buildDrawRun(const TextLineLayout &layout,
                           const AtlasBuildResult &atlas) const {
    TextDrawRun result;
    result.metrics = layout.metrics;
    result.summary = layout.summary;
    result.diagnosticsSummary = layout.diagnosticsSummary;
    result.fallbackTrace = layout.fallbackTrace;
    result.diagnostics = layout.diagnostics;
    result.diagnostics.insert(result.diagnostics.end(),
                              atlas.diagnostics.begin(),
                              atlas.diagnostics.end());
    result.glyphs = layout.glyphs;

    std::unordered_map<std::uint64_t, const GlyphAtlasEntry *> atlasEntries;
    atlasEntries.reserve(atlas.entries.size());
    for (const GlyphAtlasEntry &entry : atlas.entries) {
      atlasEntries[entry.handle.value] = &entry;
    }

    for (PositionedGlyph &glyph : result.glyphs) {
      const auto it = atlasEntries.find(glyph.handle.value);
      if (it == atlasEntries.end()) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Warning;
        diagnostic.code = DiagnosticCode::AtlasEntryMissing;
        diagnostic.message = "atlas entry missing for glyph handle";
        diagnostic.clusterText = encodeUtf8(glyph.glyph.codePoints);
        diagnostic.codePoints = glyph.glyph.codePoints;
        diagnostic.fontPath = glyph.glyph.fontPath;
        result.diagnostics.push_back(std::move(diagnostic));
        continue;
      }
      const GlyphAtlasEntry &entry = *it->second;
      glyph.atlasPage = entry.pageIndex;
      glyph.atlasPageHandle = entry.pageHandle;
      glyph.atlasRect = entry.pixelRect;
      glyph.uvRect = entry.uvRect;
      glyph.materialKind = entry.materialKind;
      glyph.samplerKind = entry.samplerKind;
      glyph.blendHint = entry.blendHint;
    }
    result.summary.atlasPageCount = atlas.pages.size();
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  MultiLineDrawRun buildDrawText(const TextLayoutContext &layoutContext,
                                 std::string_view text,
                                 const AtlasBuildContext &atlasContext) const {
    const MultiLineLayout layout = layoutText(layoutContext, text);
    MultiLineDrawRun result;
    result.lineTexts = layout.lineTexts;
    result.lineOriginsY = layout.lineOriginsY;
    result.boundingBox = layout.boundingBox;
    result.lineAdvance = layout.lineAdvance;
    result.lineSummaries = layout.lineSummaries;
    result.fallbackTrace = layout.fallbackTrace;
    result.summary = layout.summary;
    result.diagnosticsSummary = layout.diagnosticsSummary;
    result.diagnostics.clear();
    result.lines.reserve(layout.lines.size());
    result.glyphs.reserve(layout.glyphs.size());
    result.atlases.reserve(layout.lines.size());

    for (std::size_t lineIndex = 0; lineIndex < layout.lines.size();
         ++lineIndex) {
      const TextLineLayout &line = layout.lines[lineIndex];
      AtlasBuildResult atlas = buildAtlasLayout(line, atlasContext);
      TextDrawRun drawRun = buildDrawRun(line, atlas);
      assignLineIndex(atlas.diagnostics, lineIndex);
      assignLineIndex(drawRun.diagnostics, lineIndex);
      const int originY = lineIndex < result.lineOriginsY.size()
                              ? result.lineOriginsY[lineIndex]
                              : 0;
      for (PositionedGlyph &glyph : drawRun.glyphs) {
        glyph.lineIndex = lineIndex;
        glyph.originY += originY;
        result.glyphs.push_back(glyph);
      }
      for (GlyphAtlasEntry &entry : atlas.entries) {
        entry.lineIndex = lineIndex;
      }
      result.diagnostics.insert(result.diagnostics.end(),
                                drawRun.diagnostics.begin(),
                                drawRun.diagnostics.end());
      result.summary.atlasPageCount += atlas.pages.size();
      result.lines.push_back(std::move(drawRun));
      result.atlases.push_back(std::move(atlas));
    }
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

private:
  [[nodiscard]] FontFallbackChain
  resolveFallbackChain(const TextLayoutContext &layoutContext) const {
    if (layoutContext.fontChainId.value != 0) {
      return m_context.resolveFontFallbackChain(layoutContext.fontChainId);
    }
    return makeEffectiveFallbackChain(layoutContext.fallbackChain);
  }

  [[nodiscard]] TextLayoutContext
  withResolvedFallbackChain(const TextLayoutContext &layoutContext) const {
    TextLayoutContext resolved = layoutContext;
    resolved.fallbackChain = resolveFallbackChain(layoutContext);
    return resolved;
  }

  [[nodiscard]] int computeLineAdvance(const TextLayoutContext &layoutContext,
                                       const LineMetrics &metrics) const {
    int lineHeight = 0;
    if (layoutContext.paragraphOptions.baselinePolicy ==
        BaselinePolicy::Tight) {
      lineHeight = std::max(metrics.boundingBox.maxY - metrics.boundingBox.minY,
                            static_cast<int>(layoutContext.pixelSize));
    } else {
      lineHeight = std::max(metrics.ascent + metrics.descent + metrics.lineGap,
                            static_cast<int>(layoutContext.pixelSize));
    }
    const float multiplier =
        std::max(layoutContext.paragraphOptions.lineSpacingMultiplier, 0.1F);
    return std::max(1, static_cast<int>(std::lround(
                           static_cast<double>(lineHeight) * multiplier)));
  }

  [[nodiscard]] int
  computeHorizontalAlignOffset(const TextLayoutContext &layoutContext,
                               const LineMetrics &metrics) const {
    const int wrappingWidth = layoutContext.paragraphOptions.wrappingWidth;
    if (wrappingWidth <= 0 || metrics.lineWidth >= wrappingWidth) {
      return 0;
    }
    const int slack = wrappingWidth - metrics.lineWidth;
    switch (layoutContext.paragraphOptions.horizontalAlign) {
      case HorizontalAlign::Left:
        return 0;
      case HorizontalAlign::Center:
        return slack / 2;
      case HorizontalAlign::Right:
        return slack;
    }
    return 0;
  }

  [[nodiscard]] TextShapeResult
  makeEmptyLineShape(const TextLayoutContext &layoutContext) const {
    TextShapeResult result;
    const FontFallbackChain fallbackChain = resolveFallbackChain(layoutContext);
    const std::string fontPath = selectMetricsFontPath(fallbackChain);

    FontContext &font = m_context.getFont(fontPath, layoutContext.pixelSize);
    std::vector<const FontContext *> usedFonts = {&font};
    result.metrics =
        buildLineMetrics({}, usedFonts, layoutContext.pixelSize, 0);
    result.summary.usedFonts.push_back(font.m_path);
    result.diagnosticsSummary =
        buildDiagnosticsSummary(result.diagnostics, result.summary);
    return result;
  }

  [[nodiscard]] TextRasterResult
  makeEmptyLineRaster(const TextLayoutContext &layoutContext) const {
    TextRasterResult result;
    const TextShapeResult shape = makeEmptyLineShape(layoutContext);
    result.metrics = shape.metrics;
    result.summary = shape.summary;
    result.diagnosticsSummary = shape.diagnosticsSummary;
    return result;
  }

  [[nodiscard]] TextLineLayout
  makeEmptyLineLayout(const TextLayoutContext &layoutContext) const {
    return layoutLine(makeEmptyLineRaster(layoutContext));
  }

  [[nodiscard]] std::vector<ResolvedShapedCluster>
  shapeClusters(const TextLayoutContext &layoutContext, std::string_view text,
                std::vector<Diagnostic> &diagnostics,
                LineMetrics &metrics) const {
    const FontFallbackChain fallbackChain = resolveFallbackChain(layoutContext);
    if (isFallbackChainEmpty(fallbackChain)) {
      throw std::invalid_argument(
          "GlyphRenderer: FontFallbackChain is empty. Provide at least one "
          "font path in textFonts, emojiFonts, or symbolFonts.");
    }

    const std::vector<std::uint32_t> codePoints = decodeUtf8(std::string(text));
    const std::vector<TextCluster> clusters = clusterText(codePoints);
    if (clusters.empty()) {
      throw std::runtime_error("描画対象のクラスタが得られませんでした");
    }

    std::vector<ResolvedShapedCluster> resolvedClusters;
    std::vector<const FontContext *> usedFonts;
    int penX = 0;

    for (std::size_t clusterIndex = 0; clusterIndex < clusters.size();
         ++clusterIndex) {
      const TextCluster &cluster = clusters[clusterIndex];
      const FontRole requestedRole = pickRequestedRole(cluster);
      const bool prefersEmoji = requestedRole == FontRole::Emoji;
      std::vector<std::string> candidatePaths =
          prefersEmoji ? fallbackChain.emojiFonts : fallbackChain.textFonts;
      candidatePaths.insert(candidatePaths.end(),
                            fallbackChain.symbolFonts.begin(),
                            fallbackChain.symbolFonts.end());
      const std::vector<std::string> &secondary =
          prefersEmoji ? fallbackChain.textFonts : fallbackChain.emojiFonts;
      candidatePaths.insert(candidatePaths.end(), secondary.begin(),
                            secondary.end());

      FontContext *selectedFont = nullptr;
      std::vector<ShapedGlyph> shapedGlyphs;
      std::vector<std::string> attemptedFonts;
      std::size_t selectedIndex = 0;
      for (std::size_t i = 0; i < candidatePaths.size(); ++i) {
        const std::string resolvedPath =
            resolveExistingPath({candidatePaths[i]});
        attemptedFonts.push_back(resolvedPath);
        FontContext &candidate =
            m_context.getFont(resolvedPath, layoutContext.pixelSize);
        shapedGlyphs = shapeTextCluster(candidate, cluster);
        if (std::all_of(shapedGlyphs.begin(), shapedGlyphs.end(),
                        [](const ShapedGlyph &glyph) {
                          return glyph.m_glyphId != 0;
                        })) {
          selectedFont = &candidate;
          selectedIndex = i;
          break;
        }
      }

      if (selectedFont == nullptr) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostic.code = DiagnosticCode::MissingGlyphCluster;
        diagnostic.clusterIndex = clusterIndex;
        diagnostic.message = "missing glyph cluster";
        diagnostic.clusterText = encodeUtf8(cluster.m_codePoints);
        diagnostic.codePoints = cluster.m_codePoints;
        diagnostic.fontPath =
            attemptedFonts.empty() ? std::string() : attemptedFonts.back();
        diagnostics.push_back(std::move(diagnostic));
        continue;
      }

      if (selectedIndex > 0) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Info;
        diagnostic.code = DiagnosticCode::FontFallbackApplied;
        diagnostic.clusterIndex = clusterIndex;
        diagnostic.message = "font fallback applied";
        diagnostic.clusterText = encodeUtf8(cluster.m_codePoints);
        diagnostic.codePoints = cluster.m_codePoints;
        diagnostic.fontPath = selectedFont->m_path;
        diagnostics.push_back(std::move(diagnostic));
      }

      resolvedClusters.push_back(
          {cluster, shapedGlyphs, selectedFont, requestedRole, attemptedFonts,
           penX, selectedIndex > 0,
           clusterUsesColorRendering(*selectedFont, shapedGlyphs)});
      usedFonts.push_back(selectedFont);
      penX += computeShapedAdvance(shapedGlyphs);
    }

    metrics = buildLineMetrics({}, usedFonts, layoutContext.pixelSize, penX);
    return resolvedClusters;
  }

  GlyphRendererContext::Impl &m_context;
};

GlyphRendererContext::GlyphRendererContext()
    : m_impl(std::make_unique<Impl>()) {}

GlyphRendererContext::~GlyphRendererContext() = default;

GlyphRendererContext::GlyphRendererContext(GlyphRendererContext &&) noexcept =
    default;

GlyphRendererContext &
GlyphRendererContext::operator=(GlyphRendererContext &&) noexcept = default;

FontChainId GlyphRendererContext::registerFontFallbackChain(
    const FontFallbackChain &chain) {
  return m_impl->registerFontFallbackChain(chain);
}

FontFallbackChain
GlyphRendererContext::resolveFontFallbackChain(FontChainId fontChainId) const {
  return m_impl->resolveFontFallbackChain(fontChainId);
}

class GlyphAtlasCache::Impl {
public:
  explicit Impl(const AtlasBuildContext &context) : m_context(context) {
    if (m_context.pageWidth <= 0 || m_context.pageHeight <= 0) {
      throw std::invalid_argument("atlas page size が不正です");
    }
    if (m_context.padding < 0) {
      throw std::invalid_argument("atlas padding が不正です");
    }
  }

  GlyphAtlasCacheUpdate ensureGlyphs(const TextLineLayout &layout) {
    GlyphAtlasCacheUpdate update;
    update.summary = layout.summary;
    ensureGlyphsInternal(layout.glyphs, update);
    update.summary.atlasPageCount = m_pages.size();
    update.diagnosticsSummary =
        buildDiagnosticsSummary(update.diagnostics, update.summary);
    return update;
  }

  GlyphAtlasCacheUpdate ensureGlyphs(const MultiLineLayout &layout) {
    GlyphAtlasCacheUpdate update;
    update.summary = layout.summary;
    ensureGlyphsInternal(layout.glyphs, update);
    update.summary.atlasPageCount = m_pages.size();
    update.diagnosticsSummary =
        buildDiagnosticsSummary(update.diagnostics, update.summary);
    return update;
  }

  GlyphAtlasCacheUpdate evict(const std::vector<GlyphHandle> &handles) {
    GlyphAtlasCacheUpdate update;
    for (const GlyphHandle handle : handles) {
      auto glyphIt = m_glyphsByHandle.find(handle.value);
      if (glyphIt == m_glyphsByHandle.end()) {
        continue;
      }
      m_glyphsByHandle.erase(glyphIt);
      m_entriesByHandle.erase(handle.value);
      update.evictedHandles.push_back(handle);
    }
    repack(update);
    return update;
  }

  GlyphAtlasCacheUpdate compact() {
    GlyphAtlasCacheUpdate update;
    repack(update);
    return update;
  }

  [[nodiscard]] const AtlasBuildContext &atlasContext() const {
    return m_context;
  }

  [[nodiscard]] const std::vector<GlyphAtlasPage> &pages() const {
    return m_pages;
  }

  [[nodiscard]] std::vector<GlyphAtlasEntry> entries() const {
    std::vector<GlyphAtlasEntry> result;
    result.reserve(m_entriesByHandle.size());
    for (const auto &[handle, entry] : m_entriesByHandle) {
      (void)handle;
      result.push_back(entry);
    }
    std::sort(result.begin(), result.end(),
              [](const GlyphAtlasEntry &lhs, const GlyphAtlasEntry &rhs) {
                if (lhs.pageIndex != rhs.pageIndex) {
                  return lhs.pageIndex < rhs.pageIndex;
                }
                if (lhs.lineIndex != rhs.lineIndex) {
                  return lhs.lineIndex < rhs.lineIndex;
                }
                if (lhs.clusterIndex != rhs.clusterIndex) {
                  return lhs.clusterIndex < rhs.clusterIndex;
                }
                return lhs.glyphIndexInCluster < rhs.glyphIndexInCluster;
              });
    return result;
  }

  [[nodiscard]] const GlyphAtlasEntry *findEntry(GlyphHandle handle) const {
    const auto it = m_entriesByHandle.find(handle.value);
    if (it == m_entriesByHandle.end()) {
      return nullptr;
    }
    return &it->second;
  }

private:
  struct CachedGlyph {
    PositionedGlyph glyph;
  };

  void ensureGlyphsInternal(const std::vector<PositionedGlyph> &glyphs,
                            GlyphAtlasCacheUpdate &update) {
    for (std::size_t glyphIndex = 0; glyphIndex < glyphs.size(); ++glyphIndex) {
      const PositionedGlyph &glyph = glyphs[glyphIndex];
      if (m_entriesByHandle.contains(glyph.handle.value)) {
        continue;
      }
      const BitmapImage &image = glyph.glyph.image;
      if (image.width <= 0 || image.height <= 0) {
        continue;
      }
      if (image.width + m_context.padding * 2 > m_context.pageWidth ||
          image.height + m_context.padding * 2 > m_context.pageHeight) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Error;
        diagnostic.code = DiagnosticCode::AtlasGlyphTooLarge;
        diagnostic.lineIndex = glyph.lineIndex;
        diagnostic.clusterIndex = glyph.clusterIndex;
        diagnostic.message = "glyph does not fit atlas page";
        diagnostic.clusterText = encodeUtf8(glyph.glyph.codePoints);
        diagnostic.codePoints = glyph.glyph.codePoints;
        diagnostic.fontPath = glyph.glyph.fontPath;
        update.diagnostics.push_back(std::move(diagnostic));
        continue;
      }

      if (m_pages.empty()) {
        addPage(update);
      }
      if (m_penX + image.width + m_context.padding > m_context.pageWidth) {
        m_penX = m_context.padding;
        m_penY += m_rowHeight + m_context.padding;
        m_rowHeight = 0;
      }
      if (m_penY + image.height + m_context.padding > m_context.pageHeight) {
        addPage(update);
      }

      GlyphAtlasPage &page = m_pages[m_currentPage];
      blitBitmapImage(page.image, image, m_penX, m_penY);

      GlyphAtlasEntry entry;
      entry.handle = glyph.handle;
      entry.glyphId = glyph.glyphId;
      entry.lineIndex = glyph.lineIndex;
      entry.clusterIndex = glyph.clusterIndex;
      entry.glyphIndexInCluster = glyph.glyphIndexInCluster;
      entry.glyphIndex = glyphIndex;
      entry.pageIndex = m_currentPage;
      entry.pageHandle = page.handle;
      entry.pixelRect = {m_penX, m_penY, m_penX + image.width,
                         m_penY + image.height};
      entry.uvRect = toNormalizedUvRect(entry.pixelRect, m_context.pageWidth,
                                        m_context.pageHeight);
      entry.isColorGlyph = glyph.isColorGlyph;
      entry.materialKind = glyph.materialKind;
      entry.samplerKind = glyph.samplerKind;
      entry.blendHint = glyph.blendHint;
      m_entriesByHandle.emplace(entry.handle.value, entry);
      m_glyphsByHandle.emplace(entry.handle.value, CachedGlyph{glyph});
      update.updatedEntries.push_back(entry);
      update.dirtyRects.push_back({entry.pageHandle, entry.pixelRect});

      m_penX += image.width + m_context.padding;
      m_rowHeight = std::max(m_rowHeight, image.height);
    }
  }

  void addPage(GlyphAtlasCacheUpdate &update) {
    GlyphAtlasPage page;
    page.handle =
        GlyphAtlasPageHandle{static_cast<std::uint32_t>(m_pages.size() + 1U)};
    page.image.width = m_context.pageWidth;
    page.image.height = m_context.pageHeight;
    page.image.rgba.assign(static_cast<std::size_t>(m_context.pageWidth) *
                               static_cast<std::size_t>(m_context.pageHeight) *
                               4U,
                           0U);
    m_pages.push_back(page);
    update.newPages.push_back(page);
    m_currentPage = static_cast<int>(m_pages.size() - 1);
    m_penX = m_context.padding;
    m_penY = m_context.padding;
    m_rowHeight = 0;
  }

  void resetPacking() {
    m_pages.clear();
    m_entriesByHandle.clear();
    m_currentPage = -1;
    m_penX = 0;
    m_penY = 0;
    m_rowHeight = 0;
  }

  void repack(GlyphAtlasCacheUpdate &update) {
    std::vector<PositionedGlyph> glyphs;
    glyphs.reserve(m_glyphsByHandle.size());
    for (const auto &[handle, cached] : m_glyphsByHandle) {
      (void)handle;
      glyphs.push_back(cached.glyph);
    }
    std::sort(glyphs.begin(), glyphs.end(),
              [](const PositionedGlyph &lhs, const PositionedGlyph &rhs) {
                if (lhs.lineIndex != rhs.lineIndex) {
                  return lhs.lineIndex < rhs.lineIndex;
                }
                if (lhs.clusterIndex != rhs.clusterIndex) {
                  return lhs.clusterIndex < rhs.clusterIndex;
                }
                return lhs.glyphIndexInCluster < rhs.glyphIndexInCluster;
              });

    resetPacking();
    ensureGlyphsInternal(glyphs, update);
    update.summary.glyphCount = glyphs.size();
    update.summary.atlasPageCount = m_pages.size();
    for (const auto &[handle, cached] : m_glyphsByHandle) {
      (void)handle;
      appendUniqueFont(update.summary.usedFonts, cached.glyph.glyph.fontPath);
      if (cached.glyph.isColorGlyph) {
        ++update.summary.colorGlyphCount;
      }
      if (cached.glyph.fallbackApplied) {
        ++update.summary.fallbackCount;
      }
    }
    update.newPages = m_pages;
    update.dirtyRects.clear();
    for (const GlyphAtlasPage &page : m_pages) {
      update.dirtyRects.push_back(
          {page.handle, {0, 0, page.image.width, page.image.height}});
    }
    update.updatedEntries = entries();
    update.diagnosticsSummary =
        buildDiagnosticsSummary(update.diagnostics, update.summary);
  }

  AtlasBuildContext m_context;
  std::vector<GlyphAtlasPage> m_pages;
  std::unordered_map<std::uint64_t, GlyphAtlasEntry> m_entriesByHandle;
  std::unordered_map<std::uint64_t, CachedGlyph> m_glyphsByHandle;
  int m_currentPage = -1;
  int m_penX = 0;
  int m_penY = 0;
  int m_rowHeight = 0;
};

GlyphAtlasCache::GlyphAtlasCache(const AtlasBuildContext &context)
    : m_impl(std::make_unique<Impl>(context)) {}

GlyphAtlasCache::~GlyphAtlasCache() = default;

GlyphAtlasCache::GlyphAtlasCache(GlyphAtlasCache &&) noexcept = default;

GlyphAtlasCache &
GlyphAtlasCache::operator=(GlyphAtlasCache &&) noexcept = default;

GlyphAtlasCacheUpdate
GlyphAtlasCache::ensureGlyphs(const TextLineLayout &layout) {
  return m_impl->ensureGlyphs(layout);
}

GlyphAtlasCacheUpdate
GlyphAtlasCache::ensureGlyphs(const MultiLineLayout &layout) {
  return m_impl->ensureGlyphs(layout);
}

GlyphAtlasCacheUpdate
GlyphAtlasCache::evict(const std::vector<GlyphHandle> &handles) {
  return m_impl->evict(handles);
}

GlyphAtlasCacheUpdate GlyphAtlasCache::compact() { return m_impl->compact(); }

const AtlasBuildContext &GlyphAtlasCache::atlasContext() const {
  return m_impl->atlasContext();
}

const std::vector<GlyphAtlasPage> &GlyphAtlasCache::pages() const {
  return m_impl->pages();
}

std::vector<GlyphAtlasEntry> GlyphAtlasCache::entries() const {
  return m_impl->entries();
}

GlyphRenderer::GlyphRenderer(GlyphRendererContext &context)
    : m_impl(std::make_unique<Impl>(*context.m_impl)) {}

GlyphRenderer::~GlyphRenderer() = default;

GlyphRenderer::GlyphRenderer(GlyphRenderer &&) noexcept = default;

GlyphRenderer &GlyphRenderer::operator=(GlyphRenderer &&) noexcept = default;

TextShapeResult GlyphRenderer::shapeLine(const TextLayoutContext &context,
                                         std::string_view text) const {
  return m_impl->shapeLine(context, text);
}

MultiLineShapeResult GlyphRenderer::shapeText(const TextLayoutContext &context,
                                              std::string_view text) const {
  return m_impl->shapeText(context, text);
}

TextRasterResult
GlyphRenderer::rasterizeLine(const TextLayoutContext &context,
                             const TextShapeResult &shape) const {
  return m_impl->rasterizeLine(context, shape);
}

MultiLineRasterResult
GlyphRenderer::rasterizeText(const TextLayoutContext &context,
                             const MultiLineShapeResult &shape) const {
  return m_impl->rasterizeText(context, shape);
}

TextLineLayout GlyphRenderer::layoutLine(const TextLayoutContext &context,
                                         std::string_view text) const {
  return m_impl->layoutLine(context, text);
}

TextLineLayout GlyphRenderer::layoutLine(const TextRequest &request) const {
  return m_impl->layoutLine(makeTextLayoutContext(request), request.text);
}

TextLineLayout GlyphRenderer::layoutLine(const TextRasterResult &raster) const {
  return m_impl->layoutLine(raster);
}

AtlasBuildResult
GlyphRenderer::buildAtlas(const TextLineLayout &layout,
                          const AtlasBuildContext &context) const {
  return buildAtlasLayout(layout, context);
}

TextDrawRun GlyphRenderer::buildDrawRun(const TextLineLayout &layout,
                                        const AtlasBuildResult &atlas) const {
  return m_impl->buildDrawRun(layout, atlas);
}

TextDrawRun GlyphRenderer::buildDrawRun(const TextLineLayout &layout,
                                        GlyphAtlasCache &atlasCache) const {
  GlyphAtlasCacheUpdate update = atlasCache.ensureGlyphs(layout);
  TextDrawRun result;
  result.metrics = layout.metrics;
  result.summary = layout.summary;
  result.fallbackTrace = layout.fallbackTrace;
  result.diagnostics = layout.diagnostics;
  result.diagnostics.insert(result.diagnostics.end(),
                            update.diagnostics.begin(),
                            update.diagnostics.end());
  result.glyphs = layout.glyphs;

  for (PositionedGlyph &glyph : result.glyphs) {
    const GlyphAtlasEntry *entry = atlasCache.m_impl->findEntry(glyph.handle);
    if (entry == nullptr) {
      Diagnostic diagnostic;
      diagnostic.severity = DiagnosticSeverity::Warning;
      diagnostic.code = DiagnosticCode::AtlasEntryMissing;
      diagnostic.lineIndex = glyph.lineIndex;
      diagnostic.clusterIndex = glyph.clusterIndex;
      diagnostic.message = "atlas entry missing for glyph handle";
      diagnostic.clusterText = encodeUtf8(glyph.glyph.codePoints);
      diagnostic.codePoints = glyph.glyph.codePoints;
      diagnostic.fontPath = glyph.glyph.fontPath;
      result.diagnostics.push_back(std::move(diagnostic));
      continue;
    }
    glyph.atlasPage = entry->pageIndex;
    glyph.atlasPageHandle = entry->pageHandle;
    glyph.atlasRect = entry->pixelRect;
    glyph.uvRect = entry->uvRect;
    glyph.materialKind = entry->materialKind;
    glyph.samplerKind = entry->samplerKind;
    glyph.blendHint = entry->blendHint;
  }

  result.summary.atlasPageCount = atlasCache.pages().size();
  result.diagnosticsSummary =
      buildDiagnosticsSummary(result.diagnostics, result.summary);
  return result;
}

MultiLineLayout GlyphRenderer::layoutText(const TextLayoutContext &context,
                                          std::string_view text) const {
  return m_impl->layoutText(context, text);
}

MultiLineLayout GlyphRenderer::layoutText(const TextRequest &request) const {
  return m_impl->layoutText(makeTextLayoutContext(request), request.text);
}

MultiLineDrawRun
GlyphRenderer::buildDrawText(const TextLayoutContext &context,
                             std::string_view text,
                             const AtlasBuildContext &atlasContext) const {
  return m_impl->buildDrawText(context, text, atlasContext);
}

MultiLineDrawRun
GlyphRenderer::buildDrawText(const MultiLineLayout &layout,
                             GlyphAtlasCache &atlasCache) const {
  GlyphAtlasCacheUpdate update = atlasCache.ensureGlyphs(layout);
  MultiLineDrawRun result;
  result.lineTexts = layout.lineTexts;
  result.lineOriginsY = layout.lineOriginsY;
  result.lineSummaries = layout.lineSummaries;
  result.fallbackTrace = layout.fallbackTrace;
  result.summary = layout.summary;
  result.boundingBox = layout.boundingBox;
  result.lineAdvance = layout.lineAdvance;
  result.diagnostics.clear();
  result.diagnostics.insert(result.diagnostics.end(),
                            update.diagnostics.begin(),
                            update.diagnostics.end());
  result.lines.reserve(layout.lines.size());
  result.glyphs.reserve(layout.glyphs.size());

  for (const TextLineLayout &line : layout.lines) {
    TextDrawRun drawRun = buildDrawRun(line, atlasCache);
    const std::size_t lineIndex = result.lines.size();
    const int originY = lineIndex < layout.lineOriginsY.size()
                            ? layout.lineOriginsY[lineIndex]
                            : 0;
    for (PositionedGlyph &glyph : drawRun.glyphs) {
      glyph.lineIndex = lineIndex;
      glyph.originY += originY;
    }
    result.diagnostics.insert(result.diagnostics.end(),
                              drawRun.diagnostics.begin(),
                              drawRun.diagnostics.end());
    result.lines.push_back(std::move(drawRun));
  }
  result.glyphs = layout.glyphs;
  for (PositionedGlyph &glyph : result.glyphs) {
    const GlyphAtlasEntry *entry = atlasCache.m_impl->findEntry(glyph.handle);
    if (entry == nullptr) {
      continue;
    }
    glyph.atlasPage = entry->pageIndex;
    glyph.atlasPageHandle = entry->pageHandle;
    glyph.atlasRect = entry->pixelRect;
    glyph.uvRect = entry->uvRect;
    glyph.materialKind = entry->materialKind;
    glyph.samplerKind = entry->samplerKind;
    glyph.blendHint = entry->blendHint;
  }

  AtlasBuildResult atlasSnapshot;
  atlasSnapshot.pages = atlasCache.pages();
  atlasSnapshot.entries = atlasCache.entries();
  atlasSnapshot.summary.atlasPageCount = atlasSnapshot.pages.size();
  result.atlases.push_back(std::move(atlasSnapshot));
  result.summary.atlasPageCount = atlasCache.pages().size();
  result.diagnosticsSummary =
      buildDiagnosticsSummary(result.diagnostics, result.summary);
  return result;
}

TextDrawRun GlyphRenderer::prepareLine(const TextRequest &request,
                                       GlyphAtlasCache &atlasCache) const {
  const TextLineLayout layout = layoutLine(request);
  return buildDrawRun(layout, atlasCache);
}

MultiLineDrawRun GlyphRenderer::prepareText(const TextRequest &request,
                                            GlyphAtlasCache &atlasCache) const {
  const MultiLineLayout layout = layoutText(request);
  return buildDrawText(layout, atlasCache);
}

namespace {

template <typename T>
void appendUniqueValue(std::vector<T> &values, T value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

[[nodiscard]] bool isVisibleGlyph(const PositionedGlyph &glyph) {
  return glyph.atlasPage >= 0 && glyph.atlasRect.maxX > glyph.atlasRect.minX &&
         glyph.atlasRect.maxY > glyph.atlasRect.minY;
}

[[nodiscard]] Rect unionRect(const Rect &lhs, const Rect &rhs) {
  if (lhs.maxX <= lhs.minX || lhs.maxY <= lhs.minY) {
    return rhs;
  }
  if (rhs.maxX <= rhs.minX || rhs.maxY <= rhs.minY) {
    return lhs;
  }
  return {std::min(lhs.minX, rhs.minX), std::min(lhs.minY, rhs.minY),
          std::max(lhs.maxX, rhs.maxX), std::max(lhs.maxY, rhs.maxY)};
}

[[nodiscard]] Rect computeGlyphPixelRect(const PositionedGlyph &glyph) {
  const int width = glyph.glyph.image.width;
  const int height = glyph.glyph.image.height;
  return {glyph.originX, glyph.originY, glyph.originX + width,
          glyph.originY + height};
}

[[nodiscard]] std::uint32_t
pickRepresentativeCodePoint(const PositionedGlyph &glyph) {
  if (!glyph.glyph.codePoints.empty()) {
    return glyph.glyph.codePoints.front();
  }
  return 0U;
}

[[nodiscard]] bool bitmapUsesColorRendering(const BitmapImage &image) {
  for (std::size_t offset = 0; offset + 3U < image.rgba.size(); offset += 4U) {
    const std::uint8_t r = image.rgba[offset + 0U];
    const std::uint8_t g = image.rgba[offset + 1U];
    const std::uint8_t b = image.rgba[offset + 2U];
    if (r != g || g != b) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool diagnosticIndicatesMissingGlyph(DiagnosticCode code) {
  return code == DiagnosticCode::MissingGlyphCluster ||
         code == DiagnosticCode::AtlasEntryMissing;
}

[[nodiscard]] bool diagnosticIndicatesAtlasOverflow(DiagnosticCode code) {
  return code == DiagnosticCode::AtlasGlyphTooLarge;
}

void appendCompatibilityDiagnostics(TextCompatibilityReport &report) {
  if (report.requiresColorGlyph) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::Info;
    diagnostic.code = DiagnosticCode::ColorGlyphRequired;
    diagnostic.message = "text run requires color glyph rendering";
    report.diagnostics.push_back(std::move(diagnostic));
  }
  if (report.requiresMultiplePages) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::Info;
    diagnostic.code = DiagnosticCode::MultiPageRequired;
    diagnostic.message = "text run touches multiple atlas pages";
    report.diagnostics.push_back(std::move(diagnostic));
  }
}

[[nodiscard]] TextCompatibilityReport
analyzeCompatibilityFromGlyphs(const std::vector<PositionedGlyph> &glyphs,
                               const std::vector<FallbackTraceEntry> &trace,
                               const std::vector<Diagnostic> &diagnostics) {
  TextCompatibilityReport report;
  report.diagnostics = diagnostics;

  for (const PositionedGlyph &glyph : glyphs) {
    const bool usesColorBitmap =
        glyph.isColorGlyph || bitmapUsesColorRendering(glyph.glyph.image);
    if (!isVisibleGlyph(glyph)) {
      if (usesColorBitmap) {
        report.requiresColorGlyph = true;
        report.alphaOnlyCompatible = false;
      }
      continue;
    }
    appendUniqueValue(report.pagesUsed, glyph.atlasPage);
    appendUniqueValue(report.materialKindsUsed, glyph.materialKind);
    if (usesColorBitmap || glyph.materialKind != MaterialKind::AlphaMask ||
        glyph.blendHint != BlendHint::Coverage) {
      report.requiresColorGlyph = true;
      report.alphaOnlyCompatible = false;
    }
  }

  for (const FallbackTraceEntry &entry : trace) {
    if (entry.fallbackApplied && !entry.selectedFontPath.empty()) {
      appendUniqueFont(report.fallbackFonts, entry.selectedFontPath);
    }
  }

  for (const Diagnostic &diagnostic : diagnostics) {
    if (diagnosticIndicatesMissingGlyph(diagnostic.code)) {
      report.hasMissingGlyph = true;
    }
    if (diagnosticIndicatesAtlasOverflow(diagnostic.code)) {
      report.hasAtlasOverflow = true;
    }
  }

  std::sort(report.pagesUsed.begin(), report.pagesUsed.end());
  report.pagesUsed.erase(
      std::unique(report.pagesUsed.begin(), report.pagesUsed.end()),
      report.pagesUsed.end());
  report.requiresMultiplePages = report.pagesUsed.size() > 1U;
  report.singlePageCompatible = !report.requiresMultiplePages;
  appendCompatibilityDiagnostics(report);
  return report;
}

[[nodiscard]] TextCompatibilityReport
applyCompatibilityConstraints(TextCompatibilityReport report,
                              const CompatibilityConstraints &constraints) {
  if (constraints.maxPages > 0 &&
      static_cast<int>(report.pagesUsed.size()) > constraints.maxPages) {
    report.requiresMultiplePages = true;
    report.singlePageCompatible = false;
  }

  if (report.requiresColorGlyph && !constraints.allowColorGlyphs) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::Error;
    diagnostic.code = DiagnosticCode::CompatibilityConstraintViolation;
    diagnostic.message =
        "compatibility constraints reject color glyph rendering";
    report.diagnostics.push_back(std::move(diagnostic));
  }

  if (report.requiresMultiplePages &&
      (!constraints.allowMultiplePages ||
       static_cast<int>(report.pagesUsed.size()) > constraints.maxPages)) {
    Diagnostic diagnostic;
    diagnostic.severity = DiagnosticSeverity::Error;
    diagnostic.code = DiagnosticCode::CompatibilityConstraintViolation;
    diagnostic.message =
        "compatibility constraints reject multi-page atlas usage";
    report.diagnostics.push_back(std::move(diagnostic));
  }

  return report;
}

[[nodiscard]] TextDrawSubmission
buildSubmissionFromGlyphs(const std::vector<PositionedGlyph> &glyphs,
                          const LineMetrics &metrics, int lineAdvance,
                          const std::vector<FallbackTraceEntry> &trace,
                          const std::vector<Diagnostic> &diagnostics,
                          const Rect &fallbackBoundingBox) {
  TextDrawSubmission submission;
  submission.metrics = metrics;
  submission.lineAdvance = lineAdvance;
  submission.diagnostics = diagnostics;
  submission.compatibility =
      analyzeCompatibilityFromGlyphs(glyphs, trace, diagnostics);
  submission.diagnostics.insert(submission.diagnostics.end(),
                                submission.compatibility.diagnostics.begin(),
                                submission.compatibility.diagnostics.end());

  Rect boundingBox = fallbackBoundingBox;
  submission.glyphs.reserve(glyphs.size());
  for (const PositionedGlyph &glyph : glyphs) {
    PositionedDrawGlyph drawGlyph;
    drawGlyph.handle = glyph.handle;
    drawGlyph.codePoint = pickRepresentativeCodePoint(glyph);
    drawGlyph.glyphId = glyph.glyphId;
    drawGlyph.lineIndex = glyph.lineIndex;
    drawGlyph.atlasPage = glyph.atlasPage;
    drawGlyph.atlasPageHandle = glyph.atlasPageHandle;
    drawGlyph.materialKind = glyph.materialKind;
    drawGlyph.samplerKind = glyph.samplerKind;
    drawGlyph.blendHint = glyph.blendHint;
    drawGlyph.pixelRect = computeGlyphPixelRect(glyph);
    drawGlyph.uvRect = glyph.uvRect;
    drawGlyph.advanceX = glyph.glyph.advance;
    drawGlyph.bearingX = glyph.glyph.bearingX;
    drawGlyph.bearingY = glyph.glyph.bearingY;
    drawGlyph.isColorGlyph = glyph.isColorGlyph;
    drawGlyph.visible =
        glyph.glyph.image.width > 0 && glyph.glyph.image.height > 0;
    if (drawGlyph.visible) {
      boundingBox = unionRect(boundingBox, drawGlyph.pixelRect);
    }
    submission.glyphs.push_back(std::move(drawGlyph));
  }

  submission.boundingBox = boundingBox;
  return submission;
}

void appendConstraintDiagnostic(std::vector<Diagnostic> &diagnostics,
                                DiagnosticCode code, std::string message) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::Error;
  diagnostic.code = code;
  diagnostic.message = std::move(message);
  diagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] std::vector<AtlasPageUploadPlan>
buildUploadPlanFromCache(const GlyphAtlasCache &atlasCache,
                         const GlyphAtlasCacheUpdate &update) {
  std::vector<AtlasPageUploadPlan> plans;
  std::unordered_map<std::uint32_t, std::size_t> planIndexByHandle;

  const std::vector<GlyphAtlasPage> &pages = atlasCache.pages();
  for (std::size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
    AtlasPageUploadPlan plan;
    plan.pageHandle = pages[pageIndex].handle;
    plan.pageIndex = static_cast<int>(pageIndex);
    planIndexByHandle.emplace(plan.pageHandle.value, plans.size());
    plans.push_back(std::move(plan));
  }

  auto ensurePlan = [&](GlyphAtlasPageHandle handle) -> AtlasPageUploadPlan & {
    const auto it = planIndexByHandle.find(handle.value);
    if (it != planIndexByHandle.end()) {
      return plans[it->second];
    }
    AtlasPageUploadPlan plan;
    plan.pageHandle = handle;
    planIndexByHandle.emplace(handle.value, plans.size());
    plans.push_back(std::move(plan));
    return plans.back();
  };

  for (const GlyphAtlasPage &page : update.newPages) {
    AtlasPageUploadPlan &plan = ensurePlan(page.handle);
    plan.isNewPage = true;
    if (plan.pageIndex < 0) {
      for (std::size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
        if (pages[pageIndex].handle.value == page.handle.value) {
          plan.pageIndex = static_cast<int>(pageIndex);
          break;
        }
      }
    }
  }

  for (const GlyphAtlasDirtyRect &dirtyRect : update.dirtyRects) {
    AtlasPageUploadPlan &plan = ensurePlan(dirtyRect.pageHandle);
    plan.dirtyRects.push_back(dirtyRect.pixelRect);
  }

  std::sort(plans.begin(), plans.end(),
            [](const AtlasPageUploadPlan &lhs, const AtlasPageUploadPlan &rhs) {
              return lhs.pageIndex < rhs.pageIndex;
            });
  return plans;
}

} // namespace

TextCompatibilityReport analyzeTextCompatibility(const TextLineLayout &layout) {
  return analyzeCompatibilityFromGlyphs(layout.glyphs, layout.fallbackTrace,
                                        layout.diagnostics);
}

TextCompatibilityReport analyzeTextCompatibility(const TextDrawRun &run) {
  return analyzeCompatibilityFromGlyphs(run.glyphs, run.fallbackTrace,
                                        run.diagnostics);
}

TextCompatibilityReport
analyzeTextCompatibility(const MultiLineLayout &layout) {
  return analyzeCompatibilityFromGlyphs(layout.glyphs, layout.fallbackTrace,
                                        layout.diagnostics);
}

TextCompatibilityReport analyzeTextCompatibility(const MultiLineDrawRun &run) {
  return analyzeCompatibilityFromGlyphs(run.glyphs, run.fallbackTrace,
                                        run.diagnostics);
}

TextCompatibilityReport
analyzeTextCompatibility(const TextLineLayout &layout,
                         const CompatibilityConstraints &constraints) {
  return applyCompatibilityConstraints(analyzeTextCompatibility(layout),
                                       constraints);
}

TextCompatibilityReport
analyzeTextCompatibility(const TextDrawRun &run,
                         const CompatibilityConstraints &constraints) {
  return applyCompatibilityConstraints(analyzeTextCompatibility(run),
                                       constraints);
}

TextCompatibilityReport
analyzeTextCompatibility(const MultiLineLayout &layout,
                         const CompatibilityConstraints &constraints) {
  return applyCompatibilityConstraints(analyzeTextCompatibility(layout),
                                       constraints);
}

TextCompatibilityReport
analyzeTextCompatibility(const MultiLineDrawRun &run,
                         const CompatibilityConstraints &constraints) {
  return applyCompatibilityConstraints(analyzeTextCompatibility(run),
                                       constraints);
}

TextDrawSubmission buildTextDrawSubmission(const TextDrawRun &run) {
  return buildSubmissionFromGlyphs(run.glyphs, run.metrics,
                                   run.metrics.lineWidth, run.fallbackTrace,
                                   run.diagnostics, run.metrics.boundingBox);
}

TextDrawSubmission buildTextDrawSubmission(const MultiLineDrawRun &run) {
  LineMetrics metrics;
  if (!run.lines.empty()) {
    metrics = run.lines.front().metrics;
  }
  metrics.boundingBox = run.boundingBox;
  return buildSubmissionFromGlyphs(run.glyphs, metrics, run.lineAdvance,
                                   run.fallbackTrace, run.diagnostics,
                                   run.boundingBox);
}

std::vector<AtlasPageUploadPlan>
buildAtlasPageUploadPlan(const GlyphAtlasCache &atlasCache,
                         const GlyphAtlasCacheUpdate &update) {
  return buildUploadPlanFromCache(atlasCache, update);
}

BitmapImage normalizeAlphaMaskToWhite(const BitmapImage &image) {
  BitmapImage normalized = image;
  for (std::size_t offset = 0; offset + 3U < normalized.rgba.size();
       offset += 4U) {
    const std::uint8_t coverage = std::max(
        std::max(normalized.rgba[offset + 0U], normalized.rgba[offset + 1U]),
        std::max(normalized.rgba[offset + 2U], normalized.rgba[offset + 3U]));
    normalized.rgba[offset + 0U] = 255U;
    normalized.rgba[offset + 1U] = 255U;
    normalized.rgba[offset + 2U] = 255U;
    normalized.rgba[offset + 3U] = coverage;
  }
  return normalized;
}

bool isAlphaOnlyCompatible(const TextDrawRun &run) {
  return analyzeTextCompatibility(run).alphaOnlyCompatible;
}

bool isAlphaOnlyCompatible(const MultiLineDrawRun &run) {
  return analyzeTextCompatibility(run).alphaOnlyCompatible;
}

bool isSinglePageCompatible(const TextDrawRun &run, int requiredPage) {
  const TextCompatibilityReport report = analyzeTextCompatibility(run);
  if (!report.singlePageCompatible || report.pagesUsed.empty()) {
    return false;
  }
  return report.pagesUsed.front() == requiredPage;
}

bool isSinglePageCompatible(const MultiLineDrawRun &run, int requiredPage) {
  const TextCompatibilityReport report = analyzeTextCompatibility(run);
  if (!report.singlePageCompatible || report.pagesUsed.empty()) {
    return false;
  }
  return report.pagesUsed.front() == requiredPage;
}

namespace {

constexpr std::array<TextSeedRange, 1> ASCII_RANGES = {{{0x20U, 0x7EU}}};
constexpr std::array<TextSeedRange, 1> BASIC_LATIN_RANGES = {{{0x20U, 0xFFU}}};
constexpr std::array<TextSeedRange, 1> CJK_SYMBOLS_PUNCTUATION_RANGES = {
    {{0x3000U, 0x303FU}}};
constexpr std::array<TextSeedRange, 1> HIRAGANA_RANGES = {{{0x3040U, 0x309FU}}};
constexpr std::array<TextSeedRange, 1> KATAKANA_RANGES = {{{0x30A0U, 0x30FFU}}};
constexpr std::array<TextSeedRange, 1> KATAKANA_PHONETIC_EXTENSION_RANGES = {
    {{0x31F0U, 0x31FFU}}};
constexpr std::array<TextSeedRange, 1> HALFWIDTH_FULLWIDTH_RANGES = {
    {{0xFF00U, 0xFFEFU}}};
constexpr std::array<TextSeedRange, 1> CJK_UNIFIED_IDEOGRAPHS_RANGES = {
    {{0x4E00U, 0x9FFFU}}};
constexpr std::array<TextSeedRange, 2> CHINESE_RANGES = {
    {{0x3400U, 0x4DBFU}, {0x4E00U, 0x9FFFU}}};
constexpr std::array<TextSeedRange, 1> NERD_FONT_RANGES = {
    {{0xE000U, 0xF8FFU}}};
constexpr std::array<TextSeedRange, 2> EMOJI_SYMBOL_RANGES = {
    {{0x2600U, 0x26FFU}, {0x2700U, 0x27BFU}}};
constexpr std::array<TextSeedRange, 1> EMOJI_SUPPLEMENT_RANGES = {
    {{0x1F300U, 0x1FAFFU}}};
constexpr std::array<TextSeedRange, 6> JAPANESE_RANGES = {{{0x3000U, 0x303FU},
                                                           {0x3040U, 0x309FU},
                                                           {0x30A0U, 0x30FFU},
                                                           {0x31F0U, 0x31FFU},
                                                           {0x4E00U, 0x9FFFU},
                                                           {0xFF00U, 0xFFEFU}}};
constexpr std::array<TextSeedRange, 3> EMOJI_RANGES = {
    {{0x2600U, 0x26FFU}, {0x2700U, 0x27BFU}, {0x1F300U, 0x1FAFFU}}};

[[nodiscard]] constexpr std::size_t
countSeedPresetCodePoints(std::span<const TextSeedRange> ranges) {
  std::size_t count = 0;
  for (const TextSeedRange &range : ranges) {
    if (range.end < range.begin) {
      continue;
    }
    count += static_cast<std::size_t>(range.end - range.begin + 1U);
  }
  return count;
}

[[nodiscard]] TextSeedPresetInfo
makePresetInfo(TextSeedPreset preset, std::span<const TextSeedRange> ranges,
               std::size_t typicalGlyphCountAt64Px,
               std::size_t typicalAtlasPagesAt64Px,
               double typicalBakeMillisecondsAt64Px,
               std::string_view description, std::string_view costHint) {
  TextSeedPresetInfo info;
  info.preset = preset;
  info.ranges = ranges;
  info.estimatedCodePointCount = countSeedPresetCodePoints(ranges);
  info.typicalGlyphCountAt64Px = typicalGlyphCountAt64Px;
  info.typicalAtlasPagesAt64Px = typicalAtlasPagesAt64Px;
  info.typicalBakeMillisecondsAt64Px = typicalBakeMillisecondsAt64Px;
  info.description = description;
  info.costHint = costHint;
  return info;
}

void appendCodePointIfMissing(std::vector<std::uint32_t> &codePoints,
                              std::uint32_t codePoint) {
  if (std::find(codePoints.begin(), codePoints.end(), codePoint) ==
      codePoints.end()) {
    codePoints.push_back(codePoint);
  }
}

void appendCodePointRange(std::vector<std::uint32_t> &codePoints,
                          std::uint32_t begin, std::uint32_t end) {
  if (begin > end) {
    std::swap(begin, end);
  }
  for (std::uint32_t codePoint = begin; codePoint <= end; ++codePoint) {
    codePoints.push_back(codePoint);
    if (codePoint == std::numeric_limits<std::uint32_t>::max()) {
      break;
    }
  }
}

void appendSeedPreset(std::vector<std::uint32_t> &codePoints,
                      TextSeedPreset preset) {
  const TextSeedPresetInfo info = describeTextSeedPreset(preset);
  for (const TextSeedRange &range : info.ranges) {
    appendCodePointRange(codePoints, range.begin, range.end);
  }
  switch (preset) {
    case TextSeedPreset::Ascii:
    case TextSeedPreset::BasicLatin:
    case TextSeedPreset::CjkSymbolsPunctuation:
    case TextSeedPreset::Hiragana:
    case TextSeedPreset::Katakana:
    case TextSeedPreset::KatakanaPhoneticExtensions:
    case TextSeedPreset::HalfwidthFullwidthForms:
    case TextSeedPreset::CjkUnifiedIdeographs:
    case TextSeedPreset::Japanese:
    case TextSeedPreset::Chinese:
    case TextSeedPreset::NerdFont:
    case TextSeedPreset::EmojiSymbols:
    case TextSeedPreset::EmojiSupplement:
    case TextSeedPreset::Emoji:
      break;
  }

  switch (preset) {
    case TextSeedPreset::EmojiSymbols:
    case TextSeedPreset::EmojiSupplement:
    case TextSeedPreset::Emoji:
      appendCodePointIfMissing(codePoints, 0x200DU);
      appendCodePointIfMissing(codePoints, 0xFE0FU);
      break;
    case TextSeedPreset::Ascii:
    case TextSeedPreset::BasicLatin:
    case TextSeedPreset::CjkSymbolsPunctuation:
    case TextSeedPreset::Hiragana:
    case TextSeedPreset::Katakana:
    case TextSeedPreset::KatakanaPhoneticExtensions:
    case TextSeedPreset::HalfwidthFullwidthForms:
    case TextSeedPreset::CjkUnifiedIdeographs:
    case TextSeedPreset::Japanese:
    case TextSeedPreset::Chinese:
    case TextSeedPreset::NerdFont:
      break;
  }
}

[[nodiscard]] std::string
encodeSeedText(const std::vector<std::uint32_t> &codePoints,
               const std::vector<std::string> &texts) {
  std::string text;
  for (std::uint32_t codePoint : codePoints) {
    text += encodeUtf8(codePoint);
  }
  for (const std::string &seedText : texts) {
    text += seedText;
  }
  return text;
}

[[nodiscard]] TextRequest
makeAtlasBackedRequest(const TextAtlasOptions &options, std::string text,
                       const TextPrepareOptions &prepareOptions) {
  TextRequest request;
  request.text = std::move(text);
  request.fontChainId = options.fontChainId;
  request.paletteIndex = options.paletteIndex;
  request.pixelSize = options.pixelSize;
  request.verifyMetrics = prepareOptions.verifyMetrics;
  request.layoutOptions = prepareOptions.layoutOptions;
  return request;
}

[[nodiscard]] std::vector<Diagnostic>
collectPreparedDiagnostics(const std::vector<Diagnostic> &layoutDiagnostics,
                           const std::vector<Diagnostic> &drawDiagnostics,
                           const std::vector<Diagnostic> &atlasDiagnostics) {
  std::vector<Diagnostic> diagnostics = layoutDiagnostics;
  diagnostics.insert(diagnostics.end(), drawDiagnostics.begin(),
                     drawDiagnostics.end());
  diagnostics.insert(diagnostics.end(), atlasDiagnostics.begin(),
                     atlasDiagnostics.end());
  return diagnostics;
}

struct BakedGlyphKey {
  std::string m_fontPath;
  std::uint32_t m_glyphId = 0;
  unsigned int m_pixelSize = 0;
  unsigned int m_paletteIndex = 0;
  bool m_isColorGlyph = false;

  bool operator==(const BakedGlyphKey &other) const {
    return m_fontPath == other.m_fontPath && m_glyphId == other.m_glyphId &&
           m_pixelSize == other.m_pixelSize &&
           m_paletteIndex == other.m_paletteIndex &&
           m_isColorGlyph == other.m_isColorGlyph;
  }
};

struct BakedGlyphKeyHasher {
  std::size_t operator()(const BakedGlyphKey &key) const {
    std::size_t hash = 0;
    hash = static_cast<std::size_t>(fnv1aAppend(hash, key.m_fontPath));
    hash = static_cast<std::size_t>(
        fnv1aAppend(hash, static_cast<std::uint64_t>(key.m_glyphId)));
    hash = static_cast<std::size_t>(
        fnv1aAppend(hash, static_cast<std::uint64_t>(key.m_pixelSize)));
    hash = static_cast<std::size_t>(
        fnv1aAppend(hash, static_cast<std::uint64_t>(key.m_paletteIndex)));
    hash = static_cast<std::size_t>(fnv1aAppend(
        hash, static_cast<std::uint64_t>(key.m_isColorGlyph ? 1U : 0U)));
    return hash;
  }
};

[[nodiscard]] BakedGlyphKey makeBakedGlyphKey(std::string_view fontPath,
                                              std::uint32_t glyphId,
                                              unsigned int pixelSize,
                                              unsigned int paletteIndex,
                                              bool isColorGlyph) {
  return {std::string(fontPath), glyphId, pixelSize, paletteIndex,
          isColorGlyph};
}

[[nodiscard]] BakedGlyphKey makeBakedGlyphKey(const PositionedGlyph &glyph,
                                              unsigned int pixelSize,
                                              unsigned int paletteIndex) {
  return makeBakedGlyphKey(glyph.glyph.fontPath, glyph.glyphId, pixelSize,
                           paletteIndex, glyph.isColorGlyph);
}

[[nodiscard]] std::vector<std::string>
collectMissingGlyphs(const std::vector<Diagnostic> &diagnostics) {
  std::vector<std::string> missingGlyphs;
  for (const Diagnostic &diagnostic : diagnostics) {
    if (diagnostic.code != DiagnosticCode::MissingGlyphCluster &&
        diagnostic.code != DiagnosticCode::AtlasEntryMissing) {
      continue;
    }
    if (!diagnostic.clusterText.empty()) {
      appendUniqueFont(missingGlyphs, diagnostic.clusterText);
      continue;
    }
    if (!diagnostic.codePoints.empty()) {
      appendUniqueFont(missingGlyphs, encodeUtf8(diagnostic.codePoints));
    }
  }
  return missingGlyphs;
}

[[nodiscard]] std::uint64_t
computeBakedAtlasRevision(const TextAtlasBakeRequest &request,
                          const std::vector<GlyphAtlasEntry> &entries,
                          const std::vector<GlyphAtlasPage> &pages) {
  std::uint64_t hash = 0;
  hash =
      fnv1aAppend(hash, static_cast<std::uint64_t>(request.fontChainId.value));
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(request.pixelSize));
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(request.paletteIndex));
  hash = fnv1aAppend(
      hash, static_cast<std::uint64_t>(request.atlasContext.pageWidth));
  hash = fnv1aAppend(
      hash, static_cast<std::uint64_t>(request.atlasContext.pageHeight));
  hash = fnv1aAppend(hash,
                     static_cast<std::uint64_t>(request.atlasContext.padding));
  hash = fnv1aAppend(
      hash, static_cast<std::uint64_t>(request.bakeBudget.maxSeedCodePoints));
  hash = fnv1aAppend(hash,
                     static_cast<std::uint64_t>(request.bakeBudget.maxGlyphs));
  hash = fnv1aAppend(hash, static_cast<std::uint64_t>(
                               std::max(request.bakeBudget.maxPages, 0)));
  hash =
      fnv1aAppend(hash, static_cast<std::uint64_t>(
                            request.bakeBudget.maxBakeMilliseconds * 1000.0));
  for (const GlyphAtlasEntry &entry : entries) {
    hash = fnv1aAppend(hash, entry.handle.value);
    hash =
        fnv1aAppend(hash, static_cast<std::uint64_t>(entry.pageHandle.value));
    hash = fnv1aAppend(hash, static_cast<std::uint64_t>(entry.pixelRect.minX));
    hash = fnv1aAppend(hash, static_cast<std::uint64_t>(entry.pixelRect.minY));
    hash = fnv1aAppend(hash, static_cast<std::uint64_t>(entry.pixelRect.maxX));
    hash = fnv1aAppend(hash, static_cast<std::uint64_t>(entry.pixelRect.maxY));
  }
  for (const GlyphAtlasPage &page : pages) {
    hash = fnv1aAppend(hash, static_cast<std::uint64_t>(page.handle.value));
    hash = fnv1aAppend(hash, static_cast<std::uint64_t>(page.image.width));
    hash = fnv1aAppend(hash, static_cast<std::uint64_t>(page.image.height));
  }
  return hash;
}

void appendBakedAtlasMismatch(std::vector<Diagnostic> &diagnostics,
                              const std::string &message) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::Error;
  diagnostic.code = DiagnosticCode::BakedAtlasMismatch;
  diagnostic.message = message;
  diagnostics.push_back(std::move(diagnostic));
}

void appendBakeBudgetDiagnostic(std::vector<Diagnostic> &diagnostics,
                                const std::string &message) {
  Diagnostic diagnostic;
  diagnostic.severity = DiagnosticSeverity::Warning;
  diagnostic.code = DiagnosticCode::BakeBudgetExceeded;
  diagnostic.message = message;
  diagnostics.push_back(std::move(diagnostic));
}

[[nodiscard]] TextAtlasBakeStats buildBakeStats(
    const std::vector<std::uint32_t> &seedCodePoints,
    std::size_t processedSeedCodePointCount, std::size_t seedTextCount,
    const std::vector<GlyphAtlasEntry> &entries,
    const std::vector<GlyphAtlasPage> &pages,
    const std::vector<Diagnostic> &diagnostics, double elapsedMilliseconds) {
  TextAtlasBakeStats stats;
  stats.seedCodePointCount = seedCodePoints.size();
  stats.processedSeedCodePointCount = processedSeedCodePointCount;
  stats.seedTextCount = seedTextCount;
  stats.resolvedGlyphCount = entries.size();
  stats.pageCount = pages.size();
  stats.elapsedMilliseconds = elapsedMilliseconds;
  for (const GlyphAtlasEntry &entry : entries) {
    if (entry.materialKind == MaterialKind::ColorBitmap) {
      ++stats.colorBitmapGlyphCount;
      continue;
    }
    ++stats.alphaMaskGlyphCount;
  }
  stats.missingGlyphCount = collectMissingGlyphs(diagnostics).size();
  for (const Diagnostic &diagnostic : diagnostics) {
    if (diagnostic.code == DiagnosticCode::BakeBudgetExceeded) {
      stats.budgetExceeded = true;
      break;
    }
  }
  return stats;
}

} // namespace

TextSeedPresetInfo describeTextSeedPreset(TextSeedPreset preset) {
  switch (preset) {
    case TextSeedPreset::Ascii:
      return makePresetInfo(TextSeedPreset::Ascii, ASCII_RANGES, 95, 1, 2.0,
                            "Printable ASCII (U+0020-U+007E).", "VeryLight");
    case TextSeedPreset::BasicLatin:
      return makePresetInfo(TextSeedPreset::BasicLatin, BASIC_LATIN_RANGES, 224,
                            1, 4.0,
                            "Basic Latin supplement for Latin-1 style UI text "
                            "(U+0020-U+00FF).",
                            "Light");
    case TextSeedPreset::CjkSymbolsPunctuation:
      return makePresetInfo(TextSeedPreset::CjkSymbolsPunctuation,
                            CJK_SYMBOLS_PUNCTUATION_RANGES, 64, 1, 3.0,
                            "CJK symbols and punctuation (U+3000-U+303F).",
                            "VeryLight");
    case TextSeedPreset::Hiragana:
      return makePresetInfo(TextSeedPreset::Hiragana, HIRAGANA_RANGES, 96, 1,
                            5.0, "Hiragana block (U+3040-U+309F).", "Light");
    case TextSeedPreset::Katakana:
      return makePresetInfo(TextSeedPreset::Katakana, KATAKANA_RANGES, 96, 1,
                            5.0, "Katakana block (U+30A0-U+30FF).", "Light");
    case TextSeedPreset::KatakanaPhoneticExtensions:
      return makePresetInfo(TextSeedPreset::KatakanaPhoneticExtensions,
                            KATAKANA_PHONETIC_EXTENSION_RANGES, 16, 1, 2.0,
                            "Katakana Phonetic Extensions (U+31F0-U+31FF).",
                            "VeryLight");
    case TextSeedPreset::HalfwidthFullwidthForms:
      return makePresetInfo(TextSeedPreset::HalfwidthFullwidthForms,
                            HALFWIDTH_FULLWIDTH_RANGES, 240, 1, 6.0,
                            "Halfwidth and Fullwidth Forms (U+FF00-U+FFEF).",
                            "Light");
    case TextSeedPreset::CjkUnifiedIdeographs:
      return makePresetInfo(TextSeedPreset::CjkUnifiedIdeographs,
                            CJK_UNIFIED_IDEOGRAPHS_RANGES, 20992, 60, 800.0,
                            "CJK Unified Ideographs (U+4E00-U+9FFF).", "Heavy");
    case TextSeedPreset::Japanese:
      return makePresetInfo(
          TextSeedPreset::Japanese, JAPANESE_RANGES, 21504, 64, 900.0,
          "Japanese aggregate preset: CJK punctuation, Hiragana, Katakana, "
          "Katakana extensions, CJK ideographs, Halfwidth/Fullwidth forms.",
          "Heavy");
    case TextSeedPreset::Chinese:
      return makePresetInfo(
          TextSeedPreset::Chinese, CHINESE_RANGES, 27584, 84, 1200.0,
          "Chinese aggregate preset: CJK Extension A and Unified Ideographs.",
          "Heavy");
    case TextSeedPreset::NerdFont:
      return makePresetInfo(TextSeedPreset::NerdFont, NERD_FONT_RANGES, 6400, 9,
                            90.0,
                            "Private Use Area range often used by NerdFont "
                            "glyphs (U+E000-U+F8FF).",
                            "Medium");
    case TextSeedPreset::EmojiSymbols: {
      TextSeedPresetInfo info = makePresetInfo(
          TextSeedPreset::EmojiSymbols, EMOJI_SYMBOL_RANGES, 448, 4, 30.0,
          "Miscellaneous Symbols and Dingbats emoji bases. "
          "Also requires U+200D and U+FE0F during bake.",
          "Medium");
      info.estimatedCodePointCount += 2;
      return info;
    }
    case TextSeedPreset::EmojiSupplement: {
      TextSeedPresetInfo info =
          makePresetInfo(TextSeedPreset::EmojiSupplement,
                         EMOJI_SUPPLEMENT_RANGES, 2048, 18, 180.0,
                         "Supplemental emoji-heavy range (U+1F300-U+1FAFF). "
                         "Also requires U+200D and U+FE0F during bake.",
                         "Heavy");
      info.estimatedCodePointCount += 2;
      return info;
    }
    case TextSeedPreset::Emoji: {
      TextSeedPresetInfo info = makePresetInfo(
          TextSeedPreset::Emoji, EMOJI_RANGES, 2496, 22, 220.0,
          "Emoji aggregate preset: symbols + supplemental emoji ranges. "
          "Also injects U+200D and U+FE0F.",
          "Heavy");
      info.estimatedCodePointCount += 2;
      return info;
    }
  }
  return {};
}

std::vector<std::uint32_t>
makeTextSeedCodepoints(const TextSeedRequest &seedRequest) {
  std::vector<std::uint32_t> codePoints;
  for (TextSeedPreset preset : seedRequest.presets) {
    appendSeedPreset(codePoints, preset);
  }
  for (const TextSeedRange &range : seedRequest.customRanges) {
    appendCodePointRange(codePoints, range.begin, range.end);
  }
  for (const std::string &text : seedRequest.texts) {
    for (std::uint32_t codePoint : decodeUtf8(text)) {
      appendCodePointIfMissing(codePoints, codePoint);
    }
  }
  std::sort(codePoints.begin(), codePoints.end());
  codePoints.erase(std::unique(codePoints.begin(), codePoints.end()),
                   codePoints.end());
  return codePoints;
}

class TextAtlas::Impl {
public:
  explicit Impl(const TextAtlasOptions &options)
      : m_options(options), m_cache(options.atlasContext) {}

  TextAtlasOptions m_options;
  GlyphAtlasCache m_cache;
  GlyphAtlasCacheUpdate m_lastUpdate;
  std::vector<AtlasPageUploadPlan> m_lastUploadPlan;
};

TextAtlas::TextAtlas(const TextAtlasOptions &options)
    : m_impl(std::make_unique<Impl>(options)) {}

TextAtlas::~TextAtlas() = default;

TextAtlas::TextAtlas(TextAtlas &&) noexcept = default;

TextAtlas &TextAtlas::operator=(TextAtlas &&) noexcept = default;

const TextAtlasOptions &TextAtlas::options() const { return m_impl->m_options; }

const GlyphAtlasCache &TextAtlas::cache() const { return m_impl->m_cache; }

GlyphAtlasCache &TextAtlas::cache() { return m_impl->m_cache; }

const GlyphAtlasCacheUpdate &TextAtlas::lastUpdate() const {
  return m_impl->m_lastUpdate;
}

const std::vector<AtlasPageUploadPlan> &TextAtlas::lastUploadPlan() const {
  return m_impl->m_lastUploadPlan;
}

const std::vector<GlyphAtlasPage> &TextAtlas::pages() const {
  return m_impl->m_cache.pages();
}

std::vector<GlyphAtlasEntry> TextAtlas::entries() const {
  return m_impl->m_cache.entries();
}

class BakedTextAtlas::Impl {
public:
  explicit Impl(const TextAtlasBakeRequest &request) : m_request(request) {}

  [[nodiscard]] const GlyphAtlasEntry *
  findEntry(const PositionedGlyph &glyph) const {
    const BakedGlyphKey key =
        makeBakedGlyphKey(glyph, m_request.pixelSize, m_request.paletteIndex);
    const auto it = m_entriesByKey.find(key);
    if (it == m_entriesByKey.end()) {
      return nullptr;
    }
    return &m_entries[it->second];
  }

  TextAtlasBakeRequest m_request;
  std::vector<GlyphAtlasPage> m_pages;
  std::vector<GlyphAtlasEntry> m_entries;
  std::vector<AtlasPageUploadPlan> m_uploadPlan;
  TextCompatibilityReport m_compatibility;
  TextAtlasBakeStats m_bakeStats;
  LineSummary m_summary;
  DiagnosticsSummary m_diagnosticsSummary;
  std::vector<Diagnostic> m_diagnostics;
  std::vector<AtlasPixelFormat> m_pagePixelFormats;
  std::vector<std::vector<MaterialKind>> m_pageMaterialKinds;
  std::unordered_map<BakedGlyphKey, std::size_t, BakedGlyphKeyHasher>
      m_entriesByKey;
  std::uint64_t m_revision = 0;
};

BakedTextAtlas::BakedTextAtlas(const TextAtlasBakeRequest &request)
    : m_impl(std::make_unique<Impl>(request)) {}

BakedTextAtlas::~BakedTextAtlas() = default;

BakedTextAtlas::BakedTextAtlas(BakedTextAtlas &&) noexcept = default;

BakedTextAtlas &BakedTextAtlas::operator=(BakedTextAtlas &&) noexcept = default;

const TextAtlasBakeRequest &BakedTextAtlas::request() const {
  return m_impl->m_request;
}

const std::vector<GlyphAtlasPage> &BakedTextAtlas::pages() const {
  return m_impl->m_pages;
}

std::vector<BitmapImage> BakedTextAtlas::pageImages() const {
  std::vector<BitmapImage> images;
  images.reserve(m_impl->m_pages.size());
  for (const GlyphAtlasPage &page : m_impl->m_pages) {
    images.push_back(page.image);
  }
  return images;
}

const std::vector<GlyphAtlasEntry> &BakedTextAtlas::entries() const {
  return m_impl->m_entries;
}

const std::vector<AtlasPageUploadPlan> &BakedTextAtlas::uploadPlan() const {
  return m_impl->m_uploadPlan;
}

std::uint64_t BakedTextAtlas::revision() const { return m_impl->m_revision; }

AtlasUvOrigin BakedTextAtlas::uvOrigin() const { return atlasUvOrigin(); }

AtlasPixelFormat
BakedTextAtlas::pixelFormatForPage(std::size_t pageIndex) const {
  if (pageIndex >= m_impl->m_pagePixelFormats.size()) {
    throw std::out_of_range("pageIndex が範囲外です");
  }
  return m_impl->m_pagePixelFormats[pageIndex];
}

const std::vector<MaterialKind> &
BakedTextAtlas::materialKindsForPage(std::size_t pageIndex) const {
  if (pageIndex >= m_impl->m_pageMaterialKinds.size()) {
    throw std::out_of_range("pageIndex が範囲外です");
  }
  return m_impl->m_pageMaterialKinds[pageIndex];
}

const TextCompatibilityReport &BakedTextAtlas::compatibility() const {
  return m_impl->m_compatibility;
}

const TextAtlasBakeStats &BakedTextAtlas::bakeStats() const {
  return m_impl->m_bakeStats;
}

const LineSummary &BakedTextAtlas::summary() const { return m_impl->m_summary; }

const DiagnosticsSummary &BakedTextAtlas::diagnosticsSummary() const {
  return m_impl->m_diagnosticsSummary;
}

const std::vector<Diagnostic> &BakedTextAtlas::diagnostics() const {
  return m_impl->m_diagnostics;
}

class TextSystem::Impl {
public:
  Impl() : m_renderer(m_context) {}

  GlyphRendererContext m_context;
  GlyphRenderer m_renderer;
};

TextSystem::TextSystem() : m_impl(std::make_unique<Impl>()) {}

TextSystem::~TextSystem() = default;

TextSystem::TextSystem(TextSystem &&) noexcept = default;

TextSystem &TextSystem::operator=(TextSystem &&) noexcept = default;

FontChainId
TextSystem::registerFontFallbackChain(const FontFallbackChain &chain) {
  return m_impl->m_context.registerFontFallbackChain(chain);
}

FontFallbackChain
TextSystem::resolveFontFallbackChain(FontChainId fontChainId) const {
  return m_impl->m_context.resolveFontFallbackChain(fontChainId);
}

TextAtlas TextSystem::createAtlas(const TextAtlasOptions &options) const {
  return TextAtlas(options);
}

BakedTextAtlas
TextSystem::bakeTextAtlas(const TextAtlasBakeRequest &request) const {
  const auto startTime = std::chrono::steady_clock::now();
  BakedTextAtlas bakedAtlas(request);
  TextSeedRequest seedRequest;
  seedRequest.presets = request.seedPresets;
  seedRequest.customRanges = request.customRanges;
  seedRequest.texts = request.seedTexts;
  const std::vector<std::uint32_t> allCodePoints =
      makeTextSeedCodepoints(seedRequest);
  std::vector<std::uint32_t> codePoints = allCodePoints;
  if (request.bakeBudget.maxSeedCodePoints > 0 &&
      codePoints.size() > request.bakeBudget.maxSeedCodePoints) {
    codePoints.resize(request.bakeBudget.maxSeedCodePoints);
    appendBakeBudgetDiagnostic(
        bakedAtlas.m_impl->m_diagnostics,
        "seed code point budget exceeded; bake input was truncated");
  }
  if (codePoints.empty() && request.seedTexts.empty()) {
    appendConstraintDiagnostic(bakedAtlas.m_impl->m_diagnostics,
                               DiagnosticCode::CompatibilityConstraintViolation,
                               "text atlas bake request is empty");
    bakedAtlas.m_impl->m_compatibility.diagnostics =
        bakedAtlas.m_impl->m_diagnostics;
    bakedAtlas.m_impl->m_bakeStats = buildBakeStats(
        allCodePoints, codePoints.size(), request.seedTexts.size(),
        bakedAtlas.m_impl->m_entries, bakedAtlas.m_impl->m_pages,
        bakedAtlas.m_impl->m_diagnostics, 0.0);
    bakedAtlas.m_impl->m_diagnosticsSummary = buildDiagnosticsSummary(
        bakedAtlas.m_impl->m_diagnostics, bakedAtlas.m_impl->m_summary);
    return bakedAtlas;
  }

  TextRequest textRequest;
  textRequest.text = encodeSeedText(codePoints, request.seedTexts);
  textRequest.fontChainId = request.fontChainId;
  textRequest.paletteIndex = request.paletteIndex;
  textRequest.pixelSize = request.pixelSize;

  const MultiLineLayout layout = m_impl->m_renderer.layoutText(textRequest);
  GlyphAtlasCache cache(request.atlasContext);
  const GlyphAtlasCacheUpdate update = cache.ensureGlyphs(layout);

  bakedAtlas.m_impl->m_pages = cache.pages();
  bakedAtlas.m_impl->m_entries = cache.entries();
  bakedAtlas.m_impl->m_uploadPlan = buildAtlasPageUploadPlan(cache, update);
  bakedAtlas.m_impl->m_compatibility = analyzeTextCompatibility(layout);
  bakedAtlas.m_impl->m_summary = layout.summary;
  bakedAtlas.m_impl->m_summary.atlasPageCount =
      bakedAtlas.m_impl->m_pages.size();
  bakedAtlas.m_impl->m_diagnostics = collectPreparedDiagnostics(
      layout.diagnostics, bakedAtlas.m_impl->m_compatibility.diagnostics,
      update.diagnostics);
  bakedAtlas.m_impl->m_compatibility.diagnostics =
      bakedAtlas.m_impl->m_diagnostics;
  bakedAtlas.m_impl->m_diagnosticsSummary = buildDiagnosticsSummary(
      bakedAtlas.m_impl->m_diagnostics, bakedAtlas.m_impl->m_summary);
  bakedAtlas.m_impl->m_pagePixelFormats.assign(
      bakedAtlas.m_impl->m_pages.size(),
      AtlasPixelFormat::AlphaMaskStraightRgba);
  bakedAtlas.m_impl->m_pageMaterialKinds.resize(
      bakedAtlas.m_impl->m_pages.size());

  std::unordered_map<std::uint64_t, const PositionedGlyph *> glyphByHandle;
  for (const PositionedGlyph &glyph : layout.glyphs) {
    glyphByHandle.emplace(glyph.handle.value, &glyph);
  }

  for (std::size_t entryIndex = 0;
       entryIndex < bakedAtlas.m_impl->m_entries.size(); ++entryIndex) {
    const GlyphAtlasEntry &entry = bakedAtlas.m_impl->m_entries[entryIndex];
    const auto glyphIt = glyphByHandle.find(entry.handle.value);
    if (glyphIt == glyphByHandle.end()) {
      continue;
    }
    const PositionedGlyph &glyph = *glyphIt->second;
    bakedAtlas.m_impl->m_entriesByKey.emplace(
        makeBakedGlyphKey(glyph, request.pixelSize, request.paletteIndex),
        entryIndex);
    if (entry.pageIndex >= 0 &&
        static_cast<std::size_t>(entry.pageIndex) <
            bakedAtlas.m_impl->m_pageMaterialKinds.size()) {
      appendUniqueValue(bakedAtlas.m_impl->m_pageMaterialKinds[entry.pageIndex],
                        entry.materialKind);
      if (entry.materialKind == MaterialKind::ColorBitmap) {
        bakedAtlas.m_impl->m_pagePixelFormats[entry.pageIndex] =
            AtlasPixelFormat::ColorStraightRgba;
      }
    }
  }

  const auto endTime = std::chrono::steady_clock::now();
  const double elapsedMilliseconds =
      std::chrono::duration<double, std::milli>(endTime - startTime).count();
  if (request.bakeBudget.maxGlyphs > 0 &&
      bakedAtlas.m_impl->m_entries.size() > request.bakeBudget.maxGlyphs) {
    appendBakeBudgetDiagnostic(
        bakedAtlas.m_impl->m_diagnostics,
        "glyph budget exceeded; baked atlas contains more glyphs than allowed");
  }
  if (request.bakeBudget.maxPages > 0 &&
      static_cast<int>(bakedAtlas.m_impl->m_pages.size()) >
          request.bakeBudget.maxPages) {
    appendBakeBudgetDiagnostic(
        bakedAtlas.m_impl->m_diagnostics,
        "page budget exceeded; baked atlas requires more pages than allowed");
  }
  if (request.bakeBudget.maxBakeMilliseconds > 0.0 &&
      elapsedMilliseconds > request.bakeBudget.maxBakeMilliseconds) {
    appendBakeBudgetDiagnostic(
        bakedAtlas.m_impl->m_diagnostics,
        "bake time budget exceeded; atlas bake took longer than allowed");
  }
  bakedAtlas.m_impl->m_bakeStats =
      buildBakeStats(allCodePoints, codePoints.size(), request.seedTexts.size(),
                     bakedAtlas.m_impl->m_entries, bakedAtlas.m_impl->m_pages,
                     bakedAtlas.m_impl->m_diagnostics, elapsedMilliseconds);
  bakedAtlas.m_impl->m_compatibility.diagnostics =
      bakedAtlas.m_impl->m_diagnostics;
  bakedAtlas.m_impl->m_diagnosticsSummary = buildDiagnosticsSummary(
      bakedAtlas.m_impl->m_diagnostics, bakedAtlas.m_impl->m_summary);

  bakedAtlas.m_impl->m_revision = computeBakedAtlasRevision(
      request, bakedAtlas.m_impl->m_entries, bakedAtlas.m_impl->m_pages);
  return bakedAtlas;
}

TextAtlasSeedResult
TextSystem::seedAtlas(TextAtlas &atlas,
                      const TextSeedRequest &seedRequest) const {
  TextAtlasSeedResult result;
  result.seedRequest = seedRequest;
  result.codePoints = makeTextSeedCodepoints(seedRequest);
  if (result.codePoints.empty() && seedRequest.texts.empty()) {
    appendConstraintDiagnostic(result.diagnostics,
                               DiagnosticCode::CompatibilityConstraintViolation,
                               "text atlas seed request is empty");
    result.compatibility.diagnostics = result.diagnostics;
    return result;
  }

  TextPrepareOptions prepareOptions;
  TextRequest request = makeAtlasBackedRequest(
      atlas.options(), encodeSeedText(result.codePoints, seedRequest.texts),
      prepareOptions);
  const MultiLineLayout layout = m_impl->m_renderer.layoutText(request);
  result.atlasUpdate = atlas.m_impl->m_cache.ensureGlyphs(layout);
  atlas.m_impl->m_lastUpdate = result.atlasUpdate;
  atlas.m_impl->m_lastUploadPlan =
      buildAtlasPageUploadPlan(atlas.m_impl->m_cache, result.atlasUpdate);
  result.atlasUploadPlan = atlas.m_impl->m_lastUploadPlan;
  result.compatibility = analyzeTextCompatibility(layout);
  result.diagnostics = collectPreparedDiagnostics(
      layout.diagnostics, result.compatibility.diagnostics,
      result.atlasUpdate.diagnostics);
  result.compatibility.diagnostics = result.diagnostics;
  return result;
}

PreparedTextLine
TextSystem::prepareLine(TextAtlas &atlas, std::string_view text,
                        const TextPrepareOptions &options) const {
  return prepareLine(atlas, makeAtlasBackedRequest(atlas.options(),
                                                   std::string(text), options));
}

PreparedTextLine TextSystem::prepareLine(TextAtlas &atlas,
                                         const TextRequest &request) const {
  PreparedTextLine result;
  result.layout = m_impl->m_renderer.layoutLine(request);
  result.atlasUpdate = atlas.m_impl->m_cache.ensureGlyphs(result.layout);
  atlas.m_impl->m_lastUpdate = result.atlasUpdate;
  atlas.m_impl->m_lastUploadPlan =
      buildAtlasPageUploadPlan(atlas.m_impl->m_cache, result.atlasUpdate);
  result.atlasUploadPlan = atlas.m_impl->m_lastUploadPlan;
  result.drawRun =
      m_impl->m_renderer.buildDrawRun(result.layout, atlas.m_impl->m_cache);
  result.compatibility = analyzeTextCompatibility(result.drawRun);
  result.submission = buildTextDrawSubmission(result.drawRun);
  result.diagnostics = collectPreparedDiagnostics(
      result.layout.diagnostics, result.drawRun.diagnostics,
      result.atlasUpdate.diagnostics);
  result.missingGlyphs = collectMissingGlyphs(result.diagnostics);
  return result;
}

PreparedTextParagraph
TextSystem::prepareText(TextAtlas &atlas, std::string_view text,
                        const TextPrepareOptions &options) const {
  return prepareText(atlas, makeAtlasBackedRequest(atlas.options(),
                                                   std::string(text), options));
}

PreparedTextParagraph
TextSystem::prepareText(TextAtlas &atlas, const TextRequest &request) const {
  PreparedTextParagraph result;
  result.layout = m_impl->m_renderer.layoutText(request);
  result.atlasUpdate = atlas.m_impl->m_cache.ensureGlyphs(result.layout);
  atlas.m_impl->m_lastUpdate = result.atlasUpdate;
  atlas.m_impl->m_lastUploadPlan =
      buildAtlasPageUploadPlan(atlas.m_impl->m_cache, result.atlasUpdate);
  result.atlasUploadPlan = atlas.m_impl->m_lastUploadPlan;
  result.drawRun =
      m_impl->m_renderer.buildDrawText(result.layout, atlas.m_impl->m_cache);
  result.compatibility = analyzeTextCompatibility(result.drawRun);
  result.submission = buildTextDrawSubmission(result.drawRun);
  result.diagnostics = collectPreparedDiagnostics(
      result.layout.diagnostics, result.drawRun.diagnostics,
      result.atlasUpdate.diagnostics);
  result.missingGlyphs = collectMissingGlyphs(result.diagnostics);
  return result;
}

PreparedTextLine TextSystem::layoutTextLine(const BakedTextAtlas &atlas,
                                            const TextRequest &request) const {
  PreparedTextLine result;
  if (request.fontChainId.value != atlas.request().fontChainId.value ||
      request.pixelSize != atlas.request().pixelSize ||
      request.paletteIndex != atlas.request().paletteIndex) {
    appendBakedAtlasMismatch(result.diagnostics,
                             "baked atlas request does not match "
                             "fontChainId/pixelSize/paletteIndex");
    result.missingGlyphs = collectMissingGlyphs(result.diagnostics);
    return result;
  }

  result.layout = m_impl->m_renderer.layoutLine(request);
  result.drawRun.metrics = result.layout.metrics;
  result.drawRun.summary = result.layout.summary;
  result.drawRun.summary.atlasPageCount = atlas.pages().size();
  result.drawRun.fallbackTrace = result.layout.fallbackTrace;
  result.drawRun.glyphs = result.layout.glyphs;
  result.drawRun.diagnostics = result.layout.diagnostics;

  for (PositionedGlyph &glyph : result.drawRun.glyphs) {
    const GlyphAtlasEntry *entry = atlas.m_impl->findEntry(glyph);
    if (entry == nullptr) {
      Diagnostic diagnostic;
      diagnostic.severity = DiagnosticSeverity::Warning;
      diagnostic.code = DiagnosticCode::AtlasEntryMissing;
      diagnostic.lineIndex = glyph.lineIndex;
      diagnostic.clusterIndex = glyph.clusterIndex;
      diagnostic.message = "glyph is not present in baked atlas";
      diagnostic.clusterText = encodeUtf8(glyph.glyph.codePoints);
      diagnostic.codePoints = glyph.glyph.codePoints;
      diagnostic.fontPath = glyph.glyph.fontPath;
      result.drawRun.diagnostics.push_back(std::move(diagnostic));
      continue;
    }
    glyph.atlasPage = entry->pageIndex;
    glyph.atlasPageHandle = entry->pageHandle;
    glyph.atlasRect = entry->pixelRect;
    glyph.uvRect = entry->uvRect;
    glyph.materialKind = entry->materialKind;
    glyph.samplerKind = entry->samplerKind;
    glyph.blendHint = entry->blendHint;
  }

  result.drawRun.diagnosticsSummary = buildDiagnosticsSummary(
      result.drawRun.diagnostics, result.drawRun.summary);
  result.compatibility = analyzeTextCompatibility(result.drawRun);
  result.submission = buildTextDrawSubmission(result.drawRun);
  result.diagnostics = result.drawRun.diagnostics;
  result.missingGlyphs = collectMissingGlyphs(result.diagnostics);
  return result;
}

PreparedTextParagraph
TextSystem::layoutTextParagraph(const BakedTextAtlas &atlas,
                                const TextRequest &request) const {
  PreparedTextParagraph result;
  if (request.fontChainId.value != atlas.request().fontChainId.value ||
      request.pixelSize != atlas.request().pixelSize ||
      request.paletteIndex != atlas.request().paletteIndex) {
    appendBakedAtlasMismatch(result.diagnostics,
                             "baked atlas request does not match "
                             "fontChainId/pixelSize/paletteIndex");
    result.missingGlyphs = collectMissingGlyphs(result.diagnostics);
    return result;
  }

  result.layout = m_impl->m_renderer.layoutText(request);
  result.drawRun.lineTexts = result.layout.lineTexts;
  result.drawRun.lineOriginsY = result.layout.lineOriginsY;
  result.drawRun.lineSummaries = result.layout.lineSummaries;
  result.drawRun.fallbackTrace = result.layout.fallbackTrace;
  result.drawRun.summary = result.layout.summary;
  result.drawRun.summary.atlasPageCount = atlas.pages().size();
  result.drawRun.boundingBox = result.layout.boundingBox;
  result.drawRun.lineAdvance = result.layout.lineAdvance;
  result.drawRun.glyphs = result.layout.glyphs;

  result.drawRun.lines.reserve(result.layout.lines.size());
  for (std::size_t lineIndex = 0; lineIndex < result.layout.lines.size();
       ++lineIndex) {
    TextDrawRun lineDrawRun;
    lineDrawRun.metrics = result.layout.lines[lineIndex].metrics;
    lineDrawRun.summary = result.layout.lines[lineIndex].summary;
    lineDrawRun.summary.atlasPageCount = atlas.pages().size();
    lineDrawRun.fallbackTrace = result.layout.lines[lineIndex].fallbackTrace;
    lineDrawRun.glyphs = result.layout.lines[lineIndex].glyphs;
    lineDrawRun.diagnostics = result.layout.lines[lineIndex].diagnostics;

    const int originY = lineIndex < result.layout.lineOriginsY.size()
                            ? result.layout.lineOriginsY[lineIndex]
                            : 0;
    for (PositionedGlyph &glyph : lineDrawRun.glyphs) {
      const GlyphAtlasEntry *entry = atlas.m_impl->findEntry(glyph);
      glyph.lineIndex = lineIndex;
      glyph.originY += originY;
      if (entry == nullptr) {
        Diagnostic diagnostic;
        diagnostic.severity = DiagnosticSeverity::Warning;
        diagnostic.code = DiagnosticCode::AtlasEntryMissing;
        diagnostic.lineIndex = glyph.lineIndex;
        diagnostic.clusterIndex = glyph.clusterIndex;
        diagnostic.message = "glyph is not present in baked atlas";
        diagnostic.clusterText = encodeUtf8(glyph.glyph.codePoints);
        diagnostic.codePoints = glyph.glyph.codePoints;
        diagnostic.fontPath = glyph.glyph.fontPath;
        lineDrawRun.diagnostics.push_back(std::move(diagnostic));
        continue;
      }
      glyph.atlasPage = entry->pageIndex;
      glyph.atlasPageHandle = entry->pageHandle;
      glyph.atlasRect = entry->pixelRect;
      glyph.uvRect = entry->uvRect;
      glyph.materialKind = entry->materialKind;
      glyph.samplerKind = entry->samplerKind;
      glyph.blendHint = entry->blendHint;
    }
    lineDrawRun.diagnosticsSummary =
        buildDiagnosticsSummary(lineDrawRun.diagnostics, lineDrawRun.summary);
    result.drawRun.diagnostics.insert(result.drawRun.diagnostics.end(),
                                      lineDrawRun.diagnostics.begin(),
                                      lineDrawRun.diagnostics.end());
    result.drawRun.lines.push_back(std::move(lineDrawRun));
  }

  for (PositionedGlyph &glyph : result.drawRun.glyphs) {
    const GlyphAtlasEntry *entry = atlas.m_impl->findEntry(glyph);
    if (entry == nullptr) {
      Diagnostic diagnostic;
      diagnostic.severity = DiagnosticSeverity::Warning;
      diagnostic.code = DiagnosticCode::AtlasEntryMissing;
      diagnostic.lineIndex = glyph.lineIndex;
      diagnostic.clusterIndex = glyph.clusterIndex;
      diagnostic.message = "glyph is not present in baked atlas";
      diagnostic.clusterText = encodeUtf8(glyph.glyph.codePoints);
      diagnostic.codePoints = glyph.glyph.codePoints;
      diagnostic.fontPath = glyph.glyph.fontPath;
      result.drawRun.diagnostics.push_back(std::move(diagnostic));
      continue;
    }
    glyph.atlasPage = entry->pageIndex;
    glyph.atlasPageHandle = entry->pageHandle;
    glyph.atlasRect = entry->pixelRect;
    glyph.uvRect = entry->uvRect;
    glyph.materialKind = entry->materialKind;
    glyph.samplerKind = entry->samplerKind;
    glyph.blendHint = entry->blendHint;
  }

  result.drawRun.diagnosticsSummary = buildDiagnosticsSummary(
      result.drawRun.diagnostics, result.drawRun.summary);
  result.compatibility = analyzeTextCompatibility(result.drawRun);
  result.submission = buildTextDrawSubmission(result.drawRun);
  result.diagnostics = result.drawRun.diagnostics;
  result.missingGlyphs = collectMissingGlyphs(result.diagnostics);
  return result;
}

[[nodiscard]] std::string defaultFontPath() {
  return "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf";
}

[[nodiscard]] std::string defaultTextFontPath() {
  return "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf";
}

[[nodiscard]] std::string defaultText() { return "🍣"; }

int runCli(const std::vector<std::string> &args) {
  try {
    std::size_t argIndex = 0;
    bool listColrGlyphs = false;
    bool scanColrV1Glyphs = false;
    bool verifyMetricsFlag = false;
    bool traceColrV1 = false;
    bool renderGlyphIdMode = false;
    hb_codepoint_t glyphIdToRender = 0;
    unsigned int scanTopCount = 10;
    unsigned int scanLimit = 0;
    ColrV1ScanFilters scanFilters;
    while (argIndex < args.size()) {
      const std::string &flag = args[argIndex];
      if (flag == "--list-colr") {
        listColrGlyphs = true;
        ++argIndex;
        continue;
      }
      if (flag == "--verify-metrics") {
        verifyMetricsFlag = true;
        ++argIndex;
        continue;
      }
      if (flag == "--scan-colrv1") {
        scanColrV1Glyphs = true;
        ++argIndex;
        continue;
      }
      if (flag == "--scan-colrv1-top") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument("--scan-colrv1-top の値が未指定です");
        }
        scanTopCount =
            parseUnsigned(args[argIndex++].c_str(), "--scan-colrv1-top");
        continue;
      }
      if (flag == "--scan-colrv1-limit") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument("--scan-colrv1-limit の値が未指定です");
        }
        scanLimit =
            parseUnsigned(args[argIndex++].c_str(), "--scan-colrv1-limit");
        continue;
      }
      if (flag == "--scan-colrv1-min-paints") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument(
              "--scan-colrv1-min-paints の値が未指定です");
        }
        scanFilters.m_minPaints =
            parseUnsigned(args[argIndex++].c_str(), "--scan-colrv1-min-paints");
        continue;
      }
      if (flag == "--scan-colrv1-min-gradients") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument(
              "--scan-colrv1-min-gradients の値が未指定です");
        }
        scanFilters.m_minGradients = parseUnsigned(
            args[argIndex++].c_str(), "--scan-colrv1-min-gradients");
        continue;
      }
      if (flag == "--scan-colrv1-min-composites") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument(
              "--scan-colrv1-min-composites の値が未指定です");
        }
        scanFilters.m_minComposites = parseUnsigned(
            args[argIndex++].c_str(), "--scan-colrv1-min-composites");
        continue;
      }
      if (flag == "--scan-colrv1-min-transforms") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument(
              "--scan-colrv1-min-transforms の値が未指定です");
        }
        scanFilters.m_minTransforms = parseUnsigned(
            args[argIndex++].c_str(), "--scan-colrv1-min-transforms");
        continue;
      }
      if (flag == "--scan-colrv1-summary-only") {
        scanFilters.m_summaryOnly = true;
        ++argIndex;
        continue;
      }
      if (flag == "--trace-colrv1") {
        traceColrV1 = true;
        ++argIndex;
        continue;
      }
      if (flag == "--glyph-id") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument("--glyph-id の値が未指定です");
        }
        glyphIdToRender = parseUnsigned(args[argIndex++].c_str(), "--glyph-id");
        renderGlyphIdMode = true;
        continue;
      }
      break;
    }
    if (listColrGlyphs && verifyMetricsFlag) {
      throw std::invalid_argument(
          "--list-colr と --verify-metrics は同時指定できません");
    }
    if (scanColrV1Glyphs && (listColrGlyphs || verifyMetricsFlag)) {
      throw std::invalid_argument("--scan-colrv1 は --list-colr / "
                                  "--verify-metrics と同時指定できません");
    }
    if (renderGlyphIdMode && listColrGlyphs) {
      throw std::invalid_argument(
          "--glyph-id は --list-colr と同時指定できません");
    }
    if (!scanColrV1Glyphs &&
        (scanTopCount != 10 || scanLimit != 0 || scanFilters.m_minPaints != 0 ||
         scanFilters.m_minGradients != 0 || scanFilters.m_minComposites != 0 ||
         scanFilters.m_minTransforms != 0 || scanFilters.m_summaryOnly)) {
      throw std::invalid_argument("--scan-colrv1-* オプションは "
                                  "--scan-colrv1 と併用してください");
    }
#if !GLYPH_RENDERER_ENABLE_PNG_IO
    if (!listColrGlyphs && !scanColrV1Glyphs) {
      throw std::runtime_error(
          "このビルドでは PNG 出力が無効です。"
          "GLYPH_RENDERER_ENABLE_PNG_IO=ON で再ビルドしてください");
    }
#endif

    const bool legacyFontArg =
        argIndex < args.size() && fileExists(args[argIndex]);
    const std::string fontPath = legacyFontArg
                                     ? args[argIndex++]
                                     : resolveExistingPath({defaultFontPath()});

    const std::string text =
        (listColrGlyphs || renderGlyphIdMode)
            ? std::string()
            : (argIndex < args.size() ? args[argIndex++] : defaultText());

    const unsigned int paletteIndex =
        argIndex < args.size()
            ? parseUnsigned(args[argIndex++].c_str(), "paletteIndex")
            : 0U;
    const unsigned int pixelSize =
        argIndex < args.size()
            ? parseUnsigned(args[argIndex++].c_str(), "pixelSize")
            : 128U;

    FT_Library libraryRaw = nullptr;
    if (FT_Init_FreeType(&libraryRaw) != 0) {
      throw std::runtime_error("FreeTypeの初期化に失敗しました");
    }
    FreeTypeLibraryHandle library(libraryRaw);
    FontContext emojiFont = makeFontContext(library.get(), fontPath, pixelSize);
    const std::string textFontPath =
        legacyFontArg ? fontPath : resolveExistingPath({defaultTextFontPath()});
    FontContext textFont =
        makeFontContext(library.get(), textFontPath, pixelSize);

    const bool hasColrV0 = hb_ot_color_has_layers(emojiFont.m_hbFace.get());
    const bool hasColrV1 = hb_ot_color_has_paint(emojiFont.m_hbFace.get());
    std::cout << "COLRv0レイヤー対応: " << (hasColrV0 ? "あり" : "なし")
              << '\n';
    std::cout << "COLRv1ペイント対応: " << (hasColrV1 ? "あり" : "なし")
              << '\n';
    if (listColrGlyphs && !hasColrV0 && !hasColrV1) {
      throw std::runtime_error("COLR情報が含まれていません");
    }

    if (listColrGlyphs) {
      const unsigned int glyphCountFace =
          hb_face_get_glyph_count(emojiFont.m_hbFace.get());
      unsigned int listed = 0;
      for (hb_codepoint_t glyphId = 0; glyphId < glyphCountFace; ++glyphId) {
        unsigned int layerCount = 0;
        hb_ot_color_glyph_get_layers(emojiFont.m_hbFace.get(), glyphId, 0,
                                     &layerCount, nullptr);
        if (layerCount == 0 && FT_HAS_COLOR(emojiFont.m_face.get())) {
          FT_LayerIterator iterator;
          iterator.p = nullptr;
          FT_UInt layerGlyph = 0;
          FT_UInt layerColor = 0;
          while (FT_Get_Color_Glyph_Layer(emojiFont.m_face.get(), glyphId,
                                          &layerGlyph, &layerColor,
                                          &iterator)) {
            ++layerCount;
          }
        }
        const bool glyphHasV1 =
            hasColrV1 && hasColrV1RootPaint(emojiFont, glyphId);
        if (layerCount == 0 && !glyphHasV1) {
          continue;
        }
        std::cout << "glyph " << glyphId;
        if (FT_HAS_GLYPH_NAMES(emojiFont.m_face.get())) {
          char glyphName[128] = {};
          if (FT_Get_Glyph_Name(emojiFont.m_face.get(), glyphId, glyphName,
                                sizeof(glyphName)) == 0) {
            std::cout << " (" << glyphName << ")";
          }
        }
        if (layerCount > 0) {
          std::cout << " : layerCount = " << layerCount;
          if (glyphHasV1) {
            std::cout << " / COLRv1ペイントあり";
          }
          std::cout << '\n';
        } else {
          std::cout << " : COLRv1ペイントのみ" << '\n';
        }
        ++listed;
        if (listed >= 50) {
          std::cout << "(表示を50件で打ち切りました)\n";
          break;
        }
      }
      if (listed == 0) {
        std::cout << "COLRv0レイヤーを持つグリフは見つかりませんでした\n";
      }
      return 0;
    }

    if (scanColrV1Glyphs) {
      const unsigned int glyphCountFace =
          hb_face_get_glyph_count(emojiFont.m_hbFace.get());
      std::size_t scanned = 0;
      std::size_t rendered = 0;
      std::size_t failed = 0;
      ColrV1RenderStats totalStats;
      std::vector<ColrV1ScanEntry> scanEntries;
      for (hb_codepoint_t glyphId = 0; glyphId < glyphCountFace; ++glyphId) {
        if (!hasColrV1RootPaint(emojiFont, glyphId)) {
          continue;
        }
        if (scanLimit > 0 && scanned >= scanLimit) {
          break;
        }
        ++scanned;
        ColrV1RenderOptions options;
        ColrV1RenderStats stats;
        try {
          const GlyphSurface surface = renderSingleGlyph(
              emojiFont, glyphId, paletteIndex, options, &stats);
          accumulateColrV1Stats(totalStats, stats);
          ++rendered;
          if (matchesScanFilters(stats, scanFilters)) {
            scanEntries.push_back({glyphId, stats, surface.m_image.width(),
                                   surface.m_image.height(),
                                   surface.m_advance});
          }
          if (!scanFilters.m_summaryOnly &&
              matchesScanFilters(stats, scanFilters)) {
            printColrV1ScanSummary(glyphId, stats, surface, std::cout);
          }
        } catch (const std::exception &ex) {
          ++failed;
          std::cout << "glyph " << glyphId << " failed: " << ex.what() << '\n';
        }
      }
      std::cout << "COLRv1 scan 完了"
                << " scanned=" << scanned << " rendered=" << rendered
                << " failed=" << failed << '\n';
      traceColrV1Stats(totalStats, rendered, std::cout);
      printTopScanEntries(
          "paints", scanEntries, scanTopCount,
          [](const ColrV1ScanEntry &entry) {
            return entry.m_stats.m_totalPaints;
          },
          std::cout);
      printTopScanEntries(
          "gradients", scanEntries, scanTopCount,
          [](const ColrV1ScanEntry &entry) {
            return entry.m_stats.m_gradients;
          },
          std::cout);
      printTopScanEntries(
          "composites", scanEntries, scanTopCount,
          [](const ColrV1ScanEntry &entry) {
            return entry.m_stats.m_composites;
          },
          std::cout);
      printTopScanEntries(
          "transforms", scanEntries, scanTopCount,
          [](const ColrV1ScanEntry &entry) {
            return entry.m_stats.m_transformPushes;
          },
          std::cout);
      return failed == 0 ? 0 : 1;
    }

    if (renderGlyphIdMode) {
      ColrV1RenderOptions options;
      options.m_enableTrace = traceColrV1;
      options.m_traceStream = traceColrV1 ? &std::cerr : nullptr;
      ColrV1RenderStats stats;
      const GlyphSurface surface = renderSingleGlyph(
          emojiFont, glyphIdToRender, paletteIndex, options, &stats);
      const std::string outputName = makeGlyphIdOutputName(glyphIdToRender);
      if (!writeOutputPng(outputName, surface.m_image)) {
        throw std::runtime_error("PNG書き出しに失敗しました: " + outputName);
      }
      std::cout << outputName << " を出力しました "
                << "(font=" << emojiFont.m_path
                << ", glyphId=" << glyphIdToRender
                << ", advance=" << surface.m_advance
                << ", bearingX=" << surface.m_bearingX
                << ", bearingY=" << surface.m_bearingY << ")\n";
      if (stats.m_totalPaints > 0) {
        traceColrV1Stats(stats, 1, std::cout);
      }
      return 0;
    }

    const std::vector<std::uint32_t> codePoints = decodeUtf8(text);
    if (codePoints.empty()) {
      throw std::runtime_error("描画対象のグリフが得られませんでした");
    }
    const std::vector<TextCluster> clusters = clusterText(codePoints);
    if (clusters.empty()) {
      throw std::runtime_error("描画対象のクラスタが得られませんでした");
    }

    GlyphRenderCache emojiGlyphCache;
    GlyphRenderCache textGlyphCache;
    ClusterRenderCache emojiClusterCache;
    ClusterRenderCache textClusterCache;
    CacheStats emojiCacheStats;
    CacheStats textCacheStats;
    SessionColrV1Stats emojiColrV1Stats;
    SessionColrV1Stats textColrV1Stats;
    unsigned int exportedCount = 0;
    bool metricsOk = true;
    bool warnedClusterMetrics = false;
    for (unsigned int i = 0; i < clusters.size(); ++i) {
      const TextCluster &cluster = clusters[i];
      FontContext &selectedFont =
          shouldUseEmojiFont(cluster) ? emojiFont : textFont;
      GlyphRenderCache &glyphCache =
          &selectedFont == &emojiFont ? emojiGlyphCache : textGlyphCache;
      ClusterRenderCache &clusterCache =
          &selectedFont == &emojiFont ? emojiClusterCache : textClusterCache;
      CacheStats &cacheStats =
          &selectedFont == &emojiFont ? emojiCacheStats : textCacheStats;
      SessionColrV1Stats &sessionColrV1Stats =
          &selectedFont == &emojiFont ? emojiColrV1Stats : textColrV1Stats;
      ColrV1RenderOptions options;
      options.m_enableTrace = traceColrV1;
      options.m_traceStream = traceColrV1 ? &std::cerr : nullptr;
      const ClusterCacheKey clusterCacheKey{cluster.m_codePoints, paletteIndex};
      auto clusterCacheIt = clusterCache.find(clusterCacheKey);
      bool clusterCacheHit = false;
      if (clusterCacheIt == clusterCache.end()) {
        ++cacheStats.m_clusterMisses;
        ClusterRenderResult clusterResult =
            renderGlyphCluster(selectedFont, cluster, paletteIndex, glyphCache,
                               cacheStats, options);
        clusterCacheIt =
            clusterCache.emplace(clusterCacheKey, std::move(clusterResult))
                .first;
      } else {
        ++cacheStats.m_clusterHits;
        clusterCacheHit = true;
      }
      if (traceColrV1 && clusterCacheHit) {
        std::cerr << "[cache] cluster hit [" << describeCluster(cluster)
                  << "]\n";
      }
      const ClusterRenderResult &clusterResult = clusterCacheIt->second;
      if (clusterResult.m_colrV1GlyphCount > 0) {
        ++sessionColrV1Stats.m_logicalClusterCount;
        sessionColrV1Stats.m_logicalGlyphCount +=
            clusterResult.m_colrV1GlyphCount;
        accumulateColrV1Stats(sessionColrV1Stats.m_logicalStats,
                              clusterResult.m_colrV1Stats);
        if (!clusterCacheHit) {
          ++sessionColrV1Stats.m_renderedClusterCount;
          sessionColrV1Stats.m_renderedGlyphCount +=
              clusterResult.m_colrV1GlyphCount;
          accumulateColrV1Stats(sessionColrV1Stats.m_renderedStats,
                                clusterResult.m_colrV1Stats);
        }
      }
      if (traceColrV1) {
        traceClusterLayout(cluster, clusterResult, std::cerr);
      }
      const GlyphSurface &surface = clusterResult.m_surface;

      const std::string outputName = makeOutputName(i, cluster);
      if (!writeOutputPng(outputName, surface.m_image)) {
        throw std::runtime_error("PNG書き出しに失敗しました: " + outputName);
      }
      std::cout << outputName << " を出力しました "
                << "(font=" << selectedFont.m_path
                << ", clusterLength=" << cluster.m_codePoints.size()
                << ", advance=" << surface.m_advance
                << ", bearingX=" << surface.m_bearingX
                << ", bearingY=" << surface.m_bearingY << ")\n";
      if (verifyMetricsFlag) {
        if (!verifyClusterMetrics(clusterResult, cluster)) {
          metricsOk = false;
        }
        if (!warnedClusterMetrics) {
          std::cout << "注記: クラスタ合成時の --verify-metrics はクラスタ "
                       "advance のみ検証します\n";
          warnedClusterMetrics = true;
        }
      }
      ++exportedCount;
    }

    if (traceColrV1) {
      traceCacheStats("emoji", emojiGlyphCache, emojiClusterCache,
                      emojiCacheStats, std::cerr);
      traceCacheStats("text", textGlyphCache, textClusterCache, textCacheStats,
                      std::cerr);
      traceSessionColrV1Stats("emoji", emojiColrV1Stats, std::cerr);
      traceSessionColrV1Stats("text", textColrV1Stats, std::cerr);
    }

    if (exportedCount == 0) {
      throw std::runtime_error("COLR描画に成功したグリフがありません");
    }

    if (verifyMetricsFlag) {
      if (!metricsOk) {
        throw std::runtime_error("メトリクス検証に失敗しました");
      }
      std::cout << "メトリクス検証に成功しました (" << exportedCount
                << " glyphs)\n";
    }

    std::cout << "完了: emoji=" << emojiFont.m_path
              << ", text=" << textFont.m_path << " でグリフを出力しました\n";
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "エラー: " << ex.what() << '\n';
    return 1;
  }
}

int runCli(int argc, char *argv[]) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return runCli(args);
}

} // namespace glyphrenderer
