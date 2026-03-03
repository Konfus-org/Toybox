#include "pch.h"
#include "tbx/plugin_api/tests/importer_test_environment.h"
#include "tbx/plugins/stb_image_loader/stb_image_loader_plugin.h"

namespace stb_image_loader::tests
{
    /// <summary>
    /// Verifies the texture importer applies texture settings declared in sidecar metadata.
    /// </summary>
    TEST(importers, stb_image_loader_applies_texture_meta_settings)
    {
        // Arrange
        auto working_directory = tbx::tests::plugin_api::get_test_working_directory();
        tbx::tests::plugin_api::TestPluginHost host = tbx::tests::plugin_api::TestPluginHost(working_directory);
        auto file_ops = std::make_shared<tbx::tests::plugin_api::InMemoryFileOps>(working_directory);
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
            R"({
  "id": "77",
  "texture": {
    "wrap": "clamp_to_edge",
    "filter": "nearest",
    "format": "rgba",
    "mipmaps": "disabled",
    "compression": "auto"
  }
}
)");
        stb_image_loader::StbImageLoaderPlugin plugin = {};
        plugin.set_file_ops(file_ops);
        plugin.on_attach(host);
        tbx::Texture texture = {};
        tbx::LoadTextureRequest request(
            "test.png",
            &texture,
            tbx::TextureWrap::REPEAT,
            tbx::TextureFilter::LINEAR,
            tbx::TextureFormat::RGB,
            tbx::TextureMipmaps::ENABLED,
            tbx::TextureCompression::DISABLED);

        // Act
        plugin.on_recieve_message(request);

        // Assert
        EXPECT_EQ(request.state, tbx::MessageState::HANDLED);
        EXPECT_EQ(texture.wrap, tbx::TextureWrap::CLAMP_TO_EDGE);
        EXPECT_EQ(texture.filter, tbx::TextureFilter::NEAREST);
        EXPECT_EQ(texture.format, tbx::TextureFormat::RGBA);
        EXPECT_EQ(texture.mipmaps, tbx::TextureMipmaps::DISABLED);
        EXPECT_EQ(texture.compression, tbx::TextureCompression::AUTO);
    }
}
