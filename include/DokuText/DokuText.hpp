#pragma once

#include <GlyphRenderer/GlyphRenderer.hpp>

namespace doku::text {

using ::glyphrenderer::AtlasBuildContext;
using ::glyphrenderer::AtlasPageUploadPlan;
using ::glyphrenderer::BakedTextAtlas;
using ::glyphrenderer::BlendHint;
using ::glyphrenderer::CompatibilityConstraints;
using ::glyphrenderer::FontChainId;
using ::glyphrenderer::FontFallbackChain;
using ::glyphrenderer::GlyphAtlasCacheUpdate;
using ::glyphrenderer::MaterialKind;
using ::glyphrenderer::MultiLineDrawRun;
using ::glyphrenderer::PositionedDrawGlyph;
using ::glyphrenderer::PreparedTextLine;
using ::glyphrenderer::PreparedTextParagraph;
using ::glyphrenderer::SamplerKind;
using ::glyphrenderer::TextAtlas;
using ::glyphrenderer::TextAtlasOptions;
using ::glyphrenderer::TextAtlasSeedResult;
using ::glyphrenderer::TextCompatibilityReport;
using ::glyphrenderer::TextDrawRun;
using ::glyphrenderer::TextPrepareOptions;
using ::glyphrenderer::TextRequest;
using ::glyphrenderer::TextSeedPreset;
using ::glyphrenderer::TextSeedRequest;
using ::glyphrenderer::TextSystem;

} // namespace doku::text
