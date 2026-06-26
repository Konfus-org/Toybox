#include "builtin_assets.h"
#include <functional>

namespace tbx::studio_bridge::builtin
{
    // A built-in mesh paired with the token the editor sends to select it.
    struct MeshOption
    {
        std::string token;
        std::reference_wrapper<const tbx::Mesh> mesh;
    };

    //// BUILT-IN CATALOG ////

    static const std::vector<MeshOption>& meshes()
    {
        static const std::vector<MeshOption> options = {
            {"sphere", tbx::Mesh::SPHERE},
            {"cube", tbx::Mesh::CUBE},
            {"capsule", tbx::Mesh::CAPSULE},
            {"half_sphere", tbx::Mesh::HALF_SPHERE},
            {"quad", tbx::Mesh::QUAD},
            {"triangle", tbx::Mesh::TRIANGLE},
        };
        return options;
    }

    const tbx::Mesh& mesh_for(const std::string& token, const tbx::Mesh& fallback)
    {
        // "plane" is an alias the editor may send for the quad.
        if (token == "plane")
            return tbx::Mesh::QUAD;
        for (const auto& option : meshes())
            if (option.token == token)
                return option.mesh.get();
        return fallback;
    }

    const std::vector<tbx::Handle>& assets()
    {
        // Handles by path; the registry resolves each to its real id when the asset is registered (see
        // WorldManager::list_assets). Order is irrelevant — the editor matches them by name.
        static const std::vector<tbx::Handle> handles = {
            tbx::Handle("PreviewMetal.mat"),
            tbx::Handle("PreviewMatte.mat"),
            tbx::Handle("PreviewUnlit.mat"),
            tbx::Handle("Sky.mat"),
            tbx::Handle("PreviewNightSky.mat"),
            tbx::Handle("SunnySky.png"),
            tbx::Handle("DarkSky.png"),
        };
        return handles;
    }
}
