#include "tbx/systems/debugging/logging.h"
#include <gtest/gtest.h>

namespace tbx::tests::file_system
{
    TEST(logging, resolves_logs_directory_from_process_executable_directory)
    {
        // Arrange
        const auto expected_directory =
            (get_process_executable_directory() / "logs").lexically_normal();

        // Act
        const auto logs_directory = Log::get_instance().get_logs_directory();

        // Assert
        EXPECT_EQ(logs_directory, expected_directory);
    }

    TEST(logging, reuses_the_same_default_logs_directory_across_calls)
    {
        // Arrange
        const auto expected_directory =
            (get_process_executable_directory() / "logs").lexically_normal();

        // Act
        const auto first_logs_directory = Log::get_instance().get_logs_directory();
        const auto second_logs_directory = Log::get_instance().get_logs_directory();

        // Assert
        EXPECT_EQ(first_logs_directory, expected_directory);
        EXPECT_EQ(second_logs_directory, expected_directory);
    }
}
