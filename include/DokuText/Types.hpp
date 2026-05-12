// This file is part of the public API.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
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

enum class MaterialKind {
  AlphaMask,
  ColorBitmap,
};

// Straight RGBA where R = G = B = A = coverage.
enum class AtlasPixelFormat {
  AlphaMaskStraightRgba,
  // Straight RGBA intended for regular sRGB color texture sampling.
  // DokuText does not premultiply or tint these texels.
  ColorStraightRgba,
};

enum class AtlasUvOrigin {
  TopLeft,
};

enum class SamplerKind {
  LinearClamp,
};

enum class BlendHint {
  Coverage,
  PremultipliedAlpha,
};

} // namespace doku::text
