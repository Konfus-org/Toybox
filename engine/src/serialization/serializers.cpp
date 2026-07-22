#include "tbx/serialization/serializers.h"
#include "tbx/app.h"
#include "tbx/audio/clip.h"
#include "tbx/ecs/kit.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/gfx/material.h"
#include "tbx/gfx/model.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/gfx/texture.h"
#include "tbx/scripting/source.h"
#include "tbx/serialization/registration.h"
#include "tbx/ui/document.h"
#include "tbx/ui/font.h"

namespace tbx
{
    void register_builtin_serializers()
    {
        // Types with real codecs bring a reader (and a writer when writing makes sense);
        // text assets are SerializerFormat::TEXT; plain data types round-trip through their
        // reflection.
        register_serializer<Texture>()
            .format(SerializerFormat::CUSTOM)
            .extension(".png")
            .extension(".jpg")
            .extension(".jpeg")
            .extension(".tga")
            .extension(".bmp")
            .deserializer(deserialize_texture)
            .serializer(serialize_texture);
        register_serializer<Model>()
            .format(SerializerFormat::CUSTOM)
            .extension(".fbx")
            .extension(".obj")
            .extension(".gltf")
            .extension(".glb")
            .deserializer(deserialize_model);
        register_serializer<ShaderSource>()
            .format(SerializerFormat::TEXT)
            .extension(".vert")
            .extension(".frag")
            .extension(".geom")
            .extension(".glsl");
        register_serializer<AudioClip>()
            .format(SerializerFormat::CUSTOM)
            .extension(".wav")
            .extension(".ogg")
            .extension(".mp3")
            .extension(".flac")
            .deserializer(deserialize_clip);
        register_serializer<ScriptSource>()
            .format(SerializerFormat::CUSTOM)
            .extension(".luau")
            .extension(".lua")
            .deserializer(deserialize_script_source);
        register_serializer<Document>().format(SerializerFormat::TEXT).extension(".html");
        register_serializer<Font>()
            .format(SerializerFormat::CUSTOM)
            .extension(".otf")
            .extension(".ttf")
            .deserializer(deserialize_font);
        register_serializer<Kit>()
            .format(SerializerFormat::CUSTOM)
            .extension(".kit")
            .deserializer(deserialize_kit)
            .serializer(serialize_kit);
        register_serializer<Sandbox>()
            .format(SerializerFormat::CUSTOM)
            .extension(".kit")
            .deserializer(deserialize_sandbox)
            .serializer(serialize_sandbox);
        register_serializer<Material>().format(SerializerFormat::DEFAULT).extension(".mat");
        register_serializer<App>().format(SerializerFormat::DEFAULT).extension(".tapp");
    }
}
