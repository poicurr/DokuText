// This file is part of the public API.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <DokuText/Diagnostics.hpp>
#include <DokuText/RendererContract.hpp>
#include <DokuText/Types.hpp>

namespace doku::text {

struct AtlasBuildContext {
  int pageWidth = 1024;
  int pageHeight = 1024;
  int padding = 1;
};

// GlyphAtlasPage::image is always 8-bit RGBA with AtlasUvOrigin::TopLeft.
// For MaterialKind::AlphaMask glyphs, atlas texels use
// AtlasPixelFormat::AlphaMaskStraightRgba:
//   R = G = B = A = coverage
// and are not premultiplied white.
// For MaterialKind::ColorBitmap glyphs, atlas texels use
// AtlasPixelFormat::ColorStraightRgba:
//   straight RGBA with top-left origin, intended for sRGB sampling.
//   Color bitmap glyphs are not tinted by DokuText.
struct GlyphAtlasPage {
  GlyphAtlasPageHandle handle;
  BitmapImage image;
};

// GlyphAtlasEntry::pixelRect is in atlas page pixel coordinates with top-left
// origin. uvRect is normalized and uses AtlasUvOrigin::TopLeft.
struct GlyphAtlasEntry {
  GlyphHandle handle;
  std::uint32_t glyphId = 0;
  std::size_t lineIndex = 0;
  std::size_t clusterIndex = 0;
  std::size_t glyphIndexInCluster = 0;
  std::size_t glyphIndex = 0;
  int pageIndex = -1;
  GlyphAtlasPageHandle pageHandle;
  Rect pixelRect;
  FloatRect uvRect;
  bool isColorGlyph = false;
  MaterialKind materialKind = MaterialKind::AlphaMask;
  SamplerKind samplerKind = SamplerKind::LinearClamp;
  BlendHint blendHint = BlendHint::Coverage;
};

struct GlyphAtlasDirtyRect {
  GlyphAtlasPageHandle pageHandle;
  Rect pixelRect;
};

enum class TextSeedPreset {
  Ascii,
  BasicLatin,
  CjkSymbolsPunctuation,
  Hiragana,
  Katakana,
  KatakanaPhoneticExtensions,
  HalfwidthFullwidthForms,
  CjkUnifiedIdeographs,
  Japanese,
  Chinese,
  NerdFont,
  EmojiSymbols,
  EmojiSupplement,
  Emoji,
};

[[nodiscard]] constexpr std::string_view toString(TextSeedPreset preset) {
  switch (preset) {
    case TextSeedPreset::Ascii:
      return "Ascii";
    case TextSeedPreset::BasicLatin:
      return "BasicLatin";
    case TextSeedPreset::CjkSymbolsPunctuation:
      return "CjkSymbolsPunctuation";
    case TextSeedPreset::Hiragana:
      return "Hiragana";
    case TextSeedPreset::Katakana:
      return "Katakana";
    case TextSeedPreset::KatakanaPhoneticExtensions:
      return "KatakanaPhoneticExtensions";
    case TextSeedPreset::HalfwidthFullwidthForms:
      return "HalfwidthFullwidthForms";
    case TextSeedPreset::CjkUnifiedIdeographs:
      return "CjkUnifiedIdeographs";
    case TextSeedPreset::Japanese:
      return "Japanese";
    case TextSeedPreset::Chinese:
      return "Chinese";
    case TextSeedPreset::NerdFont:
      return "NerdFont";
    case TextSeedPreset::EmojiSymbols:
      return "EmojiSymbols";
    case TextSeedPreset::EmojiSupplement:
      return "EmojiSupplement";
    case TextSeedPreset::Emoji:
      return "Emoji";
  }
  return "Unknown";
}

struct TextSeedRange {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
};

struct TextSeedPresetInfo {
  TextSeedPreset preset = TextSeedPreset::Ascii;
  std::span<const TextSeedRange> ranges;
  std::size_t estimatedCodePointCount = 0;
  std::size_t typicalGlyphCountAt64Px = 0;
  std::size_t typicalAtlasPagesAt64Px = 0;
  double typicalBakeMillisecondsAt64Px = 0.0;
  std::string_view description;
  std::string_view costHint;
};

[[nodiscard]] TextSeedPresetInfo describeTextSeedPreset(TextSeedPreset preset);

struct TextSeedRequest {
  std::vector<TextSeedPreset> presets;
  std::vector<TextSeedRange> customRanges;
  std::vector<std::string> texts;
};

struct TextAtlasBakeBudget {
  // 0 means unlimited.
  std::size_t maxSeedCodePoints = 0;
  // Post-bake guard on resolved atlas entries. 0 means unlimited.
  std::size_t maxGlyphs = 0;
  // Post-bake guard on produced atlas pages. 0 means unlimited.
  int maxPages = 0;
  // Post-bake guard on elapsed bake time in milliseconds. 0 means unlimited.
  double maxBakeMilliseconds = 0.0;
};

struct TextAtlasBakeStats {
  std::size_t seedCodePointCount = 0;
  std::size_t processedSeedCodePointCount = 0;
  std::size_t seedTextCount = 0;
  std::size_t resolvedGlyphCount = 0;
  std::size_t missingGlyphCount = 0;
  std::size_t pageCount = 0;
  std::size_t alphaMaskGlyphCount = 0;
  std::size_t colorBitmapGlyphCount = 0;
  double elapsedMilliseconds = 0.0;
  bool budgetExceeded = false;
};

struct AtlasBuildResult {
  std::vector<GlyphAtlasPage> pages;
  std::vector<GlyphAtlasEntry> entries;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<Diagnostic> diagnostics;
};

struct GlyphAtlasCacheUpdate {
  std::vector<GlyphAtlasPage> newPages;
  std::vector<GlyphAtlasEntry> updatedEntries;
  std::vector<GlyphAtlasDirtyRect> dirtyRects;
  std::vector<GlyphHandle> evictedHandles;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<Diagnostic> diagnostics;
};

struct TextAtlasOptions {
  FontChainId fontChainId;
  unsigned int pixelSize = 128;
  unsigned int paletteIndex = 0;
  AtlasBuildContext atlasContext;
};

struct TextAtlasBakeRequest {
  FontChainId fontChainId;
  unsigned int pixelSize = 128;
  unsigned int paletteIndex = 0;
  AtlasBuildContext atlasContext;
  TextAtlasBakeBudget bakeBudget;
  std::vector<TextSeedPreset> seedPresets;
  std::vector<TextSeedRange> customRanges;
  std::vector<std::string> seedTexts;
};

struct TextAtlasSeedResult {
  TextSeedRequest seedRequest;
  std::vector<std::uint32_t> codePoints;
  GlyphAtlasCacheUpdate atlasUpdate;
  std::vector<AtlasPageUploadPlan> atlasUploadPlan;
  TextCompatibilityReport compatibility;
  std::vector<Diagnostic> diagnostics;

  // Total number of unique codepoints requested to be seeded.
  std::size_t requestedCodepointCount = 0;
  // Total number of glyphs resolved from the requested codepoints/texts.
  std::size_t resolvedGlyphCount = 0;
  // Number of unique rasterization handles newly inserted into the atlas during
  // this seedAtlas call.
  std::size_t insertedGlyphCount = 0;
  // Number of resolved glyph lookup events whose rasterization handle was
  // already present when that glyph was processed. This may include duplicates
  // encountered later in the same seedAtlas request.
  std::size_t alreadyPresentGlyphCount = 0;
  // Number of glyphs that could not be resolved or rasterized.
  std::size_t missingGlyphCount = 0;
  // True only if atlas contents changed, such as a new glyph entry, new page,
  // or modified atlas texture contents.
  bool atlasChanged = false;
};

/// MissingAtlasGlyph describes a glyph that was resolved through shaping
/// and font fallback but is currently missing from the TextAtlas entries.
struct MissingAtlasGlyph {
  std::uint32_t codePoint = 0;
  GlyphHandle handle{};
  std::uint32_t glyphId = 0;
  FontChainId fontChainId{};
  std::string fontPath;
  MaterialKind materialKind = MaterialKind::AlphaMask;
  bool isColorGlyph = false;
};

class GlyphAtlasCache;

class TextAtlas {
public:
  explicit TextAtlas(const TextAtlasOptions &options);
  ~TextAtlas();

  TextAtlas(TextAtlas &&) noexcept;
  TextAtlas &operator=(TextAtlas &&) noexcept;

  TextAtlas(const TextAtlas &) = delete;
  TextAtlas &operator=(const TextAtlas &) = delete;

  [[nodiscard]] const TextAtlasOptions &options() const;
  [[nodiscard]] const GlyphAtlasCache &cache() const;
  [[nodiscard]] GlyphAtlasCache &cache();
  [[nodiscard]] const GlyphAtlasCacheUpdate &lastUpdate() const;
  [[nodiscard]] const std::vector<AtlasPageUploadPlan> &lastUploadPlan() const;
  [[nodiscard]] std::uint64_t glyphSetRevision() const;
  [[nodiscard]] std::uint64_t atlasTextureRevision() const;
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

  friend class TextSystem;
};

class BakedTextAtlas {
public:
  explicit BakedTextAtlas(const TextAtlasBakeRequest &request);
  ~BakedTextAtlas();

  BakedTextAtlas(BakedTextAtlas &&) noexcept;
  BakedTextAtlas &operator=(BakedTextAtlas &&) noexcept;

  BakedTextAtlas(const BakedTextAtlas &) = delete;
  BakedTextAtlas &operator=(const BakedTextAtlas &) = delete;

  [[nodiscard]] const TextAtlasBakeRequest &request() const;
  [[nodiscard]] const std::vector<GlyphAtlasPage> &pages() const;
  [[nodiscard]] std::vector<BitmapImage> pageImages() const;
  [[nodiscard]] const std::vector<GlyphAtlasEntry> &entries() const;
  [[nodiscard]] const std::vector<AtlasPageUploadPlan> &uploadPlan() const;
  [[nodiscard]] std::uint64_t revision() const;
  [[nodiscard]] AtlasUvOrigin uvOrigin() const;
  [[nodiscard]] AtlasPixelFormat
  pixelFormatForPage(std::size_t pageIndex) const;
  [[nodiscard]] const std::vector<MaterialKind> &
  materialKindsForPage(std::size_t pageIndex) const;
  [[nodiscard]] const TextCompatibilityReport &compatibility() const;
  [[nodiscard]] const TextAtlasBakeStats &bakeStats() const;
  [[nodiscard]] const LineSummary &summary() const;
  [[nodiscard]] const DiagnosticsSummary &diagnosticsSummary() const;
  [[nodiscard]] const std::vector<Diagnostic> &diagnostics() const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;

  friend class TextSystem;
};

/// Budget used for runtime budgeted append scheduling.
/// All fields use 0 for "unlimited".
struct TextAtlasAppendBudget {
  std::size_t maxGlyphs = 0;
  std::size_t maxColorGlyphs = 0;
  std::size_t maxAtlasUploadBytes = 0;
};

/// Non-mutating plan extracted from a layout/probe result for append
/// scheduling.
struct TextAtlasAppendPlan {
  std::vector<MissingAtlasGlyph> candidates;
  std::size_t estimatedGlyphCount = 0;
  std::size_t estimatedColorGlyphCount = 0;
  std::size_t estimatedUploadBytes = 0;
  bool budgetExceeded = false;
};

} // namespace doku::text
