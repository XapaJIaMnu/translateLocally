# Adapted from https://gitlab.inria.fr/adufay/SpectralViewer/blob/master/cmake/Macdeployqt.cmake

find_package(Qt${QT_VERSION_MAJOR}Core REQUIRED)

# Retrieve the absolute path to qmake and then use that path to find
# the macdeployqt binary
get_target_property(_qmake_executable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

set(APPLE_DEVELOPER_ID "" CACHE STRING "Apple Developer ID used for notarization")

# Add commands that copy the required Qt files to the application bundle
# represented by the target
function(macdeployqt target)
    if (DEFINED APPLE_DEVELOPER_ID)
        add_custom_target(${target}.dmg
            DEPENDS ${target}
            BYPRODUCTS ${target}.app
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE_DIR:${target}>/../.."
                -always-overwrite
                -dmg
                -appstore-compliant
                -sign-for-notarization=${APPLE_DEVELOPER_ID}
            COMMENT "Deploying Qt and signing bundle for notarization..."
        )
    else()
        add_custom_target(${target}.dmg
            DEPENDS ${target}
            BYPRODUCTS ${target}.app
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_FILE_DIR:${target}>/../.."
                -always-overwrite
                -dmg
            COMMENT "Deploying Qt..."
        )
    endif()
endfunction()

mark_as_advanced(MACDEPLOYQT_EXECUTABLE)
