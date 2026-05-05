# DokuText Backend Contract

## 1. Purpose
DokuText is a comprehensive text shaping, layout, and atlas management backend for DokuEngine and other embeddable applications. It provides a bridge between raw font files and renderer-agnostic draw submissions.

## 2. Naming
- **DokuText**: The formal name of the project and subsystem.
- **GlyphRenderer**: The legacy project name, now considered a compatibility alias.
- **glyphrenderer**: The current C++ namespace used for compatibility.
- **doku::text**: The migration C++ namespace (available via `<DokuText/DokuText.hpp>`).

"GlyphRenderer" is retained in existing code, include paths, and CMake targets during the migration period, but all new documentation and high-level descriptions use DokuText.

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
- **BakedTextAtlas**: An immutable, pre-baked atlas. Ideal for fixed-set UIs where runtime mutation is undesirable.
- **TextAtlas**: A mutable, dynamic atlas that can grow or be updated at runtime.

## 7. Renderer Contract
- **Pixel Format**: Atlas images are 8-bit RGBA, row-major, top-left origin.
- **Alpha Mask**: `R=G=B=A=coverage`. Not premultiplied white.
- **Color Bitmap**: Straight RGBA, intended for sRGB sampling. No premultiplication or tinting by DokuText.
- **UV Coordinates**: Normalized (0.0 to 1.0) with top-left origin.
- **Pixel Coordinates**: Top-left origin, Y-down.

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

## 9. Dynamic Glyph Append Direction (Phase 2.0)
- `BakedTextAtlas` remains immutable.
- `TextAtlas` will support on-demand glyph appending.
- Missing glyphs should be queued or reported to the system.
- Preloading large Unicode ranges (like full CJK) is discouraged.
- Recommended approach: Small initial seed + on-demand append for CJK/Emoji.
- `TextRun` cache must not permanently cache "missing" states if they can be resolved later.

## 10. Revision Model
- **glyphSetRevision**: Incremented when the set of available glyphs changes. Used for `UiTextCache` invalidation.
- **atlasTextureRevision**: Incremented when the GPU texture needs synchronization due to atlas updates.

## 11. Migration Plan
- **Phase A: Documentation and Naming** (Current)
  - Rename repository to DokuText.
  - Update README/ROADMAP.
  - Define the backend contract.
- **Phase B: Compatibility Aliases**
  - Introduce `doku::text` namespace aliases.
  - Maintain `glyphrenderer` namespace and `GlyphRenderer::GlyphRenderer` CMake target.
- **Phase C: DokuEngine Adapter**
  - Integrate with `UiFontAtlas` using DokuText terminology.
  - Connect revisions to `UiTextCache`.
- **Phase D: Dynamic Atlas**
  - Implement on-demand glyph append in `TextAtlas`.
  - Implement dirty rect incremental uploads.
- **Phase E: Physical Rename**
  - Formal move to `doku::text` namespace.
  - Update CMake targets to `DokuText::DokuText`.
  - Update include paths to `<DokuText/DokuText.hpp>`.
  - Deprecate legacy names.
