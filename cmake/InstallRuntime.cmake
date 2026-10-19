#
# Install script of the Nest component: the third-party runtime the build copied next to the binaries.
#
# Run by `cmake --install` / `cpack` with `OWL_RUNTIME_FROM` (the build's bin/) and `OWL_RUNTIME_TO` (the install
# folder of the binaries, relative to the prefix). The tested tree also holds the tests and what their runs wrote
# (logs, settings, recovery autosaves, caches, ImGui layouts): only the files listed here ship, nothing else, not even
# an empty folder.
#
if (NOT OWL_RUNTIME_FROM OR NOT OWL_RUNTIME_TO)
    message(FATAL_ERROR "InstallRuntime: OWL_RUNTIME_FROM and OWL_RUNTIME_TO must be set.")
endif ()
set(_owl_runtime_destination "${CMAKE_INSTALL_PREFIX}/${OWL_RUNTIME_TO}")

# Shared libraries: Linux sonames (libX.so.N) and Windows DLLs; the Owl targets install themselves.
file(GLOB _owl_runtime_libraries LIST_DIRECTORIES false
        "${OWL_RUNTIME_FROM}/*.so"
        "${OWL_RUNTIME_FROM}/*.so.*"
        "${OWL_RUNTIME_FROM}/*.dll"
)
# slang-llvm (CPU targets, ~80 MB) is never loaded by the engine, which only compiles to SPIR-V; Linux never had it.
list(FILTER _owl_runtime_libraries EXCLUDE REGEX "/(owl_[^/]*|slang-llvm\\.dll)$")
if (_owl_runtime_libraries)
    file(INSTALL ${_owl_runtime_libraries} DESTINATION "${_owl_runtime_destination}")
endif ()

# Slang loads its standard modules at run time from the folder next to its library.
file(GLOB _owl_runtime_slang_modules LIST_DIRECTORIES true "${OWL_RUNTIME_FROM}/slang-standard-module-*")
foreach (_owl_module IN LISTS _owl_runtime_slang_modules)
    if (IS_DIRECTORY "${_owl_module}")
        file(INSTALL "${_owl_module}" DESTINATION "${_owl_runtime_destination}")
    endif ()
endforeach ()
