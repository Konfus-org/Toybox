#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/ECS/ThreeDSpace.h"
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    /// <summary>
    /// The game world or root 3d space.
    /// Can have different layers to organize data.
    /// </summary>
    class EXPORT WorldLayer : public Layer
    {
    public:
        WorldLayer() : Layer("World")
        {
        }

        /// <summary>
        /// Get the world space.
        /// </summary>
        std::shared_ptr<ThreeDSpace> GetWorldSpace();

    protected:
        void OnUpdate() override;

    private:
        std::shared_ptr<ThreeDSpace> _worldSpace = {};
    };
}

