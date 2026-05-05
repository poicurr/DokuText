#include "PngWriter.hpp"

#include "core/ir/Bitmap.hpp"

#include <png.h>

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
class FileHandle {
public:
  FileHandle(const std::string &path, const char *mode) {
    m_file = std::fopen(path.c_str(), mode);
    if (!m_file) {
      throw std::runtime_error("Failed to open file for PNG output: " + path);
    }
  }

  ~FileHandle() {
    if (m_file) {
      std::fclose(m_file);
    }
  }

  [[nodiscard]] std::FILE *get() const { return m_file; }

private:
  std::FILE *m_file = nullptr;
};
} // namespace

bool writePng(const std::string &path, const Bitmap &bitmap) {
  if (bitmap.width() <= 0 || bitmap.height() <= 0) {
    return false;
  }

  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    throw std::runtime_error("png_create_write_struct failed");
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, nullptr);
    throw std::runtime_error("png_create_info_struct failed");
  }

  try {
    FileHandle file(path, "wb");
    if (setjmp(png_jmpbuf(png))) {
      png_destroy_write_struct(&png, &info);
      return false;
    }
    png_init_io(png, file.get());

    png_set_IHDR(png, info, static_cast<png_uint_32>(bitmap.width()),
                 static_cast<png_uint_32>(bitmap.height()), 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    std::vector<png_bytep> rows(static_cast<std::size_t>(bitmap.height()));
    const auto rowBytes =
        static_cast<png_size_t>(bitmap.width()) * sizeof(uint8_t) * 4U;
    for (int y = 0; y < bitmap.height(); ++y) {
      rows[static_cast<std::size_t>(y)] =
          const_cast<png_bytep>(bitmap.rgba().data()) +
          static_cast<std::size_t>(y) * rowBytes;
    }

    png_set_rows(png, info, rows.data());
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);
    png_destroy_write_struct(&png, &info);
    return true;
  } catch (...) {
    png_destroy_write_struct(&png, &info);
    throw;
  }
}
