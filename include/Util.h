#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace Util {

    inline std::string ToLower(std::string_view a_text) {
        std::string result(a_text);
        std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    inline bool Contains(std::string_view a_text, std::string_view a_needle) {
        return ToLower(a_text).find(a_needle) != std::string::npos;
    }

    inline std::string FormatPoint(const RE::NiPoint3& a_point) {
        return std::format("({:.1f}, {:.1f}, {:.1f})", a_point.x, a_point.y, a_point.z);
    }
}
