# DokuText Roadmap

This roadmap outlines the evolution of DokuText from a color glyph renderer to a comprehensive text backend for DokuEngine.

## 1. Color Glyph Rendering
- [x] COLRv0 support
- [x] COLRv1 paint graph traversal
- [x] Linear / Radial / Sweep gradients with interpolation
- [x] Composite / Blend modes (Porter-Duff and separable/non-separable)
- [x] Emoji cluster shaping (ZWJ, etc.)
- [ ] SVG node support
- [ ] Bitmap / PNG node support
- [ ] Clip paths (complex shapes beyond rectangles)

## 2. Text Layout Backend
- [x] `FontFallbackChain` management
- [x] `TextSystem` facade
- [x] Multi-line paragraph layout
- [x] Word wrapping and alignment
- [x] Diagnostics and fallback tracing
- [ ] Advanced bidirectional text (Bidi) support
- [ ] Complex script refinement (Indic, etc.)

## 3. Atlas Systems
- [x] `BakedTextAtlas` immutable static atlas
- [x] `TextAtlas` mutable atlas model
- [x] Seed presets and range requests
- [x] Synchronous glyph ensure path through `seedAtlas`, `prepareLine`, and `prepareText`
- [x] Renderer-facing upload plan helpers
- [ ] Budgeted missing glyph queue for DokuEngine integration
- [ ] Max glyphs per frame policy
- [ ] Dirty rect incremental GPU upload integration
- [ ] Atlas page compaction and eviction policy

## 4. Renderer Integration
- [x] Draw-ready submission (`PositionedDrawGlyph`)
- [x] Material kind categorization (AlphaMask vs ColorBitmap)
- [x] Compatibility analysis (`analyzeTextCompatibility`)
- [x] Upload plan helpers
- [ ] OpenGL integration example / reference
- [ ] Dawn (WebGPU) integration example / reference

## 5. DokuEngine Integration
- [ ] `UiFontAtlas` adapter implementation
- [ ] `UiTextCache` invalidation via `glyphSetRevision`
- [ ] Asynchronous glyph feeding
- [ ] Shared atlas management across UI contexts

## 6. Migration & Refactoring

- [x] **Phase A: Documentation & Naming**
  - [x] Repository rename to DokuText
  - [x] README / ROADMAP updates
  - [x] Project contract definition
  - [x] Initial `<DokuText/DokuText.hpp>` migration alias header

- [ ] **Phase B: Compatibility Alias Stabilization**
  - [ ] Expand `doku::text` alias coverage for renderer-integration types
  - [ ] Add alias-header compile smoke test
  - [ ] Keep `glyphrenderer` namespace stable
  - [ ] Keep `GlyphRenderer::GlyphRenderer` target stable

- [ ] **Phase C: DokuEngine Adapter**
  - [ ] Add UiFontAtlas adapter boundary
  - [ ] Bridge DokuText atlas page metadata into DokuEngine core/ir types
  - [ ] Bridge text cache invalidation policy

- [ ] **Phase D: Budgeted Dynamic Atlas Integration**
  - [ ] Pending missing glyph queue
  - [ ] Max glyphs per frame budget
  - [ ] Dirty rect incremental GPU updates
  - [ ] DokuEngine `UiTextCache` retry/invalidation bridge
  - [ ] Atlas page compaction and eviction policy

- [ ] **Phase E: Physical Rename**
  - [ ] Physical C++ namespace migration to `doku::text`
  - [ ] CMake target migration to `DokuText::DokuText`
  - [ ] Include path migration
  - [ ] Legacy `GlyphRenderer` compatibility removal policy

---
*Last Updated: 2026-03-07*
