# DokuText — Text Shaping, Color Glyph, and Atlas Backend

## Overview

**DokuText** is a text backend for DokuEngine and other embeddable applications. It handles font fallback, UTF-8 text shaping, paragraph layout, COLRv0/COLRv1 color glyph rasterization, glyph atlas management, draw-ready text submissions, upload plans, and diagnostics.

> **Note on Naming**: *GlyphRenderer* was the original project name when the repository primarily focused on COLRv0/COLRv1 glyph rendering and PNG export. The repository is now **DokuText** because the library has grown into a comprehensive text shaping, layout, atlas, and renderer-integration backend. "GlyphRenderer" is still used in some legacy namespaces and CMake targets during the migration period.

---

## Goals

- **Comprehensive Text Backend**: Provide a bridge between font files and renderer-agnostic draw submissions.
- **Color Glyph Support**: Support **COLRv0** and **COLRv1** via **FreeType** and **HarfBuzz**.
- **Layout & Shaping**: Handle paragraph wrapping, alignment, and complex emoji clusters.
- **Atlas Management**: Efficient static and dynamic glyph atlas systems with incremental upload plans.
- **Engine Ready**: Designed for integration with DokuEngine and other custom renderers.

## Progress — 2026-03-07

- CPU-side RGBA buffer, PNG export, and glyph metrics.
- COLRv0 layer extraction and alpha-blended rendering.
- COLRv1 paint graph traversal (PaintGlyph, PaintColrGlyph, Transforms, PaintComposite).
- Linear / Radial / Sweep gradient support with color stop interpolation.
- Major blend modes (Porter-Duff, SCREEN, OVERLAY, etc.).
- Emoji cluster shaping (ZWJ, keycaps, skin tones, etc.).
- TextAtlas and BakedTextAtlas for efficient glyph caching.
- Multi-line paragraph layout with wrapping and alignment.
- Diagnostics and compatibility analysis for renderer integration.

## Usage (CLI)

The `app` CLI tool allows for quick verification of glyph rendering and font scanning.

```bash
# List COLR glyphs
./build/app --list-colr fonts/twemoji-colr/Twemoji.Mozilla.ttf

# Export a glyph to PNG
./build/app fonts/twemoji-colr/Twemoji.Mozilla.ttf 😀

# Trace COLRv1 paint traversal
./build/app --trace-colrv1 fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf 😀

# Scan a font for COLRv1 complexity
./build/app --scan-colrv1 fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf
```

Refer to the CLI help or the legacy documentation for advanced scanning and verification options.

---

## Dependencies

- [FreeType](https://freetype.org/) — font outline and metrics handling
- [HarfBuzz](https://harfbuzz.github.io/) — shaping, glyph ID resolution, COLR paint graph
- [libpng](http://www.libpng.org/pub/png/libpng.html) — PNG encoding (used for CLI/tests)

---

## Fonts
- `fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf` — COLRv1
- `fonts/twemoji-colr/Twemoji.Mozilla.ttf` — COLRv0
- `fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf` — Japanese / Latin / Nerd Font

## Build Instructions

```bash
git clone https://github.com/poicurr/DokuText.git
cd DokuText
./build.sh        # or build.bat on Windows
```

## FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  DokuText
  GIT_REPOSITORY https://github.com/poicurr/DokuText.git
  GIT_TAG main
)

FetchContent_MakeAvailable(DokuText)

# Current compatibility target:
target_link_libraries(your_target PRIVATE GlyphRenderer::GlyphRenderer)

# Planned target name:
# target_link_libraries(your_target PRIVATE DokuText::DokuText)
```

Public header: `#include <GlyphRenderer/GlyphRenderer.hpp>` (Legacy path)
Planned header: `#include <DokuText/DokuText.hpp>` (Compatibility alias available)

## High-Level Facade

DokuText provides `TextSystem` and `TextAtlas` as a high-level API.

```cpp
// Current compatibility namespace: glyphrenderer::
// Planned namespace: doku::text::

glyphrenderer::TextSystem textSystem;

glyphrenderer::FontFallbackChain chain;
chain.textFonts = { "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf" };
chain.emojiFonts = { "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf" };

glyphrenderer::FontChainId uiFontFamily = textSystem.registerFontFallbackChain(chain);

glyphrenderer::TextAtlasOptions atlasOptions;
atlasOptions.fontChainId = uiFontFamily;
atlasOptions.pixelSize = 16;
glyphrenderer::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

// Seeding the atlas
glyphrenderer::TextSeedRequest seedRequest;
seedRequest.presets = { glyphrenderer::TextSeedPreset::Ascii };
seedRequest.texts = {"Aあ🍣"};
textSystem.seedAtlas(atlas, seedRequest);

// Preparing draw-ready text
glyphrenderer::PreparedTextParagraph paragraph = textSystem.prepareText(atlas, "Hello\nWorld 🍣");
```

## Static Atlas API

For UI use cases where you want an immutable atlas built at startup.

```cpp
glyphrenderer::TextAtlasBakeRequest bakeRequest;
bakeRequest.fontChainId = fontChainId;
bakeRequest.pixelSize = 96;
bakeRequest.seedPresets = { glyphrenderer::TextSeedPreset::Ascii, glyphrenderer::TextSeedPreset::Japanese };

glyphrenderer::BakedTextAtlas bakedAtlas = textSystem.bakeTextAtlas(bakeRequest);

// Layout without mutating the atlas
glyphrenderer::PreparedTextParagraph prepared = textSystem.layoutTextParagraph(bakedAtlas, request);
```

## Renderer Contract

Renderer implementers should follow these DokuText contracts:

- **Atlas Images**: Always 8-bit RGBA, row-major, top-left origin.
- **Alpha Masks**: `R=G=B=A=coverage`. Straight RGBA.
- **Color Bitmaps**: Straight sRGB RGBA. No tinting applied.
- **UV Coordinates**: Normalized with top-left origin.
- **Pixel Coordinates**: Top-left origin, Y-down.

See `docs/text_backend_contract.md` for the full specification.

## Examples / Tests

- `examples/BasicEmbedding.cpp`: High-level `TextSystem` / `TextAtlas` usage.
- `examples/EngineSessionEmbedding.cpp`: Engine-style persistent session with low-level control.
- `examples/RendererIntegrationContract.cpp`: Upload plan and draw submission contract details.
- `examples/StaticAtlasEmbedding.cpp`: `BakedTextAtlas` static atlas flow.
- `tests/TextSystemFacadeTest.cpp`: Verification of the high-level facade.
- `tests/BakedAtlasApiTest.cpp`: Verification of the static atlas contract.

---

For more details on the evolution of this project, see `ROADMAP.md` and `docs/text_backend_contract.md`.
