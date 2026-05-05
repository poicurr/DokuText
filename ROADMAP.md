#ROADMAP

COLRv0 / COLRv1の両対応レンダラを安全に仕上げるための段階的な手順を記す。

    > 2026 - 03 -
          07 更新
    : 基盤整備とCOLRv0レイヤー描画・メトリクス検証完了。COLRv1はペイントグラフ巡回、変換、主要コンポジット、線形 /
      放射 / スイープグラデーションのCPU描画、複合 emoji クラスタ描画、trace /
      cache /
      統計出力、フォント全体の scan まで対応済み。

      ##1.基盤整備 1.1 *
      *ソフトウェア描画ターゲット準備 **（完了） -`structBitmap {
  intwidth, height;
  std::vector<uint8_t> rgba;
}
`のような、CPUバッファを定義。 - 必要に応じて`Resize`,`Clear`,`Blit`などのユーティリティ関数を用意。 1.2 *
                                                                      *PNG書き出しユーティリティ
                                                                          **（完了） -
                                                                  libpng（CMakeで検出済み）を用いて`WritePng(
                                                                      path,
                                                                      Bitmap)`を実装。
                                                                  -
                                                                  RGBA8bitのバッファをそのままPNGに書き出せることを確認。
                                                                      1.3 *
                                                                      *Glyphmetadata
                                                                          **（完了）
                                                                  -`structGlyphSurface {
  Bitmapimage;
  intbearingX;
  intbearingY;
  intadvance;
}
`を定義し、描画結果とメトリクスをペアで扱えるようにする。

        ##2.COLRv0対応（優先度高） 2.1 *
        *レイヤー情報取得 **（完了） -
    HarfBuzzのOTAPI(`hb_ot_color_has_layers`,`hb_ot_color_glyph_get_layers`,`hb_ot_color_palette_get_colors`)
            を使い、レイヤーglyphとカラーを取得。 2.2 *
        *レイヤー描画 **（完了）
    - 各レイヤーglyphをFreeTypeでラスタライズ。
    - 取得した色を乗算してCPUバッファへブレンド（`SRC_OVER`）。 -
    完成した`GlyphSurface`をPNGに書き出して確認。 2.3 *
        *メトリクス検証 **（完了）
    - advance / bearingがFreeTypeと整合するか`--verify
    - metrics`で自動検証可能。 -
    COLRv0の代表的なemoji(例 :🍣) が完全に描けるかテスト。

        ##3.COLRv1対応（段階実装） 3.1 *
        *ペイントグラフ巡回枠組み **（進行中）
    -`renderColrV1Glyph`を新設し、`FT_Get_Color_Glyph_Paint`ベースでCOLRv1ペイントグラフを巡回。
    -`PaintGlyph`/`PaintColrGlyph`/ 変換系 /`PaintComposite` および線形 / 放射
         / スイープグラデーションをCPUで描画可能。
    - gradient は color stop 補間を行って描画する。 -
    TODO : clip矩形、SVG / 画像ノードへの対応。 3.2 *
           *クリップ矩形(`push_clip_rectangle`) * *（未着手）
    - Clipスタックに矩形を保持できるよう`PaintCtx::Clip`に`Type::Rect`を追加。 -
    グリフマスクと同様、矩形もCPUバッファでマスク化し、合成時に適用。 3.3 *
        *グループ(`push_group`/`pop_group`) とコンポジット **（進行中）
    -
    内部的な一時バッファ切り替えは実装済みで、`PaintComposite` の評価に使用している。
    - Porter - Duff 系に加えて、主要 separable / non
    - separable blend モードも実装済み。 -
    残りの特殊ケースは実フォントで不足が出た時点で追う。 3.4 *
        *追加ノード対応 **（一部完了）
    -`linear_gradient`/`radial_gradient`/`sweep_gradient` は color stop
         補間込みで描画済み。
    -`push_layer`/`pop_layer`があれば、`push_group`相当として同じ仕組みで処理。
    -
    カスタムパレット(`hb_paint_funcs_set_custom_palette_color_func`)
            も必要であれば追加。 3.5 *
        *反復テスト **（進行中）
    - NotoColorEmoji を対象に、単体glyphのPNG出力までは確認済み。 -
    ZWJ / keycap / skin tone /
        regional indicator を含む複合 emoji クラスタのPNG出力と advance
        検証を確認済み。
    - 複雑な顔文字や未対応ノード混在ケースの網羅確認は未完了。
    - 不足ノードが出るたびにハンドラを追加。 -`--trace -
    colrv1` により、巡回順・clipstack・transformstack・groupstack
        に加え、クラスタ shaping、COLRv1 paint 統計、glyph
        / cluster cache 統計の確認が可能。
    -`--scan -
    colrv1` により、フォント全体を走査して gradient / composite
        / transform を多用する glyph を抽出しやすくなった。
    -`--scan - colrv1 - top` / `--scan - colrv1 - limit` / `--scan - colrv1
    - min - *` / `--scan - colrv1 - summary
    - only` により、重いフォントでも複雑 glyph の探索を絞り込みやすくなった。
    -`--glyph -
    id` により、scan で見つけた glyph を shaping
    を介さず単体再現できるようになった。

    ##4.OpenGL連携（必要に応じて）
（未着手）
    -
    CPUバッファで正しく描画できたら、OpenGL側では単に`glTexSubImage2D`でアップロードし描画するだけにする。
    -
    これによりGPU描画の複雑化を避け、まずはPNG出力で仕様準拠の実装ができていることを優先確認する。

    ##5.ライブラリ利用性
（進行中）
    - `GlyphRenderer::GlyphRenderer` ターゲットを公開し、`FetchContent` で取り込みやすい CMake 構成へ整理。
    - CLI は `GLYPH_RENDERER_BUILD_CLI` オプションで無効化可能。
    - 公開 API は `include/GlyphRenderer/GlyphRenderer.hpp` の `GlyphRendererContext` / `GlyphRenderer` / `renderText(...)`。
    - `shapeLine(...)` / `rasterizeLine(...)` / `layoutLine(...)` / `buildAtlas(...)` / `buildDrawRun(...)` を公開し、shape・raster・位置付け・atlas・描画契約を段階的に扱えるようにした。
    - `layoutText(...)` / `buildDrawText(...)` を追加し、改行を含む文字列を行単位で扱えるようにした。
    - `GlyphHandle` と正規化 `uvRect` を追加し、shape/layout/atlas 間で glyph を同定しやすくした。
    - `examples/EngineSessionEmbedding.cpp` と `examples/FetchContentConsumer/` で、永続 context を持つ利用側イメージを追加。
    - `tests/ShapeLayoutAtlasTest.cpp`、`tests/FallbackDiagnosticsTest.cpp` で、atlas 契約と fallback diagnostics の観点別入口を追加。

    ##6.その他メモ
（未着手）
    -
    COLRv1glyphをPNGに書き出す際は、ベースライン情報も出力（アトラス化やレイアウト時に使用）。
    -
    大量glyphを処理する場合は、一時バッファやFBOをキャッシュ／再利用する最適化を検討する。
    -
    完成後にOpenGL経由でリアルタイム描画したい場合も、CPUレンダラーのコードをベースにシェーダー化する方が安全。
