#include "opengl_resource.h"

namespace opengl_rendering
{
    OpenGlResourceScope::OpenGlResourceScope(IOpenGlResource& resource)
        : _resource(&resource)
    {
        _resource->bind();
    }

    OpenGlResourceScope::OpenGlResourceScope(OpenGlResourceScope&& other) noexcept
        : _resource(other._resource)
    {
        other._resource = nullptr;
    }

    OpenGlResourceScope::~OpenGlResourceScope() noexcept
    {
        if (_resource != nullptr)
            _resource->unbind();
    }

    OpenGlResourceScope& OpenGlResourceScope::operator=(OpenGlResourceScope&& other) noexcept
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
