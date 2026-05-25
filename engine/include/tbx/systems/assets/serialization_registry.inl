#pragma once

namespace tbx
{
    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::register_loader(
        Loader<TAsset> loader,
        AsyncLoader<TAsset> async_loader)
    {
        TBX_ASSERT(
            static_cast<bool>(loader) || static_cast<bool>(async_loader),
            "Serialization registry requires a sync or async loader for type '{}'.",
            typeid(TAsset).name());

        std::lock_guard lock(_mutex);
        auto& registration = get_or_create_registration<TAsset>();
        registration.loader = std::move(loader);
        registration.async_loader = std::move(async_loader);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::deregister_loader()
    {
        std::lock_guard lock(_mutex);
        auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return;

        registration->get().loader = {};
        registration->get().async_loader = {};
        erase_registration_if_empty<TAsset>();
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool SerializationRegistry::has_loader() const
    {
        std::lock_guard lock(_mutex);
        const auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return false;

        return static_cast<bool>(registration->get().loader)
               || static_cast<bool>(registration->get().async_loader);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::register_transformer(Transformer<TAsset> transformer)
    {
        TBX_ASSERT(
            static_cast<bool>(transformer),
            "Serialization registry requires a transformer for type '{}'.",
            typeid(TAsset).name());

        std::lock_guard lock(_mutex);
        auto& registration = get_or_create_registration<TAsset>();
        registration.transformers.push_back(std::move(transformer));
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::deregister_transformer()
    {
        std::lock_guard lock(_mutex);
        auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return;

        registration->get().transformers.clear();
        erase_registration_if_empty<TAsset>();
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool SerializationRegistry::has_transformer() const
    {
        std::lock_guard lock(_mutex);
        const auto registration = find_registration<TAsset>();
        return registration.has_value() && !registration->get().transformers.empty();
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool SerializationRegistry::can_read() const
    {
        std::lock_guard lock(_mutex);
        const auto registration = find_registration<TAsset>();
        const bool has_registered_loader =
            registration.has_value()
            && (static_cast<bool>(registration->get().loader)
                || static_cast<bool>(registration->get().async_loader));
        if (has_registered_loader)
            return true;

        const bool has_registered_transformer =
            registration.has_value() && !registration->get().transformers.empty();
        return _file_ops != nullptr
               && (has_registered_transformer || HAS_TBX_ASSET_SERIALIZATION_V<TAsset>
                   || HAS_TBX_META_SERIALIZATION_V<TAsset> || HAS_TBX_TEXT_SERIALIZATION_V<TAsset>);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    AssetReadResult<TAsset> SerializationRegistry::read_result(
        const std::filesystem::path& asset_path,
        const AssetLoadParameters<TAsset>& parameters) const
    {
        Loader<TAsset> loader = {};
        std::vector<Transformer<TAsset>> transformers = {};
        std::shared_ptr<IFileOps> file_ops = {};

        {
            std::lock_guard lock(_mutex);
            file_ops = _file_ops;
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
            {
                loader = registration->get().loader;
                transformers = registration->get().transformers;
            }
        }

        auto read = AssetReadResult<TAsset> {};
        read.asset = std::make_shared<TAsset>();

        auto metadata = AssetLoadMetadata {};
        auto meta_data = std::optional<Json> {};
        auto loaded_meta = false;
        if (file_ops)
        {
            auto meta_result = try_read_tbx_serialized_asset_meta<TAsset>(
                asset_path,
                file_ops,
                metadata,
                meta_data,
                loaded_meta);
            if (!meta_result.succeeded())
            {
                read.asset.reset();
                read.result = std::move(meta_result);
                return read;
            }
        }
        read.metadata = metadata;

        const bool can_apply_meta = loaded_meta || meta_data.has_value();
        if (can_apply_meta)
        {
            auto meta_apply_result =
                try_apply_tbx_serialized_asset_meta(metadata, meta_data, *read.asset);
            if (!meta_apply_result.succeeded())
            {
                read.asset.reset();
                read.result = std::move(meta_apply_result);
                return read;
            }
        }

        if (loader)
        {
            read.result = loader(asset_path, parameters, metadata, *read.asset);
        }
        else if (file_ops)
        {
            read.result =
                try_load_tbx_serialized_asset_body(asset_path, file_ops, metadata, *read.asset);
        }
        else
        {
            read.result = make_failed_result(
                std::string("Serialization loader not registered for type '")
                + typeid(TAsset).name() + "'.");
        }

        if (!read.result.succeeded())
        {
            read.asset.reset();
            return read;
        }

        if (can_apply_meta)
        {
            auto meta_apply_result =
                try_apply_tbx_serialized_asset_meta(metadata, meta_data, *read.asset);
            if (!meta_apply_result.succeeded())
            {
                read.asset.reset();
                read.result = std::move(meta_apply_result);
                return read;
            }
        }

        for (const auto& transformer : transformers)
        {
            auto transform_result = transformer(asset_path, parameters, metadata, *read.asset);
            if (!transform_result.succeeded())
            {
                read.asset.reset();
                read.result = std::move(transform_result);
                return read;
            }
        }

        read.result.flag_success();
        return read;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::shared_ptr<TAsset> SerializationRegistry::read(
        const std::filesystem::path& asset_path,
        const AssetLoadParameters<TAsset>& parameters) const
    {
        auto read = read_result<TAsset>(asset_path, parameters);
        return read.result.succeeded() ? read.asset : std::shared_ptr<TAsset>();
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    AssetPromise<TAsset> SerializationRegistry::read_async(
        const std::filesystem::path& asset_path,
        const AssetLoadParameters<TAsset>& parameters) const
    {
        AsyncLoader<TAsset> async_loader = {};
        std::vector<Transformer<TAsset>> transformers = {};
        std::shared_ptr<IFileOps> file_ops = {};

        {
            std::lock_guard lock(_mutex);
            file_ops = _file_ops;
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
            {
                async_loader = registration->get().async_loader;
                transformers = registration->get().transformers;
            }
        }

        if (!async_loader)
        {
            auto sync_read = read_result<TAsset>(asset_path, parameters);
            auto result = AssetPromise<TAsset> {};
            result.asset = std::move(sync_read.asset);
            result.metadata = sync_read.metadata;
            result.promise = make_ready_future(std::move(sync_read.result));
            return result;
        }

        auto metadata = AssetLoadMetadata {};
        auto meta_data = std::optional<Json> {};
        auto loaded_meta = false;
        if (file_ops)
        {
            auto meta_result = try_read_tbx_serialized_asset_meta<TAsset>(
                asset_path,
                file_ops,
                metadata,
                meta_data,
                loaded_meta);
            if (!meta_result.succeeded())
            {
                auto result = AssetPromise<TAsset> {};
                result.promise = make_ready_future(std::move(meta_result));
                return result;
            }
        }

        auto result = AssetPromise<TAsset> {};
        result.asset = std::make_shared<TAsset>();
        result.metadata = metadata;

        auto loader_future = async_loader(asset_path, parameters, metadata, result.asset);
        if (!loader_future.valid())
        {
            result.asset.reset();
            result.promise = make_ready_future(
                make_failed_result("Async serialization loader returned no future."));
            return result;
        }

        auto asset = result.asset;
        const bool can_transform_meta = loaded_meta || meta_data.has_value();
        if (!can_transform_meta && transformers.empty())
        {
            result.promise = std::move(loader_future);
            return result;
        }

        if (loader_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto load_result = loader_future.get();
            if (load_result.succeeded())
            {
                if (can_transform_meta)
                    load_result = try_apply_tbx_serialized_asset_meta(metadata, meta_data, *asset);
            }
            if (load_result.succeeded())
            {
                for (const auto& transformer : transformers)
                {
                    load_result = transformer(asset_path, parameters, metadata, *asset);
                    if (!load_result.succeeded())
                        break;
                }
            }
            if (!load_result.succeeded())
                result.asset.reset();
            result.promise = make_ready_future(std::move(load_result));
            return result;
        }

        result.promise =
            std::async(
                std::launch::async,
                [this,
                 asset_path,
                 parameters,
                 metadata,
                 can_transform_meta,
                 asset,
                 meta_data = std::move(meta_data),
                 loader_future = std::move(loader_future),
                 transformers = std::move(transformers)]() mutable
                {
                    auto load_result = loader_future.get();
                    if (!load_result.succeeded())
                        return load_result;

                    if (can_transform_meta)
                    {
                        load_result =
                            try_apply_tbx_serialized_asset_meta(metadata, meta_data, *asset);
                        if (!load_result.succeeded())
                            return load_result;
                    }

                    for (const auto& transformer : transformers)
                    {
                        auto transform_result =
                            transformer(asset_path, parameters, metadata, *asset);
                        if (!transform_result.succeeded())
                            return transform_result;
                    }

                    auto result = Result {};
                    result.flag_success();
                    return result;
                })
                .share();
        return result;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::register_writer(Writer<TAsset> writer)
    {
        TBX_ASSERT(
            static_cast<bool>(writer),
            "Serialization registry requires a writer for type '{}'.",
            typeid(TAsset).name());

        std::lock_guard lock(_mutex);
        auto& registration = get_or_create_registration<TAsset>();
        registration.writer = std::move(writer);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::deregister_writer()
    {
        std::lock_guard lock(_mutex);
        auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return;

        registration->get().writer = {};
        erase_registration_if_empty<TAsset>();
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool SerializationRegistry::has_writer() const
    {
        std::lock_guard lock(_mutex);
        const auto registration = find_registration<TAsset>();
        return registration.has_value() && static_cast<bool>(registration->get().writer);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    Result SerializationRegistry::write(
        const std::filesystem::path& asset_path,
        const TAsset& asset) const
    {
        Writer<TAsset> writer = {};

        {
            std::lock_guard lock(_mutex);
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
                writer = registration->get().writer;
        }

        TBX_ASSERT(
            static_cast<bool>(writer),
            "Serialization writer not registered for type '{}'.",
            typeid(TAsset).name());
        if (!writer)
        {
            return make_failed_result(
                std::string("Serialization writer not registered for type '")
                + typeid(TAsset).name() + "'.");
        }

        return writer(asset_path, asset);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    SerializationRegistry::Registration<TAsset>& SerializationRegistry::get_or_create_registration()
    {
        auto key = std::type_index(typeid(TAsset));
        auto iterator = _registrations.find(key);
        if (iterator == _registrations.end())
        {
            iterator = _registrations.emplace(key, std::make_unique<Registration<TAsset>>()).first;
        }

        return static_cast<Registration<TAsset>&>(*iterator->second);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<SerializationRegistry::Registration<TAsset>>> SerializationRegistry::
        find_registration()
    {
        auto iterator = _registrations.find(std::type_index(typeid(TAsset)));
        if (iterator == _registrations.end())
            return std::nullopt;

        return std::ref(static_cast<Registration<TAsset>&>(*iterator->second));
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<const SerializationRegistry::Registration<TAsset>>> SerializationRegistry::
        find_registration() const
    {
        auto iterator = _registrations.find(std::type_index(typeid(TAsset)));
        if (iterator == _registrations.end())
            return std::nullopt;

        return std::cref(static_cast<const Registration<TAsset>&>(*iterator->second));
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::erase_registration_if_empty()
    {
        auto iterator = _registrations.find(std::type_index(typeid(TAsset)));
        if (iterator == _registrations.end())
            return;

        const auto& registration = static_cast<const Registration<TAsset>&>(*iterator->second);
        if (registration.loader || registration.async_loader || !registration.transformers.empty()
            || registration.writer)
            return;

        _registrations.erase(iterator);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    Result SerializationRegistry::try_apply_tbx_serialized_asset_meta(
        const AssetLoadMetadata& metadata,
        const std::optional<Json>& meta_data,
        TAsset& asset) const
    {
        if constexpr (HAS_TBX_META_SERIALIZATION_V<TAsset>)
        {
            if constexpr (HAS_TBX_META_JSON_FIELDS_V<TAsset>)
            {
                if (meta_data.has_value())
                {
                    if (!meta_data->try_update(asset))
                    {
                        return make_failed_result(
                            std::string("Failed to parse Toybox asset meta JSON for type '")
                            + typeid(TAsset).name() + "'.");
                    }
                }
            }
        }

        apply_tbx_asset_common_meta(metadata, asset);
        return {};
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    Result SerializationRegistry::try_load_tbx_serialized_asset_body(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<IFileOps>& file_ops,
        const AssetLoadMetadata&,
        TAsset& asset) const
    {
        if constexpr (HAS_TBX_TEXT_SERIALIZATION_V<TAsset>)
        {
            auto contents = std::string();
            if (!file_ops->read_file(asset_path, FileDataFormat::UTF8_TEXT, contents))
            {
                return make_failed_result(
                    std::string("Failed to read Toybox text asset body '")
                        .append(asset_path.string())
                        .append("'."));
            }

            tbx_set_text_serialization(asset, std::move(contents));
            return {};
        }
        else if constexpr (!HAS_TBX_ASSET_JSON_FIELDS_V<TAsset>)
        {
            static_cast<void>(asset_path);
            static_cast<void>(file_ops);
            static_cast<void>(asset);
            if constexpr (HAS_TBX_META_SERIALIZATION_V<TAsset>)
                return {};

            return make_failed_result(
                std::string("Serialization loader not registered for type '")
                + typeid(TAsset).name() + "'.");
        }
        else
        {
            auto contents = std::string();
            if (!file_ops->read_file(asset_path, FileDataFormat::UTF8_TEXT, contents))
            {
                return make_failed_result(
                    std::string("Failed to read Toybox asset JSON '")
                        .append(asset_path.string())
                        .append("'."));
            }

            try
            {
                const auto data = Json::parse(contents);
                return data.try_update(asset)
                           ? Result()
                           : make_failed_result(
                                 std::string("Failed to parse Toybox asset JSON '")
                                     .append(asset_path.string())
                                     .append("'."));
            }
            catch (const std::exception& exception)
            {
                return make_failed_result(
                    std::string("Failed to parse Toybox asset JSON '")
                        .append(asset_path.string())
                        .append("': ")
                        .append(exception.what()));
            }
            catch (...)
            {
                return make_failed_result(
                    std::string("Failed to parse Toybox asset JSON '")
                        .append(asset_path.string())
                        .append("'."));
            }
        }
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    Result SerializationRegistry::try_read_tbx_serialized_asset_meta(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<IFileOps>& file_ops,
        AssetLoadMetadata& out_metadata,
        std::optional<Json>& out_meta_data,
        bool& out_loaded_meta) const
    {
        if constexpr (
            !HAS_TBX_ASSET_SERIALIZATION_V<TAsset> && !HAS_TBX_META_SERIALIZATION_V<TAsset>
            && !HAS_TBX_SERIALIZATION_VERSION_V<TAsset> && !HAS_TBX_TEXT_SERIALIZATION_V<TAsset>)
        {
            static_cast<void>(asset_path);
            static_cast<void>(file_ops);
            static_cast<void>(out_metadata);
            static_cast<void>(out_meta_data);
            static_cast<void>(out_loaded_meta);
            return {};
        }

        auto meta_path = asset_path;
        meta_path += ".meta";
        if (!file_ops->exists(meta_path))
            return {};

        auto contents = std::string();
        if (!file_ops->read_file(meta_path, FileDataFormat::UTF8_TEXT, contents))
        {
            return make_failed_result(
                std::string("Failed to read Toybox asset meta '")
                    .append(meta_path.string())
                    .append("'."));
        }

        try
        {
            auto data = Json::parse(contents);
            auto expected_version = uint32();
            if constexpr (HAS_TBX_SERIALIZATION_VERSION_V<TAsset>)
                expected_version = TBX_SERIALIZATION_VERSION_V<TAsset>;
            auto common_meta_result =
                try_read_tbx_asset_common_meta(data, meta_path, expected_version, out_metadata);
            if (!common_meta_result.succeeded())
                return common_meta_result;

            if constexpr (HAS_TBX_META_SERIALIZATION_V<TAsset>)
            {
                if constexpr (HAS_TBX_META_JSON_FIELDS_V<TAsset>)
                    out_meta_data = Json::parse(contents);
            }

            out_loaded_meta = true;
            return {};
        }
        catch (const std::exception& exception)
        {
            return make_failed_result(
                std::string("Failed to parse Toybox asset meta '")
                    .append(meta_path.string())
                    .append("': ")
                    .append(exception.what()));
        }
        catch (...)
        {
            return make_failed_result(
                std::string("Failed to parse Toybox asset meta '")
                    .append(meta_path.string())
                    .append("'."));
        }
    }
}
