#include "opengl_bindless.h"
#include "internal/opengl_bindless_internal.h"
#include "tbx/interfaces/opengl_context_manager.h"
#include <cstring>
#include <glad/glad.h>
namespace opengl_rendering
{
    void set_bindless_proc_loader(const tbx::GraphicsProcAddress loader)
    {
        internal::g_proc_loader = loader;
        auto& api = internal::bindless_api();
        api.loaded = false;
        api.supported = false;
        api.get_texture_handle = nullptr;
        api.make_resident = nullptr;
        api.make_non_resident = nullptr;
        api.upload_sampler = nullptr;
    }

    bool is_bindless_texture_supported()
    {
        return internal::bindless_api().supported;
    }

    bool try_make_bindless_handle_resident(const uint32 texture_id, uint64& out_handle)
    {
        const auto& api = internal::bindless_api();
        if (!api.supported || texture_id == 0)
            return false;

        const auto handle = api.get_texture_handle(texture_id);
        if (handle == 0)
            return false;

        api.make_resident(handle);
        out_handle = handle;
        return true;
    }

    void release_bindless_handle(const uint64 handle)
    {
        const auto& api = internal::bindless_api();
        if (!api.supported || handle == 0)
            return;

        api.make_non_resident(handle);
    }

    bool try_upload_bindless_sampler(
        const uint32 program_id,
        const int uniform_location,
        const uint64 handle)
    {
        const auto& api = internal::bindless_api();
        if (!api.supported || program_id == 0 || uniform_location < 0 || handle == 0)
            return false;

        api.upload_sampler(program_id, uniform_location, handle);
        return true;
    }
}
