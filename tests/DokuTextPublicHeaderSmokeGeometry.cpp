#include <DokuText/InputGeometry.hpp>

int main() {
  doku::text::TextCaretBoundary boundary;
  return boundary.byteOffset == 0 ? 0 : 1;
}
