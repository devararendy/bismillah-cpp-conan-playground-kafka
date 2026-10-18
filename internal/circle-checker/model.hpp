#pragma once

#include <string>
#include <vector>
#include <string_view>

enum class CircleCategory : uint8_t {
    Undefined,
    TouchingExternal,
    TouchingInternal,
    DisjointOutside,
    Concentric,
    Intersecting,
    DisjointInside,
};

constexpr std::string_view to_string(CircleCategory category) {
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

class CircleCheckerInterface {

public:
    virtual ~CircleCheckerInterface() = default;
    virtual std::vector<CircleCategory> CircleCategories(const std::vector<std::string>& circleXYRadius) = 0;
};