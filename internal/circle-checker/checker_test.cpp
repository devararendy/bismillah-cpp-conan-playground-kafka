#include "checker.hpp"

#include <gtest/gtest.h>

TEST(CircleCheckerRepoTest, CircleCategoriesReturnsCorrectCategories) {
    CircleCheckerRepo checker;

    std::vector<std::string> input = {
        "0 0 1 3 0 1", // DisjointOutside
        "0 0 1 2 0 1", // Touching (external tangent)
        "0 0 1 0 0 1", // Concentric
        "0 0 2 1 0 2", // Intersecting
        "0 0 2 1 0 1", // Touching (internal tangent)
        "0 0 2 0 0 1", // DisjointInside
    };

    std::vector<CircleCategory> expected = {
        CircleCategory::DisjointOutside,
        CircleCategory::TouchingExternal,
        CircleCategory::Concentric,
        CircleCategory::Intersecting,
        CircleCategory::TouchingInternal,
        CircleCategory::DisjointInside,
    };

    auto result = checker.CircleCategories(input);
    EXPECT_EQ(result, expected);
}

TEST(CircleCheckerRepoTest, IgnoresInvalidInputPairs) {
    CircleCheckerRepo checker;

    std::vector<std::string> input = {
        "0 0 1 3 0 1", // valid, DisjointOutside
        "abc def ghi", // invalid
        "1 2 3 4 5", // invalid
        "0 0 1 0 0 1", // valid, Concentric
    };

    std::vector<CircleCategory> expected = {
        CircleCategory::DisjointOutside,
        CircleCategory::Concentric,
    };

    auto result = checker.CircleCategories(input);
    EXPECT_EQ(result, expected);
}

TEST(CircleCheckerRepoTest, DestructorRunsWhenReset) {
    std::unique_ptr<CircleCheckerRepo> checker = std::make_unique<CircleCheckerRepo>();
    checker.reset();
    SUCCEED();
}