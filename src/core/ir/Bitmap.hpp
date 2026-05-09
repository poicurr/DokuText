#pragma once

#include <cstdint>
#include <vector>

class Bitmap {
public:
  void resize(int width, int height);
  void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
  void blit(const Bitmap &source, int offsetX, int offsetY);

  [[nodiscard]] static constexpr int bytesPerPixel() { return BYTES_PER_PIXEL; }

  [[nodiscard]] int width() const { return m_width; }
  [[nodiscard]] int height() const { return m_height; }
  [[nodiscard]] const std::vector<uint8_t> &rgba() const { return m_rgba; }
  [[nodiscard]] std::vector<uint8_t> &rgba() { return m_rgba; }

private:
  static constexpr int BYTES_PER_PIXEL = 4;

  int m_width = 0;
  int m_height = 0;
  std::vector<uint8_t> m_rgba;
};
