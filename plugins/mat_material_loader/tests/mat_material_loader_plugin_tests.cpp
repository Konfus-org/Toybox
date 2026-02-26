#include "pch.h"
#include "tbx/plugin_api/tests/importer_test_environment.h"
#include "../src/mat_material_loader_plugin.h"

namespace tbx::tests::plugin_api
{
    /// <summary>
    /// Verifies the material importer parses handles and typed parameters from in-memory files.
    /// </summary>
    TEST(importers, mat_loader_parses_material_from_ifileops)
    {
        // Arrange
        auto working_directory = get_test_working_directory();
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_text(
            "Simple.mat",
            R"({
  "shaders": { "vertex": "101", "fragment": "202" },
  "textures": [{ "name": "diffuse", "value": "Texture_A" }],
  "parameters": [{ "name": "roughness", "type": "float", "value": 0.4 }]
}
)");
        plugins::MatMaterialLoaderPlugin plugin = {};
        plugin.set_file_ops(file_ops);
        plugin.attach(host);
        Material material = {};
        LoadMaterialRequest request("Simple.mat", &material);

        // Act
        plugin.receive_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::HANDLED);
        EXPECT_TRUE(material.program.vertex.is_valid());
        EXPECT_TRUE(material.program.fragment.is_valid());
        EXPECT_TRUE(material.textures.has("diffuse"));
        EXPECT_TRUE(material.parameters.has("roughness"));
    }

    /// <summary>
    /// Verifies the material importer accepts compute-only shader programs.
    /// </summary>
    TEST(importers, mat_loader_parses_compute_only_material)
    {
        // Arrange
        auto working_directory = get_test_working_directory();
        TestPluginHost host = TestPluginHost(working_directory);
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_text(
            "ComputeOnly.mat",
            R"({
  "shaders": { "compute": "303" }
}
)");
        plugins::MatMaterialLoaderPlugin plugin = {};
        plugin.set_file_ops(file_ops);
        plugin.attach(host);
        Material material = {};
        LoadMaterialRequest request("ComputeOnly.mat", &material);

        // Act
        plugin.receive_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::HANDLED);
        EXPECT_TRUE(material.program.compute.is_valid());
        EXPECT_FALSE(material.program.vertex.is_valid());
        EXPECT_FALSE(material.program.fragment.is_valid());
    }
}
