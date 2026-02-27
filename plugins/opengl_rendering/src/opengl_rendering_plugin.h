#pragma once
#include "opengl_renderer.h"
#include "tbx/common/uuid.h"
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <unordered_map>

namespace opengl_rendering
{
    using namespace tbx;
    class OpenGlRenderingPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        void teardown_renderer(const Uuid& window_id);

      private:
        std::unordered_map<Uuid, std::unique_ptr<OpenGlRenderer>> _renderers = {};
    };
}
