#pragma once
#include "tbx/systems/messaging/message.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include <filesystem>
#include <utility>

namespace tbx
{
    struct TBX_API AssetFileEvent : public Event
    {
        AssetFileEvent(
            std::filesystem::path watched_asset_directory_path,
            std::filesystem::path changed_asset_path,
            Handle handle = {})
            : watched_asset_directory(std::move(watched_asset_directory_path))
            , asset_path(std::move(changed_asset_path))
            , affected_asset(std::move(handle))
        {
        }

        std::filesystem::path watched_asset_directory = {};
        std::filesystem::path asset_path = {};
        Handle affected_asset = {};
    };

    struct TBX_API AssetCreatedEvent : public AssetFileEvent
    {
        using AssetFileEvent::AssetFileEvent;
    };

    struct TBX_API AssetModifiedEvent : public AssetFileEvent
    {
        using AssetFileEvent::AssetFileEvent;
    };

    struct TBX_API AssetRemovedEvent : public AssetFileEvent
    {
        using AssetFileEvent::AssetFileEvent;
    };

    struct TBX_API AssetReloadedEvent : public Event
    {
        AssetReloadedEvent(Handle handle = {}, bool was_successful = false)
            : affected_asset(std::move(handle))
            , succeeded(was_successful)
        {
        }

        Handle affected_asset = {};
        bool succeeded = false;
    };
}
