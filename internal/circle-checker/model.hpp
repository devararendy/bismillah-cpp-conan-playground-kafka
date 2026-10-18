#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace circle_category {
    enum class CircleCategory : std::uint8_t {
        Undefined,
        TouchingExternal,
        TouchingInternal,
        DisjointOutside,
        Concentric,
        Intersecting,
        DisjointInside,
    };

    constexpr std::string_view to_string(CircleCategory category) noexcept {
        switch (category) {
            case CircleCategory::TouchingExternal: return "TouchingExternal";
            case CircleCategory::TouchingInternal: return "TouchingInternal";
            case CircleCategory::DisjointOutside: return "DisjointOutside";
            case CircleCategory::Concentric: return "Concentric";
            case CircleCategory::Intersecting: return "Intersecting";
            case CircleCategory::DisjointInside: return "DisjointInside";
            case CircleCategory::Undefined: return "Undefined";
        }
        return "Unknown";
    }
} // namespace circle_category

class CircleCheckerInterface {

public:
    virtual ~CircleCheckerInterface() = default;
    virtual std::vector<circle_category::CircleCategory> CircleCategories(const std::vector<std::string>& circleXYRadius) = 0;
};