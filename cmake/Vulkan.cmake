

function(owl_target_link_vulkan target_name)
    set(StateSave ${CMAKE_MAP_IMPORTED_CONFIG_DEBUG})
    set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release)
    find_package(VulkanHeaders REQUIRED)
    find_package(VulkanLoader REQUIRED)
    get_filename_component(VULKAN_DIR ${VulkanLoader_DIR} DIRECTORY)
    get_filename_component(VULKAN_DIR ${VULKAN_DIR} DIRECTORY)
    message(STATUS "Found Vulkan version ${VulkanLoader_VERSION} @ ${VULKAN_DIR}")
    target_link_libraries(${target_name} PRIVATE Vulkan::Loader)
    find_package(VulkanUtilityLibraries REQUIRED)
    message(STATUS "Found VulkanUtilityLibrary version ${VulkanLoader_VERSION} @ ${VULKAN_DIR}")
    target_link_libraries(${target_name} PRIVATE Vulkan::LayerSettings)
    target_link_libraries(${target_name} PRIVATE Vulkan::UtilityHeaders)
    # Vulkan layers
    if (${PROJECT_PREFIX}_ENABLE_VULKAN_LAYERS)
        if (${PROJECT_PREFIX}_PLATFORM_WINDOWS)
            set(${PROJECT_PREFIX}_VULKAN_LAYER_PATH ${VULKAN_DIR}/bin CACHE PATH "Vulkan layer path")
            set(${PROJECT_PREFIX}_VULKAN_LAYER_BIN_PATH ${VULKAN_DIR}/bin CACHE PATH "Vulkan layer path")
        else ()
            set(${PROJECT_PREFIX}_VULKAN_LAYER_PATH ${VULKAN_DIR}/share/vulkan/explicit_layer.d CACHE PATH "Vulkan layer path")
            set(${PROJECT_PREFIX}_VULKAN_LAYER_BIN_PATH ${VULKAN_DIR}/lib CACHE PATH "Vulkan layer path")
        endif ()
        target_compile_definitions(${target_name} PUBLIC
                $<BUILD_INTERFACE:${PROJECT_PREFIX}_VULKAN_LAYER_PATH="${${PROJECT_PREFIX}_VULKAN_LAYER_PATH}">
                $<BUILD_INTERFACE:${PROJECT_PREFIX}_VULKAN_LAYER_BIN_PATH="${${PROJECT_PREFIX}_VULKAN_LAYER_BIN_PATH}">)
    endif ()
    # Spir-V cross
    # ConanCenter ships SPIRV-Cross as one package with one target per component.
    find_package(spirv-cross REQUIRED)
    message(STATUS "Found spirv-cross version ${spirv-cross_VERSION} @ ${spirv-cross_DIR}")
    target_link_libraries(${target_name}Private INTERFACE spirv-cross-cpp)

    # Slang shader compiler
    find_package(slang REQUIRED)
    message(STATUS "Found slang version ${slang_VERSION} @ ${slang_DIR}")
    target_link_libraries(${target_name}Private INTERFACE slang::slang)
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
    endif ()

    set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG ${StateSave})
endfunction()
