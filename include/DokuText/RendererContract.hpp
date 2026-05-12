// This file is part of the public API.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <DokuText/Diagnostics.hpp>
#include <DokuText/Types.hpp>

namespace doku::text {

[[nodiscard]] constexpr std::string_view toString(MaterialKind kind) {
  switch (kind) {
    case MaterialKind::AlphaMask:
      return "AlphaMask";
    case MaterialKind::ColorBitmap:
      return "ColorBitmap";
  }
  return "Unknown";
}

[[nodiscard]] constexpr std::string_view toString(AtlasPixelFormat format) {
  switch (format) {
    case AtlasPixelFormat::AlphaMaskStraightRgba:
      return "AlphaMaskStraightRgba";
    case AtlasPixelFormat::ColorStraightRgba:
      return "ColorStraightRgba";
  }
  return "Unknown";
}

[[nodiscard]] constexpr std::string_view toString(AtlasUvOrigin origin) {
  switch (origin) {
    case AtlasUvOrigin::TopLeft:
      return "TopLeft";
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

[[nodiscard]] constexpr std::string_view toString(BlendHint hint) {
  switch (hint) {
    case BlendHint::Coverage:
      return "Coverage";
    case BlendHint::PremultipliedAlpha:
      return "PremultipliedAlpha";
  }
  return "Unknown";
}

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

} // namespace doku::text
