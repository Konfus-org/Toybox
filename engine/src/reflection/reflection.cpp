#include "tbx/reflection/reflection.h"
#include "reflection_internal.h"
#include "tbx/reflection.generated.h"

namespace tbx
{
    // Dedicated idempotency latch. Do NOT infer "already ran" from registry emptiness:
    // a caller may register_type<T>() before us and pre-populate the registry, which would
    // fool an emptiness check into skipping the generated builtins entirely.
    static bool g_reflection_initialized = false;

    void internal::initialize_reflection()
    {
        if (g_reflection_initialized)
            return;

        // Every builtin type's registration is generated from its [[tbx::serializable]] annotation
        // (tools/codegen). Add a type by annotating its struct, not by editing this file.
        register_generated_types();

        g_reflection_initialized = true;
    }

    bool is_reflection_ready()
    {
        return !get_type_registry().get_all().empty();
    }

    void internal::purge_reflection_registry()
    {
        get_type_registry().clear();
        g_reflection_initialized = false;
    }
}
