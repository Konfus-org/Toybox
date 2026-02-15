#include "pch.h"
#include "../src/assimp_model_loader_plugin.h"
#include "tbx/async/cancellation_token.h"
#include "../../tests/shared/importer_test_environment.h"

namespace tbx::tests::importers
{
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
        CancellationSource cancellation = {};
        cancellation.cancel();
        request.cancellation_token = cancellation.get_token();

        // Act
        plugin.on_recieve_message(request);

        // Assert
        EXPECT_EQ(request.state, MessageState::CANCELLED);
    }
}
