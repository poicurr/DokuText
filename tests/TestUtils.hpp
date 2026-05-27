#pragma once

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace doku_test {

inline const std::vector<std::string> kRequiredTestFonts = {
    "fonts/Moralerspace_v2.0.0/MoralerspaceKrypton-Regular.ttf",
    "fonts/Noto_Color_Emoji/NotoColorEmoji-Regular.ttf",
};

[[nodiscard]] inline bool testFontsAvailable() {
    for (const auto& path : kRequiredTestFonts) {
        if (!std::filesystem::exists(path)) {
            return false;
        }
    }
    return true;
}

inline void skipIfTestFontsMissing() {
    if (!testFontsAvailable()) {
        std::cout << "SKIPPED: Required test fonts not found under fonts/.\n"
                  << "         See README.md section 'Test Fonts' for download instructions.\n";
        std::exit(77);  // CTest SKIP_RETURN_CODE
    }
}

} // namespace doku_test
