#pragma once

namespace opengl_rendering
{
    template <typename TResource>
    bool OpenGlResourceManager::try_get(
        const tbx::Uuid& resource_uuid,
        std::shared_ptr<TResource>& out_resource) const
    {
        static_assert(
            std::is_base_of_v<IOpenGlResource, TResource>,
            "OpenGlResourceManager::try_get<TResource> requires TResource to derive "
            "from IOpenGlResource.");

        auto out_untyped_resource = std::shared_ptr<IOpenGlResource> {};
        if (!try_get_raw(resource_uuid, out_untyped_resource))
            return false;

        auto out_typed_resource = std::dynamic_pointer_cast<TResource>(out_untyped_resource);
        if (!out_typed_resource)
            return false;

        out_resource = std::move(out_typed_resource);
        return true;
    }
}
