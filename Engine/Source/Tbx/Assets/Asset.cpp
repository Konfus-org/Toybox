#include "Tbx/PCH.h"
#include "Tbx/Assets/Asset.h"

namespace Tbx
{
    /*std::shared_ptr<IShaderLoaderPlugin> AssetLoader::_shaderLoaderPlugin = nullptr;
    std::shared_ptr<ITextureLoaderPlugin> AssetLoader::_textureLoaderPlugin = nullptr;*/

    std::shared_ptr<IShaderLoaderPlugin> AssetLoader::GetShaderLoader()
    {
        /*if (!_shaderLoaderPlugin)
        {
            _shaderLoaderPlugin = PluginServer::Get<IShaderLoaderPlugin>();
        }*/
        return PluginServer::Get<IShaderLoaderPlugin>();
    }

    std::shared_ptr<ITextureLoaderPlugin> AssetLoader::GetTextureLoader()
    {
        /*if (!_textureLoaderPlugin)
        {
            _textureLoaderPlugin = PluginServer::Get<ITextureLoaderPlugin>();
        }*/
        return PluginServer::Get<ITextureLoaderPlugin>();
    }
}