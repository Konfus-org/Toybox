#include "pch.h"
#include "tbx/assets/asset_handle_serializer.h"
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
    /// Verifies handle serializer reads id and path-derived name from in-memory metadata.
    /// </summary>
    TEST(asset_handle_serializer, reads_handle_from_ifileops_without_disk_io)
    {
        // Arrange
        AssetHandleSerializer parser;
        InMemoryFileOps file_ops("/virtual/assets");
        auto asset_path = std::filesystem::path("wood.png");
        std::string meta_text = "{ \"id\": \"99\" }\n";
        ASSERT_TRUE(file_ops.write_file("wood.png.meta", FileDataFormat::UTF8_TEXT, meta_text));

        // Act
        auto handle = parser.read_from_disk(file_ops, asset_path);

        // Assert
        ASSERT_NE(handle, nullptr);
        EXPECT_EQ(handle->id.value, 0x99U);
        EXPECT_EQ(handle->name, "wood.png");
    }

    /// <summary>
    /// Verifies serializer only updates the id while preserving existing custom metadata fields.
    /// </summary>
    TEST(asset_handle_serializer, writes_only_id_field_in_existing_meta)
    {
        // Arrange
        AssetHandleSerializer serializer;
        InMemoryFileOps file_ops("/virtual/assets");
        auto asset_path = std::filesystem::path("stone.png");
        std::string original_meta = R"JSON(
            {
              "name": "Stone",
              "texture": { "wrap": "repeat" },
              "id": "1"
            }
            )JSON";
        ASSERT_TRUE(file_ops.write_file("stone.png.meta", FileDataFormat::UTF8_TEXT, original_meta));
        auto updated_handle = Handle(Uuid(0xABU));

        // Act
        auto write_succeeded = serializer.try_write_to_disk(file_ops, asset_path, updated_handle);

        // Assert
        ASSERT_TRUE(write_succeeded);
        auto encoded = std::string();
        ASSERT_TRUE(file_ops.read_file("stone.png.meta", FileDataFormat::UTF8_TEXT, encoded));
        EXPECT_NE(encoded.find("\"id\": \"ab\""), std::string::npos);
        EXPECT_NE(encoded.find("\"name\": \"Stone\""), std::string::npos);
        EXPECT_NE(encoded.find("\"texture\""), std::string::npos);
    }
}
