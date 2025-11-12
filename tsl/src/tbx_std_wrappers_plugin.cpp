#include "tbx_std_wrappers_plugin.h"

namespace tbx
{
    void TbxStdWrappersPlugin::on_attach(Application& host)
    {
        (void)host;
    }

    void TbxStdWrappersPlugin::on_detach()
    {
    }

    void TbxStdWrappersPlugin::on_update(const DeltaTime& dt)
    {
        (void)dt;
    }

    void TbxStdWrappersPlugin::on_message(Message& msg)
    {
        (void)msg;
    }
}
