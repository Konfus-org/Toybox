#pragma once

#if defined(TBX_PLATFORM_WINDOWS) && defined(TBX_SHARED_LIB)
    #define TBX_PLUGIN_INCLUDE_EXPORT __declspec(dllexport)
    #define TBX_PLUGIN_INCLUDE_IMPORT __declspec(dllimport)
#else
    #define TBX_PLUGIN_INCLUDE_EXPORT
    #define TBX_PLUGIN_INCLUDE_IMPORT
#endif

#define TBX_PLUGIN_INCLUDE_API(exporting_flag) TBX_PLUGIN_INCLUDE_API_VALUE(exporting_flag)
#define TBX_PLUGIN_INCLUDE_API_VALUE(exporting_flag) TBX_PLUGIN_INCLUDE_API_##exporting_flag
#define TBX_PLUGIN_INCLUDE_API_0 TBX_PLUGIN_INCLUDE_IMPORT
#define TBX_PLUGIN_INCLUDE_API_1 TBX_PLUGIN_INCLUDE_EXPORT
