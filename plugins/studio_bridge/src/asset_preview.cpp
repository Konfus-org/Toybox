#include "asset_preview.h"
#include "bridge_utils.h"
#include "builtin_assets.h"
#include "view_stream.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/material_instance.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/components/lights.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/handle.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/uuid.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace tbx::studio_bridge
{
    // Lower-cased file extension without the leading dot — the asset's editor "type" token (e.g.
    // "png", "mat", "fbx").
    static std::string preview_type_from_path(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        if (!extension.empty() && extension.front() == '.')
            extension.erase(extension.begin());
        for (auto& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        return extension;
    }

    static bool is_texture_type(const std::string& type)
    {
        return type == "png" || type == "jpg" || type == "jpeg" || type == "tga" || type == "bmp";
    }

    static bool is_model_type(const std::string& type)
    {
        return type == "fbx" || type == "obj" || type == "gltf" || type == "glb";
    }

    // Overrides a model-preview entity's materials with the chosen built-in surface material. A
    // material_id of 0 leaves the model's own materials in place; otherwise the id names a registered
    // built-in Material (the builtin.* catalog) wrapped in an in-memory MaterialInstance (deduplicated
    // by id) and set as the Renderer's single whole-model override.
    static void apply_model_material(
        tbx::AssetManager& assets,
        tbx::Entity& entity,
        uint32 material_id)
    {
        if (material_id == 0U)
            return;

        const auto base = tbx::Handle(tbx::Uuid(material_id));
        assets.load<tbx::Material>(base);
        const auto instance_handle =
            tbx::Handle("__preview_model_material:" + std::to_string(material_id));
        assets.get_or_register<tbx::MaterialInstance>(
            instance_handle,
            [&base] { return std::make_shared<tbx::MaterialInstance>(base); });

        entity.get_component<tbx::Renderer>().materials = {instance_handle};
    }

    // Registers an in-memory Dynamic Model wrapping a single built-in mesh (one material slot), so a
    // Renderer can preview a material/texture on a primitive. Reused (deduplicated) by name. The
    // preview material is supplied via the Renderer's whole-model override, not the model itself.
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

    // Registers an in-memory MaterialInstance under a name-derived handle and returns the handle, so
    // a preview Renderer can reference it as a whole-model override.
    static tbx::Handle register_preview_material(
        tbx::AssetManager& assets,
        const std::string& name,
        const tbx::MaterialInstance& instance)
    {
        const auto handle = tbx::Handle(name);
        assets.get_or_register<tbx::MaterialInstance>(
            handle,
            [&instance] { return std::make_shared<tbx::MaterialInstance>(instance); });
        return handle;
    }

    // Loads the editor's bundled asset-preview world (sky + key light) into `world`, so previews open
    // against a lit sky rather than a black void. Best-effort: the bundled assets are handed to the
    // engine as an extra asset root at launch (--register-assets); if they aren't present the preview
    // simply renders without a sky and the caller's fallback light kicks in.
    static void load_preview_base_world(tbx::AssetManager& assets, tbx::World& world)
    {
        // The bundled preview assets live under the editor's build output, and the asset registry
        // skips any path containing a "build" directory during its directory scan (it treats build
        // output as non-source). So none of them get pre-registered by id. Loading each BY PATH
        // force-registers it on demand (ensure_entry reads its .meta id), after which the id-based
        // references resolve — so load dependencies first: the texture before Sky.mat that binds
        // it, and Sky.mat before the globals whose sky entity references it.
        assets.load<tbx::Texture>(tbx::Handle("SunnySky.png"));
        assets.load<tbx::Texture>(tbx::Handle("DarkSky.png"));
        assets.load<tbx::Material>(tbx::Handle("Sky.mat"));

        if (const auto globals =
                assets.load<tbx::WorldGlobals>(tbx::Handle("AssetPreview.globals")))
            world.load_globals(*globals);
        if (const auto chunk = assets.load<tbx::WorldChunk>(tbx::Handle("AssetPreview.chunk")))
            world.add_entities(chunk->entities);
    }

    // Adjusts the preview's background sky to the editor's chosen built-in sky material (the builtin.*
    // catalog): PREVIEW_SKYBOX_DEFAULT keeps the bundled world's authored day sky, 0 removes the sky
    // entirely (plain background), and any other id retargets the sky entity to that sky material.
    static void apply_skybox(tbx::AssetManager& assets, tbx::World& world, uint32 skybox_id)
    {
        auto sky_entity = world.first_with<tbx::Sky>();
        if (!sky_entity.get_id().is_valid())
            return;

        if (skybox_id == PREVIEW_SKYBOX_DEFAULT)
            return;

        if (skybox_id == 0U)
        {
            world.destroy(sky_entity);
            return;
        }

        if (!sky_entity.has_component<tbx::Sky>())
            return;
        const auto base = tbx::Handle(tbx::Uuid(skybox_id));
        assets.load<tbx::Material>(base);
        sky_entity.get_component<tbx::Sky>().material = tbx::MaterialInstance(base);
    }

    bool build_asset_preview(
        tbx::AssetManager& assets,
        uint32 asset_id,
        const std::string& option,
        uint32 material_id,
        uint32 skybox_id,
        tbx::World& world,
        AssetPreviewFraming& out_framing)
    {
        // Resolve the registered asset by id (the value editor.listAssets advertises and the editor
        // sends back), then build a handle carrying both its path and id so the loader resolves it.
        for (const auto& entry : assets.get_registered_assets())
        {
            if (entry.asset_id.value != asset_id)
                continue;

            const auto type = preview_type_from_path(entry.resolved_path);
            const auto handle = tbx::Handle(entry.normalized_path, entry.asset_id);

            // Bring in the bundled sky + key light so the preview opens against a lit sky.
            load_preview_base_world(assets, world);

            // Fallback lighting if the bundled preview world wasn't available (so lit previews —
            // the material sphere, models — aren't pitch black). The unlit texture quad ignores it.
            if (!world.first_with<tbx::DirectionalLight>().get_id().is_valid())
            {
                auto light = world.create_entity("PreviewLight");
                auto light_transform = tbx::Transform(tbx::Vec3(0.0F));
                light_transform.rotation = tbx::look_rotation(
                    glm::normalize(glm::vec3(-0.4F, -1.0F, -0.6F)),
                    glm::vec3(0.0F, 1.0F, 0.0F));
                light.add_component<tbx::Transform>(light_transform);
                light.add_component<tbx::DirectionalLight>(
                    tbx::DirectionalLight(tbx::Color::WHITE, 1.5F, 0.35F));
            }

            // The chosen background sky applies to every asset type.
            apply_skybox(assets, world, skybox_id);

            if (is_model_type(type))
            {
                // Models render as-is; the chosen built-in surface material (when set) overrides the
                // model's own part materials in the renderer.
                auto entity = world.create_entity("Preview");
                if (!entity.get_id().is_valid())
                    return false;
                entity.add_component<tbx::Transform>(tbx::Transform(tbx::Vec3(0.0F)));
                entity.add_component<tbx::Renderer>(tbx::Renderer(handle));
                apply_model_material(assets, entity, material_id);

                // Frame the orbit camera to the model's world bounds so it opens fully in view;
                // fall back to a sensible default distance if the model hasn't produced bounds yet.
                auto minimum = glm::vec3(std::numeric_limits<float>::max());
                auto maximum = glm::vec3(std::numeric_limits<float>::lowest());
                if (accumulate_entity_world_bounds(assets, entity, minimum, maximum))
                {
                    const auto center = (minimum + maximum) * 0.5F;
                    const auto radius = glm::length(maximum - minimum) * 0.5F;
                    out_framing.target = tbx::Vec3(center.x, center.y, center.z);
                    out_framing.distance = std::max(radius * 2.5F, 1.0F);
                }
                return true;
            }

            if (type == "mat")
            {
                // A sky-typed material is the environment itself: default it to a sky-sphere so it opens
                // as the background rather than on a mesh (the editor offers only a box/sphere shape
                // toggle for it). Any other material defaults to a mesh primitive.
                const auto material = assets.load<tbx::Material>(handle);
                auto effective_option = option;
                if (effective_option.empty() && material
                    && material->type == tbx::MaterialType::SKY)
                    effective_option = "skysphere";

                // Skybox / sky-sphere: show the material AS the environment by retargeting the base
                // sky entity to it; there is no separate preview mesh.
                if (effective_option == "skybox" || effective_option == "skysphere")
                {
                    auto sky_entity = world.first_with<tbx::Sky>();
                    if (sky_entity.get_id().is_valid() && sky_entity.has_component<tbx::Sky>())
                    {
                        auto& sky = sky_entity.get_component<tbx::Sky>();
                        sky.material = tbx::MaterialInstance(handle);
                        sky.type =
                            effective_option == "skybox" ? tbx::SkyType::BOX : tbx::SkyType::SPHERE;
                    }
                    // The sky is camera-centred, so distance is cosmetic; sit just inside it.
                    out_framing.target = tbx::Vec3(0.0F);
                    out_framing.distance = 1.0F;
                    return true;
                }

                auto entity = world.create_entity("Preview");
                if (!entity.get_id().is_valid())
                    return false;
                entity.add_component<tbx::Transform>(tbx::Transform(tbx::Vec3(0.0F)));
                // Preview the material on a primitive: an in-memory Dynamic model + a Renderer whose
                // whole-model override is an instance referencing the previewed .mat directly.
                const auto preview_model = register_primitive_model(
                    assets,
                    "__preview_mat:" + std::to_string(asset_id) + ":" + effective_option,
                    builtin::mesh_for(effective_option, tbx::Mesh::SPHERE));
                const auto preview_instance = register_preview_material(
                    assets,
                    "__preview_mat_instance:" + std::to_string(asset_id) + ":" + effective_option,
                    tbx::MaterialInstance(handle));
                auto renderer = tbx::Renderer(preview_model);
                renderer.materials = {preview_instance};
                entity.add_component<tbx::Renderer>(std::move(renderer));
                out_framing.target = tbx::Vec3(0.0F);
                out_framing.distance = 3.0F;
                return true;
            }

            if (is_texture_type(type))
            {
                auto entity = world.create_entity("Preview");
                if (!entity.get_id().is_valid())
                    return false;
                entity.add_component<tbx::Transform>(tbx::Transform(tbx::Vec3(0.0F)));
                // A flat (unlit) material shows the texture's own colours without lighting bias; the
                // texture overrides the base-colour slot of the preview instance.
                auto slot_instance = tbx::MaterialInstance(tbx::Handle("Materials/Flat.mat"));
                slot_instance.set_texture("albedo_map", handle);
                const auto preview_model = register_primitive_model(
                    assets,
                    "__preview_tex:" + std::to_string(asset_id) + ":" + option,
                    builtin::mesh_for(option, tbx::Mesh::QUAD));
                const auto preview_instance = register_preview_material(
                    assets,
                    "__preview_tex_instance:" + std::to_string(asset_id) + ":" + option,
                    slot_instance);
                auto renderer = tbx::Renderer(preview_model);
                renderer.materials = {preview_instance};
                entity.add_component<tbx::Renderer>(std::move(renderer));
                out_framing.target = tbx::Vec3(0.0F);
                out_framing.distance = 2.0F;
                return true;
            }

            // A registered but non-previewable asset (e.g. a world/chunk handled elsewhere, or an
            // audio clip): nothing to show in the 3D viewer.
            return false;
        }

        return false;
    }
}
