#include "gtest/gtest.h"

#include "common/data_types.h"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(output) = "xml:test_results.xml";
    return RUN_ALL_TESTS();
}
