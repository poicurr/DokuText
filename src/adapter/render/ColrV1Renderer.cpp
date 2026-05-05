#include "ColrV1Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include FT_COLOR_H

namespace {

constexpr int kMaxRecursionDepth = 8;
constexpr float kPi = 3.14159265358979323846F;

struct Bounds {
  int minX = 0;
  int maxX = 0;
  int minY = 0;
  int maxY = 0;
};

struct AffineTransform {
  float xx = 1.0F;
  float xy = 0.0F;
  float yx = 0.0F;
  float yy = 1.0F;
  float dx = 0.0F;
  float dy = 0.0F;
};

struct AlphaMask {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> data;
};

struct Vec2 {
  float x = 0.0F;
  float y = 0.0F;
};

struct GradientStop {
  float offset = 0.0F;
  std::array<uint8_t, 4> color{};
};

[[nodiscard]] int roundFrom26Dot6(FT_Pos value) {
  return static_cast<int>(std::lround(static_cast<double>(value) / 64.0));
}

[[nodiscard]] int floorFrom26Dot6(FT_Pos value) {
  return static_cast<int>(std::floor(static_cast<double>(value) / 64.0));
}

[[nodiscard]] int ceilFrom26Dot6(FT_Pos value) {
  return static_cast<int>(std::ceil(static_cast<double>(value) / 64.0));
}

[[nodiscard]] float fixed16Dot16ToFloat(FT_Fixed value) {
  return static_cast<float>(value) / 65536.0F;
}

[[nodiscard]] float fixed2Dot14ToFloat(FT_F2Dot14 value) {
  return static_cast<float>(value) / 16384.0F;
}

[[nodiscard]] AffineTransform identityTransform() { return {}; }

[[nodiscard]] AffineTransform compose(const AffineTransform &parent,
                                      const AffineTransform &child) {
  AffineTransform result;
  result.xx = parent.xx * child.xx + parent.xy * child.yx;
  result.xy = parent.xx * child.xy + parent.xy * child.yy;
  result.yx = parent.yx * child.xx + parent.yy * child.yx;
  result.yy = parent.yx * child.xy + parent.yy * child.yy;
  result.dx = parent.xx * child.dx + parent.xy * child.dy + parent.dx;
  result.dy = parent.yx * child.dx + parent.yy * child.dy + parent.dy;
  return result;
}

[[nodiscard]] AffineTransform translateTransform(float dx, float dy) {
  AffineTransform transform;
  transform.dx = dx;
  transform.dy = dy;
  return transform;
}

[[nodiscard]] AffineTransform scaleTransform(float sx, float sy, float cx,
                                             float cy) {
  AffineTransform transform;
  transform.xx = sx;
  transform.yy = sy;
  transform.dx = cx - sx * cx;
  transform.dy = cy - sy * cy;
  return transform;
}

[[nodiscard]] AffineTransform rotateTransform(float angleRadians, float cx,
                                              float cy) {
  const float s = std::sin(angleRadians);
  const float c = std::cos(angleRadians);
  AffineTransform transform;
  transform.xx = c;
  transform.xy = -s;
  transform.yx = s;
  transform.yy = c;
  transform.dx = cx - c * cx + s * cy;
  transform.dy = cy - s * cx - c * cy;
  return transform;
}

[[nodiscard]] AffineTransform
skewTransform(float xAngleRadians, float yAngleRadians, float cx, float cy) {
  const float tanX = std::tan(xAngleRadians);
  const float tanY = std::tan(yAngleRadians);
  AffineTransform transform;
  transform.xx = 1.0F;
  transform.xy = tanX;
  transform.yx = tanY;
  transform.yy = 1.0F + tanX * tanY;
  transform.dx = -tanX * cy;
  transform.dy = -tanY * cx;
  return transform;
}

[[nodiscard]] FT_Matrix toFTMatrix(const AffineTransform &transform) {
  FT_Matrix matrix;
  matrix.xx = static_cast<FT_Fixed>(transform.xx * 65536.0F);
  matrix.xy = static_cast<FT_Fixed>(transform.xy * 65536.0F);
  matrix.yx = static_cast<FT_Fixed>(transform.yx * 65536.0F);
  matrix.yy = static_cast<FT_Fixed>(transform.yy * 65536.0F);
  return matrix;
}

[[nodiscard]] FT_Vector toFTVector(const AffineTransform &transform) {
  FT_Vector vector;
  vector.x = static_cast<FT_Pos>(std::lround(transform.dx * 64.0F));
  vector.y = static_cast<FT_Pos>(std::lround(transform.dy * 64.0F));
  return vector;
}

[[nodiscard]] AffineTransform invert(const AffineTransform &transform) {
  const float det = transform.xx * transform.yy - transform.xy * transform.yx;
  if (std::fabs(det) <= 1e-8F) {
    return identityTransform();
  }
  const float invDet = 1.0F / det;
  AffineTransform result;
  result.xx = transform.yy * invDet;
  result.xy = -transform.xy * invDet;
  result.yx = -transform.yx * invDet;
  result.yy = transform.xx * invDet;
  result.dx = -(result.xx * transform.dx + result.xy * transform.dy);
  result.dy = -(result.yx * transform.dx + result.yy * transform.dy);
  return result;
}

[[nodiscard]] Vec2 apply(const AffineTransform &transform, Vec2 point) {
  Vec2 result;
  result.x = transform.xx * point.x + transform.xy * point.y + transform.dx;
  result.y = transform.yx * point.x + transform.yy * point.y + transform.dy;
  return result;
}

[[nodiscard]] Vec2 makeVec(const FT_Vector &vector) {
  return {static_cast<float>(vector.x) / 64.0F,
          static_cast<float>(vector.y) / 64.0F};
}

[[nodiscard]] float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

[[nodiscard]] float lengthSquared(Vec2 value) { return dot(value, value); }

[[nodiscard]] Vec2 subtract(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }

struct PremulRgba {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
  float a = 0.0F;
};

struct FloatRgb {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
};

[[nodiscard]] Vec2 pixelToDevice(int x, int y, const Bounds &bounds) {
  const float deviceX =
      static_cast<float>(bounds.minX) + static_cast<float>(x) + 0.5F;
  const float deviceY =
      static_cast<float>(bounds.maxY) - (static_cast<float>(y) + 0.5F);
  return {deviceX, deviceY};
}

[[nodiscard]] PremulRgba toPremul(const uint8_t *rgba) {
  const float alpha = static_cast<float>(rgba[3]) / 255.0F;
  return {static_cast<float>(rgba[0]) / 255.0F * alpha,
          static_cast<float>(rgba[1]) / 255.0F * alpha,
          static_cast<float>(rgba[2]) / 255.0F * alpha, alpha};
}

[[nodiscard]] PremulRgba clampPremul(PremulRgba color) {
  color.r = std::clamp(color.r, 0.0F, 1.0F);
  color.g = std::clamp(color.g, 0.0F, 1.0F);
  color.b = std::clamp(color.b, 0.0F, 1.0F);
  color.a = std::clamp(color.a, 0.0F, 1.0F);
  return color;
}

void storePremul(uint8_t *dst, PremulRgba color) {
  color = clampPremul(color);
  if (color.a <= 1e-6F) {
    dst[0] = dst[1] = dst[2] = dst[3] = 0U;
    return;
  }
  dst[0] = static_cast<uint8_t>(
      std::clamp(color.r / color.a, 0.0F, 1.0F) * 255.0F + 0.5F);
  dst[1] = static_cast<uint8_t>(
      std::clamp(color.g / color.a, 0.0F, 1.0F) * 255.0F + 0.5F);
  dst[2] = static_cast<uint8_t>(
      std::clamp(color.b / color.a, 0.0F, 1.0F) * 255.0F + 0.5F);
  dst[3] =
      static_cast<uint8_t>(std::clamp(color.a, 0.0F, 1.0F) * 255.0F + 0.5F);
}

[[nodiscard]] PremulRgba composePremul(PremulRgba src, PremulRgba dst,
                                       FT_Composite_Mode mode) {
  const auto luminance = [](FloatRgb color) {
    return 0.3F * color.r + 0.59F * color.g + 0.11F * color.b;
  };
  const auto clipColor = [&](FloatRgb color) {
    const float lum = luminance(color);
    const float minValue = std::min({color.r, color.g, color.b});
    const float maxValue = std::max({color.r, color.g, color.b});
    if (minValue < 0.0F) {
      color.r = lum + (color.r - lum) * lum / (lum - minValue);
      color.g = lum + (color.g - lum) * lum / (lum - minValue);
      color.b = lum + (color.b - lum) * lum / (lum - minValue);
    }
    if (maxValue > 1.0F) {
      color.r = lum + (color.r - lum) * (1.0F - lum) / (maxValue - lum);
      color.g = lum + (color.g - lum) * (1.0F - lum) / (maxValue - lum);
      color.b = lum + (color.b - lum) * (1.0F - lum) / (maxValue - lum);
    }
    color.r = std::clamp(color.r, 0.0F, 1.0F);
    color.g = std::clamp(color.g, 0.0F, 1.0F);
    color.b = std::clamp(color.b, 0.0F, 1.0F);
    return color;
  };
  const auto saturation = [](FloatRgb color) {
    return std::max({color.r, color.g, color.b}) -
           std::min({color.r, color.g, color.b});
  };
  const auto setLuminance = [&](FloatRgb color, float targetLuminance) {
    const float delta = targetLuminance - luminance(color);
    color.r += delta;
    color.g += delta;
    color.b += delta;
    return clipColor(color);
  };
  const auto setSaturation = [](FloatRgb color, float targetSaturation) {
    float *components[3] = {&color.r, &color.g, &color.b};
    std::sort(std::begin(components), std::end(components),
              [](const float *lhs, const float *rhs) { return *lhs < *rhs; });
    float &minComponent = *components[0];
    float &midComponent = *components[1];
    float &maxComponent = *components[2];
    if (maxComponent > minComponent) {
      midComponent = ((midComponent - minComponent) * targetSaturation) /
                     (maxComponent - minComponent);
      maxComponent = targetSaturation;
    } else {
      midComponent = 0.0F;
      maxComponent = 0.0F;
    }
    minComponent = 0.0F;
    return color;
  };
  const auto unpremul = [](float component, float alpha) {
    if (alpha <= 1e-6F) {
      return 0.0F;
    }
    return std::clamp(component / alpha, 0.0F, 1.0F);
  };
  const auto overlay = [](float backdrop, float source) {
    if (backdrop <= 0.5F) {
      return 2.0F * backdrop * source;
    }
    return 1.0F - 2.0F * (1.0F - backdrop) * (1.0F - source);
  };
  const auto hardLight = [&overlay](float backdrop, float source) {
    return overlay(source, backdrop);
  };
  const auto colorDodge = [](float backdrop, float source) {
    if (backdrop <= 1e-6F) {
      return 0.0F;
    }
    if (source >= 1.0F - 1e-6F) {
      return 1.0F;
    }
    return std::min(1.0F, backdrop / (1.0F - source));
  };
  const auto colorBurn = [](float backdrop, float source) {
    if (backdrop >= 1.0F - 1e-6F) {
      return 1.0F;
    }
    if (source <= 1e-6F) {
      return 0.0F;
    }
    return 1.0F - std::min(1.0F, (1.0F - backdrop) / source);
  };
  const auto softLight = [](float backdrop, float source) {
    if (source <= 0.5F) {
      return backdrop - (1.0F - 2.0F * source) * backdrop * (1.0F - backdrop);
    }
    const auto d = [](float value) {
      if (value <= 0.25F) {
        return ((16.0F * value - 12.0F) * value + 4.0F) * value;
      }
      return std::sqrt(value);
    };
    return backdrop + (2.0F * source - 1.0F) * (d(backdrop) - backdrop);
  };
  const auto blendSeparable = [&](auto blendChannel) {
    const float srcR = unpremul(src.r, src.a);
    const float srcG = unpremul(src.g, src.a);
    const float srcB = unpremul(src.b, src.a);
    const float dstR = unpremul(dst.r, dst.a);
    const float dstG = unpremul(dst.g, dst.a);
    const float dstB = unpremul(dst.b, dst.a);
    return PremulRgba{src.r * (1.0F - dst.a) + dst.r * (1.0F - src.a) +
                          src.a * dst.a * blendChannel(dstR, srcR),
                      src.g * (1.0F - dst.a) + dst.g * (1.0F - src.a) +
                          src.a * dst.a * blendChannel(dstG, srcG),
                      src.b * (1.0F - dst.a) + dst.b * (1.0F - src.a) +
                          src.a * dst.a * blendChannel(dstB, srcB),
                      src.a + dst.a - src.a * dst.a};
  };
  const auto blendNonSeparable = [&](auto blendColor) {
    const FloatRgb source{unpremul(src.r, src.a), unpremul(src.g, src.a),
                          unpremul(src.b, src.a)};
    const FloatRgb backdrop{unpremul(dst.r, dst.a), unpremul(dst.g, dst.a),
                            unpremul(dst.b, dst.a)};
    const FloatRgb blended = blendColor(backdrop, source);
    return PremulRgba{src.r * (1.0F - dst.a) + dst.r * (1.0F - src.a) +
                          src.a * dst.a * blended.r,
                      src.g * (1.0F - dst.a) + dst.g * (1.0F - src.a) +
                          src.a * dst.a * blended.g,
                      src.b * (1.0F - dst.a) + dst.b * (1.0F - src.a) +
                          src.a * dst.a * blended.b,
                      src.a + dst.a - src.a * dst.a};
  };
  switch (mode) {
    case FT_COLR_COMPOSITE_CLEAR:
      return {};
    case FT_COLR_COMPOSITE_SRC:
      return src;
    case FT_COLR_COMPOSITE_DEST:
      return dst;
    case FT_COLR_COMPOSITE_SRC_OVER:
      return {src.r + dst.r * (1.0F - src.a), src.g + dst.g * (1.0F - src.a),
              src.b + dst.b * (1.0F - src.a), src.a + dst.a * (1.0F - src.a)};
    case FT_COLR_COMPOSITE_DEST_OVER:
      return {dst.r + src.r * (1.0F - dst.a), dst.g + src.g * (1.0F - dst.a),
              dst.b + src.b * (1.0F - dst.a), dst.a + src.a * (1.0F - dst.a)};
    case FT_COLR_COMPOSITE_SRC_IN:
      return {src.r * dst.a, src.g * dst.a, src.b * dst.a, src.a * dst.a};
    case FT_COLR_COMPOSITE_DEST_IN:
      return {dst.r * src.a, dst.g * src.a, dst.b * src.a, dst.a * src.a};
    case FT_COLR_COMPOSITE_SRC_OUT:
      return {src.r * (1.0F - dst.a), src.g * (1.0F - dst.a),
              src.b * (1.0F - dst.a), src.a * (1.0F - dst.a)};
    case FT_COLR_COMPOSITE_DEST_OUT:
      return {dst.r * (1.0F - src.a), dst.g * (1.0F - src.a),
              dst.b * (1.0F - src.a), dst.a * (1.0F - src.a)};
    case FT_COLR_COMPOSITE_SRC_ATOP:
      return {src.r * dst.a + dst.r * (1.0F - src.a),
              src.g * dst.a + dst.g * (1.0F - src.a),
              src.b * dst.a + dst.b * (1.0F - src.a), dst.a};
    case FT_COLR_COMPOSITE_DEST_ATOP:
      return {dst.r * src.a + src.r * (1.0F - dst.a),
              dst.g * src.a + src.g * (1.0F - dst.a),
              dst.b * src.a + src.b * (1.0F - dst.a), src.a};
    case FT_COLR_COMPOSITE_XOR:
      return {src.r * (1.0F - dst.a) + dst.r * (1.0F - src.a),
              src.g * (1.0F - dst.a) + dst.g * (1.0F - src.a),
              src.b * (1.0F - dst.a) + dst.b * (1.0F - src.a),
              src.a * (1.0F - dst.a) + dst.a * (1.0F - src.a)};
    case FT_COLR_COMPOSITE_PLUS:
      return {std::min(1.0F, src.r + dst.r), std::min(1.0F, src.g + dst.g),
              std::min(1.0F, src.b + dst.b), std::min(1.0F, src.a + dst.a)};
    case FT_COLR_COMPOSITE_SCREEN:
      return blendSeparable([](float backdrop, float source) {
        return backdrop + source - backdrop * source;
      });
    case FT_COLR_COMPOSITE_OVERLAY:
      return blendSeparable(overlay);
    case FT_COLR_COMPOSITE_DARKEN:
      return blendSeparable([](float backdrop, float source) {
        return std::min(backdrop, source);
      });
    case FT_COLR_COMPOSITE_LIGHTEN:
      return blendSeparable([](float backdrop, float source) {
        return std::max(backdrop, source);
      });
    case FT_COLR_COMPOSITE_COLOR_DODGE:
      return blendSeparable(colorDodge);
    case FT_COLR_COMPOSITE_COLOR_BURN:
      return blendSeparable(colorBurn);
    case FT_COLR_COMPOSITE_HARD_LIGHT:
      return blendSeparable(hardLight);
    case FT_COLR_COMPOSITE_SOFT_LIGHT:
      return blendSeparable(softLight);
    case FT_COLR_COMPOSITE_DIFFERENCE:
      return blendSeparable([](float backdrop, float source) {
        return std::fabs(backdrop - source);
      });
    case FT_COLR_COMPOSITE_EXCLUSION:
      return blendSeparable([](float backdrop, float source) {
        return backdrop + source - 2.0F * backdrop * source;
      });
    case FT_COLR_COMPOSITE_MULTIPLY:
      return blendSeparable(
          [](float backdrop, float source) { return backdrop * source; });
    case FT_COLR_COMPOSITE_HSL_HUE:
      return blendNonSeparable([&](FloatRgb backdrop, FloatRgb source) {
        return setLuminance(setSaturation(source, saturation(backdrop)),
                            luminance(backdrop));
      });
    case FT_COLR_COMPOSITE_HSL_SATURATION:
      return blendNonSeparable([&](FloatRgb backdrop, FloatRgb source) {
        return setLuminance(setSaturation(backdrop, saturation(source)),
                            luminance(backdrop));
      });
    case FT_COLR_COMPOSITE_HSL_COLOR:
      return blendNonSeparable([&](FloatRgb backdrop, FloatRgb source) {
        return setLuminance(source, luminance(backdrop));
      });
    case FT_COLR_COMPOSITE_HSL_LUMINOSITY:
      return blendNonSeparable([&](FloatRgb backdrop, FloatRgb source) {
        return setLuminance(backdrop, luminance(source));
      });
    default:
      throw std::runtime_error("未対応のCOLRv1コンポジットモードです");
  }
}

[[nodiscard]] std::array<uint8_t, 4>
resolveColor(const ColorPalette &palette, const FT_ColorIndex &colorIndex) {
  std::array<uint8_t, 4> color{};
  if (colorIndex.palette_index == 0xFFFF) {
    color = palette.m_foregroundColor;
  } else if (colorIndex.palette_index < palette.m_colors.size()) {
    color = palette.m_colors[colorIndex.palette_index];
  } else {
    throw std::runtime_error("カラーパレットインデックスが範囲外です");
  }
  const float alphaFactor =
      std::clamp(fixed2Dot14ToFloat(colorIndex.alpha), 0.0F, 1.0F);
  const float baseAlpha = static_cast<float>(color[3]) / 255.0F;
  color[3] = static_cast<uint8_t>(
      std::clamp(baseAlpha * alphaFactor, 0.0F, 1.0F) * 255.0F + 0.5F);
  return color;
}

[[nodiscard]] AlphaMask makeFullMask(int width, int height, uint8_t value) {
  AlphaMask mask;
  mask.width = width;
  mask.height = height;
  mask.data.assign(static_cast<std::size_t>(width) *
                       static_cast<std::size_t>(height),
                   value);
  return mask;
}

[[nodiscard]] std::vector<GradientStop>
collectGradientStops(FT_Face face, const FT_ColorLine &colorLine,
                     const ColorPalette &palette) {
  std::vector<GradientStop> stops;
  FT_ColorStopIterator iterator = colorLine.color_stop_iterator;
  FT_ColorStop stop{};
  while (FT_Get_Colorline_Stops(face, &stop, &iterator)) {
    GradientStop gradientStop;
    gradientStop.offset = static_cast<float>(stop.stop_offset) / 65536.0F;
    gradientStop.color = resolveColor(palette, stop.color);
    stops.push_back(gradientStop);
  }
  std::sort(stops.begin(), stops.end(),
            [](const GradientStop &lhs, const GradientStop &rhs) {
              return lhs.offset < rhs.offset;
            });
  return stops;
}

[[nodiscard]] float applyExtend(float value, float minOffset, float maxOffset,
                                FT_PaintExtend extend) {
  const float length = maxOffset - minOffset;
  if (length <= 1e-6F) {
    return minOffset;
  }
  switch (extend) {
    case FT_COLR_PAINT_EXTEND_PAD:
      return std::clamp(value, minOffset, maxOffset);
    case FT_COLR_PAINT_EXTEND_REPEAT: {
      float wrapped = std::fmod(value - minOffset, length);
      if (wrapped < 0.0F) {
        wrapped += length;
      }
      return minOffset + wrapped;
    }
    case FT_COLR_PAINT_EXTEND_REFLECT: {
      const float period = length * 2.0F;
      float wrapped = std::fmod(value - minOffset, period);
      if (wrapped < 0.0F) {
        wrapped += period;
      }
      if (wrapped > length) {
        wrapped = period - wrapped;
      }
      return minOffset + wrapped;
    }
    default:
      return value;
  }
}

[[nodiscard]] std::array<uint8_t, 4>
sampleColorLine(float value, const std::vector<GradientStop> &stops,
                FT_PaintExtend extend) {
  if (stops.empty()) {
    return {0U, 0U, 0U, 0U};
  }
  const float minOffset = stops.front().offset;
  const float maxOffset = stops.back().offset;
  if (maxOffset - minOffset <= 1e-6F) {
    return stops.front().color;
  }
  value = applyExtend(value, minOffset, maxOffset, extend);
  if (value <= stops.front().offset) {
    return stops.front().color;
  }
  if (value >= stops.back().offset) {
    return stops.back().color;
  }
  auto upper = std::upper_bound(
      stops.begin(), stops.end(), value,
      [](float lhs, const GradientStop &rhs) { return lhs < rhs.offset; });
  const GradientStop &stopB = *upper;
  const GradientStop &stopA = *(upper - 1);
  const float span = stopB.offset - stopA.offset;
  if (span <= 1e-6F) {
    return stopB.color;
  }
  const float factor = (value - stopA.offset) / span;
  std::array<uint8_t, 4> result{};
  for (int i = 0; i < 4; ++i) {
    const float c0 = static_cast<float>(stopA.color[i]);
    const float c1 = static_cast<float>(stopB.color[i]);
    result[i] =
        static_cast<uint8_t>(std::clamp(c0 + (c1 - c0) * factor, 0.0F, 255.0F));
  }
  return result;
}

[[nodiscard]] float solveRadialParameter(Vec2 point, Vec2 c0, Vec2 c1, float r0,
                                         float r1) {
  const Vec2 g = subtract(c1, c0);
  const Vec2 q = subtract(point, c0);
  const float dr = r1 - r0;
  const float A = dot(g, g) - dr * dr;
  const float B = -2.0F * (dot(q, g) + r0 * dr);
  const float C = dot(q, q) - r0 * r0;
  float t = 0.0F;
  if (std::fabs(A) <= 1e-6F) {
    if (std::fabs(B) <= 1e-6F) {
      return 0.0F;
    }
    t = -C / B;
  } else {
    float discriminant = B * B - 4.0F * A * C;
    if (discriminant < 0.0F) {
      discriminant = 0.0F;
    }
    const float sqrtDisc = std::sqrt(discriminant);
    const float t0 = (-B + sqrtDisc) / (2.0F * A);
    const float t1 = (-B - sqrtDisc) / (2.0F * A);
    t = std::max(t0, t1);
    if (t < 0.0F) {
      t = std::min(t0, t1);
    }
  }
  return t;
}

[[nodiscard]] float normalizeAngle(float angle) {
  const float twoPi = 2.0F * kPi;
  float normalized = std::fmod(angle, twoPi);
  if (normalized < 0.0F) {
    normalized += twoPi;
  }
  return normalized;
}

[[nodiscard]] Bounds computeBounds(FT_Face face, std::uint32_t glyphId) {
  Bounds bounds{};
  FT_ClipBox clipBox{};
  if (FT_Get_Color_Glyph_ClipBox(face, glyphId, &clipBox)) {
    const FT_Pos xs[] = {clipBox.bottom_left.x, clipBox.top_left.x,
                         clipBox.top_right.x, clipBox.bottom_right.x};
    const FT_Pos ys[] = {clipBox.bottom_left.y, clipBox.top_left.y,
                         clipBox.top_right.y, clipBox.bottom_right.y};
    int minX = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int minY = std::numeric_limits<int>::max();
    int maxY = std::numeric_limits<int>::min();
    for (FT_Pos x : xs) {
      minX = std::min(minX, floorFrom26Dot6(x));
      maxX = std::max(maxX, ceilFrom26Dot6(x));
    }
    for (FT_Pos y : ys) {
      minY = std::min(minY, floorFrom26Dot6(y));
      maxY = std::max(maxY, ceilFrom26Dot6(y));
    }
    bounds.minX = minX;
    bounds.maxX = maxX;
    bounds.minY = minY;
    bounds.maxY = maxY;
    return bounds;
  }

  if (FT_Load_Glyph(face, glyphId, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP) != 0) {
    throw std::runtime_error("COLRv1境界計算用のグリフ読み込みに失敗しました");
  }
  const FT_GlyphSlot slot = face->glyph;
  const int bearingX = roundFrom26Dot6(slot->metrics.horiBearingX);
  const int bearingY = roundFrom26Dot6(slot->metrics.horiBearingY);
  const int width = roundFrom26Dot6(slot->metrics.width);
  const int height = roundFrom26Dot6(slot->metrics.height);

  bounds.minX = bearingX;
  bounds.maxX = bearingX + width;
  bounds.maxY = bearingY;
  bounds.minY = bearingY - height;
  return bounds;
}

[[nodiscard]] const char *paintFormatName(FT_PaintFormat format) {
  switch (format) {
    case FT_COLR_PAINTFORMAT_COLR_LAYERS:
      return "ColrLayers";
    case FT_COLR_PAINTFORMAT_SOLID:
      return "Solid";
    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
      return "LinearGradient";
    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
      return "RadialGradient";
    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT:
      return "SweepGradient";
    case FT_COLR_PAINTFORMAT_GLYPH:
      return "Glyph";
    case FT_COLR_PAINTFORMAT_COLR_GLYPH:
      return "ColrGlyph";
    case FT_COLR_PAINTFORMAT_TRANSFORM:
      return "Transform";
    case FT_COLR_PAINTFORMAT_TRANSLATE:
      return "Translate";
    case FT_COLR_PAINTFORMAT_SCALE:
      return "Scale";
    case FT_COLR_PAINTFORMAT_ROTATE:
      return "Rotate";
    case FT_COLR_PAINTFORMAT_SKEW:
      return "Skew";
    case FT_COLR_PAINTFORMAT_COMPOSITE:
      return "Composite";
    default:
      return "Unknown";
  }
}

[[nodiscard]] const char *compositeModeName(FT_Composite_Mode mode) {
  switch (mode) {
    case FT_COLR_COMPOSITE_CLEAR:
      return "CLEAR";
    case FT_COLR_COMPOSITE_SRC:
      return "SRC";
    case FT_COLR_COMPOSITE_DEST:
      return "DEST";
    case FT_COLR_COMPOSITE_SRC_OVER:
      return "SRC_OVER";
    case FT_COLR_COMPOSITE_DEST_OVER:
      return "DEST_OVER";
    case FT_COLR_COMPOSITE_SRC_IN:
      return "SRC_IN";
    case FT_COLR_COMPOSITE_DEST_IN:
      return "DEST_IN";
    case FT_COLR_COMPOSITE_SRC_OUT:
      return "SRC_OUT";
    case FT_COLR_COMPOSITE_DEST_OUT:
      return "DEST_OUT";
    case FT_COLR_COMPOSITE_SRC_ATOP:
      return "SRC_ATOP";
    case FT_COLR_COMPOSITE_DEST_ATOP:
      return "DEST_ATOP";
    case FT_COLR_COMPOSITE_XOR:
      return "XOR";
    case FT_COLR_COMPOSITE_PLUS:
      return "PLUS";
    case FT_COLR_COMPOSITE_SCREEN:
      return "SCREEN";
    case FT_COLR_COMPOSITE_OVERLAY:
      return "OVERLAY";
    case FT_COLR_COMPOSITE_DARKEN:
      return "DARKEN";
    case FT_COLR_COMPOSITE_LIGHTEN:
      return "LIGHTEN";
    case FT_COLR_COMPOSITE_COLOR_DODGE:
      return "COLOR_DODGE";
    case FT_COLR_COMPOSITE_COLOR_BURN:
      return "COLOR_BURN";
    case FT_COLR_COMPOSITE_HARD_LIGHT:
      return "HARD_LIGHT";
    case FT_COLR_COMPOSITE_SOFT_LIGHT:
      return "SOFT_LIGHT";
    case FT_COLR_COMPOSITE_DIFFERENCE:
      return "DIFFERENCE";
    case FT_COLR_COMPOSITE_EXCLUSION:
      return "EXCLUSION";
    case FT_COLR_COMPOSITE_MULTIPLY:
      return "MULTIPLY";
    case FT_COLR_COMPOSITE_HSL_HUE:
      return "HSL_HUE";
    case FT_COLR_COMPOSITE_HSL_SATURATION:
      return "HSL_SATURATION";
    case FT_COLR_COMPOSITE_HSL_COLOR:
      return "HSL_COLOR";
    case FT_COLR_COMPOSITE_HSL_LUMINOSITY:
      return "HSL_LUMINOSITY";
    default:
      return "UNKNOWN";
  }
}

class FaceTransformGuard {
public:
  FaceTransformGuard(FT_Face face, const FT_Matrix &matrix,
                     const FT_Vector &vector)
      : m_face(face), m_matrix(matrix), m_vector(vector) {
    FT_Set_Transform(face, &m_matrix, &m_vector);
  }
  ~FaceTransformGuard() { FT_Set_Transform(m_face, nullptr, nullptr); }

private:
  FT_Face m_face;
  FT_Matrix m_matrix;
  FT_Vector m_vector;
};

class ColrV1Renderer {
public:
  ColrV1Renderer(FT_Face face, const ColorPalette &palette,
                 const Bounds &bounds, int advance, int bearingX, int bearingY,
                 const ColrV1RenderOptions &options, ColrV1RenderStats *stats)
      : m_face(face), m_palette(palette), m_bounds(bounds), m_advance(advance),
        m_bearingX(bearingX), m_bearingY(bearingY), m_options(options),
        m_stats(stats) {
    m_width = std::max(bounds.maxX - bounds.minX, 1);
    m_height = std::max(bounds.maxY - bounds.minY, 1);
    m_originY = bounds.maxY;
    m_target.resize(m_width, m_height);
    m_target.clear(0U, 0U, 0U, 0U);
    m_clipMask = makeFullMask(m_width, m_height, 255U);
    m_transformStack.push_back(identityTransform());
    m_currentTargetPtr = &m_target;
  }

  GlyphSurface render(const FT_OpaquePaint &opaque) {
    log("render begin");
    paint(opaque);
    log("render end");
    GlyphSurface surface;
    surface.m_image = std::move(m_target);
    surface.m_bearingX = m_bearingX;
    surface.m_bearingY = m_bearingY;
    surface.m_advance = m_advance;
    return surface;
  }

private:
  void paint(const FT_OpaquePaint &opaque) {
    FT_COLR_Paint paint{};
    if (!FT_Get_Paint(m_face, opaque, &paint)) {
      throw std::runtime_error("COLRv1ペイントの取得に失敗しました");
    }
    bump(m_stats ? &m_stats->m_totalPaints : nullptr);
    TraceScope scope(*this, paint.format);
    switch (paint.format) {
      case FT_COLR_PAINTFORMAT_COLR_LAYERS:
        paintLayers(paint.u.colr_layers);
        break;
      case FT_COLR_PAINTFORMAT_SOLID:
        paintSolid(paint.u.solid);
        break;
      case FT_COLR_PAINTFORMAT_GLYPH:
        paintGlyph(paint.u.glyph);
        break;
      case FT_COLR_PAINTFORMAT_COLR_GLYPH:
        paintColrGlyph(paint.u.colr_glyph);
        break;
      case FT_COLR_PAINTFORMAT_TRANSFORM:
        paintTransform(paint.u.transform);
        break;
      case FT_COLR_PAINTFORMAT_TRANSLATE:
        paintTranslate(paint.u.translate);
        break;
      case FT_COLR_PAINTFORMAT_SCALE:
        paintScale(paint.u.scale);
        break;
      case FT_COLR_PAINTFORMAT_ROTATE:
        paintRotate(paint.u.rotate);
        break;
      case FT_COLR_PAINTFORMAT_SKEW:
        paintSkew(paint.u.skew);
        break;
      case FT_COLR_PAINTFORMAT_COMPOSITE:
        paintComposite(paint.u.composite);
        break;
      case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
        paintLinearGradient(paint.u.linear_gradient);
        break;
      case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
        paintRadialGradient(paint.u.radial_gradient);
        break;
      case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT:
        paintSweepGradient(paint.u.sweep_gradient);
        break;
      default:
        throw std::runtime_error("未対応のCOLRv1ペイントフォーマットです");
    }
  }

  void paintLayers(const FT_PaintColrLayers &layers) {
    bump(m_stats ? &m_stats->m_layerPaints : nullptr);
    FT_LayerIterator iterator = layers.layer_iterator;
    FT_OpaquePaint layerPaint{};
    log("paint layers");
    while (FT_Get_Paint_Layers(m_face, &iterator, &layerPaint)) {
      paint(layerPaint);
    }
  }

  void paintSolid(const FT_PaintSolid &solid) {
    const auto color = resolveColor(m_palette, solid.color);
    blendSolid(color);
  }

  void paintGlyph(const FT_PaintGlyph &glyph) {
    bump(m_stats ? &m_stats->m_glyphPaints : nullptr);
    logValue("paint glyph", static_cast<unsigned int>(glyph.glyphID));
    pushGlyphClip(glyph.glyphID);
    paint(glyph.paint);
    popGlyphClip();
  }

  void paintColrGlyph(const FT_PaintColrGlyph &glyph) {
    bump(m_stats ? &m_stats->m_colrGlyphPaints : nullptr);
    if (m_recursionDepth >= kMaxRecursionDepth) {
      throw std::runtime_error("COLRv1グリフの再帰が深すぎます");
    }
    logValue("paint colr glyph", static_cast<unsigned int>(glyph.glyphID));
    FT_OpaquePaint child{};
    if (!FT_Get_Color_Glyph_Paint(m_face, glyph.glyphID,
                                  FT_COLOR_NO_ROOT_TRANSFORM, &child)) {
      throw std::runtime_error("COLRv1子グリフの取得に失敗しました");
    }
    ++m_recursionDepth;
    paint(child);
    --m_recursionDepth;
  }

  void paintTransform(const FT_PaintTransform &transform) {
    const AffineTransform matrix = {
        fixed16Dot16ToFloat(transform.affine.xx),
        fixed16Dot16ToFloat(transform.affine.xy),
        fixed16Dot16ToFloat(transform.affine.yx),
        fixed16Dot16ToFloat(transform.affine.yy),
        fixed16Dot16ToFloat(transform.affine.dx),
        fixed16Dot16ToFloat(transform.affine.dy),
    };
    pushTransform(matrix);
    paint(transform.paint);
    popTransform();
  }

  void paintTranslate(const FT_PaintTranslate &translate) {
    const AffineTransform transform = translateTransform(
        fixed16Dot16ToFloat(translate.dx), fixed16Dot16ToFloat(translate.dy));
    pushTransform(transform);
    paint(translate.paint);
    popTransform();
  }

  void paintScale(const FT_PaintScale &scale) {
    const AffineTransform transform = scaleTransform(
        fixed16Dot16ToFloat(scale.scale_x), fixed16Dot16ToFloat(scale.scale_y),
        fixed16Dot16ToFloat(scale.center_x),
        fixed16Dot16ToFloat(scale.center_y));
    pushTransform(transform);
    paint(scale.paint);
    popTransform();
  }

  void paintRotate(const FT_PaintRotate &rotate) {
    const float angleDegrees = fixed16Dot16ToFloat(rotate.angle) * 180.0F;
    const float angleRadians = angleDegrees * (kPi / 180.0F);
    const AffineTransform transform =
        rotateTransform(angleRadians, fixed16Dot16ToFloat(rotate.center_x),
                        fixed16Dot16ToFloat(rotate.center_y));
    pushTransform(transform);
    paint(rotate.paint);
    popTransform();
  }

  void paintSkew(const FT_PaintSkew &skew) {
    const float xAngleDegrees = fixed16Dot16ToFloat(skew.x_skew_angle) * 180.0F;
    const float yAngleDegrees = fixed16Dot16ToFloat(skew.y_skew_angle) * 180.0F;
    const AffineTransform transform = skewTransform(
        xAngleDegrees * (kPi / 180.0F), yAngleDegrees * (kPi / 180.0F),
        fixed16Dot16ToFloat(skew.center_x), fixed16Dot16ToFloat(skew.center_y));
    pushTransform(transform);
    paint(skew.paint);
    popTransform();
  }

  void paintComposite(const FT_PaintComposite &composite) {
    bump(m_stats ? &m_stats->m_composites : nullptr);
    log(std::string("paint composite ") +
        compositeModeName(composite.composite_mode));
    Bitmap backdrop = evaluateToBitmap(composite.backdrop_paint);
    Bitmap source = evaluateToBitmap(composite.source_paint);
    Bitmap combined =
        composeBitmaps(backdrop, source, composite.composite_mode);
    blendBitmap(combined, /*applyClip=*/false);
  }

  void paintLinearGradient(const FT_PaintLinearGradient &gradient) {
    bump(m_stats ? &m_stats->m_gradients : nullptr);
    const auto stops =
        collectGradientStops(m_face, gradient.colorline, m_palette);
    if (stops.empty()) {
      return;
    }
    const AffineTransform inverse = invert(m_transformStack.back());
    const Vec2 p0 = makeVec(gradient.p0);
    const Vec2 p1 = makeVec(gradient.p1);
    const Vec2 p2 = makeVec(gradient.p2);
    const Vec2 axis = subtract(p1, p0);
    const float axisLenSq = lengthSquared(axis);
    if (axisLenSq <= 1e-6F) {
      blendSolid(stops.front().color);
      return;
    }
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        const std::size_t idx = static_cast<std::size_t>(y * m_width + x);
        const float coverage =
            static_cast<float>(m_clipMask.data[idx]) / 255.0F;
        if (coverage <= 0.0F) {
          continue;
        }
        const Vec2 devicePoint = pixelToDevice(x, y, m_bounds);
        const Vec2 localPoint = apply(inverse, devicePoint);
        const float position = dot(subtract(localPoint, p2), axis) / axisLenSq;
        const auto color =
            sampleColorLine(position, stops, gradient.colorline.extend);
        accumulateColor(idx, coverage, color);
      }
    }
  }

  void paintRadialGradient(const FT_PaintRadialGradient &gradient) {
    bump(m_stats ? &m_stats->m_gradients : nullptr);
    const auto stops =
        collectGradientStops(m_face, gradient.colorline, m_palette);
    if (stops.empty()) {
      return;
    }
    const AffineTransform inverse = invert(m_transformStack.back());
    const Vec2 c0 = makeVec(gradient.c0);
    const Vec2 c1 = makeVec(gradient.c1);
    const float r0 = static_cast<float>(gradient.r0) / 64.0F;
    const float r1 = static_cast<float>(gradient.r1) / 64.0F;
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        const std::size_t idx = static_cast<std::size_t>(y * m_width + x);
        const float coverage =
            static_cast<float>(m_clipMask.data[idx]) / 255.0F;
        if (coverage <= 0.0F) {
          continue;
        }
        const Vec2 devicePoint = pixelToDevice(x, y, m_bounds);
        const Vec2 localPoint = apply(inverse, devicePoint);
        const float position = solveRadialParameter(localPoint, c0, c1, r0, r1);
        const auto color =
            sampleColorLine(position, stops, gradient.colorline.extend);
        accumulateColor(idx, coverage, color);
      }
    }
  }

  void paintSweepGradient(const FT_PaintSweepGradient &gradient) {
    bump(m_stats ? &m_stats->m_gradients : nullptr);
    const auto stops =
        collectGradientStops(m_face, gradient.colorline, m_palette);
    if (stops.empty()) {
      return;
    }
    const AffineTransform inverse = invert(m_transformStack.back());
    const Vec2 center = makeVec(gradient.center);
    float startAngle = fixed16Dot16ToFloat(gradient.start_angle) * kPi;
    float endAngle = fixed16Dot16ToFloat(gradient.end_angle) * kPi;
    startAngle = normalizeAngle(startAngle);
    endAngle = normalizeAngle(endAngle);
    float span = endAngle - startAngle;
    if (span <= 0.0F) {
      span += 2.0F * kPi;
    }
    const float minOffset = stops.front().offset;
    const float maxOffset = stops.back().offset;
    const float length = maxOffset - minOffset;
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        const std::size_t idx = static_cast<std::size_t>(y * m_width + x);
        const float coverage =
            static_cast<float>(m_clipMask.data[idx]) / 255.0F;
        if (coverage <= 0.0F) {
          continue;
        }
        const Vec2 devicePoint = pixelToDevice(x, y, m_bounds);
        const Vec2 localPoint = apply(inverse, devicePoint);
        const float dx = localPoint.x - center.x;
        const float dy = localPoint.y - center.y;
        if (std::fabs(dx) <= 1e-6F && std::fabs(dy) <= 1e-6F) {
          accumulateColor(idx, coverage, stops.front().color);
          continue;
        }
        const float angle = normalizeAngle(std::atan2(dx, dy));
        float relative = angle - startAngle;
        if (relative < 0.0F) {
          relative += 2.0F * kPi;
        }
        float position;
        if (length > 1e-6F) {
          const float normalized = relative / span;
          position = minOffset + normalized * length;
        } else {
          position = minOffset;
        }
        const auto color =
            sampleColorLine(position, stops, gradient.colorline.extend);
        accumulateColor(idx, coverage, color);
      }
    }
  }

  Bitmap evaluateToBitmap(const FT_OpaquePaint &opaque) {
    pushGroup();
    paint(opaque);
    return popGroup();
  }

  Bitmap composeBitmaps(const Bitmap &backdrop, const Bitmap &source,
                        FT_Composite_Mode mode) {
    Bitmap result;
    result.resize(m_width, m_height);
    result.clear(0U, 0U, 0U, 0U);
    const auto &src = source.rgba();
    const auto &back = backdrop.rgba();
    auto &dst = result.rgba();
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        const std::size_t idx = static_cast<std::size_t>(y * m_width + x) * 4U;
        const PremulRgba srcColor = toPremul(src.data() + idx);
        const PremulRgba dstColor = toPremul(back.data() + idx);
        storePremul(dst.data() + idx, composePremul(srcColor, dstColor, mode));
      }
    }
    return result;
  }

  void blendSolid(const std::array<uint8_t, 4> &color) {
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        const std::size_t idx = static_cast<std::size_t>(y * m_width + x);
        const float coverage =
            static_cast<float>(m_clipMask.data[idx]) / 255.0F;
        if (coverage <= 0.0F) {
          continue;
        }
        accumulateColor(idx, coverage, color);
      }
    }
  }

  void blendBitmap(const Bitmap &source, bool applyClip) {

    const auto &src = source.rgba();
    for (int y = 0; y < m_height; ++y) {
      for (int x = 0; x < m_width; ++x) {
        const std::size_t idx = static_cast<std::size_t>(y * m_width + x);
        float coverage = applyClip
                             ? static_cast<float>(m_clipMask.data[idx]) / 255.0F
                             : 1.0F;
        if (coverage <= 0.0F) {
          continue;
        }
        const std::size_t pixelIndex = idx * 4U;
        std::array<uint8_t, 4> color{src[pixelIndex + 0], src[pixelIndex + 1],
                                     src[pixelIndex + 2], src[pixelIndex + 3]};
        accumulateColor(idx, coverage, color);
      }
    }
  }

  void pushTransform(const AffineTransform &transform) {
    bump(m_stats ? &m_stats->m_transformPushes : nullptr);
    const AffineTransform combined =
        compose(m_transformStack.back(), transform);
    m_transformStack.push_back(combined);
    log("push transform");
  }

  void popTransform() {
    if (m_transformStack.size() <= 1) {
      throw std::runtime_error("変換スタックの整合性が崩れています");
    }
    m_transformStack.pop_back();
    log("pop transform");
  }

  void pushClipMask(const AlphaMask &mask) {
    bump(m_stats ? &m_stats->m_clipPushes : nullptr);
    m_clipStack.push_back(m_clipMask);
    AlphaMask combined;
    combined.width = mask.width;
    combined.height = mask.height;
    combined.data.resize(mask.data.size());
    for (std::size_t i = 0; i < mask.data.size(); ++i) {
      combined.data[i] =
          static_cast<uint8_t>((static_cast<int>(m_clipMask.data[i]) *
                                    static_cast<int>(mask.data[i]) +
                                127) /
                               255);
    }
    m_clipMask = std::move(combined);
    log("push clip mask");
  }

  void popClipMask() {
    if (m_clipStack.empty()) {
      throw std::runtime_error("クリップスタックの整合性が崩れています");
    }
    m_clipMask = std::move(m_clipStack.back());
    m_clipStack.pop_back();
    log("pop clip mask");
  }

  void pushGlyphClip(FT_UInt glyphId) {
    AlphaMask mask = rasterizeGlyphMask(glyphId);
    pushClipMask(mask);
  }

  void popGlyphClip() { popClipMask(); }

  AlphaMask rasterizeGlyphMask(FT_UInt glyphId) {
    AlphaMask mask = makeFullMask(m_width, m_height, 0U);
    const AffineTransform &transform = m_transformStack.back();
    FT_Matrix matrix = toFTMatrix(transform);
    FT_Vector delta = toFTVector(transform);
    FaceTransformGuard guard(m_face, matrix, delta);

    if (FT_Load_Glyph(m_face, glyphId, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP) !=
        0) {
      throw std::runtime_error(
          "COLRv1クリップ用のグリフ読み込みに失敗しました");
    }
    if (FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
      throw std::runtime_error("COLRv1クリップ用のラスタライズに失敗しました");
    }

    const FT_GlyphSlot slot = m_face->glyph;
    const FT_Bitmap &bitmap = slot->bitmap;
    const int destX = slot->bitmap_left - m_bounds.minX;
    const int destY = m_originY - slot->bitmap_top;
    for (int y = 0; y < static_cast<int>(bitmap.rows); ++y) {
      const int targetY = destY + y;
      if (targetY < 0 || targetY >= m_height) {
        continue;
      }
      const uint8_t *srcRow =
          bitmap.buffer + static_cast<std::size_t>(y) * bitmap.pitch;
      for (int x = 0; x < static_cast<int>(bitmap.width); ++x) {
        const int targetX = destX + x;
        if (targetX < 0 || targetX >= m_width) {
          continue;
        }
        const std::size_t index =
            static_cast<std::size_t>(targetY * m_width + targetX);
        mask.data[index] = std::max(mask.data[index], srcRow[x]);
      }
    }
    return mask;
  }

  void pushGroup() {
    bump(m_stats ? &m_stats->m_groupPushes : nullptr);
    m_targetStack.push_back(m_currentTargetPtr);
    m_groupStack.emplace_back();
    Bitmap &bitmap = m_groupStack.back();
    bitmap.resize(m_width, m_height);
    bitmap.clear(0U, 0U, 0U, 0U);
    m_currentTargetPtr = &bitmap;
    log("push group");
  }

  Bitmap popGroup() {
    if (m_groupStack.empty() || m_targetStack.empty()) {
      throw std::runtime_error("グループスタックの整合性が崩れています");
    }
    Bitmap result = std::move(m_groupStack.back());
    m_groupStack.pop_back();
    m_currentTargetPtr = m_targetStack.back();
    m_targetStack.pop_back();
    log("pop group");
    return result;
  }

  Bitmap &currentTarget() { return *m_currentTargetPtr; }
  const Bitmap &currentTarget() const { return *m_currentTargetPtr; }

private:
  class TraceScope {
  public:
    TraceScope(ColrV1Renderer &renderer, FT_PaintFormat format)
        : m_renderer(renderer) {
      m_renderer.traceEnter(format);
    }

    ~TraceScope() { m_renderer.traceLeave(); }

  private:
    ColrV1Renderer &m_renderer;
  };

  void traceEnter(FT_PaintFormat format) {
    std::ostringstream stream;
    stream << "enter " << paintFormatName(format)
           << " format=" << static_cast<int>(format);
    log(stream.str());
    ++m_traceDepth;
  }

  void traceLeave() {
    if (m_traceDepth > 0) {
      --m_traceDepth;
    }
    log("leave");
  }

  void logValue(const std::string &label, unsigned int value) {
    std::ostringstream stream;
    stream << label << "=" << value;
    log(stream.str());
  }

  void log(const std::string &message) {
    if (!m_options.m_enableTrace || m_options.m_traceStream == nullptr) {
      return;
    }
    (*m_options.m_traceStream)
        << "[COLRv1] "
        << std::string(static_cast<std::size_t>(m_traceDepth) * 2U, ' ')
        << message << " clipDepth=" << m_clipStack.size() << " transformDepth="
        << (m_transformStack.empty() ? 0U : m_transformStack.size() - 1U)
        << " groupDepth=" << m_groupStack.size() << '\n';
  }

  static void bump(std::size_t *counter) {
    if (counter != nullptr) {
      ++(*counter);
    }
  }

  void accumulateColor(std::size_t pixelIndex, float coverage,
                       const std::array<uint8_t, 4> &color) {
    if (coverage <= 0.0F) {
      return;
    }
    auto &dst = currentTarget().rgba();
    const std::size_t base = pixelIndex * 4U;
    const float srcAlpha = static_cast<float>(color[3]) / 255.0F * coverage;
    if (srcAlpha <= 0.0F) {
      return;
    }
    const float dstAlpha = static_cast<float>(dst[base + 3]) / 255.0F;
    const float outAlpha = srcAlpha + dstAlpha * (1.0F - srcAlpha);
    if (outAlpha <= 0.0F) {
      dst[base + 0] = dst[base + 1] = dst[base + 2] = dst[base + 3] = 0U;
      return;
    }
    for (int c = 0; c < 3; ++c) {
      const float srcColor = static_cast<float>(color[c]) / 255.0F;
      const float dstColor = static_cast<float>(dst[base + c]) / 255.0F;
      const float outColor =
          (srcColor * srcAlpha + dstColor * dstAlpha * (1.0F - srcAlpha)) /
          outAlpha;
      dst[base + c] = static_cast<uint8_t>(
          std::clamp(outColor, 0.0F, 1.0F) * 255.0F + 0.5F);
    }
    dst[base + 3] =
        static_cast<uint8_t>(std::clamp(outAlpha, 0.0F, 1.0F) * 255.0F + 0.5F);
  }

  FT_Face m_face = nullptr;
  const ColorPalette &m_palette;
  Bounds m_bounds{};
  int m_advance = 0;
  int m_bearingX = 0;
  int m_bearingY = 0;

  int m_width = 0;
  int m_height = 0;
  int m_originY = 0;
  int m_recursionDepth = 0;
  int m_traceDepth = 0;

  Bitmap m_target;
  AlphaMask m_clipMask;
  std::vector<AlphaMask> m_clipStack;
  std::vector<AffineTransform> m_transformStack;
  std::vector<Bitmap> m_groupStack;
  std::vector<Bitmap *> m_targetStack;
  Bitmap *m_currentTargetPtr = nullptr;
  ColrV1RenderOptions m_options;
  ColrV1RenderStats *m_stats = nullptr;
};

} // namespace

GlyphSurface renderColrV1Glyph(FT_Face face, std::uint32_t glyphId,
                               const ColorPalette &palette,
                               const ColrV1RenderOptions &options,
                               ColrV1RenderStats *stats) {
  if (!face) {
    throw std::invalid_argument("FT_Face is null");
  }
  FT_OpaquePaint opaque{};
  if (!FT_Get_Color_Glyph_Paint(face, glyphId, FT_COLOR_NO_ROOT_TRANSFORM,
                                &opaque)) {
    throw std::runtime_error("COLRv1ペイントが見つかりません");
  }

  if (FT_Load_Glyph(face, glyphId, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP) != 0) {
    throw std::runtime_error("COLRv1グリフメトリクスの取得に失敗しました");
  }

  const FT_GlyphSlot slot = face->glyph;
  const int advance = roundFrom26Dot6(slot->advance.x);

  const Bounds bounds = computeBounds(face, glyphId);
  const int bearingX = bounds.minX;
  const int bearingY = bounds.maxY;

  if (stats != nullptr) {
    *stats = {};
  }
  ColrV1Renderer renderer(face, palette, bounds, advance, bearingX, bearingY,
                          options, stats);
  return renderer.render(opaque);
}
