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

namespace tbx
{
    void register_builtin_serializers()
    {
        // Types with real codecs bring a reader (and a writer when writing makes sense);
        // text assets are SerializerFormat::TEXT; plain data types round-trip through their reflection.
        register_serializer<Texture>()
            .format(SerializerFormat::CUSTOM)
            .deserializer(gpu_deserialize_texture)
            .serializer(gpu_serialize_texture);
        register_serializer<Model>().format(SerializerFormat::CUSTOM).deserializer(gpu_deserialize_model);
        register_serializer<ShaderSource>().format(SerializerFormat::TEXT);
        register_serializer<AudioClip>().format(SerializerFormat::CUSTOM).deserializer(deserialize_clip);
        register_serializer<ScriptSource>()
            .format(SerializerFormat::CUSTOM)
            .deserializer(deserialize_script_source);
        register_serializer<Document>().format(SerializerFormat::TEXT);
        register_serializer<Font>().format(SerializerFormat::CUSTOM).deserializer(deserialize_font);
        register_serializer<Kit>()
            .format(SerializerFormat::CUSTOM)
            .deserializer(deserialize_kit)
            .serializer(serialize_kit);
        register_serializer<Sandbox>()
            .format(SerializerFormat::CUSTOM)
            .deserializer(deserialize_sandbox)
            .serializer(serialize_sandbox);
        register_serializer<Material>().format(SerializerFormat::DEFAULT);
        register_serializer<App>().format(SerializerFormat::DEFAULT);
    }
}
