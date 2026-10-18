#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

class CircleCheckerInterface {

public:
    virtual ~CircleCheckerInterface() = default;
    virtual std::vector<std::string> CircleCategories(const std::vector<std::string>& circleXYRadius) = 0;
};

class CircleCheckerService {
private:
    std::shared_ptr<CircleCheckerInterface> circleCheckerImpl;

public:
    CircleCheckerService(
        std::shared_ptr<CircleCheckerInterface> circleCheckerImpl
    ) : circleCheckerImpl(circleCheckerImpl) {}

    std::vector<std::string> CircleCategories(const std::vector<std::string>& circleXYRadius) {
        return circleCheckerImpl->CircleCategories(circleXYRadius);
    }
};