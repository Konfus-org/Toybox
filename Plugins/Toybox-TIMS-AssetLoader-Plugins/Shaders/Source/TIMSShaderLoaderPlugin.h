#pragma once
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Assets/AssetLoaders.h"

namespace Tbx::Plugins::TIMS
{
    struct TIMSShader : public Shader
    {
        using Shader::Shader;
    };

    class TIMSShaderLoaderPlugin final
        : public Plugin
        , public IShaderLoader
    {
    public:
        TIMSShaderLoaderPlugin(Ref<EventBus> eventBus) {}
        bool CanLoadShader(const std::filesystem::path& filepath) const final;
        Ref<Shader> LoadShader(const std::filesystem::path& filepath) final;
    };

    TBX_REGISTER_PLUGIN(TIMSShaderLoaderPlugin);
}