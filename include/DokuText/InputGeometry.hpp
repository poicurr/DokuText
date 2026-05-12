// This file is part of the public API.
#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <DokuText/Layout.hpp>

namespace doku::text {

struct PreparedTextLine;

[[nodiscard]] std::span<const TextCaretBoundary>
caretBoundaries(const TextLineLayout &line);

[[nodiscard]] std::span<const TextCaretBoundary>
caretBoundaries(const PreparedTextLine &line);

[[nodiscard]] TextHitTestResult hitTestTextLine(const TextLineLayout &line,
                                                float x);

[[nodiscard]] TextHitTestResult hitTestTextLine(const PreparedTextLine &line,
                                                float x);

[[nodiscard]] TextHitTestResult caretForByteOffset(const TextLineLayout &line,
                                                   std::size_t byteOffset);

[[nodiscard]] TextHitTestResult caretForByteOffset(const PreparedTextLine &line,
                                                   std::size_t byteOffset);

[[nodiscard]] float caretXForByteOffset(const TextLineLayout &line,
                                        std::size_t byteOffset);

[[nodiscard]] float caretXForByteOffset(const PreparedTextLine &line,
                                        std::size_t byteOffset);

[[nodiscard]] std::vector<TextSelectionRect>
selectionRectsForByteRange(const TextLineLayout &line,
                           std::size_t beginByteOffset,
                           std::size_t endByteOffset);

[[nodiscard]] std::vector<TextSelectionRect>
selectionRectsForByteRange(const PreparedTextLine &line,
                           std::size_t beginByteOffset,
                           std::size_t endByteOffset);

} // namespace doku::text
