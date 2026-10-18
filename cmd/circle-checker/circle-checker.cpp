#include "../../internal/circle-checker/service.hpp"
#include "../../internal/circle-checker/checker.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
    CircleCheckerRepo checkerImpl;
    CircleCheckerService service(checkerImpl);

    std::vector<std::string> circleXYRadius = {"0,0,5", "1,1,3", "2,2,4"};
    std::vector<std::string> categories = service.CircleCategories(circleXYRadius);

    // Output the categories (currently empty since CircleCategories returns an empty vector)
    for (const auto& category : categories) {
        std::cout << category << std::endl;
    }

    return 0;
}