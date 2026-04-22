#pragma once
#include "tbx/systems/graphics/light.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/post_processing.h"
#include "tbx/systems/math/transform.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include <vector>


namespace tbx
{
    /// @brief
    /// Purpose: Describes one renderable mesh/material pair already uploaded to a graphics backend.
    /// @details
    /// Ownership: Stores resource identifiers and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphRenderable
    {
        Uuid entity_uuid = {};
        Uuid mesh_resource_uuid = {};
        Uuid material_resource_uuid = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one point light submitted to a graphics backend.
    /// @details
    /// Ownership: Stores light and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphPointLight
    {
        Uuid entity_uuid = {};
        PointLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one spot light submitted to a graphics backend.
    /// @details
    /// Ownership: Stores light and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphSpotLight
    {
        Uuid entity_uuid = {};
        SpotLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one area light submitted to a graphics backend.
    /// @details
    /// Ownership: Stores light and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphAreaLight
    {
        Uuid entity_uuid = {};
        AreaLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Describes one directional light submitted to a graphics backend.
    /// @details
    /// Ownership: Stores light and transform data by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraphDirectionalLight
    {
        Uuid entity_uuid = {};
        DirectionalLight light = {};
        Transform transform = {};
        bool is_visible = true;
    };

    /// @brief
    /// Purpose: Stores backend-neutral scene data for one render submission.
    /// @details
    /// Ownership: Owns all submission lists and scene-level render settings by value.
    /// Thread Safety: Safe for concurrent reads; synchronize mutation externally.
    struct TBX_API RenderGraph
    {
        std::vector<RenderGraphRenderable> renderables = {};
        std::vector<RenderGraphPointLight> point_lights = {};
        std::vector<RenderGraphSpotLight> spot_lights = {};
        std::vector<RenderGraphAreaLight> area_lights = {};
        std::vector<RenderGraphDirectionalLight> directional_lights = {};
        Sky sky = {};
        PostProcessing post_processing = {};
    };
}
