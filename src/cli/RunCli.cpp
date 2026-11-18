#include "core/GlyphRender.hpp"
#if DOKU_TEXT_ENABLE_PNG_IO
#include "adapter/io/PngWriter.hpp"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace doku::text {
using namespace detail;
namespace {
struct SessionColrV1Stats {
  ColrV1RenderStats m_renderedStats;
  ColrV1RenderStats m_logicalStats;
  std::size_t m_renderedGlyphCount = 0;
  std::size_t m_logicalGlyphCount = 0;
  std::size_t m_renderedClusterCount = 0;
  std::size_t m_logicalClusterCount = 0;
};

struct ColrV1ScanEntry {
  hb_codepoint_t m_glyphId = 0;
  ColrV1RenderStats m_stats;
  int m_width = 0;
  int m_height = 0;
  int m_advance = 0;
};

struct ColrV1ScanFilters {
  unsigned int m_minPaints = 0;
  unsigned int m_minGradients = 0;
  unsigned int m_minComposites = 0;
  unsigned int m_minTransforms = 0;
  bool m_summaryOnly = false;
};

[[nodiscard]] unsigned int parseUnsigned(const char *value, const char *label) {
  if (value == nullptr) {
    throw std::invalid_argument(std::string(label) + " is not specified");
  }
  const unsigned long parsed = std::stoul(value);
  if (parsed > std::numeric_limits<unsigned int>::max()) {
    throw std::out_of_range(std::string(label) + " is too large");
  }
  return static_cast<unsigned int>(parsed);
}

[[nodiscard]] std::string makeOutputName(unsigned int index,
                                         const TextCluster &cluster) {
  std::ostringstream stream;
  stream << "glyph_" << index;
  for (std::uint32_t codePoint : cluster.m_codePoints) {
    stream << "_u" << std::uppercase << std::hex << codePoint;
  }
  stream << ".png";
  return stream.str();
}

[[nodiscard]] std::string makeGlyphIdOutputName(hb_codepoint_t glyphId) {
  std::ostringstream stream;
  stream << "glyph_id_" << glyphId << ".png";
  return stream.str();
}

[[nodiscard]] bool verifyClusterMetrics(const ClusterRenderResult &result,
                                        const TextCluster &cluster) {
  int expectedAdvance = 0;
  int expectedYAdvance = 0;
  for (const ShapedGlyph &glyph : result.m_shapedGlyphs) {
    expectedAdvance += glyph.m_xAdvance;
    expectedYAdvance += glyph.m_yAdvance;
  }
  return detail::verifyClusterMetrics(result.m_surface.m_advance, expectedAdvance,
                              expectedYAdvance, cluster.m_codePoints);
}

void printColrV1ScanSummary(hb_codepoint_t glyphId,
                            const ColrV1RenderStats &stats,
                            const GlyphSurface &surface, std::ostream &stream) {
  stream << "glyph " << glyphId << " paints=" << stats.m_totalPaints
         << " layers=" << stats.m_layerPaints
         << " glyphPaints=" << stats.m_glyphPaints
         << " colrGlyphPaints=" << stats.m_colrGlyphPaints
         << " composites=" << stats.m_composites
         << " gradients=" << stats.m_gradients
         << " clipPushes=" << stats.m_clipPushes
         << " transformPushes=" << stats.m_transformPushes
         << " groupPushes=" << stats.m_groupPushes
         << " size=" << surface.m_image.width() << "x"
         << surface.m_image.height() << " advance=" << surface.m_advance
         << '\n';
}

[[nodiscard]] bool matchesScanFilters(const ColrV1RenderStats &stats,
                                      const ColrV1ScanFilters &filters) {
  return stats.m_totalPaints >= filters.m_minPaints &&
         stats.m_gradients >= filters.m_minGradients &&
         stats.m_composites >= filters.m_minComposites &&
         stats.m_transformPushes >= filters.m_minTransforms;
}

template <typename Metric>
void printTopScanEntries(const char *label,
                         const std::vector<ColrV1ScanEntry> &entries,
                         std::size_t limit, Metric metric,
                         std::ostream &stream) {
  std::vector<ColrV1ScanEntry> sorted = entries;
  std::sort(sorted.begin(), sorted.end(),
            [&](const ColrV1ScanEntry &lhs, const ColrV1ScanEntry &rhs) {
              const std::size_t lhsValue = metric(lhs);
              const std::size_t rhsValue = metric(rhs);
              if (lhsValue != rhsValue) {
                return lhsValue > rhsValue;
              }
              return lhs.m_glyphId < rhs.m_glyphId;
            });
  stream << "[scan-top] " << label << '\n';
  std::size_t printed = 0;
  for (const ColrV1ScanEntry &entry : sorted) {
    const std::size_t value = metric(entry);
    if (value == 0) {
      continue;
    }
    stream << "glyph " << entry.m_glyphId << " value=" << value
           << " paints=" << entry.m_stats.m_totalPaints
           << " gradients=" << entry.m_stats.m_gradients
           << " composites=" << entry.m_stats.m_composites
           << " transforms=" << entry.m_stats.m_transformPushes
           << " size=" << entry.m_width << "x" << entry.m_height << '\n';
    ++printed;
    if (printed >= limit) {
      break;
    }
  }
}

void traceColrV1Stats(const ColrV1RenderStats &stats, std::size_t glyphCount,
                      std::ostream &stream) {
  stream << "[COLRv1-summary] glyphs=" << glyphCount
         << " paints=" << stats.m_totalPaints
         << " layers=" << stats.m_layerPaints
         << " glyphPaints=" << stats.m_glyphPaints
         << " colrGlyphPaints=" << stats.m_colrGlyphPaints
         << " composites=" << stats.m_composites
         << " gradients=" << stats.m_gradients
         << " clipPushes=" << stats.m_clipPushes
         << " transformPushes=" << stats.m_transformPushes
         << " groupPushes=" << stats.m_groupPushes << '\n';
}

void traceClusterLayout(const TextCluster &cluster,
                        const ClusterRenderResult &result,
                        std::ostream &stream) {
  stream << "[cluster] codepoints=";
  for (std::uint32_t codePoint : cluster.m_codePoints) {
    stream << " U+" << std::uppercase << std::hex << codePoint;
  }
  stream << std::dec << '\n';
  for (std::size_t i = 0; i < result.m_shapedGlyphs.size(); ++i) {
    const ShapedGlyph &glyph = result.m_shapedGlyphs[i];
    stream << "[cluster] glyph[" << i << "]"
           << " id=" << glyph.m_glyphId << " xAdvance=" << glyph.m_xAdvance
           << " yAdvance=" << glyph.m_yAdvance << " xOffset=" << glyph.m_xOffset
           << " yOffset=" << glyph.m_yOffset << '\n';
  }
  if (result.m_colrV1GlyphCount > 0) {
    traceColrV1Stats(result.m_colrV1Stats, result.m_colrV1GlyphCount, stream);
  }
}

void traceCacheStats(const char *label, const GlyphRenderCache &glyphCache,
                     const ClusterRenderCache &clusterCache,
                     const CacheStats &stats, std::ostream &stream) {
  stream << "[cache] " << label << " glyphEntries=" << glyphCache.size()
         << " glyphHits=" << stats.m_glyphHits
         << " glyphMisses=" << stats.m_glyphMisses
         << " clusterEntries=" << clusterCache.size()
         << " clusterHits=" << stats.m_clusterHits
         << " clusterMisses=" << stats.m_clusterMisses << '\n';
}

void traceSessionColrV1Stats(const char *label, const ColrV1RenderStats &stats,
                             std::size_t glyphCount, std::ostream &stream) {
  if (glyphCount == 0 || stats.m_totalPaints == 0) {
    stream << "[COLRv1-session] " << label << " glyphs=0 paints=0\n";
    return;
  }
  stream << "[COLRv1-session] " << label << " glyphs=" << glyphCount
         << " paints=" << stats.m_totalPaints
         << " layers=" << stats.m_layerPaints
         << " glyphPaints=" << stats.m_glyphPaints
         << " colrGlyphPaints=" << stats.m_colrGlyphPaints
         << " composites=" << stats.m_composites
         << " gradients=" << stats.m_gradients
         << " clipPushes=" << stats.m_clipPushes
         << " transformPushes=" << stats.m_transformPushes
         << " groupPushes=" << stats.m_groupPushes << '\n';
}

void traceSessionColrV1Stats(const char *label, const SessionColrV1Stats &stats,
                             std::ostream &stream) {
  stream << "[COLRv1-session] " << label
         << " logicalClusters=" << stats.m_logicalClusterCount
         << " renderedClusters=" << stats.m_renderedClusterCount
         << " logicalGlyphs=" << stats.m_logicalGlyphCount
         << " renderedGlyphs=" << stats.m_renderedGlyphCount << '\n';
  traceSessionColrV1Stats((std::string(label) + "/logical").c_str(),
                          stats.m_logicalStats, stats.m_logicalGlyphCount,
                          stream);
  traceSessionColrV1Stats((std::string(label) + "/rendered").c_str(),
                          stats.m_renderedStats, stats.m_renderedGlyphCount,
                          stream);
}
[[nodiscard]] bool writeOutputPng(const std::string &path,
                                  const Bitmap &bitmap) {
#if DOKU_TEXT_ENABLE_PNG_IO
  return writePng(path, bitmap);
#else
  (void)path;
  (void)bitmap;
  return false;
#endif
}

} // namespace

[[nodiscard]] std::string defaultFontPath() {
  return "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf";
}

[[nodiscard]] std::string defaultTextFontPath() {
  return "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf";
}

[[nodiscard]] std::string defaultText() { return "🍣"; }

int runCli(const std::vector<std::string> &args) {
  try {
    std::size_t argIndex = 0;
    bool listColrGlyphs = false;
    bool scanColrV1Glyphs = false;
    bool verifyMetricsFlag = false;
    bool traceColrV1 = false;
    bool renderGlyphIdMode = false;
    hb_codepoint_t glyphIdToRender = 0;
    unsigned int scanTopCount = 10;
    unsigned int scanLimit = 0;
    ColrV1ScanFilters scanFilters;
    while (argIndex < args.size()) {
      const std::string &flag = args[argIndex];
      if (flag == "--list-colr") {
        listColrGlyphs = true;
        ++argIndex;
        continue;
      }
      if (flag == "--verify-metrics") {
        verifyMetricsFlag = true;
        ++argIndex;
        continue;
      }
      if (flag == "--scan-colrv1") {
        scanColrV1Glyphs = true;
        ++argIndex;
        continue;
      }
      if (flag == "--scan-colrv1-top") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument("--scan-colrv1-top value is missing");
        }
        scanTopCount =
            parseUnsigned(args[argIndex++].c_str(), "--scan-colrv1-top");
        continue;
      }
      if (flag == "--scan-colrv1-limit") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument("--scan-colrv1-limit value is missing");
        }
        scanLimit =
            parseUnsigned(args[argIndex++].c_str(), "--scan-colrv1-limit");
        continue;
      }
      if (flag == "--scan-colrv1-min-paints") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument(
              "--scan-colrv1-min-paints value is missing");
        }
        scanFilters.m_minPaints =
            parseUnsigned(args[argIndex++].c_str(), "--scan-colrv1-min-paints");
        continue;
      }
      if (flag == "--scan-colrv1-min-gradients") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument(
              "--scan-colrv1-min-gradients value is missing");
        }
        scanFilters.m_minGradients = parseUnsigned(
            args[argIndex++].c_str(), "--scan-colrv1-min-gradients");
        continue;
      }
      if (flag == "--scan-colrv1-min-composites") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument(
              "--scan-colrv1-min-composites value is missing");
        }
        scanFilters.m_minComposites = parseUnsigned(
            args[argIndex++].c_str(), "--scan-colrv1-min-composites");
        continue;
      }
      if (flag == "--scan-colrv1-min-transforms") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument(
              "--scan-colrv1-min-transforms value is missing");
        }
        scanFilters.m_minTransforms = parseUnsigned(
            args[argIndex++].c_str(), "--scan-colrv1-min-transforms");
        continue;
      }
      if (flag == "--scan-colrv1-summary-only") {
        scanFilters.m_summaryOnly = true;
        ++argIndex;
        continue;
      }
      if (flag == "--trace-colrv1") {
        traceColrV1 = true;
        ++argIndex;
        continue;
      }
      if (flag == "--glyph-id") {
        ++argIndex;
        if (argIndex >= args.size()) {
          throw std::invalid_argument("--glyph-id value is missing");
        }
        glyphIdToRender = parseUnsigned(args[argIndex++].c_str(), "--glyph-id");
        renderGlyphIdMode = true;
        continue;
      }
      break;
    }
    if (listColrGlyphs && verifyMetricsFlag) {
      throw std::invalid_argument(
          "--list-colr and --verify-metrics cannot be specified together");
    }
    if (scanColrV1Glyphs && (listColrGlyphs || verifyMetricsFlag)) {
      throw std::invalid_argument("--scan-colrv1 は --list-colr / "
                                  "--verify-metrics と同時指定できません");
    }
    if (renderGlyphIdMode && listColrGlyphs) {
      throw std::invalid_argument(
          "--glyph-id cannot be used together with --list-colr");
    }
    if (!scanColrV1Glyphs &&
        (scanTopCount != 10 || scanLimit != 0 || scanFilters.m_minPaints != 0 ||
         scanFilters.m_minGradients != 0 || scanFilters.m_minComposites != 0 ||
         scanFilters.m_minTransforms != 0 || scanFilters.m_summaryOnly)) {
      throw std::invalid_argument("--scan-colrv1-* オプションは "
                                  "--scan-colrv1 と併用してください");
    }
#if !DOKU_TEXT_ENABLE_PNG_IO
    if (!listColrGlyphs && !scanColrV1Glyphs) {
      throw std::runtime_error(
          "このビルドでは PNG 出力が無効です。"
          "DOKU_TEXT_ENABLE_PNG_IO=ON で再ビルドしてください");
    }
#endif

    const bool legacyFontArg =
        argIndex < args.size() && fileExists(args[argIndex]);
    const std::string fontPath = legacyFontArg
                                     ? args[argIndex++]
                                     : resolveExistingPath({defaultFontPath()});

    const std::string text =
        (listColrGlyphs || renderGlyphIdMode)
            ? std::string()
            : (argIndex < args.size() ? args[argIndex++] : defaultText());

    const unsigned int paletteIndex =
        argIndex < args.size()
            ? parseUnsigned(args[argIndex++].c_str(), "paletteIndex")
            : 0U;
    const unsigned int pixelSize =
        argIndex < args.size()
            ? parseUnsigned(args[argIndex++].c_str(), "pixelSize")
            : 128U;

    FT_Library libraryRaw = nullptr;
    if (FT_Init_FreeType(&libraryRaw) != 0) {
      throw std::runtime_error("Failed to initialize FreeType");
    }
    FreeTypeLibraryHandle library(libraryRaw);
    FontContext emojiFont = makeFontContext(library.get(), fontPath, pixelSize);
    const std::string textFontPath =
        legacyFontArg ? fontPath : resolveExistingPath({defaultTextFontPath()});
    FontContext textFont =
        makeFontContext(library.get(), textFontPath, pixelSize);

    const bool hasColrV0 = hb_ot_color_has_layers(emojiFont.m_hbFace.get());
    const bool hasColrV1 = hb_ot_color_has_paint(emojiFont.m_hbFace.get());
    std::cout << "COLRv0 layers supported: " << (hasColrV0 ? "yes" : "no")
              << '\n';
    std::cout << "COLRv1 paint supported: " << (hasColrV1 ? "yes" : "no")
              << '\n';
    if (listColrGlyphs && !hasColrV0 && !hasColrV1) {
      throw std::runtime_error("COLR情報が含まれていません");
    }

    if (listColrGlyphs) {
      const unsigned int glyphCountFace =
          hb_face_get_glyph_count(emojiFont.m_hbFace.get());
      unsigned int listed = 0;
      for (hb_codepoint_t glyphId = 0; glyphId < glyphCountFace; ++glyphId) {
        unsigned int layerCount = 0;
        hb_ot_color_glyph_get_layers(emojiFont.m_hbFace.get(), glyphId, 0,
                                     &layerCount, nullptr);
        if (layerCount == 0 && FT_HAS_COLOR(emojiFont.m_face.get())) {
          FT_LayerIterator iterator;
          iterator.p = nullptr;
          FT_UInt layerGlyph = 0;
          FT_UInt layerColor = 0;
          while (FT_Get_Color_Glyph_Layer(emojiFont.m_face.get(), glyphId,
                                          &layerGlyph, &layerColor,
                                          &iterator)) {
            ++layerCount;
          }
        }
        const bool glyphHasV1 =
            hasColrV1 && hasColrV1RootPaint(emojiFont, glyphId);
        if (layerCount == 0 && !glyphHasV1) {
          continue;
        }
        std::cout << "glyph " << glyphId;
        if (FT_HAS_GLYPH_NAMES(emojiFont.m_face.get())) {
          char glyphName[128] = {};
          if (FT_Get_Glyph_Name(emojiFont.m_face.get(), glyphId, glyphName,
                                sizeof(glyphName)) == 0) {
            std::cout << " (" << glyphName << ")";
          }
        }
        if (layerCount > 0) {
          std::cout << " : layerCount = " << layerCount;
          if (glyphHasV1) {
            std::cout << " / COLRv1ペイントyes";
          }
          std::cout << '\n';
        } else {
          std::cout << " : COLRv1ペイントのみ" << '\n';
        }
        ++listed;
        if (listed >= 50) {
          std::cout << "(表示を50件で打ち切りました)\n";
          break;
        }
      }
      if (listed == 0) {
        std::cout << "COLRv0レイヤーを持つグリフは見つかりませんでした\n";
      }
      return 0;
    }

    if (scanColrV1Glyphs) {
      const unsigned int glyphCountFace =
          hb_face_get_glyph_count(emojiFont.m_hbFace.get());
      std::size_t scanned = 0;
      std::size_t rendered = 0;
      std::size_t failed = 0;
      ColrV1RenderStats totalStats;
      std::vector<ColrV1ScanEntry> scanEntries;
      for (hb_codepoint_t glyphId = 0; glyphId < glyphCountFace; ++glyphId) {
        if (!hasColrV1RootPaint(emojiFont, glyphId)) {
          continue;
        }
        if (scanLimit > 0 && scanned >= scanLimit) {
          break;
        }
        ++scanned;
        ColrV1RenderOptions options;
        ColrV1RenderStats stats;
        try {
          const GlyphSurface surface = renderSingleGlyph(
              emojiFont, glyphId, paletteIndex, options, &stats);
          accumulateColrV1Stats(totalStats, stats);
          ++rendered;
          if (matchesScanFilters(stats, scanFilters)) {
            scanEntries.push_back({glyphId, stats, surface.m_image.width(),
                                   surface.m_image.height(),
                                   surface.m_advance});
          }
          if (!scanFilters.m_summaryOnly &&
              matchesScanFilters(stats, scanFilters)) {
            printColrV1ScanSummary(glyphId, stats, surface, std::cout);
          }
        } catch (const std::exception &ex) {
          ++failed;
          std::cout << "glyph " << glyphId << " failed: " << ex.what() << '\n';
        }
      }
      std::cout << "COLRv1 scan 完了"
                << " scanned=" << scanned << " rendered=" << rendered
                << " failed=" << failed << '\n';
      traceColrV1Stats(totalStats, rendered, std::cout);
      printTopScanEntries(
          "paints", scanEntries, scanTopCount,
          [](const ColrV1ScanEntry &entry) {
            return entry.m_stats.m_totalPaints;
          },
          std::cout);
      printTopScanEntries(
          "gradients", scanEntries, scanTopCount,
          [](const ColrV1ScanEntry &entry) {
            return entry.m_stats.m_gradients;
          },
          std::cout);
      printTopScanEntries(
          "composites", scanEntries, scanTopCount,
          [](const ColrV1ScanEntry &entry) {
            return entry.m_stats.m_composites;
          },
          std::cout);
      printTopScanEntries(
          "transforms", scanEntries, scanTopCount,
          [](const ColrV1ScanEntry &entry) {
            return entry.m_stats.m_transformPushes;
          },
          std::cout);
      return failed == 0 ? 0 : 1;
    }

    if (renderGlyphIdMode) {
      ColrV1RenderOptions options;
      options.m_enableTrace = traceColrV1;
      options.m_traceStream = traceColrV1 ? &std::cerr : nullptr;
      ColrV1RenderStats stats;
      const GlyphSurface surface = renderSingleGlyph(
          emojiFont, glyphIdToRender, paletteIndex, options, &stats);
      const std::string outputName = makeGlyphIdOutputName(glyphIdToRender);
      if (!writeOutputPng(outputName, surface.m_image)) {
        throw std::runtime_error("PNG書き出しに失敗しました: " + outputName);
      }
      std::cout << outputName << " を出力しました "
                << "(font=" << emojiFont.m_path
                << ", glyphId=" << glyphIdToRender
                << ", advance=" << surface.m_advance
                << ", bearingX=" << surface.m_bearingX
                << ", bearingY=" << surface.m_bearingY << ")\n";
      if (stats.m_totalPaints > 0) {
        traceColrV1Stats(stats, 1, std::cout);
      }
      return 0;
    }

    const std::vector<std::uint32_t> codePoints = decodeUtf8(text);
    if (codePoints.empty()) {
      throw std::runtime_error("Failed to obtain glyph for rendering");
    }
    const std::vector<TextCluster> clusters = clusterText(codePoints);
    if (clusters.empty()) {
      throw std::runtime_error("Failed to obtain cluster for rendering");
    }

    GlyphRenderCache emojiGlyphCache;
    GlyphRenderCache textGlyphCache;
    ClusterRenderCache emojiClusterCache;
    ClusterRenderCache textClusterCache;
    CacheStats emojiCacheStats;
    CacheStats textCacheStats;
    SessionColrV1Stats emojiColrV1Stats;
    SessionColrV1Stats textColrV1Stats;
    unsigned int exportedCount = 0;
    bool metricsOk = true;
    bool warnedClusterMetrics = false;
    for (unsigned int i = 0; i < clusters.size(); ++i) {
      const TextCluster &cluster = clusters[i];
      FontContext &selectedFont =
          shouldUseEmojiFont(cluster) ? emojiFont : textFont;
      GlyphRenderCache &glyphCache =
          &selectedFont == &emojiFont ? emojiGlyphCache : textGlyphCache;
      ClusterRenderCache &clusterCache =
          &selectedFont == &emojiFont ? emojiClusterCache : textClusterCache;
      CacheStats &cacheStats =
          &selectedFont == &emojiFont ? emojiCacheStats : textCacheStats;
      SessionColrV1Stats &sessionColrV1Stats =
          &selectedFont == &emojiFont ? emojiColrV1Stats : textColrV1Stats;
      ColrV1RenderOptions options;
      options.m_enableTrace = traceColrV1;
      options.m_traceStream = traceColrV1 ? &std::cerr : nullptr;
      const ClusterCacheKey clusterCacheKey{cluster.m_codePoints, paletteIndex};
      auto clusterCacheIt = clusterCache.find(clusterCacheKey);
      bool clusterCacheHit = false;
      if (clusterCacheIt == clusterCache.end()) {
        ++cacheStats.m_clusterMisses;
        ClusterRenderResult clusterResult =
            renderGlyphCluster(selectedFont, cluster, paletteIndex, glyphCache,
                               cacheStats, options);
        clusterCacheIt =
            clusterCache.emplace(clusterCacheKey, std::move(clusterResult))
                .first;
      } else {
        ++cacheStats.m_clusterHits;
        clusterCacheHit = true;
      }
      if (traceColrV1 && clusterCacheHit) {
        std::cerr << "[cache] cluster hit [" << describeCluster(cluster)
                  << "]\n";
      }
      const ClusterRenderResult &clusterResult = clusterCacheIt->second;
      if (clusterResult.m_colrV1GlyphCount > 0) {
        ++sessionColrV1Stats.m_logicalClusterCount;
        sessionColrV1Stats.m_logicalGlyphCount +=
            clusterResult.m_colrV1GlyphCount;
        accumulateColrV1Stats(sessionColrV1Stats.m_logicalStats,
                              clusterResult.m_colrV1Stats);
        if (!clusterCacheHit) {
          ++sessionColrV1Stats.m_renderedClusterCount;
          sessionColrV1Stats.m_renderedGlyphCount +=
              clusterResult.m_colrV1GlyphCount;
          accumulateColrV1Stats(sessionColrV1Stats.m_renderedStats,
                                clusterResult.m_colrV1Stats);
        }
      }
      if (traceColrV1) {
        traceClusterLayout(cluster, clusterResult, std::cerr);
      }
      const GlyphSurface &surface = clusterResult.m_surface;

      const std::string outputName = makeOutputName(i, cluster);
      if (!writeOutputPng(outputName, surface.m_image)) {
        throw std::runtime_error("PNG書き出しに失敗しました: " + outputName);
      }
      std::cout << outputName << " を出力しました "
                << "(font=" << selectedFont.m_path
                << ", clusterLength=" << cluster.m_codePoints.size()
                << ", advance=" << surface.m_advance
                << ", bearingX=" << surface.m_bearingX
                << ", bearingY=" << surface.m_bearingY << ")\n";
      if (verifyMetricsFlag) {
        if (!verifyClusterMetrics(clusterResult, cluster)) {
          metricsOk = false;
        }
        if (!warnedClusterMetrics) {
          std::cout << "注記: クラスタ合成時の --verify-metrics はクラスタ "
                       "advance のみ検証します\n";
          warnedClusterMetrics = true;
        }
      }
      ++exportedCount;
    }

    if (traceColrV1) {
      traceCacheStats("emoji", emojiGlyphCache, emojiClusterCache,
                      emojiCacheStats, std::cerr);
      traceCacheStats("text", textGlyphCache, textClusterCache, textCacheStats,
                      std::cerr);
      traceSessionColrV1Stats("emoji", emojiColrV1Stats, std::cerr);
      traceSessionColrV1Stats("text", textColrV1Stats, std::cerr);
    }

    if (exportedCount == 0) {
      throw std::runtime_error("COLR描画に成功したグリフがyesません");
    }

    if (verifyMetricsFlag) {
      if (!metricsOk) {
        throw std::runtime_error("メトリクス検証に失敗しました");
      }
      std::cout << "メトリクス検証に成功しました (" << exportedCount
                << " glyphs)\n";
    }

    std::cout << "完了: emoji=" << emojiFont.m_path
              << ", text=" << textFont.m_path << " でグリフを出力しました\n";
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "エラー: " << ex.what() << '\n';
    return 1;
  }
}

int runCli(int argc, char *argv[]) {
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return runCli(args);
}

} // namespace doku::text
