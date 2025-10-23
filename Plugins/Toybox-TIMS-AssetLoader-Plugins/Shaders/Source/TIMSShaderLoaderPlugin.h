#pragma once
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Assets/AssetLoaders.h"

namespace Tbx::Plugins::TIMS
{
    struct TIMSShader : public Shader, public IProductOfPluginFactory
    {
        using Shader::Shader;
    };

    class TIMSShaderLoaderPlugin final
        : public FactoryPlugin<TIMSShader>
        , public IShaderLoader
    {
    public:
        TIMSShaderLoaderPlugin(Ref<EventBus> eventBus) {}
        bool CanLoadShader(const std::filesystem::path& filepath) const final;
        Ref<Shader> LoadShader(const std::filesystem::path& filepath) final;
    };

    TBX_REGISTER_PLUGIN(TIMSShaderLoaderPlugin);
}