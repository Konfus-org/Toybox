#include "asset_preview.h"
#include "builtin_assets.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/components/lights.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/handle.h"
#include "tbx/types/quaternions.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    // Registers (idempotently) an in-memory Dynamic Model wrapping a single built-in mesh (one material
    // slot), so a Renderer can preview a material/texture on a primitive. Reused (deduplicated) by name;
    // the preview material is supplied via the Renderer's whole-model override, not the model itself.
    static tbx::Handle register_primitive_model(
        tbx::AssetManager& assets,
        const std::string& name,
        const tbx::Mesh& mesh)
    {
        const auto model_handle = tbx::Handle(name);
        const auto slot_handle = tbx::Handle(name + ":slot");
        assets.get_or_register<tbx::Model>(
            model_handle,
            [&]
            {
                auto model = std::make_shared<tbx::Model>(mesh);
                model->mode = tbx::MeshMode::DYNAMIC;
                model->slots = {slot_handle};
                return model;
            });
        return model_handle;
    }

    void seed_preview_world(tbx::AssetManager& assets, tbx::World& world)
    {
        // The bundled preview assets live under the editor's build output, which the registry's directory
        // scan skips (it treats build output as non-source). Loading each BY PATH force-registers it on
        // demand (reading its .meta id), after which Sky.mat's id-based texture references resolve — so load
        // the textures before the Sky.mat that binds them. This makes a C#-set sky material resolvable.
        assets.load<tbx::Texture>(tbx::Handle("SunnySky.png"));
        assets.load<tbx::Texture>(tbx::Handle("DarkSky.png"));
        assets.load<tbx::Material>(tbx::Handle("Sky.mat"));

        // A key directional light so lit previews (a material sphere, models) aren't pitch black. The sky
        // entity and the previewed asset's entity are created by the editor through the world/entity API.
        auto light = world.create_entity("PreviewLight");
        auto light_transform = tbx::Transform(tbx::Vec3(0.0F));
        light_transform.rotation = tbx::look_rotation(
            glm::normalize(glm::vec3(-0.4F, -1.0F, -0.6F)),
            glm::vec3(0.0F, 1.0F, 0.0F));
        light.add_component<tbx::Transform>(light_transform);
        light.add_component<tbx::DirectionalLight>(
            tbx::DirectionalLight(tbx::Color::WHITE, 1.5F, 0.35F));
    }

    std::vector<PreviewMeshAsset> register_preview_meshes(tbx::AssetManager& assets)
    {
        // The editor-facing label paired with the built-in mesh token (resolved through builtin::mesh_for).
        // The editor shows a material/texture on the chosen primitive by setting the Renderer's model to
        // the matching in-memory model asset.
        static const std::pair<const char*, const char*> meshes[] = {
            {"Sphere", "sphere"}, {"Cube", "cube"}, {"Capsule", "capsule"},
            {"Half Sphere", "half_sphere"}, {"Plane", "quad"}, {"Triangle", "triangle"},
        };

        auto result = std::vector<PreviewMeshAsset>();
        result.reserve(std::size(meshes));
        for (const auto& [label, token] : meshes)
        {
            // A ".mesh" path so editor.listAssets surfaces it with a clean stem name + "mesh" type token.
            const auto handle = register_primitive_model(
                assets,
                std::string("PreviewMesh/") + label + ".mesh",
                builtin::mesh_for(token, tbx::Mesh::SPHERE));
            result.push_back(PreviewMeshAsset {.handle = handle, .label = label});
        }
        return result;
    }
}
