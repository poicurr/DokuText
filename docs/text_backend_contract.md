# DokuText Backend Contract

## 1. Purpose
DokuText is a comprehensive text shaping, layout, and atlas management backend. It provides a bridge between raw font files and renderer-agnostic draw submissions.

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

This is a DokuText-side synchronous atlas update path. Atlas-backed text
preparation resolves shaped clusters against the atlas before rendering.
Registered glyphs reuse the stored image dimensions and bearings. Read-only
preparation and baked-atlas layout report atlas misses and retain advance and
caret geometry for those clusters.

### Prepared results and compatibility

Rebuild C++ clients when upgrading to the prepared result types with revision
fields. This changes the binary layout of `PreparedTextLine` and
`PreparedTextParagraph`.

- High-level layout and draw glyphs carry dimensions, bearings and advances.
  Their `glyph.image.rgba` arrays are empty in both the line and paragraph
  views. Draw glyphs carry a page handle, atlas rectangle, UVs and render
  attributes for each resolved atlas entry.
- `PreparedTextParagraph::drawRun.atlases` is empty. Read texture pixels from
  the source `TextAtlas::pages()` or `BakedTextAtlas::pages()`. New-page
  records from high-level preparation and seeding carry page handles and
  image dimensions. Upload plans from these calls list the pages touched by
  the update.
- For standalone glyph images, use the low-level `DokuText::layoutLine` or
  `DokuText::layoutText`. The low-level `buildDrawText(layout, cache)`
  overload provides an owning atlas snapshot.
- Atlas handles identify rendered cluster images. Simple single-glyph
  clusters keep their existing handle. Composed clusters include the shaped
  glyph sequence and offsets in the handle so distinct images can coexist.
  `probeText` reports one atlas lookup per shaped cluster. Rebuild any
  persisted tables keyed by the old composed-cluster handles.

### Runtime budgeted integration guidance

For runtime UI, it is recommended to call with `allowGlyphAppend = false` by default, detect missing glyphs through a non-destructive path, and let the engine-side adapter perform budgeted appends.

- Call `prepareLine` / `prepareText` with `TextPrepareOptions.allowGlyphAppend = false` to obtain draw results while suppressing atlas mutations.
- Use `probeText` to discover missing glyphs.
- Use `buildTextAtlasAppendPlan` to create an execution plan from candidates (`TextAtlasAppendBudget` / `TextAtlasAppendPlan`).
- Monitor `TextAtlas::glyphSetRevision` / `TextAtlas::atlasTextureRevision` for cache invalidation and renderer synchronization.
- Within budget, the adapter calls `seedAtlas` / `prepare... (allowGlyphAppend = true)` and updates only the dirty rects in `AtlasPageUploadPlan` to the GPU.

### DokuText contracts currently available

`TextAtlasAppendBudget`:

- `maxGlyphs`: `0` means unlimited
- `maxColorGlyphs`: `0` means unlimited
- `maxAtlasUploadBytes`: `0` means unlimited

`TextAtlasAppendPlan`:

- `candidates`: unique missing cluster images identified during shaping
- `estimatedGlyphCount`, `estimatedColorGlyphCount`, `estimatedUploadBytes`
- `budgetExceeded`

The count limits are evaluated before rasterization. When either count limit
rejects a plan, `estimatedUploadBytes` is zero because byte sizing was skipped.
Otherwise byte sizing renders the unique missing candidates to measure their
RGBA payload sizes. New pages and padding are accounted for by the upload
plan produced on append.

## 11. Revision Model

### Current

`BakedTextAtlas` exposes a stable `revision()` for immutable baked atlas identity.

`TextAtlas` exposes:

- `glyphSetRevision()`: represents logical changes to the glyph set.
- `atlasTextureRevision()`: represents updates to atlas image contents.

The cache owns both dynamic counters. Successful insertions and evictions
advance both counters. Compaction of a nonempty atlas advances the texture
revision. Cache replacement advances both counters. These rules also apply
to mutations through `TextAtlas::cache()`. For a direct cache update, consume
the returned update and build its renderer upload plan using
`buildAtlasPageUploadPlan`.

Each prepared result records the dynamic `glyphSetRevision` and
`atlasTextureRevision`, or the immutable `bakedAtlasRevision`, after
preparation. Pair page handles with the source atlas. Invalidate prepared
geometry when either recorded dynamic revision differs from the current
revision. In particular, rebuild the draw run after compaction or eviction
before using its UVs. Synchronize the texture to the current texture
revision before drawing. For baked results, match the source baked atlas and
its revision.

Missing glyph states must not be permanently cached because fallback chains, font reloads, atlas updates, or dynamic append may later resolve them.

## 12. Historical Notes

This document reflects the current public contract. Earlier development focused on COLRv0/COLRv1 emoji rasterization and incremental atlas features. Future changes to the API will be documented here.
