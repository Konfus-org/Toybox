add_compile_options(
  $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
  $<$<CXX_COMPILER_ID:MSVC>:/MP>
  $<$<CXX_COMPILER_ID:MSVC>:/GR>
  $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/Od>
  $<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>>:-O0>
)
