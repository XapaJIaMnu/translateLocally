cmake_minimum_required(VERSION 3.5)

project(translateLocally LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE) # When on MacOS, we have a non-standard libarchive location because we get it via brew
#  This attempts to fix a bug, but in fact doesn't do anything.
#  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")
  set(LibArchive_INCLUDE_DIR "/usr/local/opt/libarchive/include")
endif(APPLE)
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
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CMakeDependentOption)
include(GetVersionFromFile)
message("Building ${PROJECT_NAME}-${PROJECT_VERSION_STRING}")
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

find_package(LibArchive REQUIRED)
find_package(Threads REQUIRED) # Cross-platform compatible way to get threads?
if(LibArchive_FOUND)
    list (APPEND ${INCLUDE_DIRECTORIES} ${LibArchive_INCLUDE_DIRS})
else(LibArchive_FOUND)
  MESSAGE(FATAL_ERROR "Could not find the libarchive library and development files.")
endif( LibArchive_FOUND )

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui PrintSupport Widgets LinguistTools Network DBus Svg REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui PrintSupport Widgets LinguistTools Network DBus Svg REQUIRED)

# Marian submodule things
# Documentation: https://cliutils.gitlab.io/modern-cmake/chapters/projects/submodule.html
# Ensures the submodules are set correctly during a build.
find_package(Git QUIET)
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

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        TranslatorSettingsDialog.ui
        TranslatorSettingsDialog.cpp
        TranslatorSettingsDialog.h
        MarianInterface.cpp
        MarianInterface.h
        ModelListItemDelegate.cpp
        ModelListItemDelegate.h
        ModelListItemWidget.cpp
        ModelListItemWidget.h
        ModelListItemWidget.ui
        ModelManager.cpp
        ModelManager.h
        Network.cpp
        Network.h
        types.h
        logo/logo_svg.h
        ${TS_FILES}
)

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

  ## Support CPACK package generation on debian like systems
  #
  #
  # Find ubuntu version
  execute_process (
                COMMAND bash -c "grep ^NAME /etc/os-release  | tr -d \"\\\"\" | tr -d \"NAME=\" | tr -d \"\\n\""
                OUTPUT_VARIABLE OS_NAME
                )
  execute_process (
                COMMAND bash -c "grep VERSION_ID= /etc/os-release | tr -d \"\\\"\" | tr -d \"VERSION_ID=\" | tr -d \"\\n\""
                OUTPUT_VARIABLE OS_VERSION
                )

  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpcre++-dev, qttools5-dev, qtbase5-dev, libarchive-dev, qttools5-dev-tools, libprotobuf-dev")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_PACKAGE_CONTACT "Nikolay Bogoychev <nheart@gmail.com>")
  set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/deb)
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
  set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_GIT_SHA}-${OS_NAME}-${OS_VERSION})
  set(CPACK_DEBIAN_PACKAGE_NAME ${PROJECT_NAME})
  set(CPACK_DEBIAN_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
  set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "A fast privacy focused machine translation client that translates on your own machine.")

  include(CPack)
  #
  #
  ##
endif(UNIX)

if(APPLE) # Apple specific installation of the app
  # Add the .app Target
  target_link_libraries(translateLocally PRIVATE ${LINK_LIBRARIES})
  # When on MacOS, we have a non-standard libarchive location because we get it via brew
  target_include_directories(translateLocally-bin PRIVATE ${LibArchive_INCLUDE_DIR})
  target_include_directories(translateLocally PRIVATE ${LibArchive_INCLUDE_DIR})
  # produce .dmg
  include(macdeployqt)
  macdeployqt(translateLocally)
endif(APPLE)
