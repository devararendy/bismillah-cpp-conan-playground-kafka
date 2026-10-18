#include "checker_mock.hpp"
#include "service.hpp"
#include <gtest/gtest.h>

TEST(CircleCheckerService, CircleCategoriesSuccess) {
    auto mockChecker = std::make_shared<MockCircleChecker>();
    CircleCheckerService service(mockChecker);

    std::vector<std::string> input = {"0 0 1 3 0 1", "0 0 1 2 0 1"};
    std::vector<circle_category::CircleCategory> expectedCategories = {
        circle_category::CircleCategory::DisjointOutside,
        circle_category::CircleCategory::TouchingExternal
    };

    EXPECT_CALL(*mockChecker, CircleCategories(input))
        .Times(1)
        .WillOnce(::testing::Return(expectedCategories));

    auto result = service.CircleCategories(input);
    EXPECT_EQ(result.size(), expectedCategories.size());
    EXPECT_EQ(result[0], "DisjointOutside");
    EXPECT_EQ(result[1], "TouchingExternal");
}