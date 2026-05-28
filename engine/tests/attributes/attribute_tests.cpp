#include "attribute_fixtures.h"
#include "tbx/systems/files/in_memory_file_ops.h"
#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <string>

namespace tbx
{
    TEST(AttributeTests, StructRoundtripsFieldsAndCustomNames)
    {
        // Arrange
        auto value = AttributeStruct();
        value.value = 7;
        value.renamed_value = 9;

        // Act
        const Json json = value;
        const auto roundtripped = json.get<AttributeStruct>();
        const auto registrations = get_serializable_type_registrations();

        // Assert
        EXPECT_EQ(json.at("value").get<int>(), 7);
        EXPECT_EQ(json.at("renamed").get<int>(), 9);
        EXPECT_EQ(roundtripped.value, 7);
        EXPECT_EQ(roundtripped.renamed_value, 9);
        EXPECT_TRUE(
            std::ranges::any_of(
                registrations,
                [](const SerializableTypeRegistration& entry)
                {
                    return entry.name == "attribute_struct";
                }));
    }

    TEST(AttributeTests, VersionAttributeCreatesVersionTrait)
    {
        // Arrange
        using Version = decltype(tbx_serialization_version(
            static_cast<const AttributeVersionedStruct*>(nullptr)));

        // Act
        constexpr uint32 version = Version::value;

        // Assert
        EXPECT_EQ(version, 7U);
    }

    TEST(AttributeTests, PrintableAttributeGeneratesFormatter)
    {
        // Arrange
        auto value = AttributePrintable();
        value.left = 1.0F;
        value.right = 2.0F;
        value.top = 3.0F;
        value.bottom = 4.0F;

        // Act
        const auto text = std::format("{}", value);

        // Assert
        EXPECT_EQ(text, "[Left: 1, Right: 2, Top: 3, Bottom: 4]");
    }

    TEST(AttributeTests, HashAttributeGeneratesStdHash)
    {
        // Arrange
        auto first = AttributeHash();
        first.name = "Player";
        first.id = 7;
        auto second = AttributeHash();
        second.name = "Player";
        second.id = 8;

        // Act
        const auto first_hash = std::hash<AttributeHash>()(first);
        const auto repeated_hash = std::hash<AttributeHash>()(first);
        const auto second_hash = std::hash<AttributeHash>()(second);

        // Assert
        EXPECT_EQ(first_hash, repeated_hash);
        EXPECT_NE(first_hash, second_hash);
    }

    TEST(AttributeTests, IndexedTypeRoundtripsAsArray)
    {
        // Arrange
        auto value = AttributeIndexed();
        value.values[0] = 1.0F;
        value.values[1] = 2.0F;
        value.values[2] = 3.0F;

        // Act
        const Json json = value;
        const auto roundtripped = json.get<AttributeIndexed>();

        // Assert
        ASSERT_TRUE(json.is_array());
        ASSERT_EQ(json.size(), 3U);
        EXPECT_EQ(roundtripped.values[0], 1.0F);
        EXPECT_EQ(roundtripped.values[1], 2.0F);
        EXPECT_EQ(roundtripped.values[2], 3.0F);
    }

    TEST(AttributeTests, AssetBodyLoadsFromRegisteredSerialization)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual");
        file_ops->set_text("asset.tbx.meta", R"({ "version": 1 })");
        file_ops->set_text("asset.tbx", R"({ "value": 42 })");
        auto registry = SerializationRegistry(file_ops);

        // Act
        const auto read = registry.read_result<AttributeAsset>("asset.tbx");

        // Assert
        ASSERT_TRUE(read.result.succeeded());
        ASSERT_NE(read.asset, nullptr);
        EXPECT_EQ(read.asset->value, 42);
        EXPECT_EQ(read.asset->version, 1U);
    }

    TEST(AttributeTests, AssetMetaLoadsWithoutBody)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual");
        file_ops->set_text("asset.tbx.meta", R"({ "version": 2, "import_version": 33 })");
        auto registry = SerializationRegistry(file_ops);

        // Act
        const auto read = registry.read_result<AttributeMetaAsset>("asset.tbx");

        // Assert
        ASSERT_TRUE(read.result.succeeded());
        ASSERT_NE(read.asset, nullptr);
        EXPECT_EQ(read.asset->import_version, 33);
        EXPECT_EQ(read.asset->version, 2U);
    }

    TEST(AttributeTests, AssetBodyAndMetaLoadTogether)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual");
        file_ops->set_text("asset.tbx.meta", R"({ "version": 3, "import_version": 44 })");
        file_ops->set_text("asset.tbx", R"({ "value": 12 })");
        auto registry = SerializationRegistry(file_ops);

        // Act
        const auto read = registry.read_result<AttributeBodyMetaAsset>("asset.tbx");

        // Assert
        ASSERT_TRUE(read.result.succeeded());
        ASSERT_NE(read.asset, nullptr);
        EXPECT_EQ(read.asset->value, 12);
        EXPECT_EQ(read.asset->import_version, 44);
        EXPECT_EQ(read.asset->version, 3U);
    }

    TEST(AttributeTests, TextAssetReadsAndWritesBodyText)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual");
        file_ops->set_text("script.tbx", "print('hello')");
        auto registry = SerializationRegistry(file_ops);

        // Act
        const auto read = registry.read_result<AttributeTextAsset>("script.tbx");
        auto written = AttributeTextAsset();
        written.source = "print('goodbye')";
        const auto write_result = registry.write("written.tbx", written);
        auto written_text = std::string();
        const bool loaded_written_text =
            file_ops->read_file("written.tbx", FileDataFormat::UTF8_TEXT, written_text);

        // Assert
        ASSERT_TRUE(read.result.succeeded());
        ASSERT_NE(read.asset, nullptr);
        EXPECT_EQ(read.asset->source, "print('hello')");
        EXPECT_TRUE(write_result.succeeded());
        EXPECT_TRUE(loaded_written_text);
        EXPECT_EQ(written_text, "print('goodbye')");
    }

    TEST(AttributeTests, CustomStructUsesSerializerSpecialization)
    {
        // Arrange
        auto value = AttributeCustomStruct();
        value.value = 64;

        // Act
        const Json json = value;
        const auto roundtripped = json.get<AttributeCustomStruct>();

        // Assert
        EXPECT_EQ(json.at("value").get<int>(), 64);
        EXPECT_EQ(roundtripped.value, 64);
    }

    TEST(AttributeTests, CustomAssetUsesSerializerSpecialization)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual");
        file_ops->set_text("asset.tbx.meta", R"({ "version": 5 })");
        file_ops->set_text("asset.tbx", R"({ "value": 77 })");
        auto registry = SerializationRegistry(file_ops);

        // Act
        const auto read = registry.read_result<AttributeCustomAsset>("asset.tbx");

        // Assert
        ASSERT_TRUE(read.result.succeeded());
        ASSERT_NE(read.asset, nullptr);
        EXPECT_EQ(read.asset->value, 77);
        EXPECT_EQ(read.asset->version, 5U);
    }

    TEST(AttributeTests, EnumUsesAttributedNames)
    {
        // Arrange
        const auto value = AttributeEnum::SECOND;

        // Act
        const Json json = value;
        const auto roundtripped = json.get<AttributeEnum>();

        // Assert
        EXPECT_EQ(json.get<std::string>(), "second");
        EXPECT_EQ(roundtripped, AttributeEnum::SECOND);
    }

    TEST(AttributeTests, VariantRoundtripsWithTaggedObject)
    {
        // Arrange
        auto value = AttributeVariant(8);

        // Act
        auto json = Json();
        to_json(json, value);
        auto roundtripped = AttributeVariant();
        from_json(json, roundtripped);

        // Assert
        EXPECT_EQ(json.at("type").get<std::string>(), "int");
        EXPECT_EQ(json.at("value").get<int>(), 8);
        ASSERT_TRUE(std::holds_alternative<int>(roundtripped));
        EXPECT_EQ(std::get<int>(roundtripped), 8);
    }
}
