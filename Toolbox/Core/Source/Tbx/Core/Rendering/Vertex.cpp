#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Rendering/Vertex.h"

namespace Tbx
{
    static VertexBuffer VertexVectorToBuffer(const std::vector<Vertex>& vertices)
    {
        auto numberOfVertices = vertices.size();
        std::vector<float> meshPoints(numberOfVertices * 12);
        int positionToPlace = 0;

        for (const auto& vertex : vertices)
        {
            const auto& position = vertex.Position;
            meshPoints[positionToPlace] = position.X;
            meshPoints[positionToPlace + 1] = position.Y;
            meshPoints[positionToPlace + 2] = position.Z;

            const auto& color = vertex.Color;
            meshPoints[positionToPlace + 3] = color.R;
            meshPoints[positionToPlace + 4] = color.G;
            meshPoints[positionToPlace + 5] = color.B;
            meshPoints[positionToPlace + 6] = color.A;

            const auto& normal = vertex.Normal;
            meshPoints[positionToPlace + 7] = normal.X;
            meshPoints[positionToPlace + 8] = normal.Y;
            meshPoints[positionToPlace + 9] = normal.Z;

            const auto& textCoord = vertex.TexCoord;
            meshPoints[positionToPlace + 10] = (float)textCoord.X;
            meshPoints[positionToPlace + 11] = (float)textCoord.Y;

            positionToPlace += 12;
        }

        const BufferLayout& bufferLayout =
        {
            { ShaderDataType::Float3, "position" },
            { ShaderDataType::Float4, "color" },
            { ShaderDataType::Float3, "normal" },
            { ShaderDataType::Float2, "textureCoord" },
        };

        return VertexBuffer(meshPoints, bufferLayout);
    }
}
