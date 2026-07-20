if(NOT DEFINED TBX_RESOURCES_PATH)
  if(NOT TBX_FULL_RELEASE AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/resources")
    set(TBX_RESOURCES_PATH "${CMAKE_SOURCE_DIR}/resources")
  else()
    set(TBX_RESOURCES_PATH "")
  endif()
endif()

add_compile_definitions(
  $<$<CONFIG:Debug>:TBX_DEBUG>
  $<$<CONFIG:Release>:TBX_RELEASE>
  TBX_RESOURCES_PATH="${TBX_RESOURCES_PATH}"
  $<$<CONFIG:Debug>:TBX_ASSERTS_ENABLED>
  # Enables TBX_LOG_CATEGORY_SCOPE (the "[Category]" log tagging). On for all configs today; wrap this
  # in a generator expression (e.g. $<$<CONFIG:Debug>:...>) to strip category scopes from a build.
  TBX_ENABLE_LOG_CATEGORIES
  $<$<PLATFORM_ID:Windows>:TBX_PLATFORM_WINDOWS>
  $<$<PLATFORM_ID:Darwin>:TBX_PLATFORM_MACOS>
  $<$<PLATFORM_ID:Linux>:TBX_PLATFORM_LINUX>
)
