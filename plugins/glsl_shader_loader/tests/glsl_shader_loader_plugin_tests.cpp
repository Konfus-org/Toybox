#include "pch.h"
#include "tbx/plugin_api/tests/importer_test_environment.h"
#include "../src/glsl_shader_loader_plugin.h"

namespace tbx::tests::plugin_api
{
    /// <summary>
    /// Verifies the GLSL importer expands include directives using in-memory files.
    /// </summary>
    TEST(importers, glsl_shader_loader_expands_includes_from_ifileops)
    {
        // Arrange
        auto working_directory = get_test_working_directory();
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_text(
            "Basic.vert",
            R"(#version 450
#include "Globals.glsl"
void main() {}
)");
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
}
