#include "../../internal/circle-checker/service.hpp"
#include "../../internal/circle-checker/checker.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
    auto checkerImpl = std::make_shared<CircleCheckerRepo>();
    CircleCheckerService service(checkerImpl);

    std::vector<std::string> circleXYRadius = {
        "3 0 10 3 0 10", 
        "0 1 4 1 2 4",
    };
    std::vector<std::string> categories = service.CircleCategories(circleXYRadius);
    
    for (const auto& category : categories) {
        std::cout << category << std::endl;
    }

    return 0;
}