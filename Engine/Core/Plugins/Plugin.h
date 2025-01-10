#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"
#include "Events/Event.h"

namespace Tbx
{
    class TBX_API Plugin
    {
    public:
        Plugin() = default;
        virtual ~Plugin() = default;

        virtual void OnLoad() = 0;
        virtual void OnUnload() = 0;
    };

    // TODO: we will remove the factory plugin and make all plugins event based... i.e. they listen to events
    template<typename T>
    class TBX_API FactoryPlugin : public Plugin
    {
    public:
        std::shared_ptr<T> CreateShared()
        {
            auto* ptr = Create();
            auto shared = std::shared_ptr<T>(ptr, [this](T* ptrToDestroy) 
            { 
                Destroy(ptrToDestroy); 
            });
            return shared;
        }

        std::unique_ptr<T> CreateUnique()
        {
            auto* ptr = Create();
            auto unique = std::unique_ptr<T>(ptr, [this](T* ptrToDestroy) 
            { 
                Destroy(ptrToDestroy); 
            });
            return unique;
        }

    protected:
        virtual T* Create() = 0;
        virtual void Destroy(T* toDestroy) = 0;
    };
}