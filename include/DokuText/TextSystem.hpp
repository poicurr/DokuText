// This file is part of the public API.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <DokuText/Atlas.hpp>
#include <DokuText/Layout.hpp>
#include <DokuText/RendererContract.hpp>

namespace doku::text {

enum class FontRole : std::uint8_t {
  Text,
  Emoji,
  Symbol,
};

[[nodiscard]] constexpr std::string_view toString(FontRole role) {
  switch (role) {
    case FontRole::Text:
      return "Text";
    case FontRole::Emoji:
      return "Emoji";
    case FontRole::Symbol:
      return "Symbol";
  }
  return "Unknown";
}

struct TextPrepareOptions {
  bool verifyMetrics = false;

  /// If true, prepareLine/prepareText may append missing glyphs to the provided
  /// TextAtlas. If false, prepareLine/prepareText must not mutate the atlas;
  /// missing atlas glyphs remain missing and should be diagnosed through
  /// probeText or returned diagnostics if available.
  bool allowGlyphAppend = true;

  ParagraphLayoutOptions layoutOptions;
};

struct TextProbeResult {
  TextRequest request;

  /// Number of Unicode codepoints requested for probing.
  std::size_t requestedCodepointCount = 0;
  /// Number of glyph lookup events resolved through shaping/fallback.
  std::size_t resolvedGlyphCount = 0;
  /// Number of resolved glyph lookup events whose rasterization handle is
  /// already present in the atlas. This may include duplicates if the same
  /// glyph appears multiple times in the probed text.
  std::size_t alreadyPresentGlyphCount = 0;
  /// Number of resolved glyph lookup events whose rasterization handle is not
  /// present in the atlas. This count corresponds to the number of elements in
  /// the missingGlyphs vector.
  std::size_t missingGlyphCount = 0;

  /// True if every resolved glyph required for the text is already present in
  /// the atlas.
  bool allGlyphsAvailable = false;
  /// True if prepare/seed with append behavior would need to insert one or more
  /// glyphs into the atlas.
  bool wouldAppendGlyphs = false;

  /// The corresponding atlas-missing glyph lookup events.
  /// This list may contain duplicate handles if the same missing glyph appears
  /// multiple times in the probed text. These are missing from the atlas, not
  /// necessarily missing from the font fallback chain.
  std::vector<MissingAtlasGlyph> missingGlyphs;
  /// Non-fatal diagnostics from shaping/fallback/probing.
  std::vector<Diagnostic> diagnostics;
};

struct PreparedTextLine {
  TextLineLayout layout;
  TextDrawRun drawRun;
  std::vector<TextCaretBoundary> caretBoundaries;
  TextDrawSubmission submission;
  TextCompatibilityReport compatibility;
  GlyphAtlasCacheUpdate atlasUpdate;
  std::vector<AtlasPageUploadPlan> atlasUploadPlan;
  std::vector<std::string> missingGlyphs;
  std::vector<Diagnostic> diagnostics;
};

struct PreparedTextParagraph {
  MultiLineLayout layout;
  MultiLineDrawRun drawRun;
  TextDrawSubmission submission;
  TextCompatibilityReport compatibility;
  GlyphAtlasCacheUpdate atlasUpdate;
  std::vector<AtlasPageUploadPlan> atlasUploadPlan;
  std::vector<std::string> missingGlyphs;
  std::vector<Diagnostic> diagnostics;
};

class TextSystem {
public:
  TextSystem();
  ~TextSystem();

  TextSystem(TextSystem &&) noexcept;
  TextSystem &operator=(TextSystem &&) noexcept;

  TextSystem(const TextSystem &) = delete;
  TextSystem &operator=(const TextSystem &) = delete;

  FontChainId registerFontFallbackChain(const FontFallbackChain &chain);

  [[nodiscard]] FontFallbackChain
  resolveFontFallbackChain(FontChainId fontChainId) const;

  [[nodiscard]] TextAtlas createAtlas(const TextAtlasOptions &options) const;

  [[nodiscard]] TextAtlasSeedResult
  seedAtlas(TextAtlas &atlas, const TextSeedRequest &seedRequest) const;

  [[nodiscard]] TextAtlasAppendPlan
  buildTextAtlasAppendPlan(const TextAtlas &atlas, std::string_view text,
                           const TextPrepareOptions &options = {},
                           const TextAtlasAppendBudget &budget = {}) const;

  [[nodiscard]] TextAtlasAppendPlan
  buildTextAtlasAppendPlan(const TextAtlas &atlas, const TextRequest &request,
                           const TextAtlasAppendBudget &budget = {}) const;

  [[nodiscard]] TextProbeResult
  probeText(const TextAtlas &atlas, std::string_view text,
            const TextPrepareOptions &options = {}) const;

  [[nodiscard]] TextProbeResult probeText(const TextAtlas &atlas,
                                          const TextRequest &request) const;

  [[nodiscard]] BakedTextAtlas
  bakeTextAtlas(const TextAtlasBakeRequest &request) const;

  [[nodiscard]] PreparedTextLine
  prepareLine(TextAtlas &atlas, std::string_view text,
              const TextPrepareOptions &options = {}) const;

  [[nodiscard]] PreparedTextLine prepareLine(TextAtlas &atlas,
                                             const TextRequest &request) const;

  [[nodiscard]] PreparedTextLine
  prepareLine(TextAtlas &atlas, const TextRequest &request,
              const TextPrepareOptions &options) const;

  [[nodiscard]] PreparedTextParagraph
  prepareText(TextAtlas &atlas, std::string_view text,
              const TextPrepareOptions &options = {}) const;

  [[nodiscard]] PreparedTextParagraph
  prepareText(TextAtlas &atlas, const TextRequest &request) const;

  [[nodiscard]] PreparedTextParagraph
  prepareText(TextAtlas &atlas, const TextRequest &request,
              const TextPrepareOptions &options) const;

  [[nodiscard]] PreparedTextLine
  layoutTextLine(const BakedTextAtlas &atlas, const TextRequest &request) const;

  [[nodiscard]] PreparedTextParagraph
  layoutTextParagraph(const BakedTextAtlas &atlas,
                      const TextRequest &request) const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace doku::text
