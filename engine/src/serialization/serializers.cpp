#include "tbx/serialization/serializers.h"
#include "tbx/app.h"
#include "tbx/audio/clip.h"
#include "tbx/ecs/kit.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/gpu/material.h"
#include "tbx/gpu/model.h"
#include "tbx/gpu/shader_source.h"
#include "tbx/gpu/texture.h"
#include "tbx/scripting/source.h"
#include "tbx/serialization/registration.h"
#include "tbx/ui/font.h"
#include "tbx/ui/document.h"

namespace tbx::serialization
{
    void register_builtin_serializers()
    {
        // Types with real codecs bring a reader (and a writer when writing makes sense);
        // text assets are Format::TEXT; plain data types round-trip through their reflection.
        register_serializer<gpu::Texture>()
            .format(Format::CUSTOM)
            .deserializer(gpu::deserialize_texture)
            .serializer(gpu::serialize_texture);
        register_serializer<gpu::Model>().format(Format::CUSTOM).deserializer(gpu::deserialize_model);
        register_serializer<gpu::ShaderSource>().format(Format::TEXT);
        register_serializer<audio::Clip>().format(Format::CUSTOM).deserializer(audio::deserialize_clip);
        register_serializer<scripts::Source>()
            .format(Format::CUSTOM)
            .deserializer(scripts::deserialize_script_source);
        register_serializer<ui::Document>().format(Format::TEXT);
        register_serializer<ui::Font>().format(Format::CUSTOM).deserializer(ui::deserialize_font);
        register_serializer<ecs::Kit>()
            .format(Format::CUSTOM)
            .deserializer(ecs::deserialize_kit)
            .serializer(ecs::serialize_kit);
        register_serializer<ecs::Sandbox>()
            .format(Format::CUSTOM)
            .deserializer(ecs::deserialize_sandbox)
            .serializer(ecs::serialize_sandbox);
        register_serializer<gpu::Material>().format(Format::DEFAULT);
        register_serializer<App>().format(Format::DEFAULT);
    }
}
