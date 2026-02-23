#pragma once

#include <string>

namespace Version {
    constexpr const char* BUILD_DATE = "February 22, 2026";
    constexpr int MAJOR = 1;
    constexpr int MINOR = 0;
    constexpr int PATCH = 1;

    inline std::string get_version() {
        return std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." + std::to_string(PATCH);
    }

    inline std::string get_full_info() {
        return "ttlcd-cpp v" + get_version() + " (built " + BUILD_DATE + ")";
    }
}
