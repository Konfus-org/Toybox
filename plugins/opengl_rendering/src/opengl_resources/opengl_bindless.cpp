#include "opengl_bindless.h"
#include "tbx/interfaces/opengl_context_manager.h"
#include <cstring>
#include <glad/glad.h>

namespace opengl_rendering
{
    using GlGetTextureHandleArbFn = uint64 (*)(uint32 texture);
    using GlMakeTextureHandleResidentArbFn = void (*)(uint64 handle);
    using GlMakeTextureHandleNonResidentArbFn = void (*)(uint64 handle);
    using GlProgramUniformHandleUi64ArbFn = void (*)(uint32 program, int location, uint64 value);

    struct BindlessApi final
    {
        bool loaded = false;
        bool supported = false;
        GlGetTextureHandleArbFn get_texture_handle = nullptr;
        GlMakeTextureHandleResidentArbFn make_resident = nullptr;
        GlMakeTextureHandleNonResidentArbFn make_non_resident = nullptr;
        GlProgramUniformHandleUi64ArbFn upload_sampler = nullptr;
    };

    static tbx::GraphicsProcAddress g_proc_loader = nullptr;
    static BindlessApi& bindless_api();

    void set_bindless_proc_loader(const tbx::GraphicsProcAddress loader)
    {
        g_proc_loader = loader;
        auto& api = bindless_api();
        api.loaded = false;
        api.supported = false;
        api.get_texture_handle = nullptr;
        api.make_resident = nullptr;
        api.make_non_resident = nullptr;
        api.upload_sampler = nullptr;
    }

    static void* load_gl_proc(const char* name)
    {
        if (g_proc_loader == nullptr)
            return nullptr;
        return g_proc_loader(name);
    }

    static bool has_extension(const char* name)
    {
        GLint extension_count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
        for (GLint extension_index = 0; extension_index < extension_count; ++extension_index)
        {
            const auto* extension = reinterpret_cast<const char*>(
                glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(extension_index)));
            if (extension == nullptr)
                continue;
            if (std::strcmp(extension, name) == 0)
                return true;
        }
        return false;
    }

    static BindlessApi& bindless_api()
    {
        static BindlessApi api = {};
        if (api.loaded)
            return api;

        api.loaded = true;
        api.supported = has_extension("GL_ARB_bindless_texture");
        if (!api.supported)
            return api;

        api.get_texture_handle =
            reinterpret_cast<GlGetTextureHandleArbFn>(load_gl_proc("glGetTextureHandleARB"));
        api.make_resident = reinterpret_cast<GlMakeTextureHandleResidentArbFn>(
            load_gl_proc("glMakeTextureHandleResidentARB"));
        api.make_non_resident = reinterpret_cast<GlMakeTextureHandleNonResidentArbFn>(
            load_gl_proc("glMakeTextureHandleNonResidentARB"));
        api.upload_sampler = reinterpret_cast<GlProgramUniformHandleUi64ArbFn>(
            load_gl_proc("glProgramUniformHandleui64ARB"));
        api.supported = api.get_texture_handle != nullptr && api.make_resident != nullptr
                        && api.make_non_resident != nullptr && api.upload_sampler != nullptr;
        return api;
    }

    bool is_bindless_texture_supported()
    {
        return bindless_api().supported;
    }

    bool try_make_bindless_handle_resident(const uint32 texture_id, uint64& out_handle)
    {
        const auto& api = bindless_api();
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
        const auto& api = bindless_api();
        if (!api.supported || handle == 0)
            return;

        api.make_non_resident(handle);
    }

    bool try_upload_bindless_sampler(
        const uint32 program_id,
        const int uniform_location,
        const uint64 handle)
    {
        const auto& api = bindless_api();
        if (!api.supported || program_id == 0 || uniform_location < 0 || handle == 0)
            return false;

        api.upload_sampler(program_id, uniform_location, handle);
        return true;
    }
}
