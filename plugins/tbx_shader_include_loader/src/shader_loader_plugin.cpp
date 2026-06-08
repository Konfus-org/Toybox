#include "shader_include_loader.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/assets/shader.h"
#include <sstream>

namespace tbx::shader_loader
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

    static bool try_read_include_candidate(
        const tbx::IFileOps& file_operator,
        const std::filesystem::path& candidate,
        ShaderLoadResult& out_result)
    {
        const std::filesystem::path resolved_candidate = candidate.lexically_normal();
        if (std::string data;
            file_operator.read_file(resolved_candidate, tbx::FileDataFormat::UTF8_TEXT, data))
        {
            out_result = make_shader_load_success(std::move(data), resolved_candidate);
            return true;
        }

        return false;
    }

    static bool try_load_include_from_shader_roots(
        const tbx::IFileOps& file_operator,
        const tbx::AssetManager& asset_manager,
        const std::filesystem::path& include_path,
        ShaderLoadResult& out_result)
    {
        if (include_path.is_absolute())
            return false;

        for (const auto& asset_directory : asset_manager.get_directories())
        {
            if (asset_directory.empty())
                continue;

            const std::filesystem::path shader_candidate =
                asset_directory / "Shaders" / include_path;
            if (try_read_include_candidate(file_operator, shader_candidate, out_result))
                return true;
        }

        return false;
    }

    static std::string build_load_failure_message(
        const std::filesystem::path& path,
        const std::string_view reason)
    {
        std::string message = "tbx::Shader loader failed to load shader: ";
        message.append(path.string());
        if (!reason.empty())
        {
            message.append(" (reason: ");
            message.append(reason);
            message.append(")");
        }
        return message;
    }

    // Trims whitespace from both ends of a string view, returning an owned string for convenience.
    static std::string trim_string(const std::string_view text)
    {
        const auto start = text.find_first_not_of(" \r\n\t");
        if (start == std::string_view::npos)
            return "";
        const auto end = text.find_last_not_of(" \r\n\t");
        return std::string(text.substr(start, end - start + 1U));
    }

    // Parses GLSL-style include directives:
    // - `#include "path"`
    // - `#include <path>`
    // - `#include path` (bare token; e.g. `#include Globals.glsl`)
    //
    // Returns the include path text, without quotes/angle brackets.
    static bool try_parse_include_directive(
        const std::string_view trimmed_line,
        std::string& out_path)
    {
        if (trimmed_line.rfind("#include", 0U) != 0U)
            return false;

        auto remainder = trimmed_line.substr(8U);
        const auto include_start = remainder.find_first_not_of(" \t");
        if (include_start == std::string_view::npos)
            return false;
        remainder.remove_prefix(include_start);

        if (const char opening = remainder.front(); opening == '"' || opening == '<')
        {
            const char closing = opening == '<' ? '>' : '"';
            const auto end = remainder.find(closing, 1U);
            if (end == std::string_view::npos || end <= 1U)
                return false;

            out_path = std::string(remainder.substr(1U, end - 1U));
            return !out_path.empty();
        }

        const auto end = remainder.find_first_of(" \t");
        out_path = std::string(remainder.substr(0U, end));
        return !out_path.empty();
    }

    // Used to detect malformed include lines so we can report an error instead of silently passing
    // an unknown preprocessor directive to the OpenGL driver.
    static bool is_include_directive(const std::string_view trimmed_line)
    {
        return trimmed_line.rfind("#include", 0U) == 0U;
    }

    // Resolves and reads an include file by checking:
    // 1) Relative to the including file (if any).
    // 2) Via the asset manager's search roots.
    // 3) Relative to each asset root's Shaders directory.
    static ShaderLoadResult try_load_include_file(
        const tbx::IFileOps& file_operator,
        const tbx::AssetManager& asset_manager,
        const std::filesystem::path& including_file,
        const std::filesystem::path& include_path)
    {
        if (include_path.empty())
            return make_shader_load_failure("tbx::Shader loader: empty include path.");

        if (!include_path.is_absolute() && !including_file.empty())
        {
            const std::filesystem::path local_candidate =
                (including_file.parent_path() / include_path).lexically_normal();
            if (ShaderLoadResult result = {};
                try_read_include_candidate(file_operator, local_candidate, result))
                return result;
        }

        const std::filesystem::path asset_candidate =
            asset_manager.resolve_path(include_path).lexically_normal();
        if (ShaderLoadResult result = {};
            try_read_include_candidate(file_operator, asset_candidate, result))
            return result;

        if (ShaderLoadResult result = {};
            try_load_include_from_shader_roots(file_operator, asset_manager, include_path, result))
            return result;

        return make_shader_load_failure(
            "tbx::Shader loader: failed to resolve include path '" + include_path.string() + "'.");
    }

    // Recursively expands #include directives into a single source string.
    //
    // Behavior:
    // - Strips all `#include ...` directives by replacing them with included file contents.
    // - Ensures each resolved include file is expanded only once per shader stage.
    // - Detects include cycles using the include stack.
    static ShaderLoadResult try_expand_includes(
        const tbx::IFileOps& file_operator,
        const tbx::AssetManager& asset_manager,
        const std::filesystem::path& source_file,
        const std::string& source,
        std::vector<std::filesystem::path>& include_stack,
        std::unordered_set<std::string>& included_files,
        size_t depth)
    {
        if (constexpr size_t max_depth = 32U; depth > max_depth)
            return make_shader_load_failure("tbx::Shader loader: include depth exceeded.");

        auto stream = std::istringstream(source);
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
                    return make_shader_load_failure(
                        "tbx::Shader loader: invalid #include directive.");
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
                        "tbx::Shader loader: include cycle detected for '"
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

    void ShaderIncludeLoader::on_attach()
    {
        auto registry = serialization_registry.lock();
        if (!registry)
            return;

        registry->register_transformer<tbx::Shader>(
            [this](
                const std::filesystem::path& asset_path,
                const tbx::ShaderLoadParameters& parameters,
                const tbx::AssetLoadMetadata& metadata,
                tbx::Shader& shader)
            {
                return transform_shader(asset_path, parameters, metadata, shader);
            });
    }

    void ShaderIncludeLoader::on_detach()
    {
        if (auto registry = serialization_registry.lock())
            registry->deregister_transformer<tbx::Shader>();

        asset_manager = {};
        file_ops = {};
        serialization_registry = {};
    }

    tbx::Result ShaderIncludeLoader::transform_shader(
        const std::filesystem::path& asset_path,
        const tbx::ShaderLoadParameters&,
        const tbx::AssetLoadMetadata&,
        tbx::Shader& shader)
    {
        auto result = tbx::Result();
        auto files = file_ops.lock();
        if (!files)
        {
            result.flag_failure("tbx::Shader loader: file services unavailable.");
            return result;
        }

        auto assets = asset_manager.lock();
        if (!assets)
        {
            result.flag_failure("tbx::Shader loader: asset manager unavailable.");
            return result;
        }

        if (shader.type == tbx::ShaderType::NONE)
        {
            result.flag_failure("tbx::Shader loader: shader type metadata is missing.");
            return result;
        }

        if (shader.source.empty())
        {
            result.flag_failure("tbx::Shader loader: shader source is empty.");
            return result;
        }

        std::vector include_stack = {asset_path};
        std::unordered_set<std::string> included_files = {};
        ShaderLoadResult expanded = try_expand_includes(
            *files,
            *assets,
            asset_path,
            shader.source,
            include_stack,
            included_files,
            0U);
        if (!expanded.succeeded)
        {
            result.flag_failure(build_load_failure_message(asset_path, expanded.error));
            return result;
        }

        shader.source = std::move(expanded.data);

        result.flag_success();
        return result;
    }
}
