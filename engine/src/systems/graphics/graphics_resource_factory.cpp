#include "graphics_resource_factory.h"
#include "resources/graphics_buffer_resource.h"
#include "resources/graphics_material_resource.h"
#include "resources/graphics_model_resource_group.h"
#include "resources/graphics_pipeline_resource.h"
#include "resources/graphics_sampler_resource.h"
#include "resources/graphics_texture_resource.h"
#include <cstring>
#include <utility>

namespace tbx
{
    GraphicsResourceFactory::GraphicsResourceFactory(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
    {
    }

    bool GraphicsResourceFactory::create_buffer_resource(
        const GraphicsBufferDesc& desc,
        const void* data,
        const uint64 data_size,
        std::shared_ptr<GraphicsResource>& out_resource) const
    {
        auto resource = std::make_shared<detail::GraphicsBufferResource>(
            _backend,
            desc,
            copy_bytes(data, data_size));
        if (!resource || !resource->get_uuid().is_valid())
            return false;

        out_resource = std::move(resource);
        return true;
    }

    bool GraphicsResourceFactory::create_material_resource(
        const Handle& handle,
        const Material& material,
        const bool is_post_process,
        std::shared_ptr<GraphicsResource>& out_resource) const
    {
        auto resource = std::make_shared<detail::GraphicsMaterialResource>(
            _backend,
            _asset_manager,
            handle,
            material,
            is_post_process);
        if (!resource || !resource->get_uuid().is_valid())
            return false;

        out_resource = std::move(resource);
        return true;
    }

    bool GraphicsResourceFactory::create_model_resource(
        const Handle& handle,
        const Model& model,
        std::shared_ptr<GraphicsResource>& out_resource,
        std::vector<GraphicsModelMeshResource>& out_meshes) const
    {
        out_resource = {};
        out_meshes.clear();

        auto resource = std::make_shared<detail::GraphicsModelResourceGroup>(
            _backend,
            handle,
            model);
        if (!resource || resource->get_meshes().empty() || !resource->get_uuid().is_valid())
            return false;

        out_meshes = resource->get_meshes();
        out_resource = std::move(resource);
        return true;
    }

    bool GraphicsResourceFactory::create_pipeline_resource(
        const GraphicsPipelineDesc& desc,
        std::shared_ptr<GraphicsResource>& out_resource) const
    {
        auto resource =
            std::make_shared<detail::GraphicsPipelineResource>(_backend, desc);
        if (!resource || !resource->get_uuid().is_valid())
            return false;

        out_resource = std::move(resource);
        return true;
    }

    bool GraphicsResourceFactory::create_sampler_resource(
        const GraphicsSamplerDesc& desc,
        std::shared_ptr<GraphicsResource>& out_resource) const
    {
        auto resource = std::make_shared<detail::GraphicsSamplerResource>(_backend, desc);
        if (!resource || !resource->get_uuid().is_valid())
            return false;

        out_resource = std::move(resource);
        return true;
    }

    bool GraphicsResourceFactory::create_texture_resource(
        const Handle& handle,
        const Texture& texture,
        std::shared_ptr<GraphicsResource>& out_resource) const
    {
        auto resource = std::make_shared<detail::GraphicsTextureResource>(
            _backend,
            handle,
            texture);
        if (!resource || !resource->get_uuid().is_valid())
            return false;

        out_resource = std::move(resource);
        return true;
    }

    bool GraphicsResourceFactory::create_texture_resource(
        const GraphicsTextureDesc& desc,
        const void* data,
        const uint64 data_size,
        std::shared_ptr<GraphicsResource>& out_resource) const
    {
        auto resource = std::make_shared<detail::GraphicsTextureResource>(
            _backend,
            desc,
            copy_bytes(data, data_size));
        if (!resource || !resource->get_uuid().is_valid())
            return false;

        out_resource = std::move(resource);
        return true;
    }

    std::vector<uint8> GraphicsResourceFactory::copy_bytes(const void* data, const uint64 data_size)
    {
        if (data == nullptr || data_size == 0U)
            return {};

        auto bytes = std::vector<uint8>(static_cast<size>(data_size));
        std::memcpy(bytes.data(), data, static_cast<size>(data_size));
        return bytes;
    }
}

