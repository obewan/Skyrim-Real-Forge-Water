#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
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

    // Whether two surfaces lie in the same forge trough: close together, at about the same height.
    inline bool InSameTrough(const RE::NiBound& a_first, const RE::NiBound& a_second) {
        const float horizontal = std::hypot(a_first.center.x - a_second.center.x, a_first.center.y - a_second.center.y);
        return horizontal < 30.0f && std::abs(a_first.center.z - a_second.center.z) < 15.0f;
    }
}
