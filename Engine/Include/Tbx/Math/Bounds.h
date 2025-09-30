#pragma once
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    struct TBX_EXPORT Bounds : public IPrintable
    {
    public:
        Bounds(float left, float right, float top, float bottom) 
            : Left(left), Right(right), Top(top), Bottom(bottom) {}

        std::string ToString() const override;

        static Bounds FromOrthographicProjection(float size, float aspect);
        static Bounds FromPerspectiveProjection(float fov, float aspectRatio, float zNear);

        static Bounds Identity;

        float Left;
        float Right;
        float Top;
        float Bottom;
    };
}
