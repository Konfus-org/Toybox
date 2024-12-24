#pragma once
#include "Events/Event.h"

namespace Toybox
{
    class Layer
    {
    public:
        TBX_API explicit(false) Layer(const std::string_view& name);
        TBX_API virtual ~Layer() = default;

        TBX_API virtual void OnAttach() = 0;
        TBX_API virtual void OnDetach() = 0;
        TBX_API virtual void OnUpdate() = 0;
        TBX_API virtual void OnEvent(Event& event) = 0;

        TBX_API std::string GetName() const;

    private:
        std::string _name;
    };
}