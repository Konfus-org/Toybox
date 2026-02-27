#include "../src/glsl_shader_loader_plugin.h"
#include "pch.h"
#include "tbx/plugin_api/tests/importer_test_environment.h"

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
        glsl_shader_loader::GlslShaderLoaderPlugin plugin = {};
        plugin.set_file_ops(file_ops);
        plugin.attach(host);
        Shader shader = {};
        LoadShaderRequest request("Basic.vert", &shader);

        // Act
        plugin.receive_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::HANDLED);
        ASSERT_EQ(shader.sources.size(), 1U);
        EXPECT_EQ(shader.sources[0].type, ShaderType::VERTEX);
        EXPECT_NE(shader.sources[0].source.find("make_color"), std::string::npos);
    }

    /// <summary>
    /// Verifies the GLSL importer supports compute stages and include expansion.
    /// </summary>
    TEST(importers, glsl_shader_loader_loads_compute_stage_with_includes)
    {
        // Arrange
        auto working_directory = get_test_working_directory();
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_text(
            "Culling.comp",
            R"(#version 450
#include "Common.glsl"
void main() { uint x = make_index(); }
)");
        file_ops->set_text("Common.glsl", "uint make_index(){ return 0u; }\n");
        glsl_shader_loader::GlslShaderLoaderPlugin plugin = {};
        plugin.set_file_ops(file_ops);
        plugin.attach(host);
        Shader shader = {};
        LoadShaderRequest request("Culling.comp", &shader);

        // Act
        plugin.receive_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::HANDLED);
        ASSERT_EQ(shader.sources.size(), 1U);
        EXPECT_EQ(shader.sources[0].type, ShaderType::COMPUTE);
        EXPECT_NE(shader.sources[0].source.find("make_index"), std::string::npos);
    }
}
