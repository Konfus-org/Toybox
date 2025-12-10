#include "pch.h"
#include "tbx/common/string.h"
#include <filesystem>
#include <ostream>

namespace tbx::tests::common
{
    struct StreamableExample
    {
        int id = 0;

        friend std::ostream& operator<<(std::ostream& os, const StreamableExample& example)
        {
            os << "example-" << example.id;
            return os;
        }
    };

    TEST(StringTests, TrimsWhitespaceFromBothEnds)
    {
        String value("  spaced text  ");

        auto trimmed = value.trim();

        EXPECT_EQ(trimmed.std_str(), "spaced text");
        EXPECT_EQ(value.std_str(), "  spaced text  ");
    }

    TEST(StringTests, RemovesAllWhitespace)
    {
        String value(" a\tb\nc d ");

        auto collapsed = value.remove_whitespace();

        EXPECT_EQ(collapsed.std_str(), "abcd");
    }

    TEST(StringTests, ConvertsToLowerAndUpper)
    {
        String value("MiXeD");

        auto lower = value.to_lower();
        auto upper = value.to_upper();

        EXPECT_EQ(lower.std_str(), "mixed");
        EXPECT_EQ(upper.std_str(), "MIXED");
    }

    TEST(StringTests, ChecksPrefixesSuffixesAndContainment)
    {
        String value("prefix-body-suffix");

        EXPECT_TRUE(value.starts_with("pre"));
        EXPECT_TRUE(value.contains("body"));
        EXPECT_TRUE(value.ends_with("suffix"));
        EXPECT_FALSE(value.contains("missing"));
    }

    TEST(StringTests, ReportsSizeAndEmptiness)
    {
        String empty_value("");
        String text_value("abc");

        EXPECT_TRUE(empty_value.empty());
        EXPECT_EQ(text_value.size(), 3U);
        EXPECT_FALSE(text_value.empty());
    }

    TEST(StringTests, ConvertsToFilepath)
    {
        String value("folder/file.txt");

        auto path = value.to_filepath();

        EXPECT_EQ(path, std::filesystem::path("folder/file.txt"));
    }

    TEST(StringTests, ConstructsFromStreamableType)
    {
        StreamableExample instance{42};

        auto wrapped = String::from(instance);

        EXPECT_EQ(wrapped.std_str(), "example-42");
    }

    TEST(StringTests, ProvidesCStringAccess)
    {
        String value("hello");

        EXPECT_STREQ(value.c_str(), "hello");
    }

    TEST(StringTests, SupportsIteration)
    {
        String value("abc");
        String collected;

        for (char c : value)
        {
            collected.push_back(c);
        }

        EXPECT_EQ(collected, "abc");
    }

    TEST(StringTests, SupportsConcatenationAndEquality)
    {
        String first("hello");
        String second("world");

        auto combined = first + String(" ") + second;

        EXPECT_EQ(combined.std_str(), "hello world");
        EXPECT_TRUE(combined == String("hello world"));
        EXPECT_TRUE(combined != first);
        EXPECT_STREQ(static_cast<const char*>(combined), "hello world");
    }
}
