#include "Tbx/PCH.h"
#include "Tbx/TBS/Toy.h"
#include <algorithm>

namespace Tbx
{
    Toy::Toy(const std::string& name) :
        _handle(name)
    {
    }

    void Toy::AddChild(const std::shared_ptr<Toy>& child)
    {
        if (child)
        {
            child->_parent = shared_from_this();
            _children.push_back(child);
        }
    }

    void Toy::RemoveChild(const std::shared_ptr<Toy>& child)
    {
        auto it = std::find(_children.begin(), _children.end(), child);
        if (it != _children.end())
        {
            (*it)->_parent.reset();
            _children.erase(it);
        }
    }

    std::shared_ptr<Toy> Toy::GetChild(const ToyHandle& handle) const
    {
        for (const auto& child : _children)
        {
            if (child->GetHandle() == handle)
            {
                return child;
            }
        }
        return {};
    }

    std::shared_ptr<Toy> Toy::FindChild(std::string_view name) const
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

    const std::vector<std::shared_ptr<Toy>>& Toy::GetChildren() const
    {
        return _children;
    }

    void Toy::SetParent(const std::shared_ptr<Toy>& parent)
    {
        _parent = parent;
    }

    std::shared_ptr<Toy> Toy::GetParent() const
    {
        return _parent.lock();
    }

    const std::string& Toy::GetName() const
    {
        return _handle.GetName();
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

