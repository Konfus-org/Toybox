#include "glsl_shader_loader_plugin.h"
#include "tbx/assets/messages.h"
#include "tbx/files/filesystem.h"
#include "tbx/graphics/shader.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace tbx::plugins
{
    // Trims whitespace from both ends of a string view, returning an owned string for convenience.
    static std::string trim_string(std::string_view text)
    {
        const auto start = text.find_first_not_of(" \r\n\t");
        if (start == std::string_view::npos)
        {
            return "";
        }
        const auto end = text.find_last_not_of(" \r\n\t");
        return std::string(text.substr(start, end - start + 1U));
    }

    // Minimal "uniform <type> <name>;" parser used to validate/introspect basic declarations.
    // This is not a full GLSL parser; it intentionally only supports the simple shape we use.
    static bool try_parse_simple_uniform_declaration(
        std::string_view trimmed_line,
        std::string& out_type,
        std::string& out_name)
    {
        if (trimmed_line.rfind("uniform", 0U) != 0U)
        {
            return false;
        }

        std::istringstream stream = std::istringstream(std::string(trimmed_line));
        std::string keyword;
        std::string type;
        std::string name;
        if (!(stream >> keyword >> type >> name))
        {
            return false;
        }

        if (keyword != "uniform")
        {
            return false;
        }

        if (!name.empty() && name.back() == ';')
        {
            name.pop_back();
        }

        const auto array_start = name.find('[');
        if (array_start != std::string::npos)
        {
            name.erase(array_start);
        }

        out_type = std::move(type);
        out_name = std::move(name);
        return true;
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
        {
            return false;
        }

        auto remainder = trimmed_line.substr(8U);
        const auto include_start = remainder.find_first_not_of(" \t");
        if (include_start == std::string_view::npos)
        {
            return false;
        }
        remainder.remove_prefix(include_start);

        const char opening = remainder.front();
        if (opening == '"' || opening == '<')
        {
            const char closing = opening == '<' ? '>' : '"';
            const auto end = remainder.find(closing, 1U);
            if (end == std::string_view::npos || end <= 1U)
            {
                return false;
            }

            out_path = std::string(remainder.substr(1U, end - 1U));
            return !out_path.empty();
        }

        const auto end = remainder.find_first_of(" \t");
        out_path = std::string(remainder.substr(0U, end));
        return !out_path.empty();
    }

    // Used to detect malformed include lines so we can report an error instead of silently passing an
    // unknown preprocessor directive to the OpenGL driver.
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
        {
            return false;
        }

        std::istringstream stream = std::istringstream(std::string(trimmed_line));
        std::string keyword;
        std::string pragma;
        if (!(stream >> keyword >> pragma))
        {
            return false;
        }

        return (keyword == "#pragma") && (pragma == "once");
    }

    // Resolves and reads an include file by checking:
    // 1) Relative to the including file (if any).
    // 2) Via the filesystem's asset resolution paths.
    static bool try_load_include_file(
        const IFileSystem& filesystem,
        const std::filesystem::path& including_file,
        const std::filesystem::path& include_path,
        std::filesystem::path& out_resolved,
        std::string& out_data,
        std::string& out_error)
    {
        if (include_path.empty())
        {
            out_error = "Shader loader: empty include path.";
            return false;
        }

        if (!include_path.is_absolute() && !including_file.empty())
        {
            const std::filesystem::path local_candidate =
                (including_file.parent_path() / include_path).lexically_normal();
            if (filesystem.read_file(local_candidate, FileDataFormat::Utf8Text, out_data))
            {
                out_resolved = local_candidate;
                return true;
            }
        }

        const std::filesystem::path asset_candidate =
            filesystem.resolve_asset_path(include_path).lexically_normal();
        if (filesystem.read_file(asset_candidate, FileDataFormat::Utf8Text, out_data))
        {
            out_resolved = asset_candidate;
            return true;
        }

        out_error =
            "Shader loader: failed to resolve include path '" + include_path.string() + "'.";
        return false;
    }

    // Recursively expands #include directives into a single source string.
    //
    // Behavior:
    // - Strips all `#include ...` directives by replacing them with included file contents.
    // - Strips `#pragma once` directives (they are not part of GLSL and should not reach OpenGL).
    // - Implements a basic `#pragma once` system to prevent duplicate inclusion during expansion.
    // - Detects include cycles using the include stack.
    static bool try_expand_includes(
        const IFileSystem& filesystem,
        const std::filesystem::path& source_file,
        const std::string& source,
        std::string& out_expanded,
        std::string& out_error,
        std::vector<std::filesystem::path>& include_stack,
        std::unordered_set<std::string>& pragma_once_files,
        const size_t depth)
    {
        constexpr size_t max_depth = 32U;
        if (depth > max_depth)
        {
            out_error = "Shader loader: include depth exceeded.";
            return false;
        }

        std::istringstream stream(source);
        std::string line;
        std::string expanded;
        expanded.reserve(source.size());
        bool marks_once = false;

        while (std::getline(stream, line))
        {
            const std::string trimmed = trim_string(line);
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
                    // Fail explicitly so the user gets a clear diagnostic instead of a driver error.
                    out_error = "Shader loader: invalid #include directive.";
                    return false;
                }

                // Ordinary line, keep it verbatim.
                expanded.append(line);
                expanded.append("\n");
                continue;
            }

            std::filesystem::path resolved_include;
            std::string include_data;
            std::string resolve_error;
            if (!try_load_include_file(
                    filesystem,
                    source_file,
                    std::filesystem::path(include_path_text),
                    resolved_include,
                    include_data,
                    resolve_error))
            {
                out_error = resolve_error;
                return false;
            }

            const std::string include_key = resolved_include.lexically_normal().generic_string();
            if (pragma_once_files.contains(include_key))
            {
                // This file was previously expanded and marked `#pragma once`, so skip it.
                continue;
            }

            for (const auto& entry : include_stack)
            {
                if (entry == resolved_include)
                {
                    // Simple cycle detection: if we try to include a file already on the call stack,
                    // we would recurse forever. Fail with a clear error.
                    out_error = "Shader loader: include cycle detected for '"
                                + resolved_include.string() + "'.";
                    return false;
                }
            }

            include_stack.push_back(resolved_include);
            std::string expanded_include;
            if (!try_expand_includes(
                    filesystem,
                    resolved_include,
                    include_data,
                    expanded_include,
                    out_error,
                    include_stack,
                    pragma_once_files,
                    depth + 1U))
            {
                return false;
            }
            include_stack.pop_back();

            // Paste included text directly at the include site.
            expanded.append(expanded_include);
        }

        if (marks_once && !source_file.empty())
        {
            // Record include-once status after processing the file so subsequent includes can be
            // skipped without needing to re-scan the source for the directive.
            pragma_once_files.emplace(source_file.lexically_normal().generic_string());
        }

        out_expanded = std::move(expanded);
        return true;
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

    void GlslShaderLoaderPlugin::on_attach(IPluginHost& host)
    {
        _filesystem = &host.get_filesystem();
    }

    void GlslShaderLoaderPlugin::on_detach()
    {
        _filesystem = nullptr;
    }

    void GlslShaderLoaderPlugin::on_recieve_message(Message& msg)
    {
        auto* request = handle_message<LoadShaderRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_shader_program_request(*request);
    }

    void GlslShaderLoaderPlugin::on_load_shader_program_request(LoadShaderRequest& request)
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
        if (resolved.extension() != ".glsl")
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

        for (auto& shader : shaders)
        {
            std::string expanded;
            std::string include_error;
            std::vector<std::filesystem::path> include_stack = {resolved};
            std::unordered_set<std::string> pragma_once_files = {};
            if (!try_expand_includes(
                    *_filesystem,
                    resolved,
                    shader.source,
                    expanded,
                    include_error,
                    include_stack,
                    pragma_once_files,
                    0U))
            {
                request.state = MessageState::Error;
                request.result.flag_failure(build_load_failure_message(resolved, include_error));
                return;
            }

            shader.source = std::move(expanded);
        }

        *asset = Shader(std::move(shaders));

        request.state = MessageState::Handled;
    }

    std::filesystem::path GlslShaderLoaderPlugin::resolve_asset_path(
        const std::filesystem::path& path) const
    {
        if (!_filesystem)
        {
            return path;
        }
        return _filesystem->resolve_asset_path(path);
    }
}
