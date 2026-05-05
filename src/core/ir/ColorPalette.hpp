#pragma once

#include <array>
#include <vector>

struct ColorPalette {
  std::vector<std::array<uint8_t, 4>> m_colors;
  std::array<uint8_t, 4> m_foregroundColor{0U, 0U, 0U, 255U};
};
