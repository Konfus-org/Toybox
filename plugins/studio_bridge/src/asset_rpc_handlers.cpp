#include "asset_rpc_handlers.h"
#include "asset_ops.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "world_manager.h"
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
        const RpcRegistrar& registrar, AssetOps& asset_ops, WorldManager& world_manager)
    {
        registrar.add(
            Wire::ASSET_PAIRING,
            [](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(asset_pairing_rules());
            });
        registrar.add(
            Wire::ASSET_SAVE,
            [&asset_ops](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(asset_ops.save_asset(params));
            });
        registrar.add_query(
            Wire::ASSET_CREATE,
            [&asset_ops](const tbx::Json& params, tbx::Json& reply)
            {
                return asset_ops.create_asset(params, reply);
            });
        registrar.add(
            Wire::ASSET_FORGET,
            [&asset_ops](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(asset_ops.forget_asset(params));
            });
        registrar.add_query(
            Wire::ASSET_NEW_ID,
            [&asset_ops](const tbx::Json&, tbx::Json& reply)
            {
                return asset_ops.new_asset_id(reply);
            });
        registrar.add_query(
            Wire::ASSET_DESCRIBE,
            [&asset_ops](const tbx::Json& params, tbx::Json& reply)
            {
                return asset_ops.describe_asset(params, reply);
            });
        registrar.add(
            Wire::ASSET_PREVIEW_STATS,
            [&asset_ops](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.result(asset_ops.asset_preview_stats(params));
            });
        registrar.add(
            Wire::ASSET_GENERATE_MISSING_METAS,
            [&asset_ops](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(asset_ops.generate_missing_metas());
            });
        registrar.add(
            Wire::EDITOR_LIST_ASSETS,
            [&asset_ops](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(asset_ops.list_assets());
            });
        registrar.add_query(
            Wire::EDITOR_MODEL_SLOTS,
            [&world_manager](const tbx::Json& params, tbx::Json& reply)
            {
                return world_manager.model_slots(params, reply);
            });
        registrar.add_query(
            Wire::EDITOR_PREVIEW_TEXTURE_MATERIAL,
            [&world_manager](const tbx::Json& params, tbx::Json& reply)
            {
                return world_manager.preview_texture_material(params, reply);
            });
    }
}
