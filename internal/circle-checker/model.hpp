#pragma once

#include <string>
#include <vector>
#include <string_view>

enum class CircleCategory {
    Undefined,
    Touching,
    DisjointOutside,
    Concentric,
    Intersecting,
    DisjointInside,
};

constexpr std::string_view to_string(CircleCategory category) {
    switch (category) {
        case CircleCategory::Touching: return "Touching";
        case CircleCategory::DisjointOutside: return "DisjointOutside";
        case CircleCategory::Concentric: return "Concentric";
        case CircleCategory::Intersecting: return "Intersecting";
        case CircleCategory::DisjointInside: return "DisjointInside";
        case CircleCategory::Undefined: return "Undefined";
    }
    return "Unknown";
}

class CircleCheckerInterface {

public:
    virtual ~CircleCheckerInterface() = default;
    virtual std::vector<CircleCategory> CircleCategories(const std::vector<std::string>& circleXYRadius) = 0;
};