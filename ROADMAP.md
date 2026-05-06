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
  - [x] Repository name DokuText
  - [x] text backend contract
  - [x] renderer contract

- [x] **Phase B: Clean DokuText API Migration**
  - [x] public header <DokuText/DokuText.hpp>
  - [x] namespace doku::text
  - [x] CMake target DokuText::DokuText
  - [x] examples/tests updated to DokuText API

- [ ] **Phase C: DokuEngine Adapter**
  - [ ] UiFontAtlas adapter
  - [ ] atlas metadata bridge
  - [ ] text cache invalidation bridge

- [ ] **Phase D: Budgeted Dynamic Atlas Integration**
  - [ ] pending missing glyph queue
  - [ ] max glyphs per frame
  - [ ] dirty rect GPU updates

---
*Last Updated: 2026-03-07*
