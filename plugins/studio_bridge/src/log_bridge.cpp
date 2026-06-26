#include "log_bridge.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/color.h"
#include <string_view>

namespace tbx::studio_bridge
{
    static int hex_nibble(char character)
    {
        if (character >= '0' && character <= '9')
            return character - '0';
        if (character >= 'a' && character <= 'f')
            return (character - 'a') + 10;
        if (character >= 'A' && character <= 'F')
            return (character - 'A') + 10;

        return -1;
    }

    static float hex_channel(const std::string& hex, size offset)
    {
        if (offset + 1 >= hex.size())
            return 0.0F;

        const auto high = hex_nibble(hex[offset]);
        const auto low = hex_nibble(hex[offset + 1]);
        if (high < 0 || low < 0)
            return 0.0F;

        return static_cast<float>((high * 16) + low) / 255.0F;
    }

    // Parses "#RRGGBB" (or "RRGGBB"); anything malformed falls back to white so a bad color never
    // silences the console.
    static tbx::Color parse_hex_color(const std::string& hex)
    {
        const auto body = (!hex.empty() && hex.front() == '#') ? hex.substr(1) : hex;
        if (body.size() < 6)
            return tbx::Color(1.0F, 1.0F, 1.0F, 1.0F);

        return tbx::Color(hex_channel(body, 0), hex_channel(body, 2), hex_channel(body, 4), 1.0F);
    }

    static std::string_view to_log_level_name(tbx::LogLevel level)
    {
        switch (level)
        {
            case tbx::LogLevel::INFO:
                return "info";
            case tbx::LogLevel::WARNING:
                return "warning";
            case tbx::LogLevel::ERROR:
                return "error";
            case tbx::LogLevel::CRITICAL:
                return "critical";
        }

        return "info";
    }

    // Set on the thread that is writing an editor-originated log so the listener below does not echo
    // that line straight back to the editor (which already displayed it locally). Thread-local
    // because the log listener runs synchronously on whatever thread called into tbx::Log.
    static thread_local bool t_suppress_log_forward = false;

    LogBridge::LogBridge(EngineServices& services)
        : _services(services)
    {
    }

    void LogBridge::attach()
    {
        _log_listener_id = tbx::Log::get_instance().add_listener(
            [this](tbx::LogLevel level, const std::string& message)
            {
                forward_log(level, message);
            });
    }

    void LogBridge::detach()
    {
        if (_log_listener_id != 0U)
        {
            tbx::Log::get_instance().remove_listener(_log_listener_id);
            _log_listener_id = 0U;
        }
    }

    void LogBridge::forward_log(tbx::LogLevel level, const std::string& message)
    {
        if (t_suppress_log_forward)
            return;
        const auto host = _services.get().rpc_host.lock();
        if (!host || !host->has_client())
            return;

        auto params = tbx::Json::object();
        params["level"] = to_log_level_name(level);
        params["message"] = message;
        host->send_notification("engine.log", params);
    }

    void LogBridge::write_editor_log(const tbx::Json& params)
    {
        const auto message = params.value("message", std::string());
        if (message.empty())
            return;

        const auto level_name = params.value("level", std::string("info"));
        auto level = tbx::LogLevel::INFO;
        if (level_name == "warning")
            level = tbx::LogLevel::WARNING;
        else if (level_name == "error")
            level = tbx::LogLevel::ERROR;
        else if (level_name == "critical")
            level = tbx::LogLevel::CRITICAL;

        // Route editor lines through the engine's normal logging (file + console + listeners) but
        // skip the RPC echo so the editor doesn't show its own line twice. Tag them "[Studio]" via
        // a category scope (no source file — the editor's own console keeps the file:line detail).
        t_suppress_log_forward = true;
        {
            TBX_LOG_CATEGORY_SCOPE("Studio");
            tbx::Log::get_instance().write_internal(level, nullptr, 0, message);
        }
        t_suppress_log_forward = false;
    }

    void LogBridge::set_log_colors(const tbx::Json& params)
    {
        auto& log = tbx::Log::get_instance();
        if (params.contains("info"))
            log.set_color(
                tbx::LogLevel::INFO,
                parse_hex_color(params.value("info", std::string())));

        if (params.contains("warning"))
            log.set_color(
                tbx::LogLevel::WARNING,
                parse_hex_color(params.value("warning", std::string())));

        if (params.contains("error"))
        {
            const auto error_color = parse_hex_color(params.value("error", std::string()));
            log.set_color(tbx::LogLevel::ERROR, error_color);
            log.set_color(tbx::LogLevel::CRITICAL, error_color);
        }
    }
}
