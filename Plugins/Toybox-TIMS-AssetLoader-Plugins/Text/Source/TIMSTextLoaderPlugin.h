#pragma once
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Assets/AssetLoaders.h"

namespace Tbx::Plugins::TIMS
{
    struct TIMSText : public Text
    {
        using Text::Text;
    };

    class TIMSTextLoaderPlugin final
        : public Plugin
        , public ITextLoader
    {
    public:
        TIMSTextLoaderPlugin(Ref<EventBus> eventBus) {}
        bool CanLoadText(const std::filesystem::path& filepath) const final;
        Ref<Text> LoadText(const std::filesystem::path& filepath) final;
    };

    TBX_REGISTER_PLUGIN(TIMSTextLoaderPlugin);
}