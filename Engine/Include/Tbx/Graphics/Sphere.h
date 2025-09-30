#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Stages/Toy.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Memory/Refs.h"
#include <algorithm>
#include <memory>

namespace Tbx
{
    /// <summary>
    /// A sphere represented by a center point and radius.
    /// </summary>
    struct TBX_EXPORT Sphere
    {
        Vector3 Center = Vector3::Zero;
        float Radius = 0.0f;
    };

    /// <summary>
    /// Simple bounding volume represented by a center point and radius.
    /// </summary>
    struct TBX_EXPORT BoundingSphere : public Sphere
    {
        /// <summary>
        /// Calculates a bounding sphere for a toy using its transform block if present.
        /// </summary>
        BoundingSphere(const Ref<Toy>& toy)
        {
            Vector3 position = Vector3::Zero;
            Vector3 scale = Vector3::One;

            if (toy->Blocks.Contains<Transform>())
            {
                const auto& transform = toy->Blocks.Get<Transform>();
                position = transform.Position;
                scale = transform.Scale;
            }

            Center = position;
            Radius = std::max({ std::abs(scale.X), std::abs(scale.Y), std::abs(scale.Z) }) * 0.75f;
        }
    };
}

