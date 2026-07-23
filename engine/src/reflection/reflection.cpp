#include "tbx/reflection/reflection.h"
#include "tbx/reflection.generated.h"

namespace tbx
{
    void initialize_reflection()
    {
        // Readiness IS the idempotency latch — a populated registry means we already ran.
        if (is_reflection_ready())
            return;

        // Every builtin type's registration is generated from its [[tbx::serializable]] annotation
        // (tools/codegen). Add a type by annotating its struct, not by editing this file.
        register_generated_types();
    }

    bool is_reflection_ready()
    {
        return !get_type_registry().get_all().empty();
    }

    void internal::purge_reflection_registry()
    {
        get_type_registry().clear();
    }
}
