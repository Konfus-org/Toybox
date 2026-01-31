
#include "tbx/graphics/renderer.h"

namespace tbx
{
    Renderer::Renderer() = default;

    Renderer::Renderer(std::unique_ptr<IRenderData> render_data)
        : data(std::move(render_data))
    {
    }

    Renderer::Renderer(std::string model_name, Handle material_handle)
        : data(
              std::make_unique<StaticRenderData>(
                  Handle(std::move(model_name)),
                  std::move(material_handle)))
    {
    }

    Renderer::Renderer(Handle model_handle, Handle material_handle)
        : data(
              std::make_unique<StaticRenderData>(
                  std::move(model_handle),
                  std::move(material_handle)))
    {
    }

    Renderer::Renderer(StaticRenderData render_data)
        : data(std::make_unique<StaticRenderData>(std::move(render_data)))
    {
    }

    Renderer::Renderer(const Mesh& mesh, Handle material_handle)
        : data(
              std::make_unique<ProceduralData>(
                  std::vector<Mesh> {mesh},
                  std::vector<Handle> {std::move(material_handle)}))
    {
    }

    Renderer::Renderer(ProceduralData render_data)
        : data(std::make_unique<ProceduralData>(std::move(render_data)))
    {
    }
}
