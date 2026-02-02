#include "sdr_shader_loader_plugin.h"
#include "tbx/assets/messages.h"
#include "tbx/files/filesystem.h"
#include "tbx/graphics/shader.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace tbx::plugins
{
    static std::string trim_string(std::string_view text)
    {
        const auto start = text.find_first_not_of(" \n\t");
        if (start == std::string_view::npos)
        {
            return "";
        }
        const auto end = text.find_last_not_of(" \n\t");
        return std::string(text.substr(start, end - start + 1U));
    }

    static bool try_parse_shader_type(std::string_view type_name, ShaderType& out_type)
    {
        std::string type_text(type_name);
        std::transform(
            type_text.begin(),
            type_text.end(),
            type_text.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });

        if (type_text == "vertex")
        {
            out_type = ShaderType::Vertex;
            return true;
        }
        if (type_text == "fragment")
        {
            out_type = ShaderType::Fragment;
            return true;
        }
        if (type_text == "compute")
        {
            out_type = ShaderType::Compute;
            return true;
        }

        out_type = ShaderType::None;
        return false;
    }

    static std::string build_load_failure_message(
        const std::filesystem::path& path,
        std::string_view reason)
    {
        std::string message = "Shader loader failed to load shader: ";
        message.append(path.string());
        if (!reason.empty())
        {
            message.append(" (reason: ");
            message.append(reason);
            message.append(")");
        }
        return message;
    }

    static bool try_parse_shader_source(
        const std::string& file_data,
        std::vector<ShaderSource>& shaders,
        std::string& error_message)
    {
        std::istringstream stream(file_data);
        std::string line;
        std::string pending_header;
        ShaderType current_type = ShaderType::None;
        std::string current_source;
        bool has_section = false;

        while (std::getline(stream, line))
        {
            const std::string trimmed = trim_string(line);
            if (trimmed.rfind("#type", 0U) == 0U)
            {
                const std::string type_name = trim_string(trimmed.substr(5U));
                ShaderType parsed_type = ShaderType::None;
                if (!try_parse_shader_type(type_name, parsed_type))
                {
                    error_message = "Shader loader: unknown shader type '" + type_name + "'.";
                    return false;
                }

                if (has_section)
                {
                    shaders.emplace_back(current_source, current_type);
                    current_source.clear();
                }
                else
                {
                    current_source = pending_header;
                    pending_header.clear();
                    has_section = true;
                }

                current_type = parsed_type;
                continue;
            }

            if (!has_section)
            {
                pending_header.append(line);
                pending_header.append(" \n\t");
                continue;
            }

            current_source.append(line);
            current_source.append(" \n\t");
        }

        if (!has_section)
        {
            error_message = "Shader loader: shader file does not declare any #type sections.";
            return false;
        }

        if (current_type != ShaderType::None)
        {
            shaders.emplace_back(current_source, current_type);
        }

        if (shaders.empty())
        {
            error_message = "Shader loader: no shader stages were parsed.";
            return false;
        }

        return true;
    }

    void SdrShaderLoaderPlugin::on_attach(IPluginHost& host)
    {
        _filesystem = &host.get_filesystem();
    }

    void SdrShaderLoaderPlugin::on_detach()
    {
        _filesystem = nullptr;
    }

    void SdrShaderLoaderPlugin::on_recieve_message(Message& msg)
    {
        auto* request = handle_message<LoadShaderRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_shader_program_request(*request);
    }

    void SdrShaderLoaderPlugin::on_load_shader_program_request(LoadShaderRequest& request)
    {
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Shader loader: missing shader asset wrapper.");
            return;
        }

        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = MessageState::Cancelled;
            request.result.flag_failure("Shader loader cancelled.");
            return;
        }

        if (!_filesystem)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Shader loader: filesystem unavailable.");
            return;
        }

        const std::filesystem::path resolved = resolve_asset_path(request.path);
        if (resolved.extension() != ".sdr")
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Shader loader: unsupported shader file extension.");
            return;
        }

        std::string file_data;
        if (!_filesystem->read_file(resolved, FileDataFormat::Utf8Text, file_data))
        {
            request.state = MessageState::Error;
            request.result.flag_failure(
                build_load_failure_message(resolved, "file could not be read"));
            return;
        }

        std::vector<ShaderSource> shaders;
        std::string parse_error;
        if (!try_parse_shader_source(file_data, shaders, parse_error))
        {
            request.state = MessageState::Error;
            request.result.flag_failure(build_load_failure_message(resolved, parse_error));
            return;
        }

        *asset = Shader(std::move(shaders));

        request.state = MessageState::Handled;
    }

    std::filesystem::path SdrShaderLoaderPlugin::resolve_asset_path(
        const std::filesystem::path& path) const
    {
        if (!_filesystem)
        {
            return path;
        }
        return _filesystem->resolve_asset_path(path);
    }
}
