#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/reflection/attributes.h"
#include "tbx/ui/document.h"
#include "tbx/utils/typedefs.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace tbx
{
    enum class UIMode : uint8
    {
        SCREENSPACE,
        WORLDSPACE,
    };

    /// @brief
    /// Purpose: On-screen UI owned by a toy: an RML document shown while the toy lives and
    /// is enabled (the render graph's ui pass manages loading/visibility).
    struct TBX_SERIALIZABLE() TBX_DLL_EXPORT UI : Block
    {
        AssetHandle<Document> document = {};
        AssetHandle<ShaderSource> vertex = {}; // custom stage; unset = the builtin ui.vert
        AssetHandle<ShaderSource> fragment = {}; // custom stage; unset = the builtin ui.frag
        UIMode mode = UIMode::SCREENSPACE;

        // Live value sources this document reads, keyed by document slot (data-text/-width/
        // -style name). Scripts register these via ui:bind(slot, getter); the ui pass evaluates
        // them each frame it draws this block. Runtime only — never serialized (a fresh decode
        // carries none; scripts re-bind on start), so it is not a reflected field.
        TBX_DO_NOT_SERIALIZE std::unordered_map<std::string, std::function<std::string()>> bindings = {};

        // Fluent setters — each returns *this for one-chain construction.
        UI& set_document(AssetHandle<Document> value)
        {
            document = std::move(value);
            return *this;
        }
        UI& set_vertex(AssetHandle<ShaderSource> value)
        {
            vertex = std::move(value);
            return *this;
        }
        UI& set_fragment(AssetHandle<ShaderSource> value)
        {
            fragment = std::move(value);
            return *this;
        }
        UI& set_mode(UIMode value)
        {
            mode = value;
            return *this;
        }
    };
}
