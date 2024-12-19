#pragma once
#include "tbxapi.h"
#include "Events/Event.h"

namespace Toybox
{
    class TBX_API Layer
    {
    public:
        Layer(const std::string& name);
        virtual ~Layer() = default;

        virtual void OnAttach() = 0;
        virtual void OnDetach() = 0;
        virtual void OnUpdate() = 0;
        virtual void OnEvent(Event& event) = 0;

        const std::string GetName() const;

    private:
        std::string _name;
    };
}