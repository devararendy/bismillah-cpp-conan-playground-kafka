#pragma once

#include <string>
#include <vector>

class CircleCheckerInterface {

public:
    virtual ~CircleCheckerInterface() = default;
    virtual std::vector<std::string> CircleCategories(const std::vector<std::string>& circleXYRadius) = 0;
};

class CircleCheckerService {
private:
    CircleCheckerInterface& circleCheckerImpl;

public:
    explicit CircleCheckerService(CircleCheckerInterface& circleCheckerImpl)
        : circleCheckerImpl(circleCheckerImpl) {}

    std::vector<std::string> CircleCategories(const std::vector<std::string>& circleXYRadius) {
        return circleCheckerImpl.CircleCategories(circleXYRadius);
    }
};