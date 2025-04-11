#pragma once
#include "Tbx/App/Render Pipeline/DefaultShader.h"
#include <Tbx/Core/Rendering/Material.h>

namespace Tbx
{
    const Material& DefaultMaterial = Material(DefaultShader);
}