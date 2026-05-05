#GlyphRenderer — COLRv0 / COLRv1 Compatible Font to PNG Exporter

## Overview

This tool converts text or glyphs from color vector fonts (such as **NotoColorEmoji.ttf**) into **RGBA PNG images**, while also extracting per-glyph metrics (advance, bearings, etc.).
It supports **COLRv0** and substantial parts of **COLRv1** via **FreeType**, **HarfBuzz**, and **libpng**.

---

## Goals

- Support **COLRv0** and progressively expand **COLRv1** coverage
- Input: font file (`.ttf`, `.otf`) and text or glyph ID
- Output: per-glyph PNG images and corresponding layout metrics
- Cross-platform build with **CMake**

## Progress — 2026-03-07

- CPU側のRGBAバッファ、PNG書き出し、グリフメトリクス格納を整備済み
- COLRv0のレイヤー抽出とアルファ合成レンダリングを実装済み（メトリクス検証まで完了）
- COLRv1ペイントグラフ巡回を実装済み
- COLRv1の `PaintGlyph` / `PaintColrGlyph` / 変換系 / `PaintComposite` に対応済み
- 線形 / 放射 / スイープグラデーションは color stop 補間付きでCPU描画可能
- `PaintComposite` は Porter-Duff 系に加え、`SCREEN` / `OVERLAY` / `DARKEN` / `LIGHTEN` / `MULTIPLY` / `HSL_*` を含む主要ブレンドモードを処理可能
- emoji シーケンスはクラスタ単位で shaping し、ZWJ / keycap / skin tone / regional indicator を含む複合絵文字も 1 出力として扱える
- `--trace-colrv1` はペイント巡回に加え、クラスタ shaping 結果、COLRv1 統計、glyph / cluster cache の利用状況も出力可能
- 未対応: clip矩形、SVGノード、画像ノード

## Usage

```bash
#COLRv0レイヤーを持つグリフIDを調べる
./build/app --list-colr fonts/twemoji-colr/Twemoji.Mozilla.ttf

#COLRv0グリフをPNGとして書き出す
./build/app fonts/twemoji-colr/Twemoji.Mozilla.ttf $'\u203C'        # paletteIndex=0, pixelSize=128 の既定値
./build/app fonts/twemoji-colr/Twemoji.Mozilla.ttf 😀 1 160         # 任意のパレットとピクセルサイズを指定

#COLRv1グリフも同じインターフェースで出力可能
#例 :./ build / app fonts / Noto_Color_Emoji / NotoColorEmoji - Regular.ttf 😀

#COLRv1 のペイント巡回を追跡したい場合
./build/app --trace-colrv1 fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf 😀

#COLRv1 glyph を走査して paint 統計を一覧化したい場合
./build/app --scan-colrv1 fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf

#走査件数や複雑度で絞りたい場合
./build/app --scan-colrv1 --scan-colrv1-limit 200 --scan-colrv1-min-paints 40 --scan-colrv1-top 5 fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf

#gradient や transform を使う glyph だけを要約で拾いたい場合
./build/app --scan-colrv1 --scan-colrv1-summary-only --scan-colrv1-min-gradients 8 --scan-colrv1-min-transforms 20 fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf

#scan で見つけた glyph を直接再現したい場合
./build/app --trace-colrv1 --glyph-id 1768 fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf

#メトリクスを検証しながら書き出す
./build/app --verify-metrics fonts/twemoji-colr/Twemoji.Mozilla.ttf $'\u203C'
```

- 位置引数: `fontPath text paletteIndex pixelSize`（`text`以降は省略可）
- `--list-colr`を付けると描画は行わず、FreeType/Harfbuzz双方を用いたCOLRグリフの一覧を表示します。
- `--verify-metrics`を付けると、FreeTypeで取得したadvance/bearingと出力結果を突き合わせ、全グリフが一致しなければエラーで終了します。
- `--trace-colrv1`を付けると、COLRv1のペイント巡回・clip・transform・group の状態に加え、クラスタ単位の shaping 結果、COLRv1 の paint 統計、glyph / cluster cache の hit 状況を標準エラーへ出力します。
- `--scan-colrv1`を付けると、フォント中の COLRv1 glyph を走査し、glyph ごとの paint 統計を標準出力へ一覧化します。未対応ノードや複雑な glyph の洗い出し用です。
- `--scan-colrv1-top <N>`で上位集計の件数、`--scan-colrv1-limit <N>`で走査対象数、`--scan-colrv1-min-paints <N>` / `--scan-colrv1-min-gradients <N>` / `--scan-colrv1-min-composites <N>` / `--scan-colrv1-min-transforms <N>` で一覧対象の閾値を指定できます。`--scan-colrv1-summary-only` を付けると個別行を省略して集計だけ出します。
- `--glyph-id <id>`を付けると、テキスト shaping を介さず指定 glyph ID を直接PNG化できます。`--trace-colrv1` と組み合わせると scan で見つけた複雑 glyph の再現に使えます。
- COLRv1は一部ノード未対応です。未対応ノードに遭遇した場合は例外で停止します。
- 依存パッケージは `./build.sh` 経由で **vcpkg** が解決します。

---

## Dependencies

- [FreeType](https://freetype.org/) — font outline and metrics handling
- [HarfBuzz](https://harfbuzz.github.io/) — shaping, glyph ID resolution, COLR paint graph
- [libpng](http://www.libpng.org/pub/png/libpng.html) — PNG encoding

---

## Fonts
- `fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf` — COLRv1
- `fonts/twemoji-colr/Twemoji.Mozilla.ttf` — COLRv0
- `fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf` — 日本語 / 英数 / Nerd Font 用

## Build Instructions

```bash
git clone https://github.com/poicurr/GlyphRenderer.git
cd GlyphRenderer
./build.sh        # or build.bat on Windows
```

## FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  GlyphRenderer
  GIT_REPOSITORY https://github.com/poicurr/GlyphRenderer.git
  GIT_TAG main
)

FetchContent_MakeAvailable(GlyphRenderer)

target_link_libraries(your_target PRIVATE GlyphRenderer::GlyphRenderer)
```

公開ヘッダは `#include <GlyphRenderer/GlyphRenderer.hpp>` です。CLI を不要にしたい場合は、取り込み側で `set(GLYPH_RENDERER_BUILD_CLI OFF CACHE BOOL "" FORCE)` を指定できます。
CLI / examples / tests をすべて不要にする場合は、`GLYPH_RENDERER_BUILD_CLI=OFF`、`GLYPH_RENDERER_BUILD_EXAMPLES=OFF`、`GLYPH_RENDERER_BUILD_TESTS=OFF` にすると `GLYPH_RENDERER_ENABLE_PNG_IO` は既定で `OFF` になり、`libpng` なしで core library だけを取り込めます。

## High-Level Facade

common case 向けには、`TextSystem` と `TextAtlas` を使う高レベル façade を追加しています。これは low-level API の上に乗る convenience layer で、emoji / color glyph / multi-page を前提から外していません。

```cpp
glyphrenderer::TextSystem textSystem;

glyphrenderer::FontFallbackChain chain;
chain.textFonts = {
  "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
};
chain.emojiFonts = {
  "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
};
glyphrenderer::FontChainId uiFontFamily =
    textSystem.registerFontFallbackChain(chain);

glyphrenderer::TextAtlasOptions atlasOptions;
atlasOptions.fontChainId = uiFontFamily;
atlasOptions.pixelSize = 16;
glyphrenderer::TextAtlas atlas = textSystem.createAtlas(atlasOptions);

glyphrenderer::TextSeedRequest seedRequest;
seedRequest.presets = {
  glyphrenderer::TextSeedPreset::Ascii,
  glyphrenderer::TextSeedPreset::NerdFont,
};
seedRequest.texts = {"Aあ🍣"};
glyphrenderer::TextAtlasSeedResult seedResult =
    textSystem.seedAtlas(atlas, seedRequest);

glyphrenderer::PreparedTextLine line =
    textSystem.prepareLine(atlas, "Hello");
glyphrenderer::PreparedTextParagraph paragraph =
    textSystem.prepareText(atlas, "Aあ\n🍣");
```

この façade が返すものは次です。

- `TextAtlasSeedResult`
  seed 済み codepoint 群、atlas update、upload plan、compatibility
- `PreparedTextLine` / `PreparedTextParagraph`
  layout、draw run、draw submission、compatibility、atlas update、upload plan

つまり common case では、shaping / layout / ensureGlyphs / draw submission を毎回手で繋がなくても使えます。必要になった段階で、下にある `GlyphRendererContext` / `GlyphRenderer` / `GlyphAtlasCache` の low-level API へ降りられます。

## Static Atlas API

UI のように runtime で atlas を変化させたくない用途向けに、immutable な `BakedTextAtlas` も追加しています。

```cpp
glyphrenderer::TextSystem textSystem;
glyphrenderer::FontChainId fontChainId =
    textSystem.registerFontFallbackChain(chain);

glyphrenderer::TextAtlasBakeRequest bakeRequest;
bakeRequest.fontChainId = fontChainId;
bakeRequest.pixelSize = 96;
bakeRequest.atlasContext = {512, 512, 2};
bakeRequest.seedPresets = {
  glyphrenderer::TextSeedPreset::Ascii,
  glyphrenderer::TextSeedPreset::Hiragana,
  glyphrenderer::TextSeedPreset::NerdFont,
};
bakeRequest.bakeBudget.maxPages = 4;
bakeRequest.bakeBudget.maxBakeMilliseconds = 250.0;
bakeRequest.seedTexts = {"Aあ"};

glyphrenderer::BakedTextAtlas bakedAtlas =
    textSystem.bakeTextAtlas(bakeRequest);

glyphrenderer::TextRequest request;
request.text = "Aあ\n🍣";
request.fontChainId = fontChainId;
request.pixelSize = 96;

glyphrenderer::PreparedTextParagraph prepared =
    textSystem.layoutTextParagraph(bakedAtlas, request);
```

この経路の契約は次です。

- bake 時だけ atlas を構築する
- `BakedTextAtlas` は `pages()` / `pageImages()` / `entries()` / `uploadPlan()` / `revision()` / `bakeStats()` を保持する
- `layoutTextLine(...)` / `layoutTextParagraph(...)` は atlas を mutate しない
- atlas にない glyph は grow せず、`missingGlyphs` と `DiagnosticCode::AtlasEntryMissing` で返す
- `PreparedTextLine::atlasUploadPlan` / `PreparedTextParagraph::atlasUploadPlan` は空のまま
- `TextAtlasBakeBudget` は `maxSeedCodePoints` を pre-bake 上限、`maxGlyphs` / `maxPages` / `maxBakeMilliseconds` を post-bake guard として使う
- unsupported / unresolved codepoint や一部 glyph の raster 失敗は、可能な限り `DiagnosticCode::MissingGlyphCluster` と `missingGlyphs` に寄せて継続する
- hard failure は主に、無効な atlas context、未知の `FontChainId`、フォント自体の読み込み失敗のような構成エラーに限定する

`bakeStats()` からは少なくとも次を取れます。

- `seedCodePointCount`
- `processedSeedCodePointCount`
- `resolvedGlyphCount`
- `missingGlyphCount`
- `pageCount`
- `alphaMaskGlyphCount`
- `colorBitmapGlyphCount`
- `elapsedMilliseconds`
- `budgetExceeded`

## Seed Preset Guidance

`TextSeedPreset` は coarse な preset だけでなく、段階導入しやすい粒度にも分割しています。

- 軽量 preset
  - `Ascii`
  - `BasicLatin`
  - `CjkSymbolsPunctuation`
  - `Hiragana`
  - `Katakana`
  - `KatakanaPhoneticExtensions`
  - `HalfwidthFullwidthForms`
- 中量 preset
  - `NerdFont`
  - `EmojiSymbols`
- 重量 preset
  - `CjkUnifiedIdeographs`
  - `Japanese`
  - `Chinese`
  - `EmojiSupplement`
  - `Emoji`

`describeTextSeedPreset(...)` を使うと、各 preset の収録 range、推定 codepoint 数、`64px/1024x1024 page` 前提の典型 page 数と bake 時間の目安を取得できます。特に重い preset は次です。

- `TextSeedPreset::Japanese`
  - 含む範囲: `U+3000-U+303F`, `U+3040-U+309F`, `U+30A0-U+30FF`, `U+31F0-U+31FF`, `U+4E00-U+9FFF`, `U+FF00-U+FFEF`
  - 典型規模: 約 `21504` codepoints
  - 目安: `64px` bake で約 `64` pages、約 `900ms`
- `TextSeedPreset::Emoji`
  - 含む範囲: `U+2600-U+26FF`, `U+2700-U+27BF`, `U+1F300-U+1FAFF` に加えて `U+200D`, `U+FE0F`
  - 典型規模: 約 `2498` codepoints
  - 目安: `64px` bake で約 `22` pages、約 `220ms`

安全な段階導入をしたい場合は、いきなり `Japanese` や `Emoji` を使わず、まず次のように分けるのを推奨します。

- 日本語 UI の初期段階
  - `Ascii`
  - `CjkSymbolsPunctuation`
  - `Hiragana`
  - `Katakana`
  - 必要な語だけ `seedTexts`
- icon/font 混在
  - `Ascii`
  - `NerdFont`
  - 必要な和文だけ `seedTexts`
- emoji を試験導入
  - `EmojiSymbols`
  - 必要な emoji だけ `seedTexts`
  - 本格導入時に `EmojiSupplement` または `Emoji`

`customRanges` は static atlas では広く取りすぎない方が安全です。`0x0020-0x00FF`, `0x3000-0x30FF`, `0x4E00-0x9FAF` のような一括指定は急激に重くなります。推奨は次です。

- まず preset を優先する
- ideograph は range で広く取らず、必要語を `seedTexts` に入れる
- `customRanges` は punctuation や狭い block の補完に使う
- 予算を設けるなら `maxSeedCodePoints` と `maxPages` を併用する

`customRanges` に atlas へ入らない codepoint や、fallback 先で解決できない codepoint が含まれていても、それ自体は bake 全体の失敗条件ではありません。そうした codepoint は diagnostics と `missingGlyphs` に現れ、残りの glyph は継続して bake されます。

## Recommended Flow

通常の推奨経路は `TextSystem + TextAtlas/BakedTextAtlas + TextRequest` です。`GlyphRenderer` と `GlyphAtlasCache` は low-level/advanced 用で、統合側が atlas owner や shaping 段階を自前で握りたいときにだけ直接使う想定です。

low-level API を直接使う場合も、intended integration は one-shot atlas build ではなく、次の session-based flow です。

1. `GlyphRendererContext` を生成する
2. `FontFallbackChain` を登録して `FontChainId` を取得する
3. `GlyphRenderer` を生成する
4. `GlyphAtlasCache` を生成して保持する
5. `TextRequest` で `layoutLine(...)` / `layoutText(...)` を呼ぶ
6. `GlyphAtlasCache::ensureGlyphs(...)` で atlas へ glyph を反映する
7. `buildDrawRun(...)` / `buildDrawText(...)` または `prepareLine(...)` / `prepareText(...)` で draw-ready glyph を得る
8. `analyzeTextCompatibility(...)` と diagnostics を見て、renderer 側が許容できるか判断する
9. `buildAtlasPageUploadPlan(...)` を見て、`newPages` / `dirtyRects` / `updatedEntries` を GPU へ反映する

low-level API を直接使う場合の例は次です。

```cpp
glyphrenderer::GlyphRendererContext context;
glyphrenderer::FontFallbackChain chain;
chain.emojiFonts = {
  "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
};
chain.textFonts = {
  "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
};
glyphrenderer::FontChainId fontChainId =
    context.registerFontFallbackChain(chain);

glyphrenderer::GlyphRenderer renderer(context);
glyphrenderer::GlyphAtlasCache atlasCache({});

glyphrenderer::TextRequest request;
request.text = "Aあ🍣";
request.fontChainId = fontChainId;
request.pixelSize = 128;

glyphrenderer::TextLineLayout layout = renderer.layoutLine(request);
glyphrenderer::GlyphAtlasCacheUpdate atlasUpdate =
    atlasCache.ensureGlyphs(layout);
glyphrenderer::TextDrawRun drawRun = renderer.buildDrawRun(layout, atlasCache);
std::vector<glyphrenderer::AtlasPageUploadPlan> uploadPlan =
    glyphrenderer::buildAtlasPageUploadPlan(atlasCache, atlasUpdate);
glyphrenderer::CompatibilityConstraints constraints;
glyphrenderer::TextCompatibilityReport compatibility =
    glyphrenderer::analyzeTextCompatibility(drawRun, constraints);

glyphrenderer::TextRequest paragraphRequest;
paragraphRequest.text = "Aあ\n🍣";
paragraphRequest.fontChainId = fontChainId;
paragraphRequest.pixelSize = 128;
paragraphRequest.layoutOptions.wrappingWidth = 512;
paragraphRequest.layoutOptions.horizontalAlign =
    glyphrenderer::HorizontalAlign::Center;
glyphrenderer::MultiLineLayout paragraphLayout =
    renderer.layoutText(paragraphRequest);
glyphrenderer::MultiLineDrawRun paragraphDraw =
    renderer.prepareText(paragraphRequest, atlasCache);
```

この経路では、`layout` / `paragraphLayout` でメトリクスと配置済み glyph、`drawRun` / `paragraphDraw` で atlas 参照済みの draw-ready glyph を取得できます。`GlyphAtlasCache` を使うと、`ensureGlyphs(...)` により `newPages` / `updatedEntries` / `dirtyRects` を増分取得しつつ atlas を使い回せます。さらに `evict(...)` と `compact()` で不要 glyph の削除と page compaction もできます。draw-ready glyph には `atlasPageHandle`、`uvRect`、`pixel rect`、`materialKind`、`samplerKind`、`blendHint` が入るため、renderer 側でそのまま draw command 化しやすい構成です。`summary` と `diagnosticsSummary` から、fallback 数、color glyph 数、使用フォント一覧、atlas overflow、line overflow、missing glyph list も取れます。`wrappingWidth` は cluster 単位で実際に折り返しされるので、ZWJ を含む emoji シーケンスも途中で壊しません。

atlas upload については、`buildAtlasPageUploadPlan(...)` が engine 側の目線に近い helper です。

- `pageHandle` / `pageIndex`
  どの atlas texture page を触るか
- `isNewPage`
  新規に texture を作る必要があるか
- `dirtyRects`
  既存 page のうち、どの subrect を再 upload すべきか

つまり downstream renderer は、`GlyphAtlasCacheUpdate` の生データを毎回解釈し直さなくても、`buildAtlasPageUploadPlan(...)` を見るだけで texture create / subrect upload の判断をしやすくなります。

## Renderer Contract

renderer 実装時に迷いやすい契約は次で固定しています。

- atlas image の画素契約
  `GlyphAtlasPage::image` は常に 8-bit RGBA、row-major、上原点です。
- `MaterialKind::AlphaMask` の page/entry
  `atlasPixelFormatForMaterial(MaterialKind::AlphaMask)` は `AlphaMaskStraightRgba` を返します。これは `R=G=B=A=coverage` の straight RGBA で、premultiplied white ではありません。
- `MaterialKind::ColorBitmap` の page/entry
  `atlasPixelFormatForMaterial(MaterialKind::ColorBitmap)` は `ColorStraightRgba` を返します。これは通常の straight RGBA で、sRGB color texture sampling を前提にしています。GlyphRenderer 側では premultiply も tint も行いません。
- alpha-only path 向け helper
  `normalizeAlphaMaskToWhite(...)` は alpha-mask atlas を `RGB=255, A=coverage` の straight alpha white に正規化します。
- atlas UV の原点
  `uvRect` は常に正規化座標で、`atlasUvOrigin()` が返すとおり上原点です。`minY=0` が atlas 上端です。
- glyph 座標の基準
  `PositionedGlyph::originX/Y` と `PositionedDrawGlyph::pixelRect` は、どちらも top-left の pixel 位置です。Y は下向きに増えます。
- line metrics との関係
  `LineMetrics::baseline` は line-local space の baseline Y、`LineMetrics::boundingBox` は同じ座標系での visible glyph pixel rect の union です。

つまり alpha-only renderer は、

1. `buildAtlasPageUploadPlan(...)` で page と dirty rect を得る
2. `GlyphAtlasPage::image` を upload する
3. `normalizeAlphaMaskToWhite(...)` を必要に応じて使う
4. `buildTextDrawSubmission(...)` の `pixelRect` / `uvRect` をそのまま quad 化する

という流れで扱えます。

移行しやすい統合レイヤとして、次の helper も追加しています。

- `analyzeTextCompatibility(...)`
  `TextLineLayout` / `TextDrawRun` / `MultiLineDrawRun` が renderer に何を要求するかを機械可読に返します。`pagesUsed` / `materialKindsUsed` / `requiresColorGlyph` / `alphaOnlyCompatible` に加えて、`hasMissingGlyph` / `hasAtlasOverflow` も取得できます。`CompatibilityConstraints` を渡す overload もあり、`allowColorGlyphs=false` / `allowMultiplePages=false` / `maxPages=1` のような staged migration 条件を直接評価できます。役割は「renderer requirement summary」で、詳細理由は `diagnostics` が持ちます。
- `buildTextDrawSubmission(...)`
  `TextDrawRun` / `MultiLineDrawRun` を、`materialKind` / `samplerKind` / `blendHint` と page 情報付きの `PositionedDrawGlyph` 配列へ変換します。既存 engine の draw command 化を薄くできます。
- `buildAtlasPageUploadPlan(...)`
  `GlyphAtlasCacheUpdate` を renderer-oriented な page upload plan に変換します。texture page の新規作成と dirty rect upload の判断を downstream engine がやりやすくなります。
- `atlasPixelFormatForMaterial(...)` / `atlasUvOrigin()` / `normalizeAlphaMaskToWhite(...)`
  atlas の画素契約と UV 原点を明示し、alpha-only path の texture upload を迷わず実装できるようにする helper です。
- `TextSystem` / `TextAtlas`
  high-level façade です。atlas seed、line/paragraph layout、draw-ready output、compatibility、upload plan を common case で一度に扱えます。
- `BakedTextAtlas`
  immutable な static atlas です。runtime layout では atlas mutation を行わず、missing glyph だけを diagnostics と `missingGlyphs` で返します。
- `TextSeedPreset` / `TextSeedRange` / `TextSeedRequest`
  atlas seeding を preset/range/text ベースで扱うための一般-purpose seed model です。

## Examples / Tests

- `examples/BasicEmbedding.cpp`
  `TextSystem` / `TextAtlas` を使う高レベル façade の最小例です。
- `examples/EngineSessionEmbedding.cpp`
  永続 `GlyphRendererContext` と `GlyphAtlasCache` を保持し、`analyzeTextCompatibility(...)` / `buildTextDrawSubmission(...)` / `buildAtlasPageUploadPlan(...)` まで含めたエンジン寄りの例です。
- `examples/RendererIntegrationContract.cpp`
  atlas upload plan、draw submission、alpha-only path / color path の分岐、pixel contract helper を最小構成で確認する renderer 視点の例です。
- `examples/StaticAtlasEmbedding.cpp`
  `bakeTextAtlas(...)` と `layoutTextParagraph(...)` を使う static atlas の最小例です。
- `examples/MetricsLogEmbedding.cpp`
  `--verify-metrics` と `--trace-colrv1` を使い、ログを見ながら埋め込む例です。
- `examples/FetchContentConsumer/`
  外部プロジェクト側の CMake から `FetchContent_MakeAvailable(GlyphRenderer)` してリンクし、`GlyphRendererContext` / `GlyphRenderer` を保持する最小例です。`README.md` に vcpkg toolchain 付きの configure 例を置いています。
- `tests/SmokeCliTest.cpp`
  基本の emoji 出力確認向けです。
- `tests/ShapeLayoutAtlasTest.cpp`
  `layoutLine(...)` / `prepareLine(...)` の API を確認する入口です。
- `tests/MultiLineTextApiTest.cpp`
  改行を含む文字列に対する `layoutText(...)` / `prepareText(...)` 確認向けです。
- `tests/VisualPngScenarioTest.cpp`
  atlas ページと最終段落結果を `test_outputs/stb_replacement/*.png` に出力し、目視確認するシナリオです。
- `tests/IntegrationAdapterTest.cpp`
  `TextSystem` / `TextAtlas` を使い、compatibility / draw submission / upload plan をまとめて確認する入口です。
- `tests/TextSystemFacadeTest.cpp`
  `TextSystem` / `TextAtlas` の high-level façade で、seed atlas build、line/paragraph prepare、upload plan 取得まで確認する入口です。
- `tests/BakedAtlasApiTest.cpp`
  static atlas の bake、revision、upload plan、no-mutation layout、missing glyph を確認する入口です。
- `tests/FallbackDiagnosticsTest.cpp`
  混在文字列に対する fallback 採用結果と diagnostics 確認向けです。
- `tests/MetricsLogTest.cpp`
  メトリクス検証と trace ログ確認向けです。
- `tests/ScanLogTest.cpp`
  `COLRv1` scan の集計ログ確認向けです。

今の利用側イメージは、`GlyphRendererContext` へ fallback chain を登録し、`TextRequest` を `layoutLine(...)` / `layoutText(...)` と `prepareLine(...)` / `prepareText(...)` に渡す形です。atlas は `GlyphAtlasCache` を renderer 側 owner として保持し、差分更新だけ GPU へ送る前提です。PNG 出力は CLI と visual test の確認用途に限定しています。
