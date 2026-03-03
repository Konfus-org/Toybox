#pragma once
#include "tbx/assets/asset_manager.h"
#include "tbx/assets/messages.h"
#include "tbx/files/file_ops.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <filesystem>
#include <memory>

namespace mat_material_loader
{
    /// <summary>
    /// Purpose: Loads material assets from .mat JSON files.
    /// </summary>
    /// <remarks>
    /// Ownership: tbx::Plugin lifetime is owned by the host; it keeps non-owning references to the
    /// host. Thread Safety: Handles asset messages on the dispatcher thread; no internal
    /// synchronization.
    /// </remarks>
    class TBX_PLUGIN_API MatMaterialLoaderPlugin final : public tbx::Plugin
    {
      public:
        /// <summary>
        /// Purpose: Captures host references needed for asset resolution.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the host.
        /// Thread Safety: Called on the main thread during plugin attach.
        /// </remarks>
        void on_attach(tbx::IPluginHost& host) override;

        /// <summary>
        /// Purpose: Releases any cached host references.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not destroy host resources.
        /// Thread Safety: Called on the main thread during plugin detach.
        /// </remarks>
        void on_detach() override;

        /// <summary>
        /// Purpose: Receives material load requests and dispatches file parsing.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of messages or asset payloads.
        /// Thread Safety: Executes on the dispatcher thread; relies on tbx::Material payload
        /// synchronization.
        /// </remarks>
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void on_load_material_request(tbx::LoadMaterialRequest& request);
        void set_file_ops(std::shared_ptr<tbx::IFileOps> file_ops);
        std::filesystem::path resolve_asset_path(const std::filesystem::path& path) const;

        std::filesystem::path _working_directory = {};
        std::shared_ptr<tbx::IFileOps> _file_ops = {};
    };
}
