#include "pch.h"
#include "../assimp_model_loader/src/assimp_model_loader_plugin.h"
#include "../glsl_shader_loader/src/glsl_shader_loader_plugin.h"
#include "../mat_material_loader/src/mat_material_loader_plugin.h"
#include "../stb_image_loader/src/stb_image_loader_plugin.h"
#include "tbx/async/cancellation_token.h"
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/files/file_ops.h"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx::tests::importers
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

        void set_text(std::filesystem::path path, std::string data)
        {
            _files[resolve(path)] = std::move(data);
        }

        void set_binary(std::filesystem::path path, const std::vector<unsigned char>& data)
        {
            auto encoded = std::string(
                reinterpret_cast<const char*>(data.data()),
                reinterpret_cast<const char*>(data.data() + data.size()));
            _files[resolve(path)] = std::move(encoded);
        }

      private:
        std::filesystem::path _working_directory = {};
        std::unordered_map<std::filesystem::path, std::string> _files = {};
    };

    class TestPluginHost final : public IPluginHost
    {
      public:
        explicit TestPluginHost(const std::filesystem::path& working_directory)
            : _asset_manager(working_directory, {}, {}, false)
            , _settings(_coordinator, true, GraphicsApi::OPEN_GL, {640, 480})
        {
            _settings.working_directory = working_directory;
            _settings.logs_directory = working_directory / "logs";
        }

        const std::string& get_name() const override
        {
            return _name;
        }

        AppSettings& get_settings() override
        {
            return _settings;
        }

        IMessageCoordinator& get_message_coordinator() override
        {
            return _coordinator;
        }

        EntityRegistry& get_entity_registry() override
        {
            return _registry;
        }

        AssetManager& get_asset_manager() override
        {
            return _asset_manager;
        }

      private:
        std::string _name = "ImporterTests";
        AppMessageCoordinator _coordinator = {};
        EntityRegistry _registry = {};
        AssetManager _asset_manager;
        AppSettings _settings;
    };

    /// <summary>
    /// Verifies the texture importer applies texture settings declared in sidecar metadata.
    /// </summary>
    TEST(importers, stb_image_loader_applies_texture_meta_settings)
    {
        // Arrange
        auto working_directory = std::filesystem::path("/virtual/assets");
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_binary(
            "test.png",
            std::vector<unsigned char> {
                0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00,
                0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                0x00, 0x01, 0x08, 0x04, 0x00, 0x00, 0x00, 0xb5, 0x1c, 0x0c, 0x02,
                0x00, 0x00, 0x00, 0x0b, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63,
                0xfc, 0xff, 0x1f, 0x00, 0x03, 0x03, 0x02, 0x00, 0xef, 0x6e, 0x27,
                0x9f, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
                0x60, 0x82});
        file_ops->set_text(
            "test.png.meta",
            "{\n"
            "  \"id\": \"77\",\n"
            "  \"texture\": {\n"
            "    \"wrap\": \"clamp_to_edge\",\n"
            "    \"filter\": \"nearest\",\n"
            "    \"format\": \"rgba\",\n"
            "    \"mipmaps\": \"disabled\",\n"
            "    \"compression\": \"auto\"\n"
            "  }\n"
            "}\n");
        plugins::StbImageLoaderPlugin plugin = {};
        plugin.set_file_ops(file_ops);
        plugin.on_attach(host);
        Texture texture = {};
        LoadTextureRequest request(
            "test.png",
            &texture,
            TextureWrap::REPEAT,
            TextureFilter::LINEAR,
            TextureFormat::RGB,
            TextureMipmaps::ENABLED,
            TextureCompression::DISABLED);

        // Act
        plugin.on_recieve_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::HANDLED);
        EXPECT_EQ(texture.wrap, TextureWrap::CLAMP_TO_EDGE);
        EXPECT_EQ(texture.filter, TextureFilter::NEAREST);
        EXPECT_EQ(texture.format, TextureFormat::RGBA);
        EXPECT_EQ(texture.mipmaps, TextureMipmaps::DISABLED);
        EXPECT_EQ(texture.compression, TextureCompression::AUTO);
    }

    /// <summary>
    /// Verifies the GLSL importer expands include directives using in-memory files.
    /// </summary>
    TEST(importers, glsl_shader_loader_expands_includes_from_ifileops)
    {
        // Arrange
        auto working_directory = std::filesystem::path("/virtual/assets");
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_text(
            "Basic.vert",
            "#version 450\n"
            "#include \"Globals.glsl\"\n"
            "void main() {}\n");
        file_ops->set_text("Globals.glsl", "vec3 make_color(){ return vec3(1.0); }\n");
        plugins::GlslShaderLoaderPlugin plugin = {};
        plugin.set_file_ops(file_ops);
        plugin.on_attach(host);
        Shader shader = {};
        LoadShaderRequest request("Basic.vert", &shader);

        // Act
        plugin.on_recieve_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::HANDLED);
        ASSERT_EQ(shader.sources.size(), 1U);
        EXPECT_EQ(shader.sources[0].type, ShaderType::VERTEX);
        EXPECT_NE(shader.sources[0].source.find("make_color"), std::string::npos);
    }

    /// <summary>
    /// Verifies the material importer parses handles and typed parameters from in-memory files.
    /// </summary>
    TEST(importers, mat_loader_parses_material_from_ifileops)
    {
        // Arrange
        auto working_directory = std::filesystem::path("/virtual/assets");
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_text(
            "Simple.mat",
            "{\n"
            "  \"shaders\": { \"vertex\": \"101\", \"fragment\": \"202\" },\n"
            "  \"textures\": [{ \"name\": \"diffuse\", \"value\": \"Texture_A\" }],\n"
            "  \"parameters\": [{ \"name\": \"roughness\", \"type\": \"float\", \"value\": 0.4 }]\n"
            "}\n");
        plugins::MatMaterialLoaderPlugin plugin = {};
        plugin.set_file_ops(file_ops);
        plugin.on_attach(host);
        Material material = {};
        LoadMaterialRequest request("Simple.mat", &material);

        // Act
        plugin.on_recieve_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::HANDLED);
        EXPECT_TRUE(material.program.vertex.is_valid());
        EXPECT_TRUE(material.program.fragment.is_valid());
        EXPECT_TRUE(material.textures.has("diffuse"));
        EXPECT_TRUE(material.parameters.has("roughness"));
    }

    /// <summary>
    /// Verifies the assimp importer short-circuits cancelled requests without file IO.
    /// </summary>
    TEST(importers, assimp_loader_honors_cancellation_before_import)
    {
        // Arrange
        auto working_directory = std::filesystem::path("/virtual/assets");
        TestPluginHost host = TestPluginHost(working_directory);
        plugins::AssimpModelLoaderPlugin plugin = {};
        plugin.on_attach(host);
        Model model = {};
        LoadModelRequest request("cancelled.fbx", &model);
        CancellationTokenSource cancellation = {};
        cancellation.cancel();
        request.cancellation_token = cancellation.token();

        // Act
        plugin.on_recieve_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::CANCELLED);
    }
}
