#include "tbx/assets/asset.h"
#include "tbx/files/files.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/serialization/json.h"
#include "tbx/serialization/read_write.h"
#include "tbx/serialization/registration.h"
#include <filesystem>
#include <gtest/gtest.h>

namespace tbx::tests
{
    /// @brief
    /// Purpose: A plain data asset for the DEFAULT format, with one property routed to the
    /// .meta sidecar.
    struct DiskThing : Asset
    {
        int answer = 0;
        std::string label = {};
    };

    /// @brief
    /// Purpose: A text-payload type for the TEXT format: the file IS `text`, `author` rides
    /// in the sidecar.
    struct NoteThing
    {
        std::string text = {};
        std::string author = {};
    };

    /// @brief
    /// Purpose: A CUSTOM type registered reader-only — write must refuse.
    struct BlobThing
    {
        int value = 0;
    };

    /// @brief
    /// Purpose: A type nobody registers — both endpoints must point at register_serializer.
    struct GhostThing
    {
        int nothing = 0;
    };

    static Result<BlobThing> deserialize_blob_thing(const std::filesystem::path&)
    {
        return BlobThing {.value = 7};
    }

    static void register_serialization_test_types()
    {
        static bool g_registered = false;
        if (g_registered)
            return;
        g_registered = true;
        reflection::register_type<DiskThing>("DiskThing")
            .field("answer", &DiskThing::answer)
            .field("label", &DiskThing::label);
        serialization::register_serializer<DiskThing>()
            .format(serialization::Format::DEFAULT)
            .meta(&DiskThing::label);
        reflection::register_type<NoteThing>("NoteThing")
            .field("author", &NoteThing::author);
        serialization::register_serializer<NoteThing>()
            .format(serialization::Format::TEXT)
            .meta(&NoteThing::author);
        serialization::register_serializer<BlobThing>()
            .format(serialization::Format::CUSTOM)
            .deserializer(deserialize_blob_thing);
    }

    static std::filesystem::path test_root()
    {
        const auto* info = testing::UnitTest::GetInstance()->current_test_info();
        const auto root = std::filesystem::temp_directory_path() / "tbx_serialization_tests"
            / info->name();
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        return root;
    }

    TEST(Serialization, DefaultFormatRoundTripsWithMetaSidecar)
    {
        // Arrange
        register_serialization_test_types();
        const auto path = test_root() / "thing.json";
        auto original = DiskThing();
        original.answer = 42;
        original.label = "important";

        // Act
        const auto written = serialization::serialize(original, path);
        const auto loaded = serialization::deserialize<DiskThing>(path);

        // Assert: both fields round-trip, and the meta property lives in the sidecar file,
        // not the payload.
        ASSERT_TRUE(written.has_value()) << written.error();
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(loaded->answer, 42);
        EXPECT_EQ(loaded->label, "important");
        const auto payload = serialization::Json::parse(*read_text(path));
        EXPECT_TRUE(payload.contains("answer"));
        EXPECT_FALSE(payload.contains("label"));
        const auto sidecar =
            serialization::Json::parse(*read_text(path.string() + ".meta"));
        EXPECT_EQ(sidecar.value("label", std::string()), "important");
    }

    TEST(Serialization, MetaWritesMergeIntoTheIdentitySidecar)
    {
        // Arrange: the asset system minted an identity sidecar first.
        register_serialization_test_types();
        const auto path = test_root() / "thing.json";
        ASSERT_TRUE(write_text(
                        path.string() + ".meta",
                        R"({"id": "cafe0000000000000000000000000001", "version": 1, "type": ".json"})")
                        .has_value());
        auto original = DiskThing();
        original.label = "merged";

        // Act
        ASSERT_TRUE(serialization::serialize(original, path).has_value());

        // Assert: the meta property landed WITHOUT clobbering the identity fields.
        const auto sidecar =
            serialization::Json::parse(*read_text(path.string() + ".meta"));
        EXPECT_EQ(sidecar.value("label", std::string()), "merged");
        EXPECT_EQ(
            sidecar.value("id", std::string()),
            "cafe0000000000000000000000000001");
        EXPECT_EQ(sidecar.value("type", std::string()), ".json");
    }

    TEST(Serialization, TextFormatRoundTripsRawTextPlusMeta)
    {
        // Arrange
        register_serialization_test_types();
        const auto path = test_root() / "note.txt";
        auto original = NoteThing();
        original.text = "line one\nline two";
        original.author = "jer";

        // Act
        const auto written = serialization::serialize(original, path);
        const auto loaded = serialization::deserialize<NoteThing>(path);

        // Assert: the payload file is EXACTLY the text; the author rides in the sidecar.
        ASSERT_TRUE(written.has_value()) << written.error();
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(loaded->text, original.text);
        EXPECT_EQ(loaded->author, "jer");
        EXPECT_EQ(*read_text(path), original.text);
    }

    TEST(Serialization, CustomReaderOnlyTypeRefusesWrites)
    {
        // Arrange
        register_serialization_test_types();
        const auto path = test_root() / "blob.bin";

        // Act
        const auto loaded = serialization::deserialize<BlobThing>(path);

        // Assert: the reader runs; the missing writer asserts (debug) or fails (release).
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(loaded->value, 7);
#ifdef TBX_ASSERTS_ENABLED
        EXPECT_DEATH((void)serialization::serialize(BlobThing {.value = 1}, path), "");
#else
        const auto written = serialization::serialize(BlobThing {.value = 1}, path);
        ASSERT_FALSE(written.has_value());
        EXPECT_NE(written.error().find("no writer"), std::string::npos);
#endif
    }

    TEST(Serialization, UnregisteredTypesFailWithGuidance)
    {
        // Arrange
        register_serialization_test_types();
        const auto path = test_root() / "ghost.json";

        // Act
        const auto loaded = serialization::deserialize<GhostThing>(path);
        const auto written = serialization::serialize(GhostThing(), path);

        // Assert
        ASSERT_FALSE(loaded.has_value());
        ASSERT_FALSE(written.has_value());
        EXPECT_NE(loaded.error().find("register_serializer"), std::string::npos);
        EXPECT_NE(written.error().find("register_serializer"), std::string::npos);
    }
}
