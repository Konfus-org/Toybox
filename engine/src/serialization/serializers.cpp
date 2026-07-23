#include "tbx/serialization/serializers.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/reflection.generated.h"
#include "tbx/reflection/reflection.h"
#include "tbx/serialization/registration.h"

namespace tbx
{
    void register_builtin_serializers()
    {
        if (!is_reflection_ready())
        {
            TBX_ASSERT(false, "Serializer init is dependent on reflection, init that first!");
            return;
        }

        // Readiness IS the idempotency latch — a populated registry means we already ran.
        if (is_serialization_ready())
            return;

        // Every builtin serializer is generated from its type's [[tbx::serializable]] annotation
        // (tools/codegen). Add one by annotating the struct, not by editing this file.
        register_generated_serializers();

        // Sandbox is the one exception: it is a move-only toy container, not a reflected data type, so
        // it cannot be register_type'd (std::any needs a copyable type) — codegen skips it. Its custom
        // disk codec is registered here by hand.
        register_serializer<Sandbox>()
            .format(SerializerFormat::CUSTOM)
            .deserializer(deserialize_sandbox)
            .serializer(serialize_sandbox);
    }

    bool is_serialization_ready()
    {
        return !get_serializer_registry().get_all().empty();
    }

    void purge_serialization_registry()
    {
        get_serializer_registry().clear();
    }
}
