// Copyright 2022 Kuzmin Yaroslav

#include <stdexcept>

#include <gtest/gtest.h>

#include <suggestion.hpp>

TEST(Example, EmptyTest) {
    EXPECT_THROW(example(), std::runtime_error);
}
