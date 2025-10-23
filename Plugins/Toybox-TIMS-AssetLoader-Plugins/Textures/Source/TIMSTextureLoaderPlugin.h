#pragma once
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Assets/AssetLoaders.h"

namespace Tbx::Plugins::TIMS
{
    struct TIMSTexture : public Texture, public IProductOfPluginFactory
    {
        using Texture::Texture;
    };

    class TIMSTextureLoaderPlugin final
        : public FactoryPlugin<TIMSTexture>
        , public ITextureLoader
    {
    public:
        TIMSTextureLoaderPlugin(Ref<EventBus> eventBus) {}
        bool CanLoadTexture(const std::filesystem::path& filepath) const final;
        Ref<Texture> LoadTexture(const std::filesystem::path& filepath) final;
    };

    TBX_REGISTER_PLUGIN(TIMSTextureLoaderPlugin);
}