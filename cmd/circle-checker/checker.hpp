#pragma once

#include "service.hpp"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

class CircleCheckerRepo : public CircleCheckerInterface {

public:
    ~CircleCheckerRepo() override = default;
    std::vector<std::string> CircleCategories(const std::vector<std::string>& circleXYRadius) override {
        std::cout << "CircleCheckerRepo::CircleCategories called with input: ";
        for (const auto& circle : circleXYRadius) {
            std::cout << circle << " ";
        }
        std::cout << std::endl;
        return {};
    }
};