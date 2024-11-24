#pragma once
#include "ToyboxAPI.h"
#include "Events/Event.h"

namespace Toybox::Layers
{
    class TOYBOX_API Layer
    {
    public:
        Layer(const std::string& name);
        virtual ~Layer() = default;

        virtual void OnAttach() = 0;
        virtual void OnDetach() = 0;
        virtual void OnUpdate() = 0;
        virtual void OnEvent(Events::Event& event) = 0;

        const std::string GetName() const;

    private:
        std::string _name;
    };
}