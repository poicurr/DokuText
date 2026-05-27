// This file is part of the public API.
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <DokuText/Atlas.hpp>
#include <DokuText/Layout.hpp>

namespace doku::text {

class DokuTextContext {
public:
  DokuTextContext();
  ~DokuTextContext();

  DokuTextContext(DokuTextContext &&) noexcept;
  DokuTextContext &operator=(DokuTextContext &&) noexcept;

  DokuTextContext(const DokuTextContext &) = delete;
  DokuTextContext &operator=(const DokuTextContext &) = delete;

  FontChainId registerFontFallbackChain(const FontFallbackChain &chain);

  [[nodiscard]] FontFallbackChain
  resolveFontFallbackChain(FontChainId fontChainId) const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;

  friend class DokuText;
  friend class GlyphAtlasCache;
};

class GlyphAtlasCache {
public:
  explicit GlyphAtlasCache(const AtlasBuildContext &context = {});
  ~GlyphAtlasCache();

  GlyphAtlasCache(GlyphAtlasCache &&) noexcept;
  GlyphAtlasCache &operator=(GlyphAtlasCache &&) noexcept;

  GlyphAtlasCache(const GlyphAtlasCache &) = delete;
  GlyphAtlasCache &operator=(const GlyphAtlasCache &) = delete;

  GlyphAtlasCacheUpdate ensureGlyphs(const TextLineLayout &layout);

  GlyphAtlasCacheUpdate ensureGlyphs(const MultiLineLayout &layout);

  GlyphAtlasCacheUpdate evict(const std::vector<GlyphHandle> &handles);

  GlyphAtlasCacheUpdate compact();

  [[nodiscard]] const AtlasBuildContext &atlasContext() const;

  [[nodiscard]] const std::vector<GlyphAtlasPage> &pages() const;

  [[nodiscard]] std::vector<GlyphAtlasEntry> entries() const;

  [[nodiscard]] bool containsGlyph(GlyphHandle handle) const;

  [[nodiscard]] TextAtlasPageInfo pageInfo(std::size_t pageIndex) const;

  [[nodiscard]] TextAtlasPageInfo
  pageInfo(GlyphAtlasPageHandle pageHandle) const;

  [[nodiscard]] MaterialKind materialKindForPage(std::size_t pageIndex) const;

  [[nodiscard]] MaterialKind
  materialKindForPage(GlyphAtlasPageHandle pageHandle) const;

  [[nodiscard]] AtlasPixelFormat
  pixelFormatForPage(std::size_t pageIndex) const;

  [[nodiscard]] AtlasPixelFormat
  pixelFormatForPage(GlyphAtlasPageHandle pageHandle) const;

  [[nodiscard]] SamplerKind samplerKindForPage(std::size_t pageIndex) const;

  [[nodiscard]] SamplerKind
  samplerKindForPage(GlyphAtlasPageHandle pageHandle) const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;

  friend class DokuText;
};

class DokuText {
public:
  explicit DokuText(DokuTextContext &context);
  ~DokuText();

  DokuText(DokuText &&) noexcept;
  DokuText &operator=(DokuText &&) noexcept;

  DokuText(const DokuText &) = delete;
  DokuText &operator=(const DokuText &) = delete;

  // Advanced low-level APIs. Prefer TextSystem for common usage.
  TextShapeResult shapeLine(const TextLayoutContext &context,
                            std::string_view text) const;

  MultiLineShapeResult shapeText(const TextLayoutContext &context,
                                 std::string_view text) const;

  TextRasterResult rasterizeLine(const TextLayoutContext &context,
                                 const TextShapeResult &shape) const;

  MultiLineRasterResult rasterizeText(const TextLayoutContext &context,
                                      const MultiLineShapeResult &shape) const;

  TextLineLayout layoutLine(const TextLayoutContext &context,
                            std::string_view text) const;

  TextLineLayout layoutLine(const TextRequest &request) const;

  TextLineLayout layoutLine(const TextRasterResult &raster) const;

  AtlasBuildResult buildAtlas(const TextLineLayout &layout,
                              const AtlasBuildContext &context) const;

  TextDrawRun buildDrawRun(const TextLineLayout &layout,
                           const AtlasBuildResult &atlas) const;

  TextDrawRun buildDrawRun(const TextLineLayout &layout,
                           GlyphAtlasCache &atlasCache) const;

  TextDrawRun buildDrawRun(const TextLineLayout &layout,
                           const GlyphAtlasCache &atlasCache) const;

  MultiLineLayout layoutText(const TextLayoutContext &context,
                             std::string_view text) const;

  MultiLineLayout layoutText(const TextRequest &request) const;

  MultiLineDrawRun buildDrawText(const TextLayoutContext &context,
                                 std::string_view text,
                                 const AtlasBuildContext &atlasContext) const;

  MultiLineDrawRun buildDrawText(const MultiLineLayout &layout,
                                 GlyphAtlasCache &atlasCache) const;

  MultiLineDrawRun buildDrawText(const MultiLineLayout &layout,
                                 const GlyphAtlasCache &atlasCache) const;

  TextDrawRun prepareLine(const TextRequest &request,
                          GlyphAtlasCache &atlasCache) const;

  MultiLineDrawRun prepareText(const TextRequest &request,
                               GlyphAtlasCache &atlasCache) const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

[[nodiscard]] TextCompatibilityReport
analyzeTextCompatibility(const TextLineLayout &layout);

[[nodiscard]] TextCompatibilityReport
analyzeTextCompatibility(const TextDrawRun &run);

[[nodiscard]] TextCompatibilityReport
analyzeTextCompatibility(const MultiLineLayout &layout);

[[nodiscard]] TextCompatibilityReport
analyzeTextCompatibility(const MultiLineDrawRun &run);

[[nodiscard]] TextCompatibilityReport
analyzeTextCompatibility(const TextLineLayout &layout,
                         const CompatibilityConstraints &constraints);

[[nodiscard]] TextCompatibilityReport
analyzeTextCompatibility(const TextDrawRun &run,
                         const CompatibilityConstraints &constraints);

[[nodiscard]] TextCompatibilityReport
analyzeTextCompatibility(const MultiLineLayout &layout,
                         const CompatibilityConstraints &constraints);

[[nodiscard]] TextCompatibilityReport
analyzeTextCompatibility(const MultiLineDrawRun &run,
                         const CompatibilityConstraints &constraints);

[[nodiscard]] TextDrawSubmission
buildTextDrawSubmission(const TextDrawRun &run);

[[nodiscard]] TextDrawSubmission
buildTextDrawSubmission(const MultiLineDrawRun &run);

// Converts GlyphAtlasCacheUpdate into renderer-facing upload work.
// pageIndex and dirtyRects use AtlasUvOrigin::TopLeft.
[[nodiscard]] std::vector<AtlasPageUploadPlan>
buildAtlasPageUploadPlan(const GlyphAtlasCache &atlasCache,
                         const GlyphAtlasCacheUpdate &update);

[[nodiscard]] std::vector<std::uint32_t>
makeTextSeedCodepoints(const TextSeedRequest &seedRequest);

[[nodiscard]] constexpr AtlasUvOrigin atlasUvOrigin() {
  return AtlasUvOrigin::TopLeft;
}

[[nodiscard]] constexpr AtlasPixelFormat
atlasPixelFormatForMaterial(MaterialKind kind) {
  switch (kind) {
    case MaterialKind::AlphaMask:
      return AtlasPixelFormat::AlphaMaskStraightRgba;
    case MaterialKind::ColorBitmap:
      return AtlasPixelFormat::ColorStraightRgba;
  }
  return AtlasPixelFormat::ColorStraightRgba;
}

[[nodiscard]] constexpr bool isAlphaMaskMaterial(MaterialKind kind) {
  return kind == MaterialKind::AlphaMask;
}

// Converts an AlphaMaskStraightRgba atlas image into straight-alpha white:
// RGB = 255, A = coverage. The image origin remains top-left.
[[nodiscard]] BitmapImage normalizeAlphaMaskToWhite(const BitmapImage &image);

[[nodiscard]] bool isAlphaOnlyCompatible(const TextDrawRun &run);

[[nodiscard]] bool isAlphaOnlyCompatible(const MultiLineDrawRun &run);

[[nodiscard]] bool isSinglePageCompatible(const TextDrawRun &run,
                                          int requiredPage);

[[nodiscard]] bool isSinglePageCompatible(const MultiLineDrawRun &run,
                                          int requiredPage);

} // namespace doku::text
