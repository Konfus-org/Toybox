#include "Tbx/PCH.h"
#include "Tbx/Assets/Asset.h"

namespace Tbx
{
    std::shared_ptr<IShaderLoaderPlugin> AssetLoader::_shaderLoaderPlugin = nullptr;
    std::shared_ptr<ITextureLoaderPlugin> AssetLoader::_textureLoaderPlugin = nullptr;

    std::shared_ptr<IShaderLoaderPlugin> AssetLoader::GetShaderLoader()
    {
        if (!_shaderLoaderPlugin)
        {
            _shaderLoaderPlugin = PluginServer::GetPlugin<IShaderLoaderPlugin>();
        }
        return _shaderLoaderPlugin;
    }

    std::shared_ptr<ITextureLoaderPlugin> AssetLoader::GetTextureLoader()
    {
        if (!_textureLoaderPlugin)
        {
            _textureLoaderPlugin = PluginServer::GetPlugin<ITextureLoaderPlugin>();
        }
        return _textureLoaderPlugin;
    }
}