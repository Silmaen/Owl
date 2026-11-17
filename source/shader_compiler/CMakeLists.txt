#
# Shader compiler: Slang, linked by Owl Nest, the development runner, OwlShaderBake, the tests and the benchmarks.
# An object library, so that its objects (and the static initializer that installs the compiler in the engine) land in
# every program that links it; the game runner links none, nor Slang.
#
set(OWL_SHADER_COMPILER ${CMAKE_PROJECT_NAME}ShaderCompiler)
file(GLOB_RECURSE SRCS_SHADER_COMPILER CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE HDRS_SHADER_COMPILER CONFIGURE_DEPENDS *.h)
add_library(${OWL_SHADER_COMPILER} OBJECT ${SRCS_SHADER_COMPILER} ${HDRS_SHADER_COMPILER})
set_target_properties(${OWL_SHADER_COMPILER} PROPERTIES FOLDER "Engine")
# The shader utilities it plugs into are engine internals, as for OwlShaderBake.
target_link_libraries(${OWL_SHADER_COMPILER} PUBLIC ${ENGINE_NAME} ${ENGINE_NAME}Private)

set(StateSave ${CMAKE_MAP_IMPORTED_CONFIG_DEBUG})
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)
find_package(slang REQUIRED)
message(STATUS "Found slang version ${slang_VERSION} @ ${slang_DIR}")
target_link_libraries(${OWL_SHADER_COMPILER} PUBLIC slang::slang)
if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
    # Conan's runtime_deploy copies the DLLs only: Slang also loads its standard modules from the folder next to
    # its DLL (importSharedLibs.py does it on Linux).
    string(TOUPPER "${${PROJECT_PREFIX}_CONAN_BUILD_TYPE}" _owl_build_type)
    set(_owl_slang_root "${slang_PACKAGE_FOLDER_${_owl_build_type}}")
    file(GLOB _owl_slang_modules LIST_DIRECTORIES true "${_owl_slang_root}/lib/slang-standard-module-*")
    foreach (_owl_module IN LISTS _owl_slang_modules)
        if (IS_DIRECTORY "${_owl_module}")
            file(COPY "${_owl_module}" DESTINATION "${CMAKE_BINARY_DIR}/bin")
        endif ()
    endforeach ()
    if (NOT _owl_slang_modules)
        message(WARNING "Slang: no standard module folder in ${_owl_slang_root}/lib.")
    endif ()
    if (NOT ${PROJECT_PREFIX}_BUILD_SHARED)
        add_custom_target(${OWL_SHADER_COMPILER}Dll ALL
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${slang_DIR}/../../bin/slang.dll" "${CMAKE_BINARY_DIR}/bin"
                COMMENT "Copy the slang shared dll"
        )
    endif ()
endif ()
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG ${StateSave})
