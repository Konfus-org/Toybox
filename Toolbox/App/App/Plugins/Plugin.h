#pragma once
#include <Core/ToolboxAPI.h>
#include <Core/Event Dispatcher/Event.h>

namespace Tbx
{
    class TBX_API IPlugin
    {
    public:
        virtual ~IPlugin() = default;

        virtual void OnLoad() = 0;
        virtual void OnUnload() = 0;
    };

    template<typename T>
    class TBX_API Plugin : public IPlugin
    {
    public:
        Plugin() = default;

        std::shared_ptr<T> GetImplementation()
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