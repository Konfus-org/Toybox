#include "asset_rpc_handlers.h"
#include "asset_ops.h"
#include "engine_services.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "world_ops.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/assets/asset_pairing.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    // Serialises the engine's authoritative file-pairing rules so the editor never hard-codes them.
    static tbx::Json asset_pairing_rules()
    {
        auto headers = tbx::Json::array();
        for (const auto extension : tbx::asset_pairing::source_header_extensions)
            headers.push_back(std::string(extension));

        auto impls = tbx::Json::array();
        for (const auto extension : tbx::asset_pairing::source_impl_extensions)
            impls.push_back(std::string(extension));

        auto reply = tbx::Json::object();
        reply["metadataSuffix"] = std::string(tbx::asset_pairing::metadata_suffix);
        reply["sourceHeaderExtensions"] = std::move(headers);
        reply["sourceImplExtensions"] = std::move(impls);
        return reply;
    }

    void register_asset_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views)
    {
        registrar.add(
            Wire::ASSET_PAIRING,
            [](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(asset_pairing_rules());
            });
        registrar.add(
            Wire::ASSET_SAVE,
            [&services](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(save_asset(services, params));
            });
        registrar.add_query(
            Wire::ASSET_CREATE,
            [&services](const tbx::Json& params, tbx::Json& reply)
            {
                return create_asset(services, params, reply);
            });
        registrar.add(
            Wire::ASSET_FORGET,
            [&services](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(forget_asset(services, params));
            });
        registrar.add_query(
            Wire::ASSET_NEW_ID,
            [](const tbx::Json&, tbx::Json& reply)
            {
                return new_asset_id(reply);
            });
        registrar.add_query(
            Wire::ASSET_DESCRIBE,
            [&services](const tbx::Json& params, tbx::Json& reply)
            {
                return describe_asset(services, params, reply);
            });
        registrar.add(
            Wire::ASSET_PREVIEW_STATS,
            [&services](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.result(asset_preview_stats(services, params));
            });
        registrar.add(
            Wire::ASSET_GENERATE_MISSING_METAS,
            [&services](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(generate_missing_metas(services));
            });
        registrar.add(
            Wire::EDITOR_LIST_ASSETS,
            [&services](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(list_assets(services));
            });
        registrar.add_query(
            Wire::EDITOR_PREVIEW_TEXTURE_MATERIAL,
            [&services](const tbx::Json& params, tbx::Json& reply)
            {
                return preview_texture_material(services, params, reply);
            });
    }
}
