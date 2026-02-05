#include "glsl_shader_loader_plugin.h"
#include "tbx/app/application.h"
#include "tbx/assets/messages.h"
#include "tbx/files/file_operator.h"
#include "tbx/graphics/shader.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace tbx::plugins
{
    struct ShaderLoadResult
    {
      public:
        bool succeeded = false;
        std::string data;
        std::string error;
        std::filesystem::path resolved_path;
    };

    struct ShaderUniformResult
    {
      public:
        bool succeeded = false;
        std::string type;
        std::string name;
    };

    static ShaderLoadResult make_shader_load_failure(std::string message)
    {
        return ShaderLoadResult {
            .succeeded = false,
            .data = {},
            .error = std::move(message),
            .resolved_path = {}};
    }

    static ShaderLoadResult make_shader_load_success(std::string data)
    {
        return ShaderLoadResult {
            .succeeded = true,
            .data = std::move(data),
            .error = {},
            .resolved_path = {}};
    }

    static ShaderLoadResult make_shader_load_success(
        std::string data,
        std::filesystem::path resolved_path)
    {
        return ShaderLoadResult {
            .succeeded = true,
            .data = std::move(data),
            .error = {},
            .resolved_path = std::move(resolved_path)};
    }

    // Trims whitespace from both ends of a string view, returning an owned string for convenience.
    static std::string trim_string(std::string_view text)
    {
        auto start = text.find_first_not_of(" \r\n\t");
        if (start == std::string_view::npos)
            return "";
        auto end = text.find_last_not_of(" \r\n\t");
        return std::string(text.substr(start, end - start + 1U));
    }

    // Minimal "uniform <type> <name>;" parser used to validate/introspect basic declarations.
    // This is not a full GLSL parser; it intentionally only supports the simple shape we use.
    static ShaderUniformResult try_parse_simple_uniform_declaration(std::string_view trimmed_line)
    {
        if (trimmed_line.rfind("uniform", 0U) != 0U)
            return {};

        std::istringstream stream = std::istringstream(std::string(trimmed_line));
        std::string keyword;
        std::string type;
        std::string name;
        if (!(stream >> keyword >> type >> name))
            return {};

        if (keyword != "uniform")
            return {};

        if (!name.empty() && name.back() == ';')
            name.pop_back();

        auto array_start = name.find('[');
        if (array_start != std::string::npos)
            name.erase(array_start);

        return ShaderUniformResult {
            .succeeded = true,
            .type = std::move(type),
            .name = std::move(name)};
    }

    // Parses GLSL-style include directives:
    // - `#include "path"`
    // - `#include <path>`
    // - `#include path` (bare token; e.g. `#include Globals.glsl`)
    //
    // Returns the include path text, without quotes/angle brackets.
    static bool try_parse_include_directive(std::string_view trimmed_line, std::string& out_path)
    {
        if (trimmed_line.rfind("#include", 0U) != 0U)
            return false;

        auto remainder = trimmed_line.substr(8U);
        auto include_start = remainder.find_first_not_of(" \t");
        if (include_start == std::string_view::npos)
            return false;
        remainder.remove_prefix(include_start);

        char opening = remainder.front();
        if (opening == '"' || opening == '<')
        {
            char closing = opening == '<' ? '>' : '"';
            auto end = remainder.find(closing, 1U);
            if (end == std::string_view::npos || end <= 1U)
                return false;

            out_path = std::string(remainder.substr(1U, end - 1U));
            return !out_path.empty();
        }

        auto end = remainder.find_first_of(" \t");
        out_path = std::string(remainder.substr(0U, end));
        return !out_path.empty();
    }

    // Used to detect malformed include lines so we can report an error instead of silently passing
    // an unknown preprocessor directive to the OpenGL driver.
    static bool is_include_directive(std::string_view trimmed_line)
    {
        return trimmed_line.rfind("#include", 0U) == 0U;
    }

    // Supports a header-like `#pragma once` pattern in shader includes.
    // If an included file contains `#pragma once`, the loader remembers the file and skips future
    // includes of it for the remainder of the expansion pass.
    static bool is_pragma_once_directive(std::string_view trimmed_line)
    {
        if (trimmed_line.rfind("#pragma", 0U) != 0U)
            return false;

        std::istringstream stream = std::istringstream(std::string(trimmed_line));
        std::string keyword;
        std::string pragma;
        if (!(stream >> keyword >> pragma))
            return false;

        return (keyword == "#pragma") && (pragma == "once");
    }

    // Resolves and reads an include file by checking:
    // 1) Relative to the including file (if any).
    // 2) Via the asset manager's search roots.
    static ShaderLoadResult try_load_include_file(
        const FileOperator& file_operator,
        const AssetManager& asset_manager,
        const std::filesystem::path& including_file,
        const std::filesystem::path& include_path)
    {
        if (include_path.empty())
            return make_shader_load_failure("Shader loader: empty include path.");

        if (!include_path.is_absolute() && !including_file.empty())
        {
            std::filesystem::path local_candidate =
                (including_file.parent_path() / include_path).lexically_normal();
            std::string local_data;
            if (file_operator.read_file(local_candidate, FileDataFormat::UTF8_TEXT, local_data))
                return make_shader_load_success(std::move(local_data), local_candidate);
        }

        std::filesystem::path asset_candidate =
            asset_manager.resolve_asset_path(include_path).lexically_normal();
        std::string asset_data;
        if (file_operator.read_file(asset_candidate, FileDataFormat::UTF8_TEXT, asset_data))
            return make_shader_load_success(std::move(asset_data), asset_candidate);

        return make_shader_load_failure(
            "Shader loader: failed to resolve include path '" + include_path.string() + "'.");
    }

    // Recursively expands #include directives into a single source string.
    //
    // Behavior:
    // - Strips all `#include ...` directives by replacing them with included file contents.
    // - Strips `#pragma once` directives (they are not part of GLSL and should not reach OpenGL).
    // - Implements a basic `#pragma once` system to prevent duplicate inclusion during expansion.
    // - Detects include cycles using the include stack.
    static ShaderLoadResult try_expand_includes(
        const FileOperator& file_operator,
        const AssetManager& asset_manager,
        const std::filesystem::path& source_file,
        const std::string& source,
        std::vector<std::filesystem::path>& include_stack,
        std::unordered_set<std::string>& pragma_once_files,
        size_t depth)
    {
        constexpr size_t MAX_DEPTH = 32U;
        if (depth > MAX_DEPTH)
            return make_shader_load_failure("Shader loader: include depth exceeded.");

        std::istringstream stream = std::istringstream(source);
        std::string line;
        std::string expanded;
        expanded.reserve(source.size());
        bool marks_once = false;

        while (std::getline(stream, line))
        {
            std::string trimmed = trim_string(line);
            if (is_pragma_once_directive(trimmed))
            {
                // Mark this file as include-once and omit the directive from the output.
                marks_once = true;
                continue;
            }

            std::string include_path_text;
            if (!try_parse_include_directive(trimmed, include_path_text))
            {
                if (is_include_directive(trimmed))
                {
                    // The line looks like an include but we couldn't parse a usable path.
                    // Fail explicitly so the user gets a clear diagnostic instead of a driver
                    // error.
                    return make_shader_load_failure("Shader loader: invalid #include directive.");
                }

                // Ordinary line, keep it verbatim.
                expanded.append(line);
                expanded.append("\n");
                continue;
            }

            ShaderLoadResult include_result = try_load_include_file(
                file_operator,
                asset_manager,
                source_file,
                std::filesystem::path(include_path_text));
            if (!include_result.succeeded)
                return include_result;

            std::string include_key =
                include_result.resolved_path.lexically_normal().generic_string();
            if (pragma_once_files.contains(include_key))
                // This file was previously expanded and marked `#pragma once`, so skip it.
                continue;

            for (const auto& entry : include_stack)
            {
                if (entry == include_result.resolved_path)
                {
                    // Simple cycle detection: if we try to include a file already on the call
                    // stack, we would recurse forever. Fail with a clear error.
                    return make_shader_load_failure(
                        "Shader loader: include cycle detected for '"
                        + include_result.resolved_path.string() + "'.");
                }
            }

            include_stack.push_back(include_result.resolved_path);
            ShaderLoadResult expanded_include = try_expand_includes(
                file_operator,
                asset_manager,
                include_result.resolved_path,
                include_result.data,
                include_stack,
                pragma_once_files,
                depth + 1U);
            if (!expanded_include.succeeded)
                return expanded_include;
            include_stack.pop_back();

            // Paste included text directly at the include site.
            expanded.append(expanded_include.data);
        }

        // Record include-once status after processing the file so subsequent includes can be
        // skipped without needing to re-scan the source for the directive.
        if (marks_once && !source_file.empty())
            pragma_once_files.emplace(source_file.lexically_normal().generic_string());

        return make_shader_load_success(std::move(expanded));
    }

    static bool try_parse_shader_type(std::string_view type_name, ShaderType& out_type)
    {
        std::string type_text = std::string(type_name);
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
            out_type = ShaderType::VERTEX;
            return true;
        }
        if (type_text == "fragment")
        {
            out_type = ShaderType::FRAGMENT;
            return true;
        }
        if (type_text == "compute")
        {
            out_type = ShaderType::COMPUTE;
            return true;
        }

        out_type = ShaderType::NONE;
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
        std::istringstream stream = std::istringstream(file_data);
        std::string line;
        std::string pending_header;
        ShaderType current_type = ShaderType::NONE;
        std::string current_source;
        bool has_section = false;

        while (std::getline(stream, line))
        {
            std::string trimmed = trim_string(line);
            if (trimmed.rfind("#type", 0U) == 0U)
            {
                std::string type_name = trim_string(trimmed.substr(5U));
                ShaderType parsed_type = ShaderType::NONE;
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

        if (current_type != ShaderType::NONE)
            shaders.emplace_back(current_source, current_type);

        if (shaders.empty())
        {
            error_message = "Shader loader: no shader stages were parsed.";
            return false;
        }

        return true;
    }

    void GlslShaderLoaderPlugin::on_attach(IPluginHost& host)
    {
        _asset_manager = &host.get_asset_manager();
        _working_directory = host.get_settings().working_directory;
    }

    void GlslShaderLoaderPlugin::on_detach()
    {
        _asset_manager = nullptr;
        _working_directory = std::filesystem::path();
    }

    void GlslShaderLoaderPlugin::on_recieve_message(Message& msg)
    {
        auto* request = handle_message<LoadShaderRequest>(msg);
        if (!request)
            return;

        on_load_shader_program_request(*request);
    }

    void GlslShaderLoaderPlugin::on_load_shader_program_request(LoadShaderRequest& request)
    {
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Shader loader: missing shader asset wrapper.");
            return;
        }

        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = MessageState::CANCELLED;
            request.result.flag_failure("Shader loader cancelled.");
            return;
        }

        if (!_asset_manager)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Shader loader: file services unavailable.");
            return;
        }

        FileOperator file_operator = FileOperator(_working_directory);
        if (request.path.extension() != ".glsl")
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Shader loader: unsupported shader file extension.");
            return;
        }

        std::string file_data;
        if (!file_operator.read_file(request.path, FileDataFormat::UTF8_TEXT, file_data))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(request.path, "file could not be read"));
            return;
        }

        std::vector<ShaderSource> shaders;
        std::string parse_error;
        if (!try_parse_shader_source(file_data, shaders, parse_error))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(build_load_failure_message(request.path, parse_error));
            return;
        }

        for (auto& shader : shaders)
        {
            std::vector<std::filesystem::path> include_stack = {request.path};
            std::unordered_set<std::string> pragma_once_files = {};
            ShaderLoadResult expanded = try_expand_includes(
                file_operator,
                *_asset_manager,
                request.path,
                shader.source,
                include_stack,
                pragma_once_files,
                0U);
            if (!expanded.succeeded)
            {
                request.state = MessageState::ERROR;
                request.result.flag_failure(
                    build_load_failure_message(request.path, expanded.error));
                return;
            }

            shader.source = std::move(expanded.data);
        }

        *asset = Shader(std::move(shaders));

        request.state = MessageState::HANDLED;
    }
}
