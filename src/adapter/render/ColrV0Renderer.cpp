#include "ColrV0Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include FT_COLOR_H

namespace {
constexpr unsigned int kForegroundSentinel = 0xFFFFU;

[[nodiscard]] std::array<uint8_t, 4>
colorFromPalette(const ColorPalette &palette, unsigned int colorIndex) {
  if (colorIndex == kForegroundSentinel) {
    return palette.m_foregroundColor;
  }
  if (colorIndex < palette.m_colors.size()) {
    return palette.m_colors[colorIndex];
  }
  return palette.m_foregroundColor;
}

[[nodiscard]] Bitmap createLayerBitmap(const FT_Bitmap &sourceBitmap,
                                       const std::array<uint8_t, 4> &color) {
  Bitmap layer;
  const int width = static_cast<int>(sourceBitmap.width);
  const int height = static_cast<int>(sourceBitmap.rows);
  if (width <= 0 || height <= 0) {
    return layer;
  }

  layer.resize(width, height);
  auto &dst = layer.rgba();

  const int pitch = sourceBitmap.pitch;
  const uint8_t *buffer = sourceBitmap.buffer;
  for (int y = 0; y < height; ++y) {
    const uint8_t *srcRow = nullptr;
    if (pitch >= 0) {
      srcRow = buffer +
               static_cast<std::size_t>(y) * static_cast<std::size_t>(pitch);
    } else {
      const int invertedRow = height - 1 - y;
      srcRow = buffer + static_cast<std::size_t>(invertedRow) *
                            static_cast<std::size_t>(-pitch);
    }
    for (int x = 0; x < width; ++x) {
      const uint8_t coverage = srcRow[x];
      const std::size_t index =
          static_cast<std::size_t>(y * width + x) *
          static_cast<std::size_t>(Bitmap::bytesPerPixel());
      dst[index + 0] = color[0];
      dst[index + 1] = color[1];
      dst[index + 2] = color[2];
      const uint16_t alpha = static_cast<uint16_t>(coverage) *
                             static_cast<uint16_t>(color[3]) / 255U;
      dst[index + 3] = static_cast<uint8_t>(alpha > 255U ? 255U : alpha);
    }
  }

  return layer;
}

[[nodiscard]] int roundFrom26Dot6(FT_Pos value) {
  return static_cast<int>(std::lround(static_cast<double>(value) / 64.0));
}

} // namespace

GlyphSurface renderColrV0Glyph(FT_Face face, hb_face_t *hbFace,
                               hb_codepoint_t glyphId,
                               const ColorPalette &palette) {
  if (!face) {
    throw std::invalid_argument("FT_Face is null");
  }
  if (!hbFace) {
    throw std::invalid_argument("hb_face_t is null");
  }
  if (!hb_ot_color_has_layers(hbFace)) {
    throw std::runtime_error("COLRv0レイヤーが見つかりません");
  }

  unsigned int layerCount = 0;
  hb_ot_color_glyph_get_layers(hbFace, glyphId, 0, &layerCount, nullptr);

  std::vector<hb_ot_color_layer_t> layers;
  if (layerCount > 0) {
    layers.resize(layerCount);
    hb_ot_color_glyph_get_layers(hbFace, glyphId, 0, &layerCount,
                                 layers.data());
    layers.resize(layerCount);
  } else {
    if (!FT_HAS_COLOR(face)) {
      throw std::runtime_error("指定グリフにCOLRv0レイヤーがありません");
    }
    FT_LayerIterator iterator;
    iterator.p = nullptr;
    FT_UInt layerGlyph = 0;
    FT_UInt layerColor = 0;
    while (FT_Get_Color_Glyph_Layer(face, glyphId, &layerGlyph, &layerColor,
                                    &iterator)) {
      hb_ot_color_layer_t record;
      record.glyph = layerGlyph;
      record.color_index = layerColor;
      layers.push_back(record);
    }
    layerCount = static_cast<unsigned int>(layers.size());
  }

  if (layerCount == 0) {
    throw std::runtime_error("指定グリフにCOLRv0レイヤーがありません");
  }

  if (FT_Load_Glyph(face, glyphId, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP) != 0) {
    throw std::runtime_error("グリフメトリクスの取得に失敗しました");
  }

  const FT_GlyphSlot baseSlot = face->glyph;
  const int bearingX = roundFrom26Dot6(baseSlot->metrics.horiBearingX);
  const int bearingY = roundFrom26Dot6(baseSlot->metrics.horiBearingY);
  const int advance = roundFrom26Dot6(baseSlot->advance.x);

  struct LayerInstance {
    Bitmap m_bitmap;
    int m_left = 0;
    int m_top = 0;
  };
  std::vector<LayerInstance> renderLayers;
  renderLayers.reserve(layers.size());

  bool hasBounds = false;
  int minX = 0;
  int maxX = 0;
  int minY = 0;
  int maxY = 0;

  for (const auto &layer : layers) {
    if (FT_Load_Glyph(face, layer.glyph, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP) !=
        0) {
      continue;
    }
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
      continue;
    }
    const FT_GlyphSlot slot = face->glyph;
    const FT_Bitmap &bitmap = slot->bitmap;
    if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
      continue;
    }

    const auto color = colorFromPalette(palette, layer.color_index);
    Bitmap layerBitmap = createLayerBitmap(bitmap, color);
    if (layerBitmap.width() <= 0 || layerBitmap.height() <= 0) {
      continue;
    }

    LayerInstance instance;
    instance.m_left = slot->bitmap_left;
    instance.m_top = slot->bitmap_top;
    instance.m_bitmap = std::move(layerBitmap);

    const int left = instance.m_left;
    const int right = left + instance.m_bitmap.width();
    const int top = instance.m_top;
    const int bottom = top - instance.m_bitmap.height();

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

    renderLayers.push_back(std::move(instance));
  }

  if (renderLayers.empty()) {
    throw std::runtime_error("COLRv0レイヤーの描画に失敗しました");
  }

  Bitmap target;
  const int width = maxX - minX;
  const int height = maxY - minY;
  target.resize(width, height);
  target.clear(0U, 0U, 0U, 0U);

  for (const auto &layer : renderLayers) {
    const int offsetX = layer.m_left - minX;
    const int offsetY = maxY - layer.m_top;
    target.blit(layer.m_bitmap, offsetX, offsetY);
  }

  GlyphSurface surface;
  surface.m_image = std::move(target);
  surface.m_bearingX = bearingX;
  surface.m_bearingY = bearingY;
  surface.m_advance = advance;
  return surface;
}
