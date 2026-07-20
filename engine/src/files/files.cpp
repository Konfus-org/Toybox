#include "tbx/files/files.h"
#include "tbx/core/typedefs.h"
#include <fstream>

namespace tbx
{
    //// FILES ////

    Result<std::vector<std::byte>> Files::read_bytes(const std::filesystem::path& path) const
    {
        auto stream = std::ifstream(path, std::ios::binary | std::ios::ate);
        if (!stream)
            return fail("could not open '{}'", path.string());
        const auto file_size = static_cast<size>(stream.tellg());
        stream.seekg(0);
        auto bytes = std::vector<std::byte>(file_size);
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(file_size));
        if (!stream)
            return fail("read failed for '{}'", path.string());
        return bytes;
    }

    Result<std::string> Files::read_text(const std::filesystem::path& path) const
    {
        auto stream = std::ifstream(path, std::ios::binary);
        if (!stream)
            return fail("could not open '{}'", path.string());
        auto text =
            std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        return text;
    }

    Result<void> Files::write_text(const std::filesystem::path& path, std::string_view text) const
    {
        if (path.has_parent_path())
        {
            auto ec = std::error_code {};
            std::filesystem::create_directories(path.parent_path(), ec);
        }
        auto stream = std::ofstream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            return fail("could not open '{}' for writing", path.string());
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
            return fail("write failed for '{}'", path.string());
        return {};
    }
}
