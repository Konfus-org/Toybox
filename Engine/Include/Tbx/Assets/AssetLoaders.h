//#pragma once
//#include "Tbx/Assets/Asset.h"
//#include "Tbx/PluginAPI/PluginServer.h"
//#include "Tbx/PluginAPI/PluginInterfaces.h"
//#include "Tbx/Graphics/Texture.h"
//#include "Tbx/Graphics/Shader.h"
//#include <type_traits>
//
//namespace Tbx
//{
//    class AssetLoader
//    {
//    public:
//        /// <summary>
//        /// Loads asset data of type AssetType from the specified filename.
//        /// </summary>
//        /// <typeparam name="AssetType">The type of asset to load</typeparam>
//        /// <param name="filename">The filename of the asset to load</param>
//        /// <returns>An Asset object containing the loaded asset, or an empty Asset object if the load failed.</returns>
//        template <typename AssetType>
//        AssetType Load(const std::string& filename)
//        {
//            if constexpr (std::is_same<AssetType, Shader>) 
//            {
//                auto plugin = PluginServer::GetPlugin<IShaderLoaderPlugin>();
//                TBX_ASSERT(plugin, "Shader loading plugin not found!");
//                return plugin->LoadShader(filename);
//            }
//            else if constexpr (std::is_same<AssetType, Texture>) 
//            {
//                auto plugin = PluginServer::GetPlugin<ITextureLoaderPlugin>();
//                TBX_ASSERT(plugin, "Texture loading plugin not found!");
//                return plugin->LoadTexture(filename);
//            }
//            else
//            {
//                TBX_ASSERT(false, "Loading an asset of type {} failed! This means this type of asset is currently unsupported.", typeid(AssetType).name());
//                return Asset();
//            }
//        }
//    };
//}