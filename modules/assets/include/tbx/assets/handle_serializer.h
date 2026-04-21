#pragma once
#include "tbx/common/handle.h"
#include "tbx/files/ops.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: Defines read/write operations for asset handles stored in .meta sidecar files.
    /// @details
    /// Ownership: Returns owned Handle instances to callers and does not retain file resources.
    /// Thread Safety: Implementations are safe for concurrent use when injected file services are
    /// thread-safe.
    class TBX_API IAssetHandleSerializer
    {
      public:
        virtual ~IAssetHandleSerializer() = default;

        /// @brief
        /// Purpose: Reads an asset handle from disk and returns nullptr when metadata is missing or
        /// invalid.
        /// @details
        /// Ownership: Returns an owning unique_ptr to a Handle on success.
        /// Thread Safety: Safe when filesystem services used by the implementation are thread-safe.
        virtual std::unique_ptr<Handle> read_from_disk(
            const std::filesystem::path& working_directory,
            const std::filesystem::path& asset_path) const = 0;

        /// @brief
        /// Purpose: Reads an asset handle from disk through injected file operations.
        /// @details
        /// Ownership: Returns an owning unique_ptr to a Handle on success.
        /// Thread Safety: Safe when file_ops is thread-safe.
        virtual std::unique_ptr<Handle> read_from_disk(
            const IFileOps& file_ops,
            const std::filesystem::path& asset_path) const = 0;

        /// @brief
        /// Purpose: Parses an asset handle from raw .meta JSON text.
        /// @details
        /// Ownership: Returns an owning unique_ptr to a Handle on success.
        /// Thread Safety: Safe to call concurrently.
        virtual std::unique_ptr<Handle> read_from_source(
            std::string_view meta_text,
            const std::filesystem::path& asset_path) const = 0;

        /// @brief
        /// Purpose: Writes only the id field in the sidecar .meta file, preserving other metadata.
        /// @details
        /// Ownership: Performs a read-modify-write and does not retain file contents.
        /// Thread Safety: Safe when filesystem services used by the implementation are thread-safe.
        virtual bool try_write_to_disk(
            const std::filesystem::path& working_directory,
            const std::filesystem::path& asset_path,
            const Handle& handle) const = 0;

        /// @brief
        /// Purpose: Writes only the id field in the sidecar .meta file via injected file services.
        /// @details
        /// Ownership: Performs a read-modify-write and does not retain file contents.
        /// Thread Safety: Safe when file_ops is thread-safe.
        virtual bool try_write_to_disk(
            IFileOps& file_ops,
            const std::filesystem::path& asset_path,
            const Handle& handle) const = 0;
    };

    /// @brief
    /// Purpose: Default .meta serializer that reads and writes asset handle ids.
    /// @details
    /// Ownership: Returns owned Handle values and does not retain file content across calls.
    /// Thread Safety: Safe to call concurrently when file services are thread-safe.
    class TBX_API AssetHandleSerializer final : public IAssetHandleSerializer
    {
      public:
        std::unique_ptr<Handle> read_from_disk(
            const std::filesystem::path& working_directory,
            const std::filesystem::path& asset_path) const override;

        std::unique_ptr<Handle> read_from_disk(
            const IFileOps& file_ops,
            const std::filesystem::path& asset_path) const override;

        std::unique_ptr<Handle> read_from_source(
            std::string_view meta_text,
            const std::filesystem::path& asset_path) const override;

        bool try_write_to_disk(
            const std::filesystem::path& working_directory,
            const std::filesystem::path& asset_path,
            const Handle& handle) const override;

        bool try_write_to_disk(
            IFileOps& file_ops,
            const std::filesystem::path& asset_path,
            const Handle& handle) const override;
    };
}
