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
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));
        const bool has_default_loader =
            asset_registration.has_value()
            && (asset_registration->read_body || asset_registration->transform_meta);
        if (!registration.has_value())
            return has_default_loader;

        return static_cast<bool>(registration->get().loader)
               || static_cast<bool>(registration->get().async_loader) || has_default_loader;
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
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));
        return (registration.has_value() && !registration->get().transformers.empty())
               || (asset_registration.has_value() && asset_registration->transform_meta);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool SerializationRegistry::can_read() const
    {
        const auto file_ops = lock_file_ops();
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
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));
        const bool has_default_loader =
            asset_registration.has_value()
            && (asset_registration->read_body || asset_registration->transform_meta);
        return file_ops != nullptr && (has_registered_transformer || has_default_loader);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    AssetReadResult<TAsset> SerializationRegistry::read_result(
        const std::filesystem::path& asset_path,
        const AssetLoadParameters<TAsset>& parameters) const
    {
        Loader<TAsset> loader = {};
        std::vector<Transformer<TAsset>> transformers = {};
        auto file_ops = lock_file_ops();
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));

        {
            std::lock_guard lock(_mutex);
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
            {
                loader = registration->get().loader;
                transformers = registration->get().transformers;
            }
        }

        auto read = AssetReadResult<TAsset> {};
        read.asset = std::make_shared<TAsset>();
        if (!file_ops)
        {
            read.asset.reset();
            read.result = make_failed_result("Serialization registry has no file operations.");
            return read;
        }

        auto metadata = AssetLoadMetadata {};
        auto meta_data = std::optional<std::string> {};
        auto loaded_meta = false;
        const auto expected_version =
            asset_registration.has_value() ? asset_registration->version : 0U;
        auto meta_result = try_read_serialized_asset_meta(
            asset_path,
            *file_ops,
            expected_version,
            metadata,
            meta_data,
            loaded_meta);
        if (!meta_result.succeeded())
        {
            read.asset.reset();
            read.result = std::move(meta_result);
            return read;
        }
        read.metadata = metadata;

        if (loader)
        {
            read.result = loader(asset_path, parameters, metadata, *read.asset);
        }
        else if (asset_registration.has_value() && asset_registration->read_body)
        {
            read.result = try_load_registered_asset_body(
                asset_path,
                *file_ops,
                *asset_registration,
                read.asset.get());
        }
        else if (asset_registration.has_value() && asset_registration->transform_meta)
        {
            read.result = Result();
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

        prepend_default_meta_transformers(asset_registration, meta_data, loaded_meta, transformers);

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

        read.result.ok();
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
        auto file_ops = lock_file_ops();
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));

        {
            std::lock_guard lock(_mutex);
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
        auto meta_data = std::optional<std::string> {};
        auto loaded_meta = false;
        if (!file_ops)
        {
            auto result = AssetPromise<TAsset> {};
            result.promise =
                make_ready_future(make_failed_result("Serialization registry has no file operations."));
            return result;
        }

        const auto expected_version =
            asset_registration.has_value() ? asset_registration->version : 0U;
        auto meta_result = try_read_serialized_asset_meta(
            asset_path,
            *file_ops,
            expected_version,
            metadata,
            meta_data,
            loaded_meta);
        if (!meta_result.succeeded())
        {
            auto result = AssetPromise<TAsset> {};
            result.promise = make_ready_future(std::move(meta_result));
            return result;
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
        prepend_default_meta_transformers(asset_registration, meta_data, loaded_meta, transformers);
        if (transformers.empty())
        {
            result.promise = std::move(loader_future);
            return result;
        }

        if (loader_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto load_result = loader_future.get();
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

        result.promise = std::async(
                             std::launch::async,
                             [this,
                              asset_path,
                              parameters,
                              metadata,
                              asset,
                              loader_future = std::move(loader_future),
                              transformers = std::move(transformers)]() mutable
                             {
                                 auto load_result = loader_future.get();
                                 if (!load_result.succeeded())
                                     return load_result;

                                 for (const auto& transformer : transformers)
                                 {
                                     auto transform_result =
                                         transformer(asset_path, parameters, metadata, *asset);
                                     if (!transform_result.succeeded())
                                         return transform_result;
                                 }

                                 auto result = Result();
                                 result.ok();
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
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));
        return (registration.has_value() && static_cast<bool>(registration->get().writer))
               || (asset_registration.has_value() && asset_registration->write_body);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    Result SerializationRegistry::write(
        const std::filesystem::path& asset_path,
        const TAsset& asset) const
    {
        Writer<TAsset> writer = {};
        auto file_ops = lock_file_ops();
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));

        {
            std::lock_guard lock(_mutex);
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
                writer = registration->get().writer;
        }

        if (!writer)
        {
            if (!asset_registration.has_value() || !asset_registration->write_body)
            {
                return make_failed_result(
                    std::string("Serialization writer not registered for type '")
                    + typeid(TAsset).name() + "'.");
            }
            if (!file_ops)
                return make_failed_result("Serialization registry has no file operations.");

            return try_write_registered_asset_body(
                asset_path,
                *file_ops,
                *asset_registration,
                &asset);
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
    void SerializationRegistry::prepend_default_meta_transformers(
        const std::optional<AssetTypeRegistration>& asset_registration,
        const std::optional<std::string>& meta_data,
        bool loaded_meta,
        std::vector<Transformer<TAsset>>& transformers)
    {
        if (!loaded_meta)
            return;

        auto default_transformers = std::vector<Transformer<TAsset>> {};
        if (asset_registration.has_value() && asset_registration->transform_meta
            && meta_data.has_value())
        {
            default_transformers.push_back(
                [asset_registration, meta_data](
                    const std::filesystem::path&,
                    const AssetLoadParameters<TAsset>&,
                    const AssetLoadMetadata&,
                    TAsset& asset)
                {
                    auto result =
                        asset_registration->transform_meta(*meta_data, static_cast<void*>(&asset));
                    if (!result.succeeded())
                        return result;

                    return Result();
                });
        }

        default_transformers.push_back(
            [](const std::filesystem::path&,
               const AssetLoadParameters<TAsset>&,
               const AssetLoadMetadata& metadata,
               TAsset& asset)
            {
                apply_asset_common_meta(metadata, asset);
                return Result();
            });

        default_transformers.insert(
            default_transformers.end(),
            std::make_move_iterator(transformers.begin()),
            std::make_move_iterator(transformers.end()));
        transformers = std::move(default_transformers);
    }

    inline Result SerializationRegistry::try_load_registered_asset_body(
        const std::filesystem::path& asset_path,
        const IFileOps& file_ops,
        const AssetTypeRegistration& asset_registration,
        void* asset)
    {
        auto contents = std::string();
        if (!file_ops.read_file(asset_path, FileDataFormat::UTF8_TEXT, contents))
        {
            return make_failed_result(
                std::string("Failed to read Toybox asset body '")
                    .append(asset_path.string())
                    .append("'."));
        }

        auto result = asset_registration.read_body(contents, asset);
        if (!result.succeeded())
        {
            result.failure(
                std::string(result.get_report()).append(" Asset: ").append(asset_path.string()));
        }
        return result;
    }

    inline Result SerializationRegistry::try_write_registered_asset_body(
        const std::filesystem::path& asset_path,
        IFileOps& file_ops,
        const AssetTypeRegistration& asset_registration,
        const void* asset)
    {
        auto contents = std::string();
        auto result = asset_registration.write_body(asset, contents);
        if (!result.succeeded())
            return result;

        if (!file_ops.write_file(asset_path, FileDataFormat::UTF8_TEXT, contents))
        {
            return make_failed_result(
                std::string("Failed to write Toybox asset body '")
                    .append(asset_path.string())
                    .append("'."));
        }

        return Result();
    }

    inline Result SerializationRegistry::try_read_serialized_asset_meta(
        const std::filesystem::path& asset_path,
        const IFileOps& file_ops,
        uint32 expected_version,
        AssetLoadMetadata& out_metadata,
        std::optional<std::string>& out_meta_data,
        bool& out_loaded_meta) const
    {
        auto meta_path = asset_path;
        meta_path += ".meta";
        if (!file_ops.exists(meta_path))
            return Result();

        auto contents = std::string();
        if (!file_ops.read_file(meta_path, FileDataFormat::UTF8_TEXT, contents))
        {
            return make_failed_result(
                std::string("Failed to read Toybox asset meta '")
                    .append(meta_path.string())
                    .append("'."));
        }

        try
        {
            auto data = JsonParser::parse(contents);
            auto common_meta_result =
                try_read_asset_common_meta(data, meta_path, expected_version, out_metadata);
            if (!common_meta_result.succeeded())
                return common_meta_result;

            out_meta_data = std::move(contents);
            out_loaded_meta = true;
            return Result();
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

