tbx_add_plugin(
    NAME OpenGlRendering
    PRIVATE_LINKS
        glad::glad
        # GPU texture sharing: D3D11 owns the shared surface, DXGI exposes the cross-process
        # handle + keyed mutex, opengl32 provides wglGetProcAddress for the WGL_NV_DX_interop2
        # entry points. Windows-only.
        $<$<PLATFORM_ID:Windows>:d3d11>
        $<$<PLATFORM_ID:Windows>:dxgi>
        $<$<PLATFORM_ID:Windows>:opengl32>
)
