#include "tbx/assets/asset_loaders.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/assets/asset_requests.h"
#include "tbx/debugging/macros.h"
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tbx
{
    static std::shared_ptr<AudioClip> create_audio_data()
    {
        return std::make_shared<AudioClip>();
    }

    static std::shared_ptr<Material> create_material_data()
    {
        auto material = Material();
        material.program.vertex = PbrVertexShader::HANDLE;
        material.program.fragment = PbrFragmentShader::HANDLE;
        material.parameters.set("color", Color(1.0f, 0.0f, 1.0f, 1.0f));
        material.parameters.set("diffuse_strength", 1.0f);
        material.parameters.set("normal_strength", 1.0f);
        material.parameters.set("specular_strength", 0.5f);
        material.parameters.set("shininess_strength", 32.0f);
        material.parameters.set("emissive", Color(1.0f, 0.0f, 1.0f, 1.0f));
        material.parameters.set("emissive_strength", 1.0f);
        material.parameters.set("alpha_cutoff", 0.1f);
        material.parameters.set("transparency_amount", 0.0f);
        material.parameters.set("exposure", 1.0f);
        material.config = MaterialConfig {
            .depth =
                MaterialDepthConfig {
                    .is_test_enabled = true,
                    .is_write_enabled = true,
                    .is_prepass_enabled = false,
                    .function = MaterialDepthFunction::Less,
                },
            .transparency =
                MaterialTransparencyConfig {
                    .blend_mode = MaterialBlendMode::Opaque,
                },
        };
        return std::make_shared<Material>(std::move(material));
    }

    static std::shared_ptr<Texture> create_fallback_texture(
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        TextureMipmaps mipmaps,
        TextureCompression compression)
    {
        auto pixels = std::vector<Pixel>();
        if (format == TextureFormat::RGB)
            pixels = {255, 0, 255};
        else
            pixels = {255, 0, 255, 255};

        return std::make_shared<Texture>(
            Size(1, 1),
            wrap,
            filter,
            format,
            mipmaps,
            compression,
            pixels);
    }

    static std::shared_ptr<Shader> create_shader_data()
    {
        auto vertex_shader = ShaderSource(
            "#version 450 core\n"
            "layout(location = 0) in vec3 a_position;\n"
            "layout(location = 8) in mat4 a_model;\n"
            "layout(location = 12) in uint a_instance_id;\n"
            "uniform mat4 u_view_proj = mat4(1.0);\n"
            "void main()\n"
            "{\n"
            "    gl_Position = u_view_proj * (a_model * vec4(a_position, 1.0 + "
            "float(a_instance_id & 0u)));\n"
            "}\n",
            ShaderType::VERTEX);

        auto fragment_shader = ShaderSource(
            "#version 450 core\n"
            "layout(location = 0) out vec4 o_color;\n"
            "void main()\n"
            "{\n"
            "    o_color = vec4(1.0, 0.0, 1.0, 1.0);\n"
            "}\n",
            ShaderType::FRAGMENT);

        auto sources = std::vector<ShaderSource>();
        sources.push_back(std::move(vertex_shader));
        sources.push_back(std::move(fragment_shader));
        return std::make_shared<Shader>(std::move(sources));
    }

    static Mesh create_two_sided_fallback_mesh()
    {
        auto mesh = Mesh(quad.vertices, quad.indices);
        const auto original_index_count = mesh.indices.size();
        mesh.indices.reserve(original_index_count * 2U);
        for (size_t index = 0U; index + 2U < original_index_count; index += 3U)
        {
            mesh.indices.push_back(mesh.indices[index]);
            mesh.indices.push_back(mesh.indices[index + 2U]);
            mesh.indices.push_back(mesh.indices[index + 1U]);
        }
        return mesh;
    }

    static std::shared_ptr<Model> create_model_data()
    {
        Material material = {};
        material.textures.set("diffuse_map", NotFoundIcon::HANDLE);
        return std::make_shared<Model>(create_two_sided_fallback_mesh(), material);
    }

    void warn_missing_dispatcher(std::string_view action)
    {
        std::string message = std::string("No global dispatcher available to ").append(action);
        std::fprintf(stderr, "Toybox warning: %s\n", message.c_str());
    }

    std::shared_future<Result> make_missing_dispatcher_future(std::string_view action)
    {
        std::promise<Result> promise;
        Result result = {};
        result.flag_failure(std::string("No global dispatcher available to ").append(action));
        promise.set_value(result);
        return promise.get_future().share();
    }

    AssetPromise<Model> load_model_async(const std::filesystem::path& asset_path)
    {
        auto asset = create_model_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Model> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a model asynchronously");
            result.promise = make_missing_dispatcher_future("load a model asynchronously");
            return result;
        }

        LoadModelRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto future = dispatcher->post(message);
        AssetPromise<Model> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Model> load_model(const std::filesystem::path& asset_path)
    {
        auto asset = create_model_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a model synchronously");
            return asset;
        }

        LoadModelRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        dispatcher->send(message);
        return asset;
    }

    AssetPromise<Texture> load_texture_async(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        TextureMipmaps mipmaps,
        TextureCompression compression)
    {
        auto asset = create_fallback_texture(wrap, filter, format, mipmaps, compression);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Texture> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a texture asynchronously");
            result.promise = make_missing_dispatcher_future("load a texture asynchronously");
            return result;
        }

        LoadTextureRequest
            message(asset_path, asset.get(), wrap, filter, format, mipmaps, compression);
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto future = dispatcher->post(message);
        AssetPromise<Texture> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Texture> load_texture(
        const std::filesystem::path& asset_path,
        TextureWrap wrap,
        TextureFilter filter,
        TextureFormat format,
        TextureMipmaps mipmaps,
        TextureCompression compression)
    {
        auto asset = create_fallback_texture(wrap, filter, format, mipmaps, compression);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a texture synchronously");
            return asset;
        }

        LoadTextureRequest
            message(asset_path, asset.get(), wrap, filter, format, mipmaps, compression);
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto result = dispatcher->send(message);
        if (!result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Texture load request failed for '{}': {}. Using fallback pink texture.",
                asset_path.string(),
                result.get_report());
        }
        return asset;
    }

    AssetPromise<AudioClip> load_audio_async(const std::filesystem::path& asset_path)
    {
        auto asset = create_audio_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<AudioClip> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load audio asynchronously");
            result.promise = make_missing_dispatcher_future("load audio asynchronously");
            return result;
        }

        LoadAudioRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto future = dispatcher->post(message);
        AssetPromise<AudioClip> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<AudioClip> load_audio(const std::filesystem::path& asset_path)
    {
        auto asset = create_audio_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load audio synchronously");
            return asset;
        }

        LoadAudioRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        dispatcher->send(message);
        return asset;
    }

    AssetPromise<Shader> load_shader_async(const std::filesystem::path& asset_path)
    {
        auto asset = create_shader_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Shader> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a shader asynchronously");
            result.promise = make_missing_dispatcher_future("load a shader asynchronously");
            return result;
        }

        LoadShaderRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto future = dispatcher->post(message);
        AssetPromise<Shader> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Shader> load_shader(const std::filesystem::path& asset_path)
    {
        auto asset = create_shader_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a shader synchronously");
            return asset;
        }

        LoadShaderRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto result = dispatcher->send(message);
        if (!result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Shader load request failed for '{}': {}. Using fallback pink shader.",
                asset_path.string(),
                result.get_report());
        }
        return asset;
    }

    AssetPromise<Material> load_material_async(const std::filesystem::path& asset_path)
    {
        auto asset = create_material_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            AssetPromise<Material> result = {};
            result.asset = asset;
            warn_missing_dispatcher("load a material asynchronously");
            result.promise = make_missing_dispatcher_future("load a material asynchronously");
            return result;
        }

        LoadMaterialRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto future = dispatcher->post(message);
        AssetPromise<Material> result = {};
        result.asset = asset;
        result.promise = future;
        return result;
    }

    std::shared_ptr<Material> load_material(const std::filesystem::path& asset_path)
    {
        auto asset = create_material_data();
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            warn_missing_dispatcher("load a material synchronously");
            return asset;
        }

        LoadMaterialRequest message(asset_path, asset.get());
        message.not_handled_behavior = MessageNotHandledBehavior::WARN;
        auto result = dispatcher->send(message);
        if (!result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Material load request failed for '{}': {}. Using fallback pink material.",
                asset_path.string(),
                result.get_report());
        }
        return asset;
    }
}
