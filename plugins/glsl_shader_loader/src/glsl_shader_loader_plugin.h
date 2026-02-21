#pragma once
#include "tbx/assets/asset_manager.h"
#include "tbx/assets/messages.h"
#include "tbx/files/file_ops.h"
#include "tbx/plugin_api/plugin.h"
#include <filesystem>
#include <memory>

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Loads shader assets from stage-specific GLSL files
    /// (.vert/.tes/.geom/.frag/.comp).
    /// </summary>
    /// <remarks>
    /// Include Support: Expands `#include "..."` directives by pasting the referenced asset text.
    /// Include-Once: Each resolved include file is expanded at most once per shader stage.
    /// Ownership: Plugin lifetime is owned by the host; it keeps non-owning references to the host.
    /// Thread Safety: Handles asset messages on the dispatcher thread; no internal synchronization.
    /// </remarks>
    class GlslShaderLoaderPlugin final : public Plugin
    {
      public:
        /// <summary>
        /// Purpose: Captures host references needed for asset resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the host.
        /// Thread Safety: Called on the main thread during plugin attach.
        /// </remarks>
        void on_attach(IPluginHost& host) override;

        /// <summary>
        /// Purpose: Releases any cached host references.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not destroy host resources.
        /// Thread Safety: Called on the main thread during plugin detach.
        /// </remarks>
        void on_detach() override;

        /// <summary>
        /// Purpose: Receives shader load requests and dispatches file parsing.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of messages or asset payloads.
        /// Thread Safety: Executes on the dispatcher thread; relies on Shader payload
        /// synchronization.
        /// </remarks>
        void on_recieve_message(Message& msg) override;

        /// <summary>
        /// Purpose: Overrides filesystem operations used by the loader.
        /// </summary>
        /// <remarks>
        /// Ownership: Shares ownership of file_ops with the caller.
        /// Thread Safety: Call before dispatching load messages.
        /// </remarks>
        void set_file_ops(std::shared_ptr<IFileOps> file_ops);

      private:
        void on_load_shader_program_request(LoadShaderRequest& request);

        AssetManager* _asset_manager = nullptr;
        std::filesystem::path _working_directory = {};
        std::shared_ptr<IFileOps> _file_ops = {};
    };
}
