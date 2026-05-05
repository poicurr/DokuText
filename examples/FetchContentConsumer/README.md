# FetchContentConsumer

外部プロジェクト側から `FetchContent` で `GlyphRenderer` を取り込む最小例です。

Windows + vcpkg の例:

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

この例は推奨経路として `TextSystem` / `TextAtlas` を使い、

- fallback chain 登録
- atlas seed
- `prepareLine(...)`

までを最小構成で呼びます。`GlyphRendererContext` / `GlyphRenderer` /
`TextLayoutContext` は、より低レベルな制御が必要な場合だけ直接使う想定です。
