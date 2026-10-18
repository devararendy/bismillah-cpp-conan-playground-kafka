#pragma once

#include "model.hpp"
#include <gmock/gmock.h>

class MockCircleChecker : public CircleCheckerInterface {
public:
    MOCK_METHOD(std::vector<circle_category::CircleCategory>, CircleCategories, (const std::vector<std::string>& circleXYRadius), (override));
};