# Standalone Asio (header-only) supplies the cross-platform socket layer. The repo has no root
# CMakeLists at this tag, so FetchContent only downloads it and we wrap the headers in our own
# INTERFACE target.
include(FetchContent)
if(NOT TARGET asio_headers)
    FetchContent_Declare(asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-38-1
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(asio)

    add_library(asio_headers INTERFACE)
    target_include_directories(asio_headers SYSTEM INTERFACE "${asio_SOURCE_DIR}/include")
    # NOGDI keeps wingdi.h (and its ERROR macro, which collides with tbx::LogLevel::ERROR) out of
    # the Windows headers Asio pulls in; NOMINMAX likewise suppresses the min/max macros.
    target_compile_definitions(asio_headers
        INTERFACE
            ASIO_STANDALONE
            ASIO_NO_DEPRECATED
            $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0A00;WIN32_LEAN_AND_MEAN;NOMINMAX;NOGDI>)
endif()

tbx_add_plugin(NAME TcpRpc PRIVATE_LINKS asio_headers)
if(WIN32)
    target_link_libraries(TcpRpc PRIVATE ws2_32)
endif()
