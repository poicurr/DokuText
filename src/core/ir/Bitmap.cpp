#include "Bitmap.hpp"

#include <algorithm>
#include <stdexcept>

namespace {
[[nodiscard]] std::size_t safeRowBytes(int width) {
  if (width < 0) {
    throw std::invalid_argument("Bitmap::width must be non-negative");
  }
  return static_cast<std::size_t>(width) * 4U;
}

[[nodiscard]] std::size_t safeBufferSize(int width, int height) {
  if (height < 0) {
    throw std::invalid_argument("Bitmap::height must be non-negative");
  }
  return safeRowBytes(width) * static_cast<std::size_t>(height);
}
} // namespace

void Bitmap::resize(int width, int height) {
  const auto bufferSize = safeBufferSize(width, height);
  m_width = width;
  m_height = height;
  m_rgba.assign(bufferSize, 0);
}

void Bitmap::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (m_width <= 0 || m_height <= 0) {
    return;
  }
  for (int y = 0; y < m_height; ++y) {
    auto *row = m_rgba.data() + static_cast<std::size_t>(y) * BYTES_PER_PIXEL *
                                    static_cast<std::size_t>(m_width);
    for (int x = 0; x < m_width; ++x) {
      auto *pixel = row + static_cast<std::size_t>(x) * BYTES_PER_PIXEL;
      pixel[0] = r;
      pixel[1] = g;
      pixel[2] = b;
      pixel[3] = a;
    }
  }
}

void Bitmap::blit(const Bitmap &source, int offsetX, int offsetY) {
  if (m_width <= 0 || m_height <= 0) {
    throw std::runtime_error("Destination bitmap is empty");
  }
  if (source.m_width <= 0 || source.m_height <= 0) {
    return;
  }
  for (int sy = 0; sy < source.m_height; ++sy) {
    const int dy = sy + offsetY;
    if (dy < 0 || dy >= m_height) {
      continue;
    }
    auto *dstRow = m_rgba.data() + static_cast<std::size_t>(dy) *
                                       BYTES_PER_PIXEL *
                                       static_cast<std::size_t>(m_width);
    const auto *srcRow =
        source.m_rgba.data() + static_cast<std::size_t>(sy) * BYTES_PER_PIXEL *
                                   static_cast<std::size_t>(source.m_width);
    for (int sx = 0; sx < source.m_width; ++sx) {
      const int dx = sx + offsetX;
      if (dx < 0 || dx >= m_width) {
        continue;
      }
      auto *dstPixel = dstRow + static_cast<std::size_t>(dx) * BYTES_PER_PIXEL;
      const auto *srcPixel =
          srcRow + static_cast<std::size_t>(sx) * BYTES_PER_PIXEL;

      const float srcAlpha =
          static_cast<float>(srcPixel[3]) / static_cast<float>(UINT8_MAX);
      if (srcAlpha <= 0.0F) {
        continue;
      }
      const float invAlpha = 1.0F - srcAlpha;
      for (int c = 0; c < 3; ++c) {
        dstPixel[c] = static_cast<uint8_t>(std::clamp(
            srcPixel[c] * srcAlpha + dstPixel[c] * invAlpha, 0.0F, 255.0F));
      }
      dstPixel[3] = static_cast<uint8_t>(
          std::clamp(srcPixel[3] + dstPixel[3] * invAlpha, 0.0F, 255.0F));
    }
  }
}
