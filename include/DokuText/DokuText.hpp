#pragma once

#include <GlyphRenderer/GlyphRenderer.hpp>

namespace doku::text {

using TextSystem = ::glyphrenderer::TextSystem;
using TextAtlas = ::glyphrenderer::TextAtlas;
using BakedTextAtlas = ::glyphrenderer::BakedTextAtlas;
using PreparedTextLine = ::glyphrenderer::PreparedTextLine;
using PreparedTextParagraph = ::glyphrenderer::PreparedTextParagraph;
using FontFallbackChain = ::glyphrenderer::FontFallbackChain;
using FontChainId = ::glyphrenderer::FontChainId;
using TextRequest = ::glyphrenderer::TextRequest;
using TextSeedRequest = ::glyphrenderer::TextSeedRequest;
using TextSeedPreset = ::glyphrenderer::TextSeedPreset;
using GlyphAtlasCacheUpdate = ::glyphrenderer::GlyphAtlasCacheUpdate;
using GlyphAtlasUpdate = ::glyphrenderer::GlyphAtlasCacheUpdate;
using AtlasPageUploadPlan = ::glyphrenderer::AtlasPageUploadPlan;
using TextCompatibilityReport = ::glyphrenderer::TextCompatibilityReport;
using TextAtlasOptions = ::glyphrenderer::TextAtlasOptions;
using TextAtlasSeedResult = ::glyphrenderer::TextAtlasSeedResult;
using TextPrepareOptions = ::glyphrenderer::TextPrepareOptions;
using AtlasBuildContext = ::glyphrenderer::AtlasBuildContext;

} // namespace doku::text
