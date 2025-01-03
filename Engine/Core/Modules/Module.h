#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"
#include "ModuleAPI.h"

namespace Tbx
{
    class TBX_API Module
    {
    public:
        Module() = default;
        virtual ~Module() = default;

        virtual std::string GetName() const = 0;
        virtual std::string GetAuthor() const = 0;
        virtual int GetVersion() const = 0;
    };

    template<typename T>
    class TBX_API FactoryModule : public Module
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

    protected:
        virtual T* Create() = 0;
        virtual void Destroy(T* toDestroy) = 0;
    };
}