#include <DokuText/RendererContract.hpp>

int main() {
  doku::text::MaterialKind kind = doku::text::MaterialKind::AlphaMask;
  return kind == doku::text::MaterialKind::AlphaMask ? 0 : 1;
}
