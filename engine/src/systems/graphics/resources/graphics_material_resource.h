#pragma once
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/graphics_resource.h"
#include "tbx/types/handle.h"
#include "tbx/types/material.h"
#include <memory>
#include <vector>

namespace tbx::detail
{
    class GraphicsMaterialResource final : public GraphicsResource
    {
      public:
        GraphicsMaterialResource(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            Handle handle,
            Material material,
            bool is_post_process);
        ~GraphicsMaterialResource() noexcept override;

      private:
        bool create_resource();

      private:
        bool append_shader_sources(
            const Handle& handle,
            std::vector<Uuid>& loaded_shader_ids,
            std::vector<ShaderSource>& shader_sources) const;
        static void append_instance_layout_attributes(
            std::vector<GraphicsVertexAttributeDesc>& out_attributes);
        static void append_vertex_layout_attributes(
            const VertexBufferLayout& layout,
            std::vector<GraphicsVertexAttributeDesc>& out_attributes);
        bool build_material_shader(Shader& out_shader) const;
        static uint32 get_vertex_attribute_location(VertexAttributeSemantic semantic);
        std::shared_ptr<AssetManager> lock_asset_manager() const;
        GraphicsPipelineDesc make_material_pipeline_desc(Shader shader) const;
        GraphicsPipelineDesc make_post_process_pipeline_desc(Shader shader) const;
        static GraphicsVertexFormat to_graphics_vertex_format(const VertexData& data);

      private:
        std::weak_ptr<AssetManager> _asset_manager = {};
        Handle _handle = {};
        Material _material = {};
        bool _is_post_process = false;
    };
}
