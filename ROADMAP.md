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
- [x] `BakedTextAtlas` (Immutable static atlas)
- [x] `TextAtlas` (Mutable dynamic atlas base)
- [x] Seed presets and range requests
- [x] Incremental upload plans
- [ ] **Phase 2.0: Dynamic Glyph Append**
  - [ ] On-demand glyph rasterization during layout
  - [ ] Missing glyph queue and background processing
  - [ ] Dirty rect incremental GPU updates
  - [ ] Atlas page compaction and eviction policies

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
- [ ] **Phase B: Compatibility Aliases**
  - [ ] `doku::text` namespace aliases
- [ ] **Phase C: DokuEngine Adapter**
- [ ] **Phase D: Dynamic Atlas**
- [ ] **Phase E: Physical Rename**
  - [ ] C++ namespace `doku::text`
  - [ ] CMake target `DokuText::DokuText`
  - [ ] Include path `<DokuText/DokuText.hpp>`

---
*Last Updated: 2026-03-07*
