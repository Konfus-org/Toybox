#include "tbx/interfaces/rpc_host.h"

// Anchor TU: IRpcHost is a TBX_API interface referenced only by plugins, so its vtable/ctor/dtor would
// not be emitted into Engine.dll without an out-of-line definition here (plugins would fail to link
// with an "undefined dllimport"). Mirrors interfaces/rpc_router.cpp.
namespace tbx
{
    IRpcHost::IRpcHost() = default;

    IRpcHost::~IRpcHost() noexcept = default;
}
