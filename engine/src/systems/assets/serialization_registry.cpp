#include "tbx/systems/assets/serialization_registry.h"
#include <utility>

namespace tbx
{
    SerializationRegistry::SerializationRegistry()
        : _file_ops(std::make_shared<FileOperator>())
    {
    }

    SerializationRegistry::SerializationRegistry(std::shared_ptr<IFileOps> file_ops)
        : _file_ops(std::move(file_ops))
    {
    }

    std::shared_future<Result> SerializationRegistry::make_ready_future(Result result)
    {
        std::promise<Result> promise = {};
        promise.set_value(std::move(result));
        return promise.get_future().share();
    }

    Result SerializationRegistry::make_failed_result(std::string report)
    {
        auto result = Result {};
        result.flag_failure(std::move(report));
        return result;
    }

    Result SerializationRegistry::try_read_tbx_asset_common_meta(
        const Json& data,
        const std::filesystem::path& meta_path,
        uint32 expected_version,
        AssetLoadMetadata& out_metadata)
    {
        data.try_get("id", out_metadata.id);

        auto version = uint32();
        if (!data.try_get("version", version))
        {
            if (expected_version != 0U)
            {
                return make_failed_result(
                    std::string("Toybox asset meta '")
                        .append(meta_path.string())
                        .append("' is missing version ")
                        .append(std::to_string(expected_version))
                        .append(". The file needs versioned."));
            }

            return {};
        }

        if (expected_version != 0U && version != expected_version)
        {
            return make_failed_result(
                std::string("Toybox asset meta '")
                    .append(meta_path.string())
                    .append("' has version ")
                    .append(std::to_string(version))
                    .append(" but expected version ")
                    .append(std::to_string(expected_version))
                    .append(". The file needs versioned."));
        }

        out_metadata.version = version;
        return {};
    }

    void SerializationRegistry::apply_tbx_asset_common_meta(
        const AssetLoadMetadata& metadata,
        Asset& asset)
    {
        asset.id = metadata.id;
        asset.version = metadata.version;
    }
}
