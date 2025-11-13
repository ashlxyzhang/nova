#include <gtest/gtest.h>
#include "../src/EventData.hh"
#include <iostream>


TEST(YES, NO)
{
    EXPECT_STRNE("hello", "world");
}