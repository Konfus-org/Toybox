#include "glsl_shader_loader_plugin.h"
#include "tbx/app/app_settings.h"
#include "tbx/assets/messages.h"
#include "tbx/files/file_ops.h"
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

    // Resolves and reads an include file by checking:
    // 1) Relative to the including file (if any).
    // 2) Via the asset manager's search roots.
    static ShaderLoadResult try_load_include_file(
        const IFileOps& file_operator,
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
    // - Ensures each resolved include file is expanded only once per shader stage.
    // - Detects include cycles using the include stack.
    static ShaderLoadResult try_expand_includes(
        const IFileOps& file_operator,
        const AssetManager& asset_manager,
        const std::filesystem::path& source_file,
        const std::string& source,
        std::vector<std::filesystem::path>& include_stack,
        std::unordered_set<std::string>& included_files,
        size_t depth)
    {
        constexpr size_t MAX_DEPTH = 32U;
        if (depth > MAX_DEPTH)
            return make_shader_load_failure("Shader loader: include depth exceeded.");

        std::istringstream stream = std::istringstream(source);
        std::string line;
        std::string expanded;
        expanded.reserve(source.size());

        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            std::string trimmed = trim_string(line);
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

            if (included_files.contains(include_key))
                continue;

            included_files.emplace(include_key);
            include_stack.push_back(include_result.resolved_path);
            ShaderLoadResult expanded_include = try_expand_includes(
                file_operator,
                asset_manager,
                include_result.resolved_path,
                include_result.data,
                include_stack,
                included_files,
                depth + 1U);
            if (!expanded_include.succeeded)
                return expanded_include;
            include_stack.pop_back();

            // Paste included text directly at the include site.
            expanded.append(expanded_include.data);
        }

        return make_shader_load_success(std::move(expanded));
    }

    static bool try_get_shader_type_from_extension(
        const std::filesystem::path& path,
        ShaderType& out_type)
    {
        std::string extension = path.extension().generic_string();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });

        if (extension == ".vert")
        {
            out_type = ShaderType::VERTEX;
            return true;
        }
        if (extension == ".tes")
        {
            out_type = ShaderType::TESSELATION;
            return true;
        }
        if (extension == ".geom")
        {
            out_type = ShaderType::GEOMETRY;
            return true;
        }
        if (extension == ".frag")
        {
            out_type = ShaderType::FRAGMENT;
            return true;
        }
        if (extension == ".comp")
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

    static ShaderLoadResult try_read_shader_file(
        const IFileOps& file_operator,
        const std::filesystem::path& path,
        std::string& out_data)
    {
        if (path.empty())
            return make_shader_load_failure("Shader loader: empty shader path.");

        if (!file_operator.read_file(path, FileDataFormat::UTF8_TEXT, out_data))
        {
            return make_shader_load_failure(
                build_load_failure_message(path, "file could not be read"));
        }

        return make_shader_load_success({});
    }

    void GlslShaderLoaderPlugin::on_attach(IPluginHost& host)
    {
        _working_directory = host.get_settings().paths.working_directory;
        if (!_file_ops)
            _file_ops = std::make_shared<FileOperator>(_working_directory);
    }

    void GlslShaderLoaderPlugin::on_detach()
    {
        _working_directory = std::filesystem::path();
    }

    void GlslShaderLoaderPlugin::set_file_ops(std::shared_ptr<IFileOps> file_ops)
    {
        _file_ops = std::move(file_ops);
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

        if (!_file_ops)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Shader loader: file services unavailable.");
            return;
        }

        ShaderType requested_type = ShaderType::NONE;
        if (!try_get_shader_type_from_extension(request.path, requested_type))
        {
            request.state = MessageState::ERROR;
            if (request.path.extension() == ".glsl")
            {
                request.result.flag_failure(
                    "Shader loader: .glsl files are include-only; use .vert/.tes/.geom/.frag/.comp "
                    "for shader programs.");
            }
            else
            {
                request.result.flag_failure("Shader loader: unsupported shader file extension.");
            }
            return;
        }

        std::string stage_data;
        auto read_result = try_read_shader_file(*_file_ops, request.path, stage_data);
        if (!read_result.succeeded)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(read_result.error);
            return;
        }

        auto shader = ShaderSource(std::move(stage_data), requested_type);
        std::vector<std::filesystem::path> include_stack = {request.path};
        std::unordered_set<std::string> included_files = {};
        auto& asset_manager = get_host().get_asset_manager();
        ShaderLoadResult expanded = try_expand_includes(
            *_file_ops,
            asset_manager,
            request.path,
            shader.source,
            include_stack,
            included_files,
            0U);
        if (!expanded.succeeded)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(build_load_failure_message(request.path, expanded.error));
            return;
        }

        shader.source = std::move(expanded.data);
        *asset = Shader(std::move(shader));

        request.state = MessageState::HANDLED;
    }
}
