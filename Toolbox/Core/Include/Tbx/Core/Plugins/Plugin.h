#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Events/Event.h"
#include <memory>

namespace Tbx
{
    class EXPORT IPlugin
    {
    public:
        virtual ~IPlugin() = default;

        virtual void OnLoad() = 0;
        virtual void OnUnload() = 0;
    };

    template<typename T>
    class Plugin : public IPlugin
    {
    public:
        EXPORT Plugin() = default;

        /// <summary>
        /// Returns a shared pointer to a new implementation of T
        /// </summary>
        EXPORT std::shared_ptr<T> ProvideImplementation()
        {
            auto* ptr = Provide();
            auto shared = std::shared_ptr<T>(ptr, [this](T* ptrToDestroy)
            {
                Destroy(ptrToDestroy);
            });
            return shared;
        }
        
    protected:
        virtual T* Provide() = 0;
        virtual void Destroy(T* toDestroy) = 0;
    };
}