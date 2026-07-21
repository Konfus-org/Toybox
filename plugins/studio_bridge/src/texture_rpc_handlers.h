#pragma once
#include <memory>

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct EditorTextureTable;

    /// @brief Registers texture.upload: editor-supplied RGBA8 raster data (base64) stored in the
    /// shared texture table for the draw lane's sprite commands to reference by the replied id.
    void register_texture_handlers(
        const RpcRegistrar& registrar, const std::shared_ptr<EditorTextureTable>& textures);
}
