#include "texture_rpc_handlers.h"
#include "rpc_registrar.h"
#include "texture_state.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"
#include <mutex>
#include <string>
#include <vector>

namespace tbx::studio_bridge
{
    // RGBA8 uploads are capped well past any plausible icon/badge (4096² × 4 B = 64 MB would be
    // absurd over the control plane anyway; big textures belong to the asset pipeline).
    constexpr uint32 MAX_TEXTURE_EDGE = 1024U;

    static int decode_base64_symbol(char symbol)
    {
        if (symbol >= 'A' && symbol <= 'Z')
            return symbol - 'A';
        if (symbol >= 'a' && symbol <= 'z')
            return symbol - 'a' + 26;
        if (symbol >= '0' && symbol <= '9')
            return symbol - '0' + 52;
        if (symbol == '+')
            return 62;
        if (symbol == '/')
            return 63;
        return -1;
    }

    static bool decode_base64(const std::string& text, std::vector<uint8>& out_bytes)
    {
        out_bytes.clear();
        out_bytes.reserve((text.size() / 4U) * 3U);
        auto accumulator = 0U;
        auto bits = 0;
        for (const auto symbol : text)
        {
            if (symbol == '=' || symbol == '\n' || symbol == '\r')
                continue;
            const auto value = decode_base64_symbol(symbol);
            if (value < 0)
                return false;
            accumulator = (accumulator << 6U) | static_cast<uint32>(value);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out_bytes.push_back(static_cast<uint8>((accumulator >> bits) & 0xFFU));
            }
        }
        return true;
    }

    void register_texture_handlers(
        const RpcRegistrar& registrar, const std::shared_ptr<EditorTextureTable>& textures)
    {
        registrar.add_query(
            Wire::TEXTURE_UPLOAD,
            [textures](const tbx::Json& params, tbx::Json& out_reply) -> tbx::Result
            {
                const auto width = params.value(Wire::WIDTH, 0U);
                const auto height = params.value(Wire::HEIGHT, 0U);
                const auto format = params.value(Wire::FORMAT, std::string("rgba8"));
                if (format != "rgba8")
                    return tbx::Result(false, "texture.upload: only rgba8 is supported.");
                if (width == 0U || height == 0U || width > MAX_TEXTURE_EDGE
                    || height > MAX_TEXTURE_EDGE)
                    return tbx::Result(false, "texture.upload: bad dimensions.");

                auto pixels = std::vector<uint8>();
                if (!decode_base64(params.value(Wire::DATA, std::string()), pixels))
                    return tbx::Result(false, "texture.upload: data is not valid base64.");
                if (pixels.size() != static_cast<size>(width) * height * 4U)
                    return tbx::Result(false, "texture.upload: data size mismatches dimensions.");

                auto lock = std::lock_guard(textures->mutex);
                const auto id = textures->next_id++;
                textures->textures.emplace(
                    id, EditorTexture {width, height, std::move(pixels), tbx::INVALID_GPU_ID});
                out_reply[Wire::ID] = id;
                return tbx::Result::OK;
            });
    }
}
