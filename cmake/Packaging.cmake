# ============================================================================
# Packaging — produces a standalone, redistributable build of NOVA via CPack.
#
#   macOS   : NOVA.app inside a .dmg          (DragNDrop)
#   Windows : NOVA/ tree inside a .zip        (ZIP)
#   Linux   : NOVA/ tree inside a .tar.gz     (TGZ)
#
# Build with:
#   cmake --preset packaging
#   cmake --build --preset packaging --target package
#
# macOS bundles MoltenVK so end users don't need the Vulkan SDK. Linux/Windows
# end users need a GPU driver with Vulkan support (standard with any modern
# Mesa/NVIDIA/AMD/Intel driver).
# ============================================================================

set(NOVA_BUNDLE_ID  "edu.tamu.nova.NOVA")
set(NOVA_COPYRIGHT  "Copyright (C) NOVA Team")

# ----- Locate Metavision HAL plugin tree shipped by the openeb vcpkg port -----
# Plugins are dlopen()ed at runtime, so file(GET_RUNTIME_DEPENDENCIES) won't
# pick them up. We have to install them explicitly. Derive the path from the
# imported HAL target rather than from internal vcpkg cache vars so it keeps
# working if the layout shifts.
get_target_property(_hal_lib_path Metavision::HAL IMPORTED_LOCATION_RELEASE)
if(NOT _hal_lib_path)
    get_target_property(_hal_lib_path Metavision::HAL IMPORTED_LOCATION)
endif()
get_filename_component(_hal_lib_dir "${_hal_lib_path}" DIRECTORY)
set(NOVA_HAL_PLUGIN_DIR  "${_hal_lib_dir}/metavision/hal/plugins")
set(NOVA_HAL_BIASES_DIR  "${_hal_lib_dir}/../share/metavision/hal/resources/biases")

if(NOT EXISTS "${NOVA_HAL_PLUGIN_DIR}")
    message(WARNING "Metavision HAL plugin dir not found at ${NOVA_HAL_PLUGIN_DIR} — camera support won't work in the package.")
endif()

# ----- Locate MoltenVK so the bundle ships its own Vulkan driver on macOS -----
# Without this, end users without the Vulkan SDK installed will see the loader
# fail to find a driver and the app won't render.
if(APPLE)
    find_package(Vulkan QUIET COMPONENTS MoltenVK)
    if(Vulkan_MoltenVK_FOUND)
        set(NOVA_MOLTENVK_LIB "${Vulkan_MoltenVK_LIBRARY}")
        message(STATUS "Found MoltenVK: ${NOVA_MOLTENVK_LIB}")
    else()
        message(WARNING "libMoltenVK.dylib not found — packaged .app will not run on machines without the Vulkan SDK. Install the Vulkan SDK or set VULKAN_SDK.")
    endif()
endif()

# ----- Per-platform install layout -----
if(APPLE)
    # Determine the actual macOS floor for LSMinimumSystemVersion. Without
    # CMAKE_OSX_DEPLOYMENT_TARGET the linker stamps the build host's version
    # into LC_BUILD_VERSION, and dyld enforces that regardless of what the
    # plist claims — so the plist needs to track reality, not fiction.
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(NOVA_MIN_MACOS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        execute_process(
            COMMAND sw_vers -productVersion
            OUTPUT_VARIABLE NOVA_MIN_MACOS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    # Pre-substitute Info.plist.in via configure_file so we can drive
    # LSMinimumSystemVersion from a custom variable. CMake's per-target plist
    # mechanism only substitutes a fixed set of MACOSX_BUNDLE_* names.
    set(MACOSX_BUNDLE_BUNDLE_NAME           "${PROJECT_NAME}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION        "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING  "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER        "${NOVA_BUNDLE_ID}")
    set(MACOSX_BUNDLE_EXECUTABLE_NAME       "${PROJECT_NAME}")
    set(MACOSX_BUNDLE_COPYRIGHT             "${NOVA_COPYRIGHT}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in"
        "${CMAKE_BINARY_DIR}/Info.plist"
        @ONLY
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE             TRUE
        MACOSX_BUNDLE_INFO_PLIST  "${CMAKE_BINARY_DIR}/Info.plist"
        INSTALL_RPATH             "@executable_path/../Frameworks"
    )

    set(NOVA_APP_REL          "${PROJECT_NAME}.app")
    set(NOVA_INSTALL_BIN      "${NOVA_APP_REL}/Contents/MacOS")
    set(NOVA_INSTALL_LIB      "${NOVA_APP_REL}/Contents/Frameworks")
    set(NOVA_INSTALL_PLUGINS  "${NOVA_APP_REL}/Contents/Frameworks/metavision/hal/plugins")
    set(NOVA_INSTALL_BIASES   "${NOVA_APP_REL}/Contents/Resources/metavision/hal/biases")
else()
    set(NOVA_INSTALL_BIN      "bin")
    # Windows resolves DLLs from the executable's directory; Linux uses rpath.
    if(WIN32)
        set(NOVA_INSTALL_LIB  "bin")
    else()
        set(NOVA_INSTALL_LIB  "lib")
        set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    endif()
    set(NOVA_INSTALL_PLUGINS  "lib/metavision/hal/plugins")
    set(NOVA_INSTALL_BIASES   "share/metavision/hal/biases")
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${NOVA_INSTALL_BIN}"
    BUNDLE  DESTINATION "."
)

install(DIRECTORY "${NOVA_HAL_PLUGIN_DIR}/"
    DESTINATION "${NOVA_INSTALL_PLUGINS}"
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING
        PATTERN "*.dylib"
        PATTERN "*.so*"
        PATTERN "*.dll"
)

if(EXISTS "${NOVA_HAL_BIASES_DIR}")
    install(DIRECTORY "${NOVA_HAL_BIASES_DIR}/"
        DESTINATION "${NOVA_INSTALL_BIASES}"
    )
endif()

# ----- Bundle MoltenVK + ICD manifest (macOS only) -----
if(APPLE AND NOVA_MOLTENVK_LIB)
    install(FILES "${NOVA_MOLTENVK_LIB}"
        DESTINATION "${NOVA_INSTALL_LIB}"
    )
    # Write a manifest with library_path relative to the JSON's location, so
    # the Vulkan loader resolves the driver inside the bundle no matter where
    # the .app is dropped. The startup bootstrap sets VK_ICD_FILENAMES to point
    # at this file.
    set(_icd_dest "${NOVA_APP_REL}/Contents/Resources/vulkan/icd.d")
    file(WRITE "${CMAKE_BINARY_DIR}/MoltenVK_icd.json"
[=[{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../Frameworks/libMoltenVK.dylib",
        "api_version": "1.2.0",
        "is_portability_driver": true
    }
}
]=])
    install(FILES "${CMAKE_BINARY_DIR}/MoltenVK_icd.json"
        DESTINATION "${_icd_dest}"
    )
endif()

# ----- Bundle transitive shared library deps -----
# macOS: BundleUtilities::fixup_bundle handles dep discovery, copying, and
# rewrites every Mach-O load command + rpath so the .app is relocatable.
# Linux/Windows: file(GET_RUNTIME_DEPENDENCIES) plus $ORIGIN-style rpath.
set(_dep_search_dirs "${_hal_lib_dir}" "${NOVA_HAL_PLUGIN_DIR}")

if(APPLE)
    install(CODE "
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        include(BundleUtilities)
        file(GLOB _plugins
            \"\${CMAKE_INSTALL_PREFIX}/${NOVA_INSTALL_PLUGINS}/*.dylib\"
            \"\${CMAKE_INSTALL_PREFIX}/${NOVA_INSTALL_PLUGINS}/*.so\"
        )
        # Skip MoltenVK: it's already self-contained, correctly install-named,
        # and dlopened by the loader via absolute path from the ICD JSON, so
        # fixup_bundle has nothing useful to do for it.
        fixup_bundle(
            \"\${CMAKE_INSTALL_PREFIX}/${NOVA_APP_REL}\"
            \"\${_plugins}\"
            \"${_dep_search_dirs}\"
            IGNORE_ITEM \"libMoltenVK.dylib\"
        )
        # Re-sign with an ad-hoc signature; install_name_tool invalidated the
        # original. Apple Silicon refuses to launch unsigned Mach-Os.
        execute_process(
            COMMAND codesign --force --deep --sign -
                    \"\${CMAKE_INSTALL_PREFIX}/${NOVA_APP_REL}\"
        )
    ")
else()
    set(_sys_lib_regexes
        "^/lib/.*" "^/lib64/.*" "^/usr/lib/.*" "^/usr/lib64/.*"
        "^[Cc]:[\\\\/][Ww]indows[\\\\/].*"
    )
    install(CODE "
        file(GLOB _plugins
            \"\${CMAKE_INSTALL_PREFIX}/${NOVA_INSTALL_PLUGINS}/*.so*\"
            \"\${CMAKE_INSTALL_PREFIX}/${NOVA_INSTALL_PLUGINS}/*.dll\"
        )
        file(GET_RUNTIME_DEPENDENCIES
            RESOLVED_DEPENDENCIES_VAR   _resolved
            UNRESOLVED_DEPENDENCIES_VAR _unresolved
            EXECUTABLES \"\${CMAKE_INSTALL_PREFIX}/${NOVA_INSTALL_BIN}/${PROJECT_NAME}\"
            MODULES     \${_plugins}
            DIRECTORIES \"${_dep_search_dirs}\"
            PRE_EXCLUDE_REGEXES
                ${_sys_lib_regexes}
                \"^api-ms-.*\" \"^ext-ms-.*\"
                \"^libvulkan.*\" \"^vulkan-1.*\"
            POST_EXCLUDE_REGEXES
                ${_sys_lib_regexes}
                \".*libvulkan.*\"
        )
        foreach(_lib IN LISTS _resolved)
            file(INSTALL \"\${_lib}\"
                DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${NOVA_INSTALL_LIB}\"
                FOLLOW_SYMLINK_CHAIN
            )
        endforeach()
        foreach(_lib IN LISTS _unresolved)
            message(STATUS \"Unresolved runtime dep (skipped): \${_lib}\")
        endforeach()
    ")
endif()

# ----- CPack -----
set(CPACK_PACKAGE_NAME                  "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR                "NOVA Team")
set(CPACK_PACKAGE_VERSION               "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY   "Neuromorphic Optics and Visualization Application")
set(CPACK_PACKAGE_INSTALL_DIRECTORY     "${PROJECT_NAME}")

string(TOLOWER "${CMAKE_SYSTEM_NAME}" _sys)
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${_sys}-${CMAKE_SYSTEM_PROCESSOR}")

if(APPLE)
    set(CPACK_GENERATOR        "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME  "${PROJECT_NAME} ${PROJECT_VERSION}")
elseif(WIN32)
    set(CPACK_GENERATOR  "ZIP")
else()
    set(CPACK_GENERATOR  "TGZ")
endif()

include(CPack)
