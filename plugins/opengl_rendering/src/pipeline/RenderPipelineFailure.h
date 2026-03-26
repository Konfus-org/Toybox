#pragma once

namespace opengl_rendering
{
    inline bool g_render_pipeline_failure = false;

    inline void report_render_pipeline_failure()
    {
        g_render_pipeline_failure = true;
    }

    inline void clear_render_pipeline_failure()
    {
        g_render_pipeline_failure = false;
    }

    inline bool has_render_pipeline_failure()
    {
        return g_render_pipeline_failure;
    }
}
