#pragma once
#include <Tbx/Plugins/Plugin.h>

namespace Tbx::Plugins::Template
{
    class TemplatePlug final
        : public Plugin
        , public ILogger
    {
    public:
        TemplatePlug(Ref<EventBus> eventBus);
        ~TemplatePlug() override;
    };

    TBX_REGISTER_PLUGIN(TemplatePlug);
}

