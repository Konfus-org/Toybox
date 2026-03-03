#include "pch.h"
#include "tbx/plugins/assimp_model_loader/assimp_model_loader_plugin.h"
#include "tbx/async/cancellation_token.h"
#include "tbx/plugin_api/tests/importer_test_environment.h"

namespace assimp_model_loader::tests
{
    /// <summary>
    /// Verifies the assimp importer short-circuits cancelled requests without file IO.
    /// </summary>
    TEST(importers, assimp_loader_honors_cancellation_before_import)
    {
        // Arrange
        auto working_directory = tbx::tests::plugin_api::get_test_working_directory();
        tbx::tests::plugin_api::TestPluginHost host = tbx::tests::plugin_api::TestPluginHost(working_directory);
        assimp_model_loader::AssimpModelLoaderPlugin plugin = {};
        plugin.on_attach(host);
        tbx::Model model = {};
        tbx::LoadModelRequest request("cancelled.fbx", &model);
        tbx::CancellationSource cancellation = {};
        cancellation.cancel();
        request.cancellation_token = cancellation.get_token();

        // Act
        plugin.on_recieve_message(request);

        // Assert
        EXPECT_EQ(request.state, tbx::MessageState::CANCELLED);
    }
}
