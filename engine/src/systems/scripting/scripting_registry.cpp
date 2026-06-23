#include "tbx/systems/scripting/scripting_registry.h"
#include <algorithm>
#include <utility>

namespace tbx
{
    Result ScriptingRegistry::register_backend(std::shared_ptr<IScriptingBackend> backend, Uuid owner)
    {
        if (!backend)
            return Result(false, "Cannot register a null scripting backend.");

        const auto info = backend->language();
        for (const auto& entry : _backends)
        {
            if (entry.backend && entry.backend->language().name == info.name)
                return Result(
                    false,
                    "A scripting backend for language '" + info.name + "' is already registered.");
        }

        _backends.push_back(Entry { .owner = owner, .backend = std::move(backend) });
        return Result();
    }

    void ScriptingRegistry::unregister_all(Uuid owner)
    {
        std::erase_if(
            _backends,
            [&owner](const Entry& entry)
            {
                return entry.owner == owner;
            });
    }

    std::vector<std::shared_ptr<IScriptingBackend>> ScriptingRegistry::backends() const
    {
        auto result = std::vector<std::shared_ptr<IScriptingBackend>> {};
        result.reserve(_backends.size());
        for (const auto& entry : _backends)
        {
            if (entry.backend)
                result.push_back(entry.backend);
        }
        return result;
    }

    std::weak_ptr<IScriptingBackend> ScriptingRegistry::for_language(std::string_view name) const
    {
        for (const auto& entry : _backends)
        {
            if (entry.backend && entry.backend->language().name == name)
                return entry.backend;
        }
        return {};
    }

    std::weak_ptr<IScriptingBackend> ScriptingRegistry::for_extension(std::string_view extension) const
    {
        for (const auto& entry : _backends)
        {
            if (!entry.backend)
                continue;

            const auto info = entry.backend->language();
            if (std::ranges::find(info.extensions, extension) != info.extensions.end())
                return entry.backend;
        }
        return {};
    }
}
