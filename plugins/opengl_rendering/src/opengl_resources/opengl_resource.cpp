#include "opengl_resource.h"

namespace opengl_rendering
{
    OpenGlResourceScope::OpenGlResourceScope(IOpenGlResource& resource)
        : _resource(resource)
    {
        _resource->get().bind();
    }

    OpenGlResourceScope::OpenGlResourceScope(OpenGlResourceScope&& other) noexcept
        : _resource(other._resource)
    {
        other._resource = std::nullopt;
    }

    OpenGlResourceScope::~OpenGlResourceScope() noexcept
    {
        if (_resource.has_value())
            _resource->get().unbind();
    }

    OpenGlResourceScope& OpenGlResourceScope::operator=(OpenGlResourceScope&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_resource.has_value())
            _resource->get().unbind();

        _resource = other._resource;
        other._resource = std::nullopt;
        return *this;
    }
}
