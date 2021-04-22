# Adapted from https://gitlab.inria.fr/adufay/SpectralViewer/blob/master/cmake/Macdeployqt.cmake

find_package(Qt${QT_VERSION_MAJOR}Core REQUIRED)

# Retrieve the absolute path to qmake and then use that path to find
# the macdeployqt binary
get_target_property(_qmake_executable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

# Add commands that copy the required Qt files to the application bundle
# represented by the target
function(macdeployqt target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${MACDEPLOYQT_EXECUTABLE}"
            \"${target}.app\"
            -always-overwrite -dmg
        COMMENT "Deploying Qt..."
    )
endfunction()

mark_as_advanced(MACDEPLOYQT_EXECUTABLE)
