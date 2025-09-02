#pragma once
#include "Tbx/DllExport.h"
#include <any>

namespace Tbx
{
    /// <summary>
    /// The type of render command AKA what the renderer should do.
    /// </summary>
    enum class EXPORT DrawCommandType
    {
        /// <summary>
        /// Does nothing.
        /// </summary>
        None = 0,
        /// <summary>
        /// Clears the screen.
        /// </summary>
        Clear = 1,
        /// <summary>
        /// Compiles a materials shader.
        /// </summary>
        CompileMaterial = 2,
        /// <summary>
        /// Sets the material to use for rendering.
        /// </summary>
        SetMaterial = 4,
        /// <summary>
        /// Uploads a uniform to the GPU (things like view matrix, model position, color, etc..).
        /// </summary>
        UploadUniform = 5,
        /// <summary>
        /// Uploads a mesh to the GPU.
        /// </summary>
        UploadMesh = 6,
        /// <summary>
        /// Renders a mesh.
        /// </summary>
        DrawMesh = 7,
    };

    /// <summary>
    /// A render command is an instruction type and a payload for a renderer.
    /// </summary>
    struct DrawCommand
    {
    public:
        EXPORT DrawCommand() = default;
        EXPORT DrawCommand(const DrawCommandType& type, const std::any& payload)
            : _type(type), _payload(payload) {}

        EXPORT const DrawCommandType& GetType() const { return _type; }
        EXPORT const std::any& GetPayload() const { return _payload; }

    private:
        DrawCommandType _type = DrawCommandType::None;
        std::any _payload = nullptr;
    };
}