include(engine_test_targets)

# Runtime reflection API coverage. Reflects engine-registered components (e.g. Transform) directly, so
# no per-test attribute codegen is needed — the reflection records are registered inside the Engine DLL.
tbx_add_engine_test(NAME ReflectionTests)
