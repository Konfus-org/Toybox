#pragma once
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Ids/Uid.h"

namespace Tbx
{
    /// <summary>
    /// Manages a graphics resource, i.e. texture, shader program, or mesh on the GPU.
    /// Once this object goes out of scope the destructor is responsible for cleaning up the resource.
    /// </summary>
    class TBX_EXPORT GraphicsResource
    {
    public:
        virtual ~GraphicsResource();
        virtual void Activate() = 0;
        virtual void Release() = 0;

    public:
        Uid RenderId = Uid::Invalid;
    };

    class TBX_EXPORT ShaderResource : public GraphicsResource
    {
    public:
        virtual ~ShaderResource();
    };

    class TBX_EXPORT ShaderProgramResource : public GraphicsResource
    {
    public:
        virtual ~ShaderProgramResource();
        virtual void Upload(const ShaderUniform& uniform) = 0;
    };

    class TBX_EXPORT TextureResource : public GraphicsResource
    {
    public:
        virtual ~TextureResource();
        virtual void SetSlot(uint32 slot) = 0;
    };

    class TBX_EXPORT MeshResource : public GraphicsResource
    {
    public:
        virtual ~MeshResource();
        virtual void SetVertexBuffer(const VertexBuffer& vbuff) = 0;
        virtual void SetIndexBuffer(const IndexBuffer& ibuff) = 0;
        virtual void Draw() = 0;
    };

    /// <summary>
    /// A RAII wrapper for GraphicsResource that automatically calls Activate() and Release() on construction and destruction.
    /// </summary>
    class TBX_EXPORT UseGraphicsResourceScope
    {
    public:
        UseGraphicsResourceScope(Ref<GraphicsResource> resource) : _resource(resource) { _resource->Activate(); }
        ~UseGraphicsResourceScope() { _resource->Release(); }

    private:
        Ref<GraphicsResource> _resource;
    };

}