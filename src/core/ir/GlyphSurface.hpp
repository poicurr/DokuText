#pragma once

#include "core/ir/Bitmap.hpp"

struct GlyphSurface {
  Bitmap m_image;
  int m_bearingX = 0;
  int m_bearingY = 0;
  int m_advance = 0;
};
