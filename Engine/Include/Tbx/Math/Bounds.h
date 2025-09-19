#pragma once
#include "Tbx/Core/StringConvertible.h"
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    struct EXPORT Bounds : public IStringConvertible
    {
    public:
        Bounds(float left, float right, float top, float bottom) 
            : Left(left), Right(right), Top(top), Bottom(bottom) {}

        std::string ToString() const override;

        static Bounds FromOrthographicProjection(float size, float aspect);
        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear);

        inline static const Bounds Identity = Bounds(-1.0f, 1.0f, -1.0f, 1.0f);

        float Left;
        float Right;
        float Top;
        float Bottom;
    };
}
