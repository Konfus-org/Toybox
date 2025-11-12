#pragma once
#include "tbx/plugin_api/plugin.h"

namespace tbx
{
    /// Static plugin that anchors the Toybox standard library wrappers in a dedicated module.
    /// Ownership: The host application owns plugin instances and controls their lifetimes.
    /// Thread-safety: Not thread-safe; all callbacks must originate from the main thread.
    class TbxStdWrappersPlugin final : public Plugin
    {
      public:
        TbxStdWrappersPlugin() = default;
        ~TbxStdWrappersPlugin() override = default;
        TbxStdWrappersPlugin(const TbxStdWrappersPlugin&) = delete;
        TbxStdWrappersPlugin& operator=(const TbxStdWrappersPlugin&) = delete;
        TbxStdWrappersPlugin(TbxStdWrappersPlugin&&) = delete;
        TbxStdWrappersPlugin& operator=(TbxStdWrappersPlugin&&) = delete;

        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(Message& msg) override;
    };
}
