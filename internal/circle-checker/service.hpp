#pragma once

#include <string>
#include <vector>
#include "model.hpp"
class CircleCheckerService {
private:
    std::shared_ptr<CircleCheckerInterface> circleCheckerImpl;

public:
    explicit CircleCheckerService(std::shared_ptr<CircleCheckerInterface> circleCheckerImpl)
        : circleCheckerImpl(circleCheckerImpl) {}

    std::vector<std::string> CircleCategories(const std::vector<std::string>& circleXYRadius) {
        const auto categories = circleCheckerImpl->CircleCategories(circleXYRadius);
        std::vector<std::string> categoryNames;
        categoryNames.reserve(categories.size());

        for (const auto category : categories) {
            categoryNames.emplace_back(to_string(category));
        }

        return categoryNames;
    }
};