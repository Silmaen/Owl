function(target_import_so_files TARGET)
    if (${PROJECT_PREFIX}_PLATFORM_LINUX)
        get_target_property(TARGET_TYPE ${TARGET} TYPE)
        if (${TARGET_TYPE} STREQUAL "EXECUTABLE" OR ${TARGET_TYPE} STREQUAL "SHARED_LIBRARY")
            message(STATUS "Target: ${TARGET} of type ${TARGET_TYPE}: copy additional shared libs.")
            set(_owl_import_sysroot)
            if (CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
                set(_owl_import_sysroot "--sysroot=${CMAKE_SYSROOT}")
            endif ()
            add_custom_command(TARGET ${TARGET} POST_BUILD
                    COMMAND ${Python3_EXECUTABLE} -u ${PROJECT_SOURCE_DIR}/cmake/importSharedLibs.py
                    "$<TARGET_FILE:${TARGET}>" \"${CMAKE_PREFIX_PATH}\" \"${${PROJECT_PREFIX}_SHARED_LIB_ROOTS}\"
                    ${_owl_import_sysroot}
                    COMMENT "Copy the needed shared libraries"
                    USES_TERMINAL
            )
        endif ()
    endif ()
endfunction()

function(pretty_platform_str INVAR OUTVAR)
    string(REPLACE "Darwin" "MacOS" TMP "${INVAR}")
    string(TOLOWER "${TMP}" TMP)
    set(${OUTVAR} ${TMP} PARENT_SCOPE)
endfunction()

function(pretty_architecture_str INVAR OUTVAR)
    if (INVAR MATCHES "(AMD64|amd64|x86_64|x64)")
        set(${OUTVAR} "x64" PARENT_SCOPE)
    elseif (INVAR MATCHES "(ARM64|arm64|aarch64)")
        set(${OUTVAR} "arm64" PARENT_SCOPE)
    else ()
        set(${OUTVAR} ${INVAR} PARENT_SCOPE)
    endif ()
endfunction()

function(print_system_n_target_infos)
    message(STATUS "---------- HOST SYSTEM ---------")
    message(STATUS " OS       : ${${PROJECT_PREFIX}_HOST_PLATFORM_STR}")
    message(STATUS " ARCH     : ${${PROJECT_PREFIX}_HOST_ARCH_STR}")
    message(STATUS " VERSION  : ${CMAKE_HOST_SYSTEM_VERSION}")
    message(STATUS "--------- TARGET SYSTEM --------")
    message(STATUS " OS       : ${${PROJECT_PREFIX}_PLATFORM_STR}")
    message(STATUS " ARCH     : ${${PROJECT_PREFIX}_ARCH_STR}")
    message(STATUS " COMPILER : ${${PROJECT_PREFIX}_COMPILER_STR} - ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})")
    if (NOT ${CMAKE_CXX_COMPILER_TARGET} STREQUAL "")
        message(STATUS " TARGET   : ${CMAKE_CXX_COMPILER_TARGET}")
    endif ()
    if (NOT ${CMAKE_CXX_COMPILER_ABI} STREQUAL "")
        message(STATUS " ABI      : ${CMAKE_CXX_COMPILER_ABI}")
    endif ()
    if (NOT ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} STREQUAL "")
        message(STATUS " ARCH     : ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
    endif ()
    message(STATUS " LINKER   : ${CMAKE_CXX_COMPILER_LINKER_ID} - ${CMAKE_CXX_COMPILER_LINKER_VERSION} (${CMAKE_CXX_COMPILER_LINKER})")
    message(STATUS " FEATURES : ${CMAKE_CXX_COMPILE_FEATURES}")
    message(STATUS " FLAGS    : ${CMAKE_CXX_FLAGS}")
    message(STATUS "--------------------------------")
endfunction()

# Compile the imgui backends and imgui_stdlib that the ConanCenter recipe ships as sources
# (`res/bindings`, `res/misc/cpp`), into `${PROJECT_PREFIX_LOWER}_imgui_bindings`. The headers are staged
# under `backends/` so `#include <backends/imgui_impl_glfw.h>` resolves.
function(owl_conan_imgui_bindings)
    set(BindingsTarget ${PROJECT_PREFIX_LOWER}_imgui_bindings)
    if (TARGET ${BindingsTarget})
        return()
    endif ()
    # Again here: the package variables are function-scoped and imgui may come in through imguizmo first.
    find_package(imgui CONFIG REQUIRED)
    string(TOUPPER "${${PROJECT_PREFIX}_CONAN_BUILD_TYPE}" BuildType)
    set(ImguiRoot "${imgui_PACKAGE_FOLDER_${BuildType}}")
    if (NOT EXISTS "${ImguiRoot}/res/bindings")
        message(FATAL_ERROR "imgui bindings not found in the Conan package (${ImguiRoot}).")
    endif ()
    set(Staged "${CMAKE_BINARY_DIR}/imgui_bindings")
    file(COPY "${ImguiRoot}/res/bindings/" DESTINATION "${Staged}/backends")
    find_package(glfw3 REQUIRED)
    find_package(VulkanHeaders REQUIRED)
    find_package(VulkanLoader REQUIRED)
    find_package(OpenGL REQUIRED)
    # imgui_stdlib declares its functions IMGUI_API (dllimport from a shared imgui on Windows) but is compiled into
    # the engine: staged without the macro (file(CONFIGURE) rewrites it only when it changes).
    file(READ "${ImguiRoot}/res/misc/cpp/imgui_stdlib.h" StdlibHeader)
    string(REPLACE "IMGUI_API " "" StdlibHeader "${StdlibHeader}")
    file(CONFIGURE OUTPUT "${Staged}/misc/imgui_stdlib.h" CONTENT "${StdlibHeader}" @ONLY)
    file(COPY "${ImguiRoot}/res/misc/cpp/imgui_stdlib.cpp" DESTINATION "${Staged}/misc")
    add_library(${BindingsTarget} STATIC
            "${Staged}/backends/imgui_impl_glfw.cpp"
            "${Staged}/backends/imgui_impl_opengl2.cpp"
            "${Staged}/backends/imgui_impl_opengl3.cpp"
            "${Staged}/backends/imgui_impl_vulkan.cpp"
            "${Staged}/misc/imgui_stdlib.cpp")
    # Third-party sources: never analysed by clang-tidy.
    set_target_properties(${BindingsTarget} PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "External" CXX_CLANG_TIDY "")
    target_include_directories(${BindingsTarget} SYSTEM PUBLIC "${Staged}" "${Staged}/misc")
    target_link_libraries(${BindingsTarget} PUBLIC imgui::imgui PRIVATE glfw Vulkan::Headers Vulkan::Loader OpenGL::GL)
    # The backends default to IMGUI_API, dllimport from a shared imgui on Windows; they live in the engine instead.
    target_compile_definitions(${BindingsTarget} PUBLIC IMGUI_IMPL_API=)
endfunction()

function(owl_target_link_libraries Target LinkType Module)
    set(FindPackageArgs "")
    set(ModuleTarget "${Module}::${Module}")
    foreach (arg IN LISTS ARGV)
        if (arg STREQUAL "REQUIRED" OR
                arg STREQUAL "QUIET" OR
                arg STREQUAL "CONFIG" OR
                arg MATCHES "^[0-9]+\\.[0-9]+.*"
        )
            list(APPEND FindPackageArgs ${arg})
        elseif (arg STREQUAL MODULE_TARGET)
            list(FIND ARGV MODULE_TARGET index)
            math(EXPR next_index "${index} + 1")
            list(GET ARGV ${next_index} ModuleTarget)
        endif ()
    endforeach ()
    # Release third parties in Debug: `CMAKE_MAP_IMPORTED_CONFIG_DEBUG`, set once in the top-level CMakeLists.txt.

    # A few Conan packages use another CMake name or target than the module name (see cmake/Conan.cmake).
    set(Package ${Module})
    if (DEFINED ${PROJECT_PREFIX}_CONAN_PACKAGE_${Module})
        set(Package ${${PROJECT_PREFIX}_CONAN_PACKAGE_${Module}})
    endif ()
    if (DEFINED ${PROJECT_PREFIX}_CONAN_TARGET_${Module})
        set(ModuleTarget ${${PROJECT_PREFIX}_CONAN_TARGET_${Module}})
    endif ()

    # find_package again even when the target exists: the Conan package variables are function-scoped.
    message(STATUS "Loading ${Package}....")
    find_package(${Package} ${FindPackageArgs})
    if (NOT TARGET ${ModuleTarget})
        message(FATAL_ERROR "Module ${ModuleTarget} not found. Please ensure it is built and available in the CMake path.")
    endif ()
    message(STATUS "Found ${Package} version ${${Package}_VERSION} @ ${${Package}_DIR}")

    string(STRIP ${LinkType} LinkType)
    if (NOT ("${LinkType}" STREQUAL "PRIVATE" OR
            "${LinkType}" STREQUAL "PUBLIC" OR
            "${LinkType}" STREQUAL "INTERFACE"))
        message(FATAL_ERROR "LinkType must be one of 'PRIVATE', 'PUBLIC', or 'INTERFACE'. Provided: '${LinkType}'")
    endif ()

    target_link_libraries(${Target} ${LinkType} ${ModuleTarget})
    if (DEFINED ${PROJECT_PREFIX}_CONAN_INCLUDE_SUBDIR_${Module})
        string(TOUPPER "${${PROJECT_PREFIX}_CONAN_BUILD_TYPE}" BuildType)
        foreach (IncludeDir IN LISTS ${Package}_INCLUDE_DIRS_${BuildType})
            target_include_directories(${Target} SYSTEM ${LinkType}
                    "${IncludeDir}/${${PROJECT_PREFIX}_CONAN_INCLUDE_SUBDIR_${Module}}")
        endforeach ()
    endif ()
    if (Module STREQUAL "imgui")
        owl_conan_imgui_bindings()
        # Build tree only: the bindings are compiled into the engine, the installed package does not export them.
        target_link_libraries(${Target} ${LinkType} $<BUILD_INTERFACE:${PROJECT_PREFIX_LOWER}_imgui_bindings>)
    endif ()
endfunction()
