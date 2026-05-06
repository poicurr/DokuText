# DokuText Backend Contract

## 1. Purpose
DokuText is a comprehensive text shaping, layout, and atlas management backend for DokuEngine and other embeddable applications. It provides a bridge between raw font files and renderer-agnostic draw submissions.

## 2. Naming

- **DokuText**: The formal repository and subsystem name.
- **GlyphRenderer**: The legacy project name. It remains visible in current code as the CMake project name, CMake target, include path, namespace, and low-level class name during migration.
- **glyphrenderer**: The current C++ namespace.
- **doku::text**: The migration alias namespace provided by `<DokuText/DokuText.hpp>`.
- **GlyphRenderer::GlyphRenderer**: The current compatibility CMake target.
- **DokuText::DokuText**: Planned CMake target for a later physical migration phase.

Current stable usage remains `GlyphRenderer::GlyphRenderer` and `glyphrenderer::`.

The alias header is migration support, not a completed physical namespace/target rename.

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
- **High-Level Facade (`TextSystem`)**: Recommended for common use cases. Handles `TextAtlas` management and provides `prepareLine`/`prepareText` APIs.
- **Atlas Layer (`TextAtlas` / `BakedTextAtlas`)**: Manages the lifecycle of glyph images and UV mapping.
- **Low-Level Core (`GlyphRenderer`)**: Provides direct access to shaping, rasterization, and layout stages.

## 6. Atlas Models

### BakedTextAtlas

`BakedTextAtlas` is an immutable/static atlas representation.

Use it for fixed-set or precomputed atlas workflows.

### TextAtlas

`TextAtlas` is a mutable atlas model.

Current DokuText APIs can synchronously ensure glyphs during:

- `seedAtlas`
- `prepareLine`
- `prepareText`

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

### Renderer
- Consumes draw commands and atlas textures.
- Performs actual GPU uploads (OpenGL/Dawn).
- Does not need to know about font fallback or shaping details.

## 9. Dynamic Glyph Append Direction

### Current DokuText behavior

`TextAtlas` is mutable.

`TextSystem::prepareLine(...)`, `TextSystem::prepareText(...)`, and `TextSystem::seedAtlas(...)` may synchronously ensure required glyphs in the atlas and return upload plans.

This is a DokuText-side synchronous atlas update path.

### Planned DokuEngine integration behavior

DokuEngine integration should add a budgeted scheduling layer over this capability:

- small initial seed atlas
- pending missing glyph queue
- max glyphs per frame
- no full Unicode-range preload for runtime UI
- no large one-frame synchronous CJK/Emoji rasterization spike
- dirty-rect GPU upload based on atlas upload plans
- non-permanent missing glyph cache state

## 10. Revision Model

### Current

`BakedTextAtlas` exposes a stable `revision()` for immutable baked atlas identity.

`TextAtlas` currently exposes update objects and upload plans through seeding and preparation results.

### Planned DokuEngine integration revisions

DokuEngine integration should distinguish two revision concepts:

- `glyphSetRevision`
  - logical glyph availability changed
  - used for `UiTextCache` / prepared text invalidation

- `atlasTextureRevision`
  - atlas page texture contents changed
  - used for renderer texture synchronization and dirty-rect upload

Missing glyph states must not be permanently cached because fallback chains, font reloads, atlas updates, or dynamic append may later resolve them.

## 11. Migration Plan

- **Phase A: Documentation & Naming**
  - Repository rename to DokuText.
  - README / ROADMAP / contract alignment.
  - Initial `<DokuText/DokuText.hpp>` migration alias header.

- **Phase B: Compatibility Alias Stabilization**
  - Expand and test `doku::text` namespace aliases.
  - Maintain `glyphrenderer` namespace.
  - Maintain `GlyphRenderer::GlyphRenderer` CMake target.
  - Do not perform physical namespace or target rename.

- **Phase C: DokuEngine Adapter**
  - Add UiFontAtlas adapter boundary.
  - Convert DokuText atlas/draw data into DokuEngine core/ir types.
  - Bridge text cache invalidation policy.

- **Phase D: Budgeted Dynamic Atlas Integration**
  - Add pending missing glyph queue.
  - Add max glyphs per frame budget.
  - Add dirty rect texture upload integration.
  - Avoid large synchronous runtime glyph rasterization spikes.

- **Phase E: Physical Rename**
  - Migrate physical namespace to `doku::text`.
  - Add or switch to `DokuText::DokuText`.
  - Migrate include paths.
  - Decide legacy compatibility removal policy.
