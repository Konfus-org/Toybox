#include "tbx/serialization/serializers.h"
#include "serializers_internal.h"
#include "tbx/debug/log.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/reflection.generated.h"
#include "tbx/reflection/reflection.h"
#include "tbx/serialization/registration.h"

namespace tbx
{
    // Dedicated idempotency latch. Do NOT infer "already ran" from registry emptiness:
    // a caller may register_serializer<T>() before us and pre-populate the registry, which
    // would fool an emptiness check into skipping the generated builtins entirely.
    namespace internal { static bool g_serializers_initialized = false; }

    void internal::register_builtin_serializers()
    {
        if (!is_reflection_ready())
        {
            TBX_ASSERT(false, "Serializer init is dependent on reflection, init that first!");
            return;
        }

        if (g_serializers_initialized)
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

        g_serializers_initialized = true;
    }

    bool is_serialization_ready()
    {
        return !get_serializer_registry().get_all().empty();
    }

    void internal::purge_serialization_registry()
    {
        get_serializer_registry().clear();
        g_serializers_initialized = false;
    }
}
