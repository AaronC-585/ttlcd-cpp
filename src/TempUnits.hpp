#pragma once

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>

namespace TempUnits {

inline bool& use_fahrenheit_flag() {
    static bool fahrenheit = true;
    return fahrenheit;
}

inline void configure(const nlohmann::json& config = {}) {
    if (const char* override_unit = std::getenv("TTLCD_TEMP_UNIT")) {
        if (std::strcmp(override_unit, "F") == 0 || std::strcmp(override_unit, "f") == 0) {
            use_fahrenheit_flag() = true;
            return;
        }
        if (std::strcmp(override_unit, "C") == 0 || std::strcmp(override_unit, "c") == 0) {
            use_fahrenheit_flag() = false;
            return;
        }
    }

    if (config.contains("temp_unit")) {
        const std::string unit = config["temp_unit"].get<std::string>();
        use_fahrenheit_flag() = (unit == "F" || unit == "f");
        return;
    }

    use_fahrenheit_flag() = true;
}

inline bool use_fahrenheit() {
    return use_fahrenheit_flag();
}

inline double celsius_to_display(double celsius) {
    if (use_fahrenheit()) {
        return celsius * 9.0 / 5.0 + 32.0;
    }
    return celsius;
}

inline int round_display(double celsius) {
    return static_cast<int>(std::lround(celsius_to_display(celsius)));
}

inline const char* degree_suffix() {
    return use_fahrenheit() ? "°F" : "°C";
}

}  // namespace TempUnits
