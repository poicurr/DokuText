// This file is part of the public API.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <DokuText/Atlas.hpp>
#include <DokuText/Diagnostics.hpp>
#include <DokuText/Types.hpp>

namespace doku::text {

enum class FontRole : std::uint8_t;

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
  std::size_t beginByteOffset = 0;
  std::size_t endByteOffset = 0;
  std::vector<std::uint32_t> codePoints;
  std::vector<ShapedGlyphInfo> glyphs;
  std::string fontPath;
  FontRole requestedRole = static_cast<FontRole>(0);
  bool fallbackApplied = false;
};

struct FallbackTraceEntry {
  std::size_t lineIndex = 0;
  std::size_t clusterIndex = 0;
  FontRole requestedRole = static_cast<FontRole>(0);
  bool fallbackApplied = false;
  bool resolved = false;
  bool isColorGlyph = false;
  std::string clusterText;
  std::vector<std::uint32_t> codePoints;
  std::string selectedFontPath;
  std::vector<std::string> attemptedFonts;
};

struct TextShapeResult {
  std::string text;
  std::vector<ShapedCluster> clusters;
  LineMetrics metrics;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<FallbackTraceEntry> fallbackTrace;
  std::vector<Diagnostic> diagnostics;
};

struct TextClusterLayout {
  std::size_t beginByteOffset = 0;
  std::size_t endByteOffset = 0;
  float xMin = 0.0F;
  float xMax = 0.0F;
  std::uint32_t glyphBegin = 0;
  std::uint32_t glyphEnd = 0;
};

struct TextCaretBoundary {
  std::size_t byteOffset = 0;
  float x = 0.0F;
  std::uint32_t clusterIndex = 0;
  bool leading = true;
};

struct TextHitTestResult {
  std::size_t byteOffset = 0;
  std::uint32_t boundaryIndex = 0;
  float caretX = 0.0F;
};

struct TextSelectionRect {
  float x = 0.0F;
  float y = 0.0F;
  float width = 0.0F;
  float height = 0.0F;
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
  std::string text;
  std::vector<RasterizedGlyph> glyphs;
  std::vector<TextClusterLayout> clusters;
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
  std::string text;
  std::vector<PositionedGlyph> glyphs;
  std::vector<TextClusterLayout> clusters;
  std::vector<TextCaretBoundary> caretBoundaries;
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

struct MultiLineLayout {
  std::vector<TextLineLayout> lines;
  std::vector<std::string> lineTexts;
  std::vector<int> lineOriginsY;
  std::vector<LineSummary> lineSummaries;
  std::vector<FallbackTraceEntry> fallbackTrace;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<Diagnostic> diagnostics;
  std::vector<PositionedGlyph> glyphs;
  Rect boundingBox;
  int lineAdvance = 0;
};

struct MultiLineDrawRun {
  std::vector<TextDrawRun> lines;
  std::vector<AtlasBuildResult> atlases;
  std::vector<std::string> lineTexts;
  std::vector<int> lineOriginsY;
  std::vector<LineSummary> lineSummaries;
  std::vector<FallbackTraceEntry> fallbackTrace;
  LineSummary summary;
  DiagnosticsSummary diagnosticsSummary;
  std::vector<Diagnostic> diagnostics;
  std::vector<PositionedGlyph> glyphs;
  Rect boundingBox;
  int lineAdvance = 0;
};

} // namespace doku::text
