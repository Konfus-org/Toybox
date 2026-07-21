#pragma once

namespace tbx
{
    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::register_reader(
        std::vector<std::string> extensions,
        Reader<TAsset> reader,
        AsyncReader<TAsset> async_reader)
    {
        TBX_ASSERT(
            static_cast<bool>(reader) || static_cast<bool>(async_reader),
            "Serialization registry requires a sync or async reader for type '{}'.",
            typeid(TAsset).name());

        // Normalize each claimed extension to a lower-case, dot-less token so matching is trivial.
        for (auto& extension : extensions)
        {
            if (!extension.empty() && extension.front() == '.')
                extension.erase(extension.begin());
            for (auto& character : extension)
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        auto type_name = std::string();
        if (const auto asset_registration =
                get_asset_type_registration(std::type_index(typeid(TAsset)));
            asset_registration.has_value())
            type_name = asset_registration->type_name;

        std::lock_guard lock(_mutex);
        auto& registration = get_or_create_registration<TAsset>();
        registration.reader = std::move(reader);
        registration.async_reader = std::move(async_reader);
        registration.extensions = std::move(extensions);
        registration.type_name = std::move(type_name);
        // Type-erased deserialize: read the asset through its typed reader path and up-cast the result to
        // the Asset base, so a type-erased caller (read_registered_asset_result) can load a reader-backed
        // asset (a Model with its geometry/slots) without knowing TAsset.
        registration.read_erased =
            [](const SerializationRegistry& registry, const std::filesystem::path& asset_path)
        {
            auto typed = registry.read_result<TAsset>(asset_path);
            auto erased = AssetReadResult<Asset> {};
            erased.asset = std::move(typed.asset);
            erased.metadata = typed.metadata;
            erased.result = std::move(typed.result);
            return erased;
        };
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::deregister_reader()
    {
        std::lock_guard lock(_mutex);
        auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return;

        registration->get().reader = {};
        registration->get().async_reader = {};
        registration->get().extensions = {};
        registration->get().type_name = {};
        registration->get().read_erased = {};
        erase_registration_if_empty<TAsset>();
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
        registration.transformer = std::move(transformer);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void SerializationRegistry::deregister_transformer()
    {
        std::lock_guard lock(_mutex);
        auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return;

        registration->get().transformer = {};
        erase_registration_if_empty<TAsset>();
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool SerializationRegistry::can_read() const
    {
        const auto file_ops = lock_file_ops();
        std::lock_guard lock(_mutex);
        const auto registration = find_registration<TAsset>();
        const bool has_registered_reader =
            registration.has_value()
            && (static_cast<bool>(registration->get().reader)
                || static_cast<bool>(registration->get().async_reader));
        if (has_registered_reader)
            return true;

        const bool has_registered_transformer =
            registration.has_value() && static_cast<bool>(registration->get().transformer);
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));
        const bool has_default_reader =
            asset_registration.has_value()
            && (asset_registration->read_body || asset_registration->transform_meta);
        return file_ops != nullptr && (has_registered_transformer || has_default_reader);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    AssetReadResult<TAsset> SerializationRegistry::read_result(
        const std::filesystem::path& asset_path) const
    {
        Reader<TAsset> reader = {};
        Transformer<TAsset> transformer = {};
        auto file_ops = lock_file_ops();
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));

        {
            std::lock_guard lock(_mutex);
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
            {
                reader = registration->get().reader;
                transformer = registration->get().transformer;
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

        if (reader)
        {
            read.result = reader(asset_path, metadata, *read.asset);
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
                std::string("No serialization reader or JSON body registered for type '")
                + typeid(TAsset).name() + "'.");
        }

        if (!read.result.succeeded())
        {
            read.asset.reset();
            return read;
        }

        auto post_read_result = apply_post_read_steps<TAsset>(
            asset_registration,
            meta_data,
            loaded_meta,
            metadata,
            asset_path,
            transformer,
            *read.asset);
        if (!post_read_result.succeeded())
        {
            read.asset.reset();
            read.result = std::move(post_read_result);
            return read;
        }

        read.result.ok();
        return read;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    AssetPromise<TAsset> SerializationRegistry::read_async(
        const std::filesystem::path& asset_path) const
    {
        AsyncReader<TAsset> async_reader = {};
        Transformer<TAsset> transformer = {};
        auto file_ops = lock_file_ops();
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));

        {
            std::lock_guard lock(_mutex);
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
            {
                async_reader = registration->get().async_reader;
                transformer = registration->get().transformer;
            }
        }

        if (!async_reader)
        {
            auto sync_read = read_result<TAsset>(asset_path);
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

        auto loader_future = async_reader(asset_path, metadata, result.asset);
        if (!loader_future.valid())
        {
            result.asset.reset();
            result.promise = make_ready_future(
                make_failed_result("Async serialization reader returned no future."));
            return result;
        }

        // Nothing to do after the loader: hand its future straight through.
        if (!loaded_meta && !transformer)
        {
            result.promise = std::move(loader_future);
            return result;
        }

        auto asset = result.asset;
        if (loader_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto load_result = loader_future.get();
            if (load_result.succeeded())
            {
                load_result = apply_post_read_steps<TAsset>(
                    asset_registration,
                    meta_data,
                    loaded_meta,
                    metadata,
                    asset_path,
                    transformer,
                    *asset);
            }
            if (!load_result.succeeded())
                result.asset.reset();
            result.promise = make_ready_future(std::move(load_result));
            return result;
        }

        result.promise = std::async(
                             std::launch::async,
                             [asset_registration,
                              meta_data,
                              loaded_meta,
                              metadata,
                              asset_path,
                              transformer = std::move(transformer),
                              asset,
                              loader_future = std::move(loader_future)]() mutable
                             {
                                 auto load_result = loader_future.get();
                                 if (!load_result.succeeded())
                                     return load_result;

                                 return apply_post_read_steps<TAsset>(
                                     asset_registration,
                                     meta_data,
                                     loaded_meta,
                                     metadata,
                                     asset_path,
                                     transformer,
                                     *asset);
                             })
                             .share();
        return result;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    Result SerializationRegistry::write(
        const std::filesystem::path& asset_path,
        const TAsset& asset) const
    {
        // Serialization is always JSON: reader-backed types (model/texture) have no writer and are never
        // written back over their source file; every serializable body writes through the codegen
        // write_body.
        auto file_ops = lock_file_ops();
        const auto asset_registration =
            get_asset_type_registration(std::type_index(typeid(TAsset)));

        if (!asset_registration.has_value() || !asset_registration->write_body)
        {
            return make_failed_result(
                std::string("No serializable body registered for type '")
                + typeid(TAsset).name() + "'.");
        }
        if (!file_ops)
            return make_failed_result("Serialization registry has no file operations.");

        return try_write_registered_asset_body(asset_path, *file_ops, *asset_registration, &asset);
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
        if (registration.reader || registration.async_reader || registration.transformer
            || !registration.extensions.empty())
            return;

        _registrations.erase(iterator);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    Result SerializationRegistry::apply_post_read_steps(
        const std::optional<AssetTypeRegistration>& asset_registration,
        const std::optional<std::string>& meta_data,
        bool loaded_meta,
        const AssetLoadMetadata& metadata,
        const std::filesystem::path& asset_path,
        const Transformer<TAsset>& transformer,
        TAsset& asset)
    {
        if (loaded_meta)
        {
            // Apply the asset's [[meta]] import settings from the sidecar, then the common
            // id/version fields.
            if (asset_registration.has_value() && asset_registration->transform_meta
                && meta_data.has_value())
            {
                auto meta_result =
                    asset_registration->transform_meta(*meta_data, static_cast<void*>(&asset));
                if (!meta_result.succeeded())
                    return meta_result;
            }
            apply_asset_common_meta(metadata, asset);
        }

        if (transformer)
        {
            auto transform_result = transformer(asset_path, metadata, asset);
            if (!transform_result.succeeded())
                return transform_result;
        }

        auto result = Result();
        result.ok();
        return result;
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

