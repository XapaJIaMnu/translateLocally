cmake_minimum_required(VERSION 3.5)
CMAKE_POLICY(SET CMP0020 NEW) # This should help linking on windows in some edge cases. It is also a new default behaviour and we will get warnings unless we set it with cmake 3.22+

if(APPLE) # This needs to happen before the project definition due to a cmake bug
  # https://gitlab.kitware.com/cmake/cmake/-/issues/21608
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
endif(APPLE)

project(translateLocally LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CMakeDependentOption)

find_package(Git QUIET)
include(GetVersionFromFile)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

##### translateLocally options begin #####
set(BUILD_EXTERNAL_LIBARCHIVE OFF CACHE BOOL "Build libarchive as external project.")
set(APPLE_FORCE_STATIC_LIBARCHIVE ON CACHE BOOL "Link to static libarchive on Mac.")
##### translateLocally options end   #####

#MSVC can't seem to pick up correct flags otherwise:
if(MSVC)
  add_definitions(-DUSE_SSE2=1) # Supposed to fix something in the sse_mathfun.h but not sure it does
  set(INTRINSICS "/arch:AVX2") # ARCH we're targetting on win32. @TODO variable

  set(CMAKE_CXX_FLAGS           "/EHsc /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE /D_CRT_NONSTDC_NO_WARNINGS /D_CRT_SECURE_NO_WARNINGS /bigobj")
  set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS} /MT /O2 ${INTRINSICS} /Zi /MP /GL /DNDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS} /MTd /Od /Ob0 ${INTRINSICS} /RTC1 /Zi /D_DEBUG")

  # ignores warning LNK4049: locally defined symbol free imported - this comes from zlib
  set(CMAKE_EXE_LINKER_FLAGS         "${CMAKE_EXE_LINKER_FLAGS} /DEBUG /LTCG:incremental /INCREMENTAL:NO /ignore:4049")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRTD")
  set(CMAKE_STATIC_LINKER_FLAGS      "${CMAKE_STATIC_LINKER_FLAGS} /LTCG:incremental")
endif(MSVC)
message("Building ${PROJECT_NAME}-${PROJECT_VERSION_STRING_FULL}")
# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check https://doc.qt.io/qt/deployment-android.html for more information.
# They need to be set before the find_package( ...) calls below.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()
include(findLibarchiveCustom)
find_package(Threads REQUIRED) # Cross-platform compatible way to get threads?

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui PrintSupport Widgets LinguistTools Network DBus Svg REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui PrintSupport Widgets LinguistTools Network DBus Svg REQUIRED)

# Marian submodule things
# Documentation: https://cliutils.gitlab.io/modern-cmake/chapters/projects/submodule.html
# Ensures the submodules are set correctly during a build.
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

SET(COMPILE_CUDA OFF CACHE BOOL "Compile GPU version")
SET(USE_SENTENCEPIECE ON CACHE BOOL "Download and compile SentencePiece")
SET(USE_STATIC_LIBS ON CACHE BOOL "Link statically against non-system libs")
SET(USE_WASM_COMPATIBLE_SOURCE OFF CACHE BOOL "Don't build wasm compatible sources")

add_subdirectory(3rd_party)
# end of marian options

#set(TS_FILES translateLocally_es_ES.ts) //Causes compilation to go on forever...
# Set up source
include_directories(src)
include_directories(logo)
add_subdirectory(src)
# This doesn't quite work if places in the subdirectory's CMakeLists.txt
set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/mainwindow.ui
        src/AlignmentHighlighter.cpp
        src/AlignmentHighlighter.h
        src/AlignmentWorker.cpp
        src/AlignmentWorker.h
        src/ColorWell.cpp
        src/ColorWell.h
        src/TranslatorSettingsDialog.ui
        src/TranslatorSettingsDialog.cpp
        src/TranslatorSettingsDialog.h
        src/MarianInterface.cpp
        src/MarianInterface.h
        src/ModelManager.cpp
        src/ModelManager.h
        src/Network.cpp
        src/Network.h
        src/Settings.cpp
        src/Settings.h
        src/Translation.h
        src/Translation.cpp
        src/types.h
        logo/logo_svg.h
        ${TS_FILES}
)

# Make PROJECT_VERSION_* available to our code as TRANSLATELOCALLY_VERSION_*
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

# Add CURRENT_BINARY_DIR as include path to find the version.h file
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Set icon file for windows
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/logo/translateLocally.rc")
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE "translateLocally.icns")
    set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/logo/translateLocally.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES
           MACOSX_PACKAGE_LOCATION "Resources")
endif(WIN32)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
   if(APPLE)
     qt_add_executable(translateLocally MACOSX_BUNDLE
       ${PROJECT_SOURCES} ${APP_ICON_MACOSX}
     )
   endif()

    qt_add_executable(translateLocally-bin
       ${PROJECT_SOURCES}
       ${APP_ICON_RESOURCE_WINDOWS}
    )

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(translateLocally-bin SHARED
            ${PROJECT_SOURCES}
        )
    else()
        if(APPLE)
          add_executable(translateLocally MACOSX_BUNDLE
              ${PROJECT_SOURCES} ${APP_ICON_MACOSX}
          )
        endif()
        add_executable(translateLocally-bin
              ${PROJECT_SOURCES}
              ${APP_ICON_RESOURCE_WINDOWS}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

if(WIN32) # Do not launch console on win32
    set_property(TARGET translateLocally-bin PROPERTY WIN32_EXECUTABLE TRUE)
endif()


set(LINK_LIBRARIES
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Svg
    bergamot-translator
    ${CMAKE_THREAD_LIBS_INIT} # This should work on all platforms?
    ${LibArchive_LIBRARIES}
    ${CMAKE_DL_LIBS}) # On unix necessary sometimes

# Reserver the translateLocally name for the MacOS and Windows (potentially) executables. Rename the unix executable to that
target_link_libraries(translateLocally-bin PRIVATE ${LINK_LIBRARIES})
set_target_properties(translateLocally-bin PROPERTIES OUTPUT_NAME translateLocally)

if(UNIX)  # Add Linux and apple support for make install
  include(GNUInstallDirs)
  install(TARGETS translateLocally-bin
          RUNTIME
         )
  install(FILES ${CMAKE_SOURCE_DIR}/dist/translateLocally.desktop DESTINATION share/applications)
  install(FILES ${CMAKE_SOURCE_DIR}/logo/translateLocally_logo.png DESTINATION share/icons)
  install(FILES ${CMAKE_SOURCE_DIR}/logo/translateLocally_logo.svg DESTINATION share/icons)

    if(NOT APPLE)
        ## Support CPACK package generation on debian like systems
        # this is triggered by running cpack after make to generate a .deb
        # This won't work on apple and would need to be modified for rpm based distros
        # Find ubuntu version
        execute_process (
            COMMAND bash -c "grep ^NAME /etc/os-release  | tr -d \"\\\"\" | tr -d \"NAME=\""
            OUTPUT_VARIABLE OS_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        execute_process (
            COMMAND bash -c "grep VERSION_ID= /etc/os-release | tr -d \"\\\"\" | tr -d \"VERSION_ID=\""
            OUTPUT_VARIABLE OS_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        execute_process (
            COMMAND ${CMAKE_SOURCE_DIR}/dist/cpu_feature.sh
            OUTPUT_VARIABLE CPU_FEATURE
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )

        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpcre++-dev, qttools5-dev, qtbase5-dev, libqt5svg5-dev, libarchive-dev, qttools5-dev-tools, libprotobuf-dev")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(CPACK_PACKAGE_CONTACT "Nikolay Bogoychev <nheart@gmail.com>")
        set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/deb)
        set(CPACK_GENERATOR "DEB")
        set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
        set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${PROJECT_VERSION_STRING_FULL}-${OS_NAME}-${OS_VERSION}.${CPU_FEATURE})
        set(CPACK_DEBIAN_PACKAGE_NAME ${PROJECT_NAME})
        set(CPACK_DEBIAN_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
        set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "A fast privacy focused machine translation client that translates on your own machine.")

        include(CPack)
        #
        #
        ##
    endif(NOT APPLE)
endif(UNIX)

if(APPLE) # Apple specific installation of the app
  # Add the .app Target
  target_link_libraries(translateLocally PRIVATE ${LINK_LIBRARIES})
  set_target_properties(translateLocally PROPERTIES
    BUNDLE True
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION_STRING_FULL}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    )
  # When on MacOS, we have a non-standard libarchive location because we get it via brew
  target_include_directories(translateLocally-bin PRIVATE ${LibArchive_INCLUDE_DIR})
  target_include_directories(translateLocally PRIVATE ${LibArchive_INCLUDE_DIR})
  # produce .dmg
  include(macdeployqt)
  macdeployqt(translateLocally)
endif(APPLE)
