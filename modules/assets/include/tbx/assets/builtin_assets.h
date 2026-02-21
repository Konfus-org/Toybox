#pragma once
#include "tbx/common/handle.h"

namespace tbx
{
    /// <summary>
    /// Purpose: Handle for the built-in shader include library at resources/Shaders/Globals.glsl.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle globals_shader_library = Handle(Uuid(0x4U));

    /// <summary>
    /// Purpose: Handle for the built-in DefaultLit vertex shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle lit_vertex_shader = Handle(Uuid(0x1U));

    /// <summary>
    /// Purpose: Handle for the built-in DefaultLit fragment shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle lit_fragment_shader = Handle(Uuid(0x5U));

    /// <summary>
    /// Purpose: Handle for the built-in DefaultLit material.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle lit_material = Handle(Uuid(0x2U));

    /// <summary>
    /// Purpose: Handle for the built-in placeholder texture returned when an asset cannot be found.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle not_found_texture = Handle(Uuid(0x3U));

    /// <summary>
    /// Purpose: Handle for the built-in DefaultUnlit vertex shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle unlit_vertex_shader = Handle(Uuid(0x6U));

    /// <summary>
    /// Purpose: Handle for the built-in DefaultUnlit fragment shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle unlit_fragment_shader = Handle(Uuid(0x7U));

    /// <summary>
    /// Purpose: Handle for the built-in DefaultUnlit material.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle unlit_material = Handle(Uuid(0x8U));

    /// <summary>
    /// Purpose: Legacy handle for the built-in default shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle default_shader = lit_vertex_shader;

    /// <summary>
    /// Purpose: Handle for the built-in deferred geometry vertex shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle deferred_geometry_vertex_shader = Handle(Uuid(0x9U));

    /// <summary>
    /// Purpose: Handle for the built-in deferred geometry fragment shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle deferred_geometry_fragment_shader = Handle(Uuid(0xAU));

    /// <summary>
    /// Purpose: Handle for the built-in deferred lighting vertex shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle deferred_lighting_vertex_shader = Handle(Uuid(0xBU));

    /// <summary>
    /// Purpose: Handle for the built-in deferred lighting fragment shader stage.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle deferred_lighting_fragment_shader = Handle(Uuid(0xCU));

    /// <summary>
    /// Purpose: Handle for the built-in deferred lighting material.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle deferred_lighting_material = Handle(Uuid(0xDU));

    /// <summary>
    /// Purpose: Legacy handle for the built-in default material.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning reference to a built-in asset.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    inline const Handle default_material = lit_material;
}
