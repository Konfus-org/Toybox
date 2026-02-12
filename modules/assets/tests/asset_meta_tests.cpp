#include "pch.h"
#include "tbx/assets/asset_meta.h"
#include <filesystem>
#include <unordered_map>

namespace tbx::tests::assets
{
    class InMemoryFileOps final : public IFileOps
    {
      public:
        explicit InMemoryFileOps(std::filesystem::path working_directory)
            : _working_directory(std::move(working_directory))
        {
        }

        std::filesystem::path get_working_directory() const override
        {
            return _working_directory;
        }

        std::filesystem::path resolve(const std::filesystem::path& path) const override
        {
            if (path.is_absolute())
                return path.lexically_normal();
            return (_working_directory / path).lexically_normal();
        }

        bool exists(const std::filesystem::path& path) const override
        {
            return _files.contains(resolve(path));
        }

        FileType get_type(const std::filesystem::path& path) const override
        {
            if (_files.contains(resolve(path)))
                return FileType::FILE;
            return FileType::NONE;
        }

        std::vector<std::filesystem::path> read_directory(const std::filesystem::path&) const override
        {
            return {};
        }

        bool read_file(
            const std::filesystem::path& path,
            FileDataFormat,
            std::string& out_data) const override
        {
            auto iterator = _files.find(resolve(path));
            if (iterator == _files.end())
                return false;
            out_data = iterator->second;
            return true;
        }

        bool write_file(
            const std::filesystem::path& path,
            FileDataFormat,
            const std::string& data) override
        {
            _files[resolve(path)] = data;
            return true;
        }

      private:
        std::filesystem::path _working_directory;
        std::unordered_map<std::filesystem::path, std::string> _files = {};
    };

    /// <summary>
    /// Verifies metadata parsing reads id and explicit name values.
    /// </summary>
    TEST(asset_meta_parser, reads_name_and_id_from_json)
    {
        // Arrange
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/hero.png");
        constexpr const char* meta_text = R"JSON({ "id": "1a2b", "name": "Hero" })JSON";

        // Act
        ASSERT_TRUE(parser.try_parse_from_source(meta_text, asset_path, meta));

        // Assert
        EXPECT_EQ(meta.asset_path, asset_path);
        EXPECT_EQ(meta.name, "Hero");
        EXPECT_EQ(meta.id.value, 0x1a2bU);
    }

    /// <summary>
    /// Verifies missing names fall back to the source filename stem.
    /// </summary>
    TEST(asset_meta_parser, falls_back_to_stem_name)
    {
        // Arrange
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/rock.tbx");
        constexpr const char* meta_text = R"JSON({ "id": "10" })JSON";

        // Act
        ASSERT_TRUE(parser.try_parse_from_source(meta_text, asset_path, meta));

        // Assert
        EXPECT_EQ(meta.name, "rock");
        EXPECT_EQ(meta.id.value, 0x10U);
    }

    /// <summary>
    /// Verifies texture settings deserialize from metadata sidecars.
    /// </summary>
    TEST(asset_meta_parser, parses_meta_text)
    {
        // Arrange
        constexpr const char* meta_text = R"JSON({ "id": "a1", "name": "Crystal" })JSON";
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/crystal.asset");

        // Act
        ASSERT_TRUE(parser.try_parse_from_source(meta_text, asset_path, meta));

        // Assert
        EXPECT_EQ(meta.asset_path, asset_path);
        EXPECT_EQ(meta.name, "Crystal");
        EXPECT_EQ(meta.id.value, 0xA1U);
    }

    /// <summary>
    /// Verifies invalid JSON input is rejected.
    /// </summary>
    TEST(asset_meta_parser, rejects_invalid_json)
    {
        // Arrange
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/broken.asset");

        // Act / Assert
        EXPECT_FALSE(parser.try_parse_from_source("{ invalid json }", asset_path, meta));
    }

    /// <summary>
    /// Verifies texture sidecar settings are parsed from metadata JSON.
    /// </summary>
    TEST(asset_meta_parser, parses_texture_settings)
    {
        // Arrange
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/diffuse.png");
        constexpr const char* meta_text = R"JSON(
            {
              "id": "55",
              "texture": {
                "wrap": "clamp_to_edge",
                "filter": "nearest",
                "format": "rgba",
                "mipmaps": "disabled",
                "compression": "auto"
              }
            })JSON";

        // Act
        ASSERT_TRUE(parser.try_parse_from_source(meta_text, asset_path, meta));

        // Assert
        ASSERT_TRUE(meta.texture_settings.has_value());
        EXPECT_EQ(meta.texture_settings->wrap, TextureWrap::CLAMP_TO_EDGE);
        EXPECT_EQ(meta.texture_settings->filter, TextureFilter::NEAREST);
        EXPECT_EQ(meta.texture_settings->format, TextureFormat::RGBA);
        EXPECT_EQ(meta.texture_settings->mipmaps, TextureMipmaps::DISABLED);
        EXPECT_EQ(meta.texture_settings->compression, TextureCompression::AUTO);
    }

    /// <summary>
    /// Verifies metadata can be parsed from in-memory file operations.
    /// </summary>
    TEST(asset_meta_parser, parses_from_ifileops_without_disk_io)
    {
        // Arrange
        AssetMetaParser parser;
        InMemoryFileOps file_ops("/virtual/assets");
        auto asset_path = std::filesystem::path("wood.png");
        std::string meta_text = "{ \"id\": \"99\" }\n";
        ASSERT_TRUE(file_ops.write_file("wood.png.meta", FileDataFormat::UTF8_TEXT, meta_text));
        AssetMeta meta = {};

        // Act
        auto succeeded = parser.try_parse_from_disk(file_ops, asset_path, meta);

        // Assert
        EXPECT_TRUE(succeeded);
        EXPECT_EQ(meta.id.value, 0x99U);
    }

    /// <summary>
    /// Verifies texture settings serialize into metadata sidecar text.
    /// </summary>
    TEST(asset_meta_parser, serializes_texture_settings)
    {
        // Arrange
        AssetMetaParser parser;
        TextureSettings settings = {};
        settings.wrap = TextureWrap::MIRRORED_REPEAT;
        settings.filter = TextureFilter::LINEAR;
        settings.format = TextureFormat::RGB;
        settings.mipmaps = TextureMipmaps::ENABLED;
        settings.compression = TextureCompression::DISABLED;
        AssetMeta meta = {
            .asset_path = "/virtual/assets/ui.png",
            .id = Uuid(0x33U),
            .name = "ui",
            .texture_settings = settings,
        };

        // Act
        auto encoded = parser.serialize_to_source(meta);

        // Assert
        EXPECT_NE(encoded.find("\"texture\""), std::string::npos);
        EXPECT_NE(encoded.find("\"wrap\": \"mirrored_repeat\""), std::string::npos);
        EXPECT_NE(encoded.find("\"format\": \"rgb\""), std::string::npos);
    }
}
