#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/graphics_resource.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/components/model.h"
#include "tbx/types/material.h"
#include "tbx/types/texture.h"
#include <memory>

namespace tbx
{
    class GraphicsResourceFactory final
    {
      public:
        GraphicsResourceFactory(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager);
        ~GraphicsResourceFactory() = default;

      public:
        GraphicsResourceFactory(const GraphicsResourceFactory&) = delete;
        GraphicsResourceFactory& operator=(const GraphicsResourceFactory&) = delete;
        GraphicsResourceFactory(GraphicsResourceFactory&&) noexcept = default;
        GraphicsResourceFactory& operator=(GraphicsResourceFactory&&) noexcept = default;

      public:
        bool create_buffer_resource(
            const GraphicsBufferDesc& desc,
            const void* data,
            uint64 data_size,
            std::shared_ptr<GraphicsResource>& out_resource) const;
        bool create_material_resource(
            const Handle& handle,
            const Material& material,
            bool is_post_process,
            std::shared_ptr<GraphicsResource>& out_resource) const;
        bool create_model_resource(
            const Handle& handle,
            const Model& model,
            std::shared_ptr<GraphicsResource>& out_resource,
            std::vector<GraphicsModelMeshResource>& out_meshes) const;
        bool create_pipeline_resource(
            const GraphicsPipelineDesc& desc,
            std::shared_ptr<GraphicsResource>& out_resource) const;
        bool create_sampler_resource(
            const GraphicsSamplerDesc& desc,
            std::shared_ptr<GraphicsResource>& out_resource) const;
        bool create_texture_resource(
            const Handle& handle,
            const Texture& texture,
            std::shared_ptr<GraphicsResource>& out_resource) const;
        bool create_texture_resource(
            const GraphicsTextureDesc& desc,
            const void* data,
            uint64 data_size,
            std::shared_ptr<GraphicsResource>& out_resource) const;

      private:
        static std::vector<uint8> copy_bytes(const void* data, uint64 data_size);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
    };
}

