#pragma once
#include "Tbx/Assets/AssetLoaders.h"
#include "Tbx/Plugins/Plugin.h"

namespace Tbx::Plugins::TIMS
{
    struct TIMSText
        : public Text
        , public IProductOfPluginFactory
    {
        using Text::Text;
    };

    class TIMSTextLoaderPlugin final
        : public FactoryPlugin<TIMSText>
        , public ITextLoader
    {
      public:
        TIMSTextLoaderPlugin(Ref<EventBus> eventBus) {}
        bool CanLoadText(const std::filesystem::path& filepath) const final;
        Ref<Text> LoadText(const std::filesystem::path& filepath) final;
    };

    TBX_REGISTER_PLUGIN(TIMSTextLoaderPlugin);
}
