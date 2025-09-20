#include "Tbx/PCH.h"
#include "Tbx/TSS/Toy.h"
#include "Tbx/Debug/Debugging.h"
#include <algorithm>

namespace Tbx
{
    Toy::Toy(const std::string& name)
        : _handle(name)
    {
    }

    Tbx::Ref<Toy> Toy::EmplaceChild(const std::string& name)
    {
        auto child = std::make_shared<Toy>(name);
        child->_parent = shared_from_this();
        _children.push_back(child);
        return child;
    }

    Tbx::Ref<Toy> Toy::AddChild(const Tbx::Ref<Toy>& child)
    {
        if (!child)
        {
            TBX_ASSERT(child, "Cannot add a null child!");
            return nullptr;
        }

        child->_parent = shared_from_this();
        _children.push_back(child);
        return child;
    }

    void Toy::RemoveChild(const Tbx::Ref<Toy>& child)
    {
        auto it = std::find(_children.begin(), _children.end(), child);
        if (it != _children.end())
        {
            (*it)->_parent.reset();
            _children.erase(it);
        }
    }

    Tbx::Ref<Toy> Toy::GetChild(const ToyHandle& handle) const
    {
        for (const auto& child : _children)
        {
            if (child->GetHandle().GetId() == handle.GetId())
            {
                return child;
            }
        }
        return {};
    }

    Tbx::Ref<Toy> Toy::FindChild(std::string_view name) const
    {
        for (const auto& child : _children)
        {
            if (child->GetName() == name)
            {
                return child;
            }
        }
        return {};
    }

    const std::vector<Tbx::Ref<Toy>>& Toy::GetChildren() const
    {
        return _children;
    }

    void Toy::SetParent(const Tbx::Ref<Toy>& parent)
    {
        _parent = parent;
    }

    Tbx::Ref<Toy> Toy::GetParent() const
    {
        return _parent.lock();
    }

    const std::string& Toy::GetName() const
    {
        return _handle.Name;
    }

    const ToyHandle& Toy::GetHandle() const
    {
        return _handle;
    }

    void Toy::SetEnabled(bool enabled)
    {
        _enabled = enabled;
    }

    bool Toy::IsEnabled() const
    {
        return _enabled;
    }

    void Toy::Update()
    {
        if (!_enabled)
        {
            return;
        }

        OnUpdate();
        for (auto& child : _children)
        {
            child->Update();
        }
    }
}

