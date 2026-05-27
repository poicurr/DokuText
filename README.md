# DokuText

**DokuText** is a C++ library for text shaping, layout, and high-quality emoji rasterization.

It provides strong support for both COLRv0 and COLRv1 color glyphs, complex script shaping, and efficient atlas management, producing renderer-agnostic draw commands and textures.

## Quick Start

```cmake
FetchContent_Declare(
  DokuText
  GIT_REPOSITORY https://github.com/poicurr/DokuText.git
  GIT_TAG main
)
FetchContent_MakeAvailable(DokuText)

target_link_libraries(your_target PRIVATE DokuText::DokuText)
```

```cpp
#include <DokuText/DokuText.hpp>

using namespace doku::text;

TextSystem ts;
FontFallbackChain chain{ .textFonts = {"MyFont.ttf"} };
auto font = ts.registerFontFallbackChain(chain);

TextAtlas atlas = ts.createAtlas({.fontChainId = font, .pixelSize = 24});
ts.seedAtlas(atlas, {.presets = {TextSeedPreset::BasicLatin}});

auto draw = ts.prepareText(atlas, "Hello 世界 🍣");
```

## Documentation

- Full specification: `docs/text_backend_contract.md`
- Examples: `examples/`

## Optional CLI Tool

```bash
cmake -S . -B build -DDOKU_TEXT_BUILD_CLI=ON
cmake --build build
./build/dokutext-cli --help
```
