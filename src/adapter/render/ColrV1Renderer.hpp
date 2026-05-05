#pragma once

#include "core/ir/Bitmap.hpp"
#include "core/ir/ColorPalette.hpp"
#include "core/ir/GlyphSurface.hpp"

#include <cstdint>
#include <iosfwd>

#include <ft2build.h>
#include FT_FREETYPE_H

struct ColrV1RenderOptions {
  bool m_enableTrace = false;
  std::ostream *m_traceStream = nullptr;
};

struct ColrV1RenderStats {
  std::size_t m_totalPaints = 0;
  std::size_t m_layerPaints = 0;
  std::size_t m_glyphPaints = 0;
  std::size_t m_colrGlyphPaints = 0;
  std::size_t m_composites = 0;
  std::size_t m_gradients = 0;
  std::size_t m_clipPushes = 0;
  std::size_t m_transformPushes = 0;
  std::size_t m_groupPushes = 0;
};

GlyphSurface renderColrV1Glyph(FT_Face face, std::uint32_t glyphId,
                               const ColorPalette &palette,
                               const ColrV1RenderOptions &options = {},
                               ColrV1RenderStats *stats = nullptr);
