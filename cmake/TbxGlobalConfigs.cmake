# Global configs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if(MSVC)
    foreach(flag_var IN ITEMS
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
    )
        if(DEFINED ${flag_var})
            string(REPLACE "/W0" "" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()

if(CMAKE_CONFIGURATION_TYPES)
    foreach(config_name IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${config_name}" config_name_upper)
        if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_name_upper})
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_name_upper} "${CMAKE_BINARY_DIR}/${config_name}")
        endif()
        if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_name_upper})
            set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_name_upper} "${CMAKE_BINARY_DIR}/${config_name}")
        endif()
        if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_name_upper})
            set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_name_upper} "${CMAKE_BINARY_DIR}/${config_name}")
        endif()
    endforeach()
endif()
