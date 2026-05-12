# DokuText — Text Shaping, Color Glyph, and Atlas Backend

## Overview

**DokuText** is a text backend for DokuEngine and other embeddable applications. It handles font fallback, UTF-8 text shaping, paragraph layout, COLRv0/COLRv1 color glyph rasterization, glyph atlas management, draw-ready text submissions, upload plans, and diagnostics.

Historical Note: This project was formerly known as *GlyphRenderer*.

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

Refer to the CLI help or the documentation for advanced scanning and verification options.

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

```bash
# CMake直接での構成
cmake -S . -B build -DDOKU_TEXT_BUILD_TESTS=ON -DDOKU_TEXT_BUILD_EXAMPLES=ON -DDOKU_TEXT_BUILD_CLI=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
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

target_link_libraries(your_target PRIVATE DokuText::DokuText)
```

Public header: `#include <DokuText/DokuText.hpp>`
Namespace: `doku::text`
CMake target: `DokuText::DokuText`

## Install and `find_package`

```bash
cmake --install build --prefix path/to/install
```

```cmake
find_package(DokuText CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE DokuText::DokuText)
```

## High-Level Facade

DokuText provides `TextSystem` and `TextAtlas` as a high-level API.

```cpp
#include <DokuText/DokuText.hpp>

doku::text::TextSystem textSystem;

doku::text::FontFallbackChain chain;
chain.textFonts = { "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf" };
chain.emojiFonts = { "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf" };

doku::text::FontChainId uiFontFamily = textSystem.registerFontFallbackChain(chain);

doku::text::TextAtlasOptions atlasOptions;
atlasOptions.fontChainId = uiFontFamily;
atlasOptions.pixelSize = 16;
doku::text::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

// Seeding the atlas
doku::text::TextSeedRequest seedRequest;
seedRequest.presets = { doku::text::TextSeedPreset::Ascii };
seedRequest.texts = {"Aあ🍣"};
textSystem.seedAtlas(atlas, seedRequest);

// Preparing draw-ready text
doku::text::PreparedTextParagraph paragraph = textSystem.prepareText(atlas, "Hello\nWorld 🍣");
```

## Static Atlas API

For UI use cases where you want an immutable atlas built at startup.

```cpp
doku::text::TextAtlasBakeRequest bakeRequest;
bakeRequest.fontChainId = fontChainId;
bakeRequest.pixelSize = 96;
bakeRequest.seedPresets = { doku::text::TextSeedPreset::Ascii, doku::text::TextSeedPreset::Japanese };

doku::text::BakedTextAtlas bakedAtlas = textSystem.bakeTextAtlas(bakeRequest);

// Layout without mutating the atlas
doku::text::PreparedTextParagraph prepared = textSystem.layoutTextParagraph(bakedAtlas, request);
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

## Runtime atlas append strategy for DokuEngine integration

DokuText の現在の API では、UI runtime でのフレーム内一括 `allowGlyphAppend = true` を避ける前提の構成を案内できます。

推奨手順:

1. `prepareLine` / `prepareText` を `allowGlyphAppend = false` で呼び出して、必要な箇所の描画結果を取得しつつ atlas 変更を抑止する。  
2. 同一テキストは `probeText` または `buildTextAtlasAppendPlan` で不足 glyph を取得する。  
3. DokuEngine 側の列（キュー）に候補を格納し、1 フレームあたりの budget（`TextAtlasAppendBudget`）で `glyphSetRevision` / `atlasTextureRevision` の差分に追従して一部だけ append する。  
4. `atlasUploadPlan` / dirty rect を用いて、変更分だけ renderer texture を更新する。

補助:

```cpp
doku::text::TextPrepareOptions options;
options.allowGlyphAppend = false;

doku::text::TextAtlasAppendBudget budget;
budget.maxGlyphs = 32;            // 0 は無制限
budget.maxColorGlyphs = 8;        // 0 は無制限
budget.maxAtlasUploadBytes = 0;    // 0 は無制限

const auto plan = textSystem.buildTextAtlasAppendPlan(atlas, "UI text", options, budget);
// plan.budgetExceeded を確認し、adapter 側で実行可否を決める
```

`TextAtlas` は revision API を持ちます。

```cpp
auto glyphSet = atlas.glyphSetRevision();
auto atlasTexture = atlas.atlasTextureRevision();
```

 - `glyphSetRevision`: atlas の論理的な glyph set が変化した時に進む。テキスト cache の無効化に利用。  
 - `atlasTextureRevision`: atlas 画像データや dirty rect を伴う変更時に進む。GPU への再アップロード条件に利用。

不足 glyph の情報を永続キャッシュしない点は意図的です。フォントリロードや今後の append 結果で解決し得るため、DokuEngine 側は毎フレーム probe / plan を更新する運用を推奨します。

---

For more details on the evolution of this project, see `ROADMAP.md` and `docs/text_backend_contract.md`.
