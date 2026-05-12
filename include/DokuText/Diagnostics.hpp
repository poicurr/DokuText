#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <DokuText/Types.hpp>

namespace doku::text {

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
  InvalidCaretOffset,
  UnsupportedGraphemeClusterSplit,
  HitTestClusterFallback,
  SelectionRangeClamped,
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
    case DiagnosticCode::InvalidCaretOffset:
      return "InvalidCaretOffset";
    case DiagnosticCode::UnsupportedGraphemeClusterSplit:
      return "UnsupportedGraphemeClusterSplit";
    case DiagnosticCode::HitTestClusterFallback:
      return "HitTestClusterFallback";
    case DiagnosticCode::SelectionRangeClamped:
      return "SelectionRangeClamped";
  }
  return "Unknown";
}

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

} // namespace doku::text
