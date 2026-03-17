set(VCPKG_LIBRARY_LINKAGE dynamic)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO prophesee-ai/openeb
    REF 5.2.0
    SHA512 5eed691571a21049760368727b19e9679a29d5dd4518b2b8bf3b7ef5c9cfa055eaa6d6d914954ea835824c4bb03230aef96c36657757bdd9b3976b3a44c337d3
    HEAD_REF main
    PATCHES
        0001-fix-libusb-macos-frameworks.patch
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    INVERTED_FEATURES
        hdf5 HDF5_DISABLED
)

# openeb sets -Werror=unused-but-set-variable in release builds but its own code triggers it
vcpkg_replace_string("${SOURCE_PATH}/CMakeLists.txt" "-Werror=unused-but-set-variable" "")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DBUILD_SAMPLES=OFF
        -DCOMPILE_PYTHON3_BINDINGS=OFF
        -DBUILD_TESTING=OFF
        -DGENERATE_DOC=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME MetavisionHAL CONFIG_PATH share/cmake/MetavisionHAL DO_NOT_DELETE_PARENT_CONFIG_PATH)
vcpkg_cmake_config_fixup(PACKAGE_NAME MetavisionSDK CONFIG_PATH share/cmake/MetavisionSDK DO_NOT_DELETE_PARENT_CONFIG_PATH)
vcpkg_cmake_config_fixup(PACKAGE_NAME MetavisionPSEEHWLayer CONFIG_PATH share/cmake/MetavisionPSEEHWLayer)

# vcpkg_cmake_config_fixup hardcodes _IMPORT_PREFIX to 3 PATH levels (share/<pkg>/),
# but MetavisionSDK has nested targets at share/MetavisionSDK/Modules/<component>/
# which need 5 levels to reach the install root.
file(GLOB_RECURSE _nested_targets "${CURRENT_PACKAGES_DIR}/share/MetavisionSDK/Modules/*Targets.cmake")
foreach(_file IN LISTS _nested_targets)
    file(READ "${_file}" _contents)
    string(REPLACE
[[get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)]]
[[get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)]]
        _contents "${_contents}")
    file(WRITE "${_file}" "${_contents}")
endforeach()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/licensing/LICENSE_OPEN")
