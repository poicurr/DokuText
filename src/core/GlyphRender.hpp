#pragma once

#include "adapter/render/ColrV0Renderer.hpp"
#include "adapter/render/ColrV1Renderer.hpp"
#include <DokuText/DokuText.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_COLOR_H
#include FT_GLYPH_H

#include <hb-ft.h>

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

namespace doku::text::detail {
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
  std::size_t m_beginByteOffset = 0;
  std::size_t m_endByteOffset = 0;
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
  const FontContext *m_font = nullptr;
  hb_codepoint_t m_glyphId = 0;
  unsigned int m_paletteIndex = 0;

  bool operator==(const GlyphCacheKey &other) const {
    return m_font == other.m_font && m_glyphId == other.m_glyphId &&
           m_paletteIndex == other.m_paletteIndex;
  }
};

struct GlyphCacheKeyHasher {
  std::size_t operator()(const GlyphCacheKey &key) const {
    return std::hash<const FontContext *>{}(key.m_font) ^
           (static_cast<std::size_t>(key.m_glyphId) << 1U) ^
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

void accumulateColrV1Stats(ColrV1RenderStats &dst,
                           const ColrV1RenderStats &src);

[[nodiscard]] bool fileExists(const std::string &path);

[[nodiscard]] std::string
resolveExistingPath(std::initializer_list<std::string> candidates);

[[nodiscard]] std::vector<std::uint32_t> decodeUtf8(const std::string &text);

[[nodiscard]] std::string describeCluster(const TextCluster &cluster);

[[nodiscard]] bool shouldUseEmojiFont(const TextCluster &cluster);

[[nodiscard]] FontContext makeFontContext(FT_Library library,
                                          const std::string &fontPath,
                                          unsigned int pixelSize);

[[nodiscard]] bool hasColrV1RootPaint(FontContext &font,
                                      hb_codepoint_t glyphId);

[[nodiscard]] std::vector<TextCluster>
clusterText(const std::vector<std::uint32_t> &codePoints);

[[nodiscard]] GlyphSurface
renderSingleGlyph(FontContext &font, hb_codepoint_t glyphId,
                  unsigned int paletteIndex, const ColrV1RenderOptions &options,
                  ColrV1RenderStats *stats = nullptr);

[[nodiscard]] ClusterRenderResult
renderGlyphCluster(FontContext &font, const TextCluster &cluster,
                   const std::vector<ShapedGlyph> &shapedGlyphs,
                   unsigned int paletteIndex, GlyphRenderCache &glyphCache,
                   CacheStats &cacheStats, const ColrV1RenderOptions &options);

[[nodiscard]] ClusterRenderResult
renderGlyphCluster(FontContext &font, const TextCluster &cluster,
                   unsigned int paletteIndex, GlyphRenderCache &glyphCache,
                   CacheStats &cacheStats, const ColrV1RenderOptions &options);

[[nodiscard]] bool
verifyClusterMetrics(int actualAdvance, int expectedAdvance,
                     int expectedYAdvance,
                     const std::vector<std::uint32_t> &codePoints);

} // namespace doku::text::detail
