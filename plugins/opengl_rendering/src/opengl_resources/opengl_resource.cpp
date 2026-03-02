#include "opengl_resource.h"

namespace opengl_rendering
{
    GlResourceScope::GlResourceScope(IOpenGlResource& resource)
        : _resource(&resource)
    {
        _resource->bind();
    }

    GlResourceScope::GlResourceScope(GlResourceScope&& other) noexcept
        : _resource(other._resource)
    {
        other._resource = nullptr;
    }

    GlResourceScope::~GlResourceScope() noexcept
    {
        if (_resource != nullptr)
            _resource->unbind();
    }

    GlResourceScope& GlResourceScope::operator=(GlResourceScope&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_resource != nullptr)
            _resource->unbind();

        _resource = other._resource;
        other._resource = nullptr;
        return *this;
    }
}
