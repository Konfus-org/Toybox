# The C++ scripting backend. Two pieces:
#  - runtime/  -> Tbx::CppScripting, a normal shared lib holding tbx::Script + the C++ script-type
#                 registry + CppScriptingBackend. The app links it (to compile scripts) and so does this
#                 plugin. It is NOT a plugin, so the loader never shadow-copies it: one registry instance.
#  - this dir  -> the CppScripting plugin, which registers the backend with the engine's ScriptingRegistry
#                 on attach. It links the runtime lib for the backend type.
add_subdirectory(runtime)

tbx_add_plugin(
    NAME CppScripting
    PRIVATE_LINKS
        Tbx::CppScripting
)
