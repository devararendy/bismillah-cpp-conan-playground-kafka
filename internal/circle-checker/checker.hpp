#pragma once

#include "model.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

class CircleCheckerRepo : public CircleCheckerInterface {

public:
    ~CircleCheckerRepo() override = default;
    std::vector<CircleCategory> CircleCategories(const std::vector<std::string>& pairCirclesXYRadius) override {
        std::vector<CircleCategory> resultCategories;

        for (const auto& circlePair : pairCirclesXYRadius) {
            std::cout << "CircleCheckerRepo::CircleCategories called with input: " << circlePair << std::endl;
    
            std::stringstream ss(circlePair);
            double x1, y1, r1, x2, y2, r2;
            ss >> x1 >> y1 >> r1 >> x2 >> y2 >> r2;
            // if (!(ss >> x1 >> y1 >> r1 >> x2 >> y2 >> r2)) {
            //     std::cerr << "Error: Invalid input format for circle pair: " << circlePair << std::endl;
            //     continue;  // Skip this pair and continue with the next
            // }

            std::cout << "Circle 1: (" << x1 << ", " << y1 << "), Radius: " << r1 << std::endl;
            std::cout << "Circle 2: (" << x2 << ", " << y2 << "), Radius: " << r2 << std::endl;

            // Distance between centers
            double sq = ((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1));
            double dist = sqrt(sq);
            std::cout << "Distance between centers: " << dist << std::endl;

            if (dist == (r1 + r2)) {
                std::cout << "Circles are touching." << std::endl;
                resultCategories.push_back(CircleCategory::Touching);
            } else if (dist > (r1 + r2)) {
                std::cout << "Circles are disjoint outside." << std::endl;
                resultCategories.push_back(CircleCategory::DisjointOutside);
            } else if (dist == 0 && r1 == r2) {
                std::cout << "Circles are concentric." << std::endl;
                resultCategories.push_back(CircleCategory::Concentric);
            } else if (dist < (r1 + r2) && dist > abs(r1 - r2)) {
                std::cout << "Circles are intersecting." << std::endl;
                resultCategories.push_back(CircleCategory::Intersecting);
            } else if (dist < abs(r1 - r2)) {
                std::cout << "One circle is inside the other." << std::endl;
                resultCategories.push_back(CircleCategory::DisjointInside);
            } else {
                std::cout << "Undefined relationship." << std::endl;
                resultCategories.push_back(CircleCategory::Undefined);
            }
        }
        std::cout << std::endl;
        return resultCategories;  // Return the vector with all determined categories
    }
};