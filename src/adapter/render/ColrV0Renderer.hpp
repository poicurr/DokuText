#pragma once

#include "core/ir/Bitmap.hpp"
#include "core/ir/ColorPalette.hpp"
#include "core/ir/GlyphSurface.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb-ot.h>

GlyphSurface renderColrV0Glyph(FT_Face face, hb_face_t *hbFace,
                               hb_codepoint_t glyphId,
                               const ColorPalette &palette);
