#include "serialization/serializers_internal.h"
#include "tbx/assets/asset.h"
#include "tbx/files/files.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/serialization/json.h"
#include "tbx/serialization/read_write.h"
#include "tbx/serialization/registration.h"
#include "tbx/serialization/serializers.h"
#include <filesystem>
#include <gtest/gtest.h>

namespace tbx
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
        // Types register once — .field() appends, so a second pass would duplicate fields. Guard
        // on the type table (never on the dangling SerializerSlot after a purge) so this stays
        // safe and repeatable across the purge tests below.
        if (!get_type_registry().find("DiskThing"))
        {
            register_type<DiskThing>("DiskThing")
                .field("answer", &DiskThing::answer)
                .field("label", &DiskThing::label);
            register_type<NoteThing>("NoteThing")
                .field("author", &NoteThing::author);
        }
        // Serializers re-register whenever they're missing (after purge_serialization_registry),
        // which also re-points the SerializerSlot<T>::info pointers.
        if (!get_serializer_registry().find(typeid(DiskThing).hash_code()))
        {
            register_serializer<DiskThing>()
                .format(SerializerFormat::DEFAULT)
                .meta(&DiskThing::label);
            register_serializer<NoteThing>()
                .format(SerializerFormat::TEXT)
                .meta(&NoteThing::author);
            register_serializer<BlobThing>()
                .format(SerializerFormat::CUSTOM)
                .deserializer(deserialize_blob_thing);
        }
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
        const auto written = serialize(original, path);
        const auto loaded = deserialize<DiskThing>(path);

        // Assert: both fields round-trip, and the meta property lives in the sidecar file,
        // not the payload.
        ASSERT_TRUE(written.has_value()) << written.error();
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(loaded->answer, 42);
        EXPECT_EQ(loaded->label, "important");
        const auto payload = Json::parse(*read_text(path));
        EXPECT_TRUE(payload.contains("answer"));
        EXPECT_FALSE(payload.contains("label"));
        const auto sidecar =
            Json::parse(*read_text(path.string() + ".meta"));
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
        ASSERT_TRUE(serialize(original, path).has_value());

        // Assert: the meta property landed WITHOUT clobbering the identity fields.
        const auto sidecar =
            Json::parse(*read_text(path.string() + ".meta"));
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
        const auto written = serialize(original, path);
        const auto loaded = deserialize<NoteThing>(path);

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
        const auto loaded = deserialize<BlobThing>(path);

        // Assert: the reader runs; the missing writer asserts (debug) or fails (release).
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(loaded->value, 7);
#ifdef TBX_ASSERTS_ENABLED
        EXPECT_DEATH((void)serialize(BlobThing {.value = 1}, path), "");
#else
        const auto written = serialize(BlobThing {.value = 1}, path);
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
        const auto loaded = deserialize<GhostThing>(path);
        const auto written = serialize(GhostThing(), path);

        // Assert
        ASSERT_FALSE(loaded.has_value());
        ASSERT_FALSE(written.has_value());
        EXPECT_NE(loaded.error().find("register_serializer"), std::string::npos);
        EXPECT_NE(written.error().find("register_serializer"), std::string::npos);
    }

    TEST(Serialization, PurgeEmptiesTheRegistryAndReInitRestoresIt)
    {
        // Arrange: builtins present.
        internal::register_builtin_serializers();
        ASSERT_TRUE(is_serialization_ready());

        // Act + Assert: purge empties it, re-init refills it.
        internal::purge_serialization_registry();
        EXPECT_FALSE(is_serialization_ready());
        internal::register_builtin_serializers();
        EXPECT_TRUE(is_serialization_ready());

        // Restore the custom test serializers a purge dropped, so order-independent siblings still
        // find them (the helper re-points SerializerSlot<T>::info as it re-registers).
        register_serialization_test_types();
        EXPECT_TRUE(get_serializer_registry().find(typeid(DiskThing).hash_code()).has_value());
    }
}
