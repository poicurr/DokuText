# DokuText Backend Contract

## 1. Purpose
DokuText is a comprehensive text shaping, layout, and atlas management backend for DokuEngine and other embeddable applications. It provides a bridge between raw font files and renderer-agnostic draw submissions.

**Legacy Note**: This project was formerly known as *GlyphRenderer*. The `glyphrenderer` namespace, old `<GlyphRenderer/...>` include paths, and `GlyphRenderer::GlyphRenderer` CMake target no longer exist in the source tree (see Migration Plan Phase B). Current and future integrations use **DokuText** exclusively. See "Legacy Cleanup Phases (L1-L3)" below.

## 2. Naming

- **DokuText**: The formal repository and subsystem name.
- **doku::text**: The current C++ namespace.
- **DokuText::DokuText**: The current CMake target.
- **<DokuText/DokuText.hpp>**: The primary public include path.

## 3. Responsibilities
DokuText is responsible for:
- Font fallback chain management
- UTF-8 text handling and shaping (via HarfBuzz)
- Glyph lookup and metric extraction (via FreeType)
- Line and paragraph layout (including wrapping and alignment)
- COLRv0 / COLRv1 color glyph rasterization
- Alpha mask glyph rasterization
- Emoji cluster handling (ZWJ sequences, etc.)
- Atlas management (`TextAtlas`, `BakedTextAtlas`)
- Atlas seeding, baking, and incremental updates
- Generation of atlas upload plans and dirty rects
- Producing draw-ready text submissions
- Text compatibility analysis and diagnostics
- Engine integration support helpers

## 4. Non-responsibilities
DokuText does **NOT** handle:
- UI widget tree or UI layout tree management
- User input handling (mouse/keyboard/touch)
- UI component behavior (Popup, Button, ScrollView, etc.)
- Actual execution of OpenGL / Dawn / Vulkan texture uploads
- Game-specific UI string management or localization
- UI themes and styling (beyond basic text properties)
- Text editor selection, caret management, or undo/redo logic
- IME (Input Method Editor) integration

## 5. Public API Layers

**Recommended for new integrations (including DokuEngine Runtime UI / TextInput / TextEditor):**
- **High-Level Facade (`TextSystem`)**: Primary entry point. `registerFontFallbackChain`, `createAtlas`/`bakeTextAtlas`, `seedAtlas`, `prepareLine`/`prepareText`, `probeText`, `buildTextAtlasAppendPlan`.
- **Atlas Types**: `TextAtlas` (mutable, dynamic UI / TextInput path with `allowGlyphAppend` control + revisions) and `BakedTextAtlas` (immutable, static UI path via `layoutText*`).
- **Input Support**: `caretBoundaries`, `hitTestTextLine`, `caretForByteOffset`/`caretXForByteOffset`, `selectionRectsForByteRange` (and fields on `PreparedTextLine` / `TextLineLayout`).

**Renderer Integration (stable contract):**
- `buildTextDrawSubmission`, `buildAtlasPageUploadPlan`, `TextDraw*` / `MultiLineDrawRun`, `PositionedDrawGlyph`, `TextAtlasPageInfo`, `AtlasPageUploadPlan`, `MaterialKind` / `SamplerKind` / `BlendHint`, `pageInfo` / `materialKindForPage` accessors, compatibility reports.

**Advanced / Low-Level (custom pipelines, tests, or maximum control only):**
- `DokuTextContext`, `GlyphAtlasCache` (ensureGlyphs / evict / compact), `DokuText` class (shapeLine/rasterize*/layout*/build* direct stage access), `TextLayoutContext`.

**Guideline**: New integrations and DokuEngine embedding **should prefer the high-level path**. Low-level remains fully supported but is not the primary surface. See README "Recommended API Paths" and "Legacy & Deprecation Policy".

## 6. Atlas Models

### BakedTextAtlas (Recommended for static UI)

`BakedTextAtlas` is an immutable/static atlas representation.

Use it for fixed-set or precomputed atlas workflows (startup bake, no runtime mutation, `layoutText*` for non-mutating prepare).

### TextAtlas (Recommended for dynamic UI / TextInput / TextEditor)

`TextAtlas` is a mutable atlas model.

Current DokuText APIs can synchronously ensure glyphs during:

- `seedAtlas`
- `prepareLine`
- `prepareText`

**DokuEngine integration recommendation**: Use `allowGlyphAppend=false` + `probeText` / `buildTextAtlasAppendPlan` + budget + revisions (`glyphSetRevision` / `atlasTextureRevision`) for controlled append. See README runtime strategy section.

The existence of `TextAtlas` does not imply that DokuEngine budgeted on-demand glyph append integration is complete.

## 7. Renderer Contract

DokuText does not own backend GPU resources.

DokuText produces renderer-facing atlas and draw data.
The renderer owns:

- texture creation
- texture upload
- resource lifetime
- backend-specific draw command encoding
- blend-state binding
- sampler binding

Each `PositionedDrawGlyph` is renderer-facing draw data and includes:

- atlas page id / handle
- `materialKind`
  - `AlphaMask`
  - `ColorBitmap`
- `samplerKind`
- `blendHint`
- `pixelRect`
- `uvRect`

Each `AtlasPageUploadPlan` references an atlas page handle and dirty rectangles.

### Pixel format

Atlas images are 8-bit RGBA, row-major, top-left origin.

`AlphaMask` pages use straight RGBA where:

- R = coverage
- G = coverage
- B = coverage
- A = coverage

`ColorBitmap` pages use straight RGBA and are intended for sRGB color sampling.

### Coordinates

`pixelRect` and upload dirty rectangles use top-left origin and Y-down pixel coordinates.

`uvRect` is normalized and uses top-left origin.

### Tint and premultiply

DokuText does not apply final renderer tint.

`AlphaMask` supports renderer-side tinting.

`ColorBitmap` is already colored and should not be vertex-tinted unless the renderer explicitly implements a separate policy.

DokuText does not premultiply atlas texels.

## 8. DokuEngine Integration Boundary
### DokuText
- Shapes text and lays out glyphs.
- Rasterizes glyphs into buffers.
- Owns font fallback logic.
- Produces atlas pages, upload plans, and draw submissions.

### DokuEngine UiFontAtlas Adapter
- Calls DokuText APIs to prepare text.
- Converts DokuText output types to DokuEngine core/ir UI types.
- Bridges atlas page metadata to renderer-specific texture bindings.
- Connects DokuText revisions to `UiTextCache` invalidation.

### UiContext
- Builds the UI node tree.
- Resolves layout sizes.
- Calls `UiFontAtlas` for text preparation.
- Does not directly handle rasterization.

## 9. Runtime TextInput Support

`PreparedTextLine` / `TextLineLayout` expose single-line caret and hit-test data
for DokuEngine runtime UI TextInput.

Public API:

- `caretBoundaries(...)`
  - returns line-local caret boundaries in ascending X order.
  - includes start and end boundaries.
  - returns one `{byteOffset=0, x=0}` boundary for an empty line.
- `hitTestTextLine(...)`
  - maps a line-local X coordinate to the nearest caret boundary.
  - clamps before the line start and after the line end.
  - uses cluster midpoint selection.
- `caretForByteOffset(...)` / `caretXForByteOffset(...)`
  - maps a UTF-8 byte offset to a valid caret boundary.
  - invalid byte offsets inside a UTF-8 sequence or text cluster are clamped to
    the nearest valid boundary.
- `selectionRectsForByteRange(...)`
  - returns zero or one line-local rect for a single-line byte range.
  - begin/end offsets are normalized and clamped through the caret boundary API.

`TextLineLayout::clusters` stores cluster byte ranges and line-local X ranges.
`TextLineLayout::caretBoundaries` is derived from these layout results, so
fallback font changes, color glyphs, and atlas page assignment do not require
DokuEngine-side glyph advance reconstruction.

Supported scope:

- `PreparedTextLine` / single-line `TextLineLayout`.
- UTF-8 byte offsets at DokuText-produced caret boundaries.
- Existing DokuText emoji clustering, including supported ZWJ sequences.
- Fallback font and color glyph layout produced by DokuText.

Unsupported or future scope:

- bidi / RTL caret behavior.
- vertical text.
- IME composition geometry.
- multi-line paragraph hit-test and editor selection.
- full Unicode grapheme segmentation. DokuText currently groups emoji joiners,
  variation selectors, emoji modifiers, keycap combiners, tag code points, and
  regional indicator pairs, but does not implement the full Unicode grapheme
  break algorithm.

### Renderer
- Consumes draw commands and atlas textures.
- Performs actual GPU uploads (OpenGL/Dawn).
- Does not need to know about font fallback or shaping details.

## 10. Dynamic Glyph Append Direction

### Current DokuText behavior

`TextAtlas` is mutable.

`TextSystem::prepareLine(...)`, `TextSystem::prepareText(...)`, and `TextSystem::seedAtlas(...)` may synchronously ensure required glyphs in the atlas and return upload plans.

This is a DokuText-side synchronous atlas update path.

### Runtime budgeted integration guidance

Runtime UI では `allowGlyphAppend = false` を基本とし、欠落 glyph は非破壊経路で検出してから
DokuEngine 側の adapter が予算付きで append することを推奨します。

- `TextPrepareOptions.allowGlyphAppend = false` で `prepareLine` / `prepareText` を実行し、描画を構築しつつ atlas 変更を抑止する
- `probeText` で不足 glyph を検出する
- `buildTextAtlasAppendPlan` で不足候補から実行計画を作成する（`TextAtlasAppendBudget` / `TextAtlasAppendPlan`）
- `TextAtlas::glyphSetRevision` / `TextAtlas::atlasTextureRevision` を監視し、cache 無効化と renderer 同期待ちに反映する
- adapter が budget 内で `seedAtlas` / `prepare... (allowGlyphAppend = true)` を実行し、`AtlasPageUploadPlan` の dirty rect だけを GPU 更新する

### DokuText contracts currently available

`TextAtlasAppendBudget`:

- `maxGlyphs`: `0` で無制限
- `maxColorGlyphs`: `0` で無制限
- `maxAtlasUploadBytes`: `0` で無制限

`TextAtlasAppendPlan`:

- `candidates`: layout 内で欠落した `MissingAtlasGlyph` 候補
- `estimatedGlyphCount`, `estimatedColorGlyphCount`, `estimatedUploadBytes`
- `budgetExceeded`

## 11. Revision Model

### Current

`BakedTextAtlas` exposes a stable `revision()` for immutable baked atlas identity.

`TextAtlas` exposes:

- `glyphSetRevision()`: glyph set の論理的変化を表す
- `atlasTextureRevision()`: atlas 画像内容更新を表す

`glyphSetRevision` は `evictedHandles` / `updatedEntries` の変化を追跡し、prepared text cache などの無効化トリガになります。

`atlasTextureRevision` は `newPages` / `dirtyRects` の発生を追跡し、renderer への再アップロードトリガになります。

### Planned DokuEngine integration revisions

`DokuEngine` 側 adapter では上記 revision 概念を使って `UiTextCache` の invalidation と renderer texture sync の境界を分離してください。

Missing glyph states must not be permanently cached because fallback chains, font reloads, atlas updates, or dynamic append may later resolve them.

## 12. Migration Plan

- **Phase A: Documentation & Naming**
  - Repository name DokuText.
  - text backend contract.
  - renderer contract.

- **Phase B: Clean DokuText API Migration**
  - public header <DokuText/DokuText.hpp>.
  - namespace doku::text.
  - CMake target DokuText::DokuText.
  - examples/tests updated to DokuText API.

- **Phase C: DokuEngine Adapter**
  - UiFontAtlas adapter.
  - atlas metadata bridge.
  - text cache invalidation bridge.

- **Phase D: Budgeted Dynamic Atlas Integration**
  - pending missing glyph queue.
  - max glyphs per frame.
  - dirty rect GPU updates.

## Legacy Cleanup Phases (L1-L3) — API Polish 2026

This cleanup round (API inventory, docs, alias coverage) completes **L1** without any breaking changes or physical renames.

- **L1 (Docs & Inventory, current)**: 
  - Public API surface fully documented as DokuText high-level recommended.
  - Legacy GlyphRenderer references reduced to historical notes only.
  - Recommended paths (static Baked vs dynamic TextAtlas vs low-level GlyphAtlasCache) clarified in README and this document.
  - No code changes, no deprecation macros, no removals.
  - DokuEngine-facing APIs (TextSystem, caret/hit-test, MaterialKind, upload plans, revisions, compatibility) confirmed stable and covered by `<DokuText/DokuText.hpp>`.

- **L2 (Future, non-breaking)**:
  - Introduce `[[deprecated("Use ...")]]` or a `DOKU_TEXT_DEPRECATED` macro for any transitional symbols if needed (currently none required).
  - Add deprecation comments to hypothetical old include paths.
  - Possibly provide thin compatibility headers (opt-in) that emit warnings.
  - Update any downstream CMake find_package examples.

- **L3 (Major version only)**:
  - Removal of any remaining legacy symbols after a deprecation window (e.g. 1-2 minor releases).
  - At that point, `glyphrenderer` namespace and old targets would be gone forever.

**Rule**: Old API removal **always** requires explicit deprecation phase. No direct deletion of symbols used by DokuEngine or external embedders.

See README "Legacy & Deprecation Policy" for user guidance.
