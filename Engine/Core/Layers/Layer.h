#pragma once
#include "Events/Event.h"

namespace Toybox
{
    class Layer
    {
    public:
        explicit(false) Layer(const std::string_view& name);
        virtual ~Layer() = default;

        virtual void OnAttach() = 0;
        virtual void OnDetach() = 0;
        virtual void OnUpdate() = 0;
        virtual void OnEvent(Event& event) = 0;

        std::string GetName() const;

    private:
        std::string _name;
    };
}