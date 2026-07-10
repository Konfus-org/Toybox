# The C++ scripting backend plugin. Registers CppScriptingBackend with the engine's ScriptingRegistry
# on attach so ScriptSystem can run compiled tbx::Script types. The Script base class and the script
# registry now live in the engine (Tbx::Engine), so this plugin is entirely in src/: the backend plus
# the thin plugin wrapper, the only C++-language-specific pieces.
tbx_add_plugin(
    NAME CppScripting
)
