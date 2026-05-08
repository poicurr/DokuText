#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace doku::text {

// BitmapImage::rgba is always 8-bit RGBA in row-major order.
// The first texel is the top-left texel of the image.
struct BitmapImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> rgba;
};

struct GlyphBitmap {
  BitmapImage image;
  int bearingX = 0;
  int bearingY = 0;
  int advance = 0;
  std::vector<std::uint32_t> codePoints;
  std::string fontPath;
};

struct Rect {
  int minX = 0;
  int minY = 0;
  int maxX = 0;
  int maxY = 0;
};

struct FloatRect {
  float minX = 0.0F;
  float minY = 0.0F;
  float maxX = 0.0F;
  float maxY = 0.0F;
};

struct GlyphHandle {
  std::uint64_t value = 0;
};

struct FontChainId {
  std::uint32_t value = 0;
};

struct GlyphAtlasPageHandle {
  std::uint32_t value = 0;
};

// LineMetrics uses the same coordinate space as PositionedGlyph::originX/Y.
// X grows to the right. Y grows downward.
// baseline is the Y coordinate of the baseline within the line-local space.
// boundingBox is the union of visible glyph pixel rectangles in the same space.
struct LineMetrics {
  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  int baseline = 0;
  int lineWidth = 0;
  unsigned int pixelSize = 128;
  Rect boundingBox;
};

struct LineSummary {
  std::size_t clusterCount = 0;
  std::size_t glyphCount = 0;
  std::size_t colorGlyphCount = 0;
  std::size_t fallbackCount = 0;
  std::size_t atlasPageCount = 0;
  std::vector<std::string> usedFonts;
};

struct DiagnosticsSummary {
  std::vector<std::string> missingGlyphClusters;
  std::size_t fallbackAppliedCount = 0;
  std::size_t colorGlyphCount = 0;
  std::size_t atlasOverflowCount = 0;
  std::size_t lineOverflowCount = 0;
};

enum class FontRole {
  Text,
  Emoji,
  Symbol,
};

[[nodiscard]] constexpr std::string_view toString(FontRole role) {
  switch (role) {
    case FontRole::Text:
      return "Text";
    case FontRole::Emoji:
      return "Emoji";
    case FontRole::Symbol:
      return "Symbol";
  }
  return "Unknown";
}

enum class DiagnosticSeverity {
  Info,
  Warning,
  Error,
};

[[nodiscard]] constexpr std::string_view toString(DiagnosticSeverity severity) {
  switch (severity) {
    case DiagnosticSeverity::Info:
      return "Info";
    case DiagnosticSeverity::Warning:
      return "Warning";
    case DiagnosticSeverity::Error:
      return "Error";
  }
  return "Unknown";
}

enum class DiagnosticCode {
  None,
  MissingGlyphCluster,
  FontFallbackApplied,
  MetricsVerificationFailed,
  AtlasGlyphTooLarge,
  AtlasEntryMissing,
  LineOverflow,
  ColorGlyphRequired,
  MultiPageRequired,
  CompatibilityConstraintViolation,
  AsciiConstraintViolation,
  BakeBudgetExceeded,
  BakedAtlasMismatch,
};

[[nodiscard]] constexpr std::string_view toString(DiagnosticCode code) {
  switch (code) {
    case DiagnosticCode::None:
      return "None";
    case DiagnosticCode::MissingGlyphCluster:
      return "MissingGlyphCluster";
    case DiagnosticCode::FontFallbackApplied:
      return "FontFallbackApplied";
    case DiagnosticCode::MetricsVerificationFailed:
      return "MetricsVerificationFailed";
    case DiagnosticCode::AtlasGlyphTooLarge:
      return "AtlasGlyphTooLarge";
    case DiagnosticCode::AtlasEntryMissing:
      return "AtlasEntryMissing";
    case DiagnosticCode::LineOverflow:
      return "LineOverflow";
    case DiagnosticCode::ColorGlyphRequired:
      return "ColorGlyphRequired";
    case DiagnosticCode::MultiPageRequired:
      return "MultiPageRequired";
    case DiagnosticCode::CompatibilityConstraintViolation:
      return "CompatibilityConstraintViolation";
    case DiagnosticCode::AsciiConstraintViolation:
      return "AsciiConstraintViolation";
    case DiagnosticCode::BakeBudgetExceeded:
      return "BakeBudgetExceeded";
    case DiagnosticCode::BakedAtlasMismatch:
      return "BakedAtlasMismatch";
  }
  return "Unknown";
}

enum class MaterialKind {
  AlphaMask,
  ColorBitmap,
};

[[nodiscard]] constexpr std::string_view toString(MaterialKind kind) {
  switch (kind) {
    case MaterialKind::AlphaMask:
      return "AlphaMask";
    case MaterialKind::ColorBitmap:
      return "ColorBitmap";
  }
  return "Unknown";
}

enum class AtlasPixelFormat {
  // Straight RGBA where R = G = B = A = coverage.
  AlphaMaskStraightRgba,
  // Straight RGBA intended for regular sRGB color texture sampling.
  // DokuText does not premultiply or tint these texels.
  ColorStraightRgba,
};

[[nodiscard]] constexpr std::string_view toString(AtlasPixelFormat format) {
  switch (format) {
    case AtlasPixelFormat::AlphaMaskStraightRgba:
      return "AlphaMaskStraightRgba";
    case AtlasPixelFormat::ColorStraightRgba:
      return "ColorStraightRgba";
  }
  return "Unknown";
}

[[nodiscard]] constexpr bool materialSupportsVertexTint(MaterialKind kind) {
  switch (kind) {
    case MaterialKind::AlphaMask:
      return true;
    case MaterialKind::ColorBitmap:
      return false;
  }
  return false;
}

enum class AtlasUvOrigin {
  TopLeft,
};

[[nodiscard]] constexpr std::string_view toString(AtlasUvOrigin origin) {
  switch (origin) {
    case AtlasUvOrigin::TopLeft:
      return "TopLeft";
  }
  return "Unknown";
}

enum class SamplerKind {
  LinearClamp,
};

struct TextAtlasPageInfo {
  GlyphAtlasPageHandle page;
  std::uint32_t pageIndex = 0;
  MaterialKind materialKind = MaterialKind::AlphaMask;
  AtlasPixelFormat pixelFormat = AtlasPixelFormat::AlphaMaskStraightRgba;
  SamplerKind samplerKind = SamplerKind::LinearClamp;
};

[[nodiscard]] constexpr std::string_view toString(SamplerKind kind) {
  switch (kind) {
    case SamplerKind::LinearClamp:
      return "LinearClamp";
  }
  return "Unknown";
}

enum class BlendHint {
  Coverage,
  PremultipliedAlpha,
};

[[nodiscard]] constexpr std::string_view toString(BlendHint hint) {
  switch (hint) {
    case BlendHint::Coverage:
      return "Coverage";
    case BlendHint::PremultipliedAlpha:
      return "PremultipliedAlpha";
  }
  return "Unknown";
}

enum class HorizontalAlign {
  Left,
  Center,
  Right,
};

[[nodiscard]] constexpr std::string_view toString(HorizontalAlign align) {
  switch (align) {
    case HorizontalAlign::Left:
      return "Left";
    case HorizontalAlign::Center:
      return "Center";
    case HorizontalAlign::Right:
      return "Right";
  }
  return "Unknown";
}

enum class BaselinePolicy {
  FontMetrics,
  Tight,
};

[[nodiscard]] constexpr std::string_view toString(BaselinePolicy policy) {
  switch (policy) {
    case BaselinePolicy::FontMetrics:
      return "FontMetrics";
    case BaselinePolicy::Tight:
      return "Tight";
  }
  return "Unknown";
}

struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Info;
  DiagnosticCode code = DiagnosticCode::None;
  std::size_t lineIndex = 0;
  std::size_t clusterIndex = 0;
  std::string message;
  std::string clusterText;
  std::vector<std::uint32_t> codePoints;
  std::string fontPath;
};

struct FontFallbackChain {
  std::vector<std::string> textFonts;
  std::vector<std::string> emojiFonts;
  std::vector<std::string> symbolFonts;
};

// PositionedGlyph::originX/Y is the top-left pixel position of the glyph bitmap
// in line-local or paragraph-local space depending on the containing result.
// atlasRect and uvRect describe the glyph image area inside the atlas page.
// uvRect is normalized and uses AtlasUvOrigin::TopLeft.
struct PositionedGlyph {
  GlyphHandle handle;
  std::uint32_t glyphId = 0;
  std::size_t lineIndex = 0;
  std::size_t clusterIndex = 0;
  std::size_t glyphIndexInCluster = 0;
  GlyphBitmap glyph;
  int originX = 0;
  int originY = 0;
  bool isColorGlyph = false;
  bool fallbackApplied = false;
  int atlasPage = -1;
  GlyphAtlasPageHandle atlasPageHandle;
  Rect atlasRect;
  FloatRect uvRect;
  MaterialKind materialKind = MaterialKind::AlphaMask;
  SamplerKind samplerKind = SamplerKind::LinearClamp;
  BlendHint blendHint = BlendHint::Coverage;
};

struct ParagraphLayoutOptions {
  int wrappingWidth = 0;
  HorizontalAlign horizontalAlign = HorizontalAlign::Left;
  float lineSpacingMultiplier = 1.0F;
  BaselinePolicy baselinePolicy = BaselinePolicy::FontMetrics;
};

// TextRequest is the preferred public input contract for common usage.
struct TextRequest {
  std::string text;
  FontChainId fontChainId;
  unsigned int paletteIndex = 0;
  unsigned int pixelSize = 128;
  bool verifyMetrics = false;
  ParagraphLayoutOptions layoutOptions;
};

// TextLayoutContext is the advanced low-level contract.
// Prefer TextSystem + TextAtlas/BakedTextAtlas + TextRequest unless you need
// to control shaping/raster/layout stages directly.
struct TextLayoutContext {
  FontFallbackChain fallbackChain;
  FontChainId fontChainId;
  unsigned int paletteIndex = 0;
  unsigned int pixelSize = 128;
  bool verifyMetrics = false;
  ParagraphLayoutOptions paragraphOptions;
};

struct ShapedGlyphInfo {
  GlyphHandle handle;
  std::uint32_t glyphId = 0;
  std::size_t clusterIndex = 0;
  std::size_t glyphIndexInCluster = 0;
  int xAdvance = 0;
  int yAdvance = 0;
  int xOffset = 0;
  int yOffset = 0;
  bool isColorGlyph = false;
};

struct ShapedCluster {
  std::string text;
  std::vector<std::uint32_t> codePoints;
  std::vector<ShapedGlyphInfo> glyphs;
  std::string fontPath;
  FontRole requestedRole = FontRole::Text;
  bool fallbackApplied = false;
};

struct FallbackTraceEntry {
  std::size_t lineIndex = 0;
  std::size_t clusterIndex = 0;
  FontRole requestedRole = FontRole::Text;
  bool fallbackApplied = false;
  bool resolved = false;
  bool isColorGlyph = false;
  std::string clusterText;
  std::vector<std::uint32_t> codePoints;
  std::string selectedFontPath;
  std::vector<std::string> attemptedFonts;
};

struct TextShapeResult {
  std::vector<ShapedCluster> clusters;
  LineMetrics metrics;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<FallbackTraceEntry> fallbackTrace;
  std::vector<Diagnostic> diagnostics;
};

struct RasterizedGlyph {
  GlyphHandle handle;
  std::uint32_t glyphId = 0;
  std::size_t lineIndex = 0;
  std::size_t clusterIndex = 0;
  std::size_t glyphIndexInCluster = 0;
  GlyphBitmap glyph;
  int penX = 0;
  int penY = 0;
  bool isColorGlyph = false;
  bool fallbackApplied = false;
};

struct TextRasterResult {
  std::vector<RasterizedGlyph> glyphs;
  LineMetrics metrics;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<FallbackTraceEntry> fallbackTrace;
  std::vector<Diagnostic> diagnostics;
};

struct MultiLineShapeResult {
  std::vector<TextShapeResult> lines;
  std::vector<std::string> lineTexts;
  std::vector<LineSummary> lineSummaries;
  std::vector<FallbackTraceEntry> fallbackTrace;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<Diagnostic> diagnostics;
};

struct MultiLineRasterResult {
  std::vector<TextRasterResult> lines;
  std::vector<RasterizedGlyph> glyphs;
  std::vector<std::string> lineTexts;
  std::vector<int> lineOriginsY;
  std::vector<LineSummary> lineSummaries;
  std::vector<FallbackTraceEntry> fallbackTrace;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<Diagnostic> diagnostics;
  Rect boundingBox;
  int lineAdvance = 0;
};

struct TextLineLayout {
  std::vector<PositionedGlyph> glyphs;
  LineMetrics metrics;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<FallbackTraceEntry> fallbackTrace;
  std::vector<Diagnostic> diagnostics;
};

struct TextDrawRun {
  std::vector<PositionedGlyph> glyphs;
  LineMetrics metrics;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<FallbackTraceEntry> fallbackTrace;
  std::vector<Diagnostic> diagnostics;
};

// TextCompatibilityReport is a stable renderer-requirement summary.
// It answers what the renderer must support for the analyzed result.
// diagnostics explains why a constrained mode is not compatible.
struct TextCompatibilityReport {
  bool alphaOnlyCompatible = true;
  bool singlePageCompatible = true;
  bool requiresColorGlyph = false;
  bool requiresMultiplePages = false;
  bool hasMissingGlyph = false;
  bool hasAtlasOverflow = false;
  std::vector<int> pagesUsed;
  std::vector<std::string> fallbackFonts;
  std::vector<MaterialKind> materialKindsUsed;
  std::vector<Diagnostic> diagnostics;
};

struct CompatibilityConstraints {
  bool allowColorGlyphs = false;
  bool allowMultiplePages = false;
  int maxPages = 1;
};

// PositionedDrawGlyph::pixelRect is the final draw quad in the same coordinate
// space as TextDrawSubmission::metrics and boundingBox.
// uvRect is normalized and uses AtlasUvOrigin::TopLeft.
struct PositionedDrawGlyph {
  GlyphHandle handle;
  std::uint32_t codePoint = 0;
  std::uint32_t glyphId = 0;
  std::size_t lineIndex = 0;
  int atlasPage = -1;
  GlyphAtlasPageHandle atlasPageHandle;
  MaterialKind materialKind = MaterialKind::AlphaMask;
  SamplerKind samplerKind = SamplerKind::LinearClamp;
  BlendHint blendHint = BlendHint::Coverage;
  Rect pixelRect;
  FloatRect uvRect;
  int advanceX = 0;
  int bearingX = 0;
  int bearingY = 0;
  bool isColorGlyph = false;
  bool visible = false;
};

struct TextDrawSubmission {
  LineMetrics metrics;
  Rect boundingBox;
  int lineAdvance = 0;
  TextCompatibilityReport compatibility;
  std::vector<PositionedDrawGlyph> glyphs;
  std::vector<Diagnostic> diagnostics;
};

// AtlasPageUploadPlan refers to GlyphAtlasPage::image.
// image.rgba always uses a top-left origin. dirtyRects use the same origin.
struct AtlasPageUploadPlan {
  GlyphAtlasPageHandle pageHandle;
  int pageIndex = -1;
  bool isNewPage = false;
  std::vector<Rect> dirtyRects;
};

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

struct MultiLineLayout {
  std::vector<TextLineLayout> lines;
  std::vector<PositionedGlyph> glyphs;
  std::vector<std::string> lineTexts;
  std::vector<int> lineOriginsY;
  std::vector<LineSummary> lineSummaries;
  std::vector<FallbackTraceEntry> fallbackTrace;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<Diagnostic> diagnostics;
  Rect boundingBox;
  int lineAdvance = 0;
};

struct MultiLineDrawRun {
  std::vector<TextDrawRun> lines;
  std::vector<PositionedGlyph> glyphs;
  std::vector<AtlasBuildResult> atlases;
  std::vector<std::string> lineTexts;
  std::vector<int> lineOriginsY;
  std::vector<LineSummary> lineSummaries;
  std::vector<FallbackTraceEntry> fallbackTrace;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<Diagnostic> diagnostics;
  Rect boundingBox;
  int lineAdvance = 0;
};

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

struct TextPrepareOptions {
  bool verifyMetrics = false;

  /// If true, prepareLine/prepareText may append missing glyphs to the provided
  /// TextAtlas. If false, prepareLine/prepareText must not mutate the atlas;
  /// missing atlas glyphs remain missing and should be diagnosed through
  /// probeText or returned diagnostics if available.
  bool allowGlyphAppend = true;

  ParagraphLayoutOptions layoutOptions;
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

struct PreparedTextLine {
  TextLineLayout layout;
  TextDrawRun drawRun;
  TextDrawSubmission submission;
  TextCompatibilityReport compatibility;
  GlyphAtlasCacheUpdate atlasUpdate;
  std::vector<AtlasPageUploadPlan> atlasUploadPlan;
  std::vector<std::string> missingGlyphs;
  std::vector<Diagnostic> diagnostics;
};

struct PreparedTextParagraph {
  MultiLineLayout layout;
  MultiLineDrawRun drawRun;
  TextDrawSubmission submission;
  TextCompatibilityReport compatibility;
  GlyphAtlasCacheUpdate atlasUpdate;
  std::vector<AtlasPageUploadPlan> atlasUploadPlan;
  std::vector<std::string> missingGlyphs;
  std::vector<Diagnostic> diagnostics;
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

/// TextProbeResult contains diagnostics for a text request without mutating the
/// atlas.
struct TextProbeResult {
  TextRequest request;

  /// Number of Unicode codepoints requested for probing.
  std::size_t requestedCodepointCount = 0;
  /// Number of glyph lookup events resolved through shaping/fallback.
  std::size_t resolvedGlyphCount = 0;
  /// Number of resolved glyph lookup events whose rasterization handle is
  /// already present in the atlas. This may include duplicates if the same
  /// glyph appears multiple times in the text.
  std::size_t alreadyPresentGlyphCount = 0;
  /// Number of resolved glyph lookup events whose rasterization handle is not
  /// present in the atlas. This count corresponds to the number of elements in
  /// the missingGlyphs vector.
  std::size_t missingGlyphCount = 0;

  /// True if every resolved glyph required for the text is already present in
  /// the atlas.
  bool allGlyphsAvailable = false;
  /// True if prepare/seed with append behavior would need to insert one or more
  /// glyphs into the atlas.
  bool wouldAppendGlyphs = false;

  /// The corresponding atlas-missing glyph lookup events.
  /// This list may contain duplicate handles if the same missing glyph appears
  /// multiple times in the probed text. These are missing from the atlas, not
  /// necessarily missing from the font fallback chain.
  std::vector<MissingAtlasGlyph> missingGlyphs;
  /// Non-fatal diagnostics from shaping/fallback/probing.
  std::vector<Diagnostic> diagnostics;
};

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

class TextSystem {
public:
  TextSystem();
  ~TextSystem();

  TextSystem(TextSystem &&) noexcept;
  TextSystem &operator=(TextSystem &&) noexcept;

  TextSystem(const TextSystem &) = delete;
  TextSystem &operator=(const TextSystem &) = delete;

  FontChainId registerFontFallbackChain(const FontFallbackChain &chain);

  [[nodiscard]] FontFallbackChain
  resolveFontFallbackChain(FontChainId fontChainId) const;

  [[nodiscard]] TextAtlas createAtlas(const TextAtlasOptions &options) const;

  [[nodiscard]] TextAtlasSeedResult
  seedAtlas(TextAtlas &atlas, const TextSeedRequest &seedRequest) const;

  [[nodiscard]] TextProbeResult
  probeText(const TextAtlas &atlas, std::string_view text,
            const TextPrepareOptions &options = {}) const;

  [[nodiscard]] TextProbeResult probeText(const TextAtlas &atlas,
                                          const TextRequest &request) const;

  [[nodiscard]] BakedTextAtlas
  bakeTextAtlas(const TextAtlasBakeRequest &request) const;

  [[nodiscard]] PreparedTextLine
  prepareLine(TextAtlas &atlas, std::string_view text,
              const TextPrepareOptions &options = {}) const;

  [[nodiscard]] PreparedTextLine prepareLine(TextAtlas &atlas,
                                             const TextRequest &request) const;

  [[nodiscard]] PreparedTextLine
  prepareLine(TextAtlas &atlas, const TextRequest &request,
              const TextPrepareOptions &options) const;

  [[nodiscard]] PreparedTextParagraph
  prepareText(TextAtlas &atlas, std::string_view text,
              const TextPrepareOptions &options = {}) const;

  [[nodiscard]] PreparedTextParagraph
  prepareText(TextAtlas &atlas, const TextRequest &request) const;

  [[nodiscard]] PreparedTextParagraph
  prepareText(TextAtlas &atlas, const TextRequest &request,
              const TextPrepareOptions &options) const;

  [[nodiscard]] PreparedTextLine
  layoutTextLine(const BakedTextAtlas &atlas, const TextRequest &request) const;

  [[nodiscard]] PreparedTextParagraph
  layoutTextParagraph(const BakedTextAtlas &atlas,
                      const TextRequest &request) const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

int runCli(const std::vector<std::string> &args);

int runCli(int argc, char *argv[]);

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
