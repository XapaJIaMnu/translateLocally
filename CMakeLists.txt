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
if(WIN32) # It's difficult to have all dependencies installed on Windows, so fetch them instead
   include(FetchContent)
   set(FETCHCONTENT_QUIET_OFF)
   set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR})
   FetchContent_Declare(libarchive_src
                        URL
                        https://www.libarchive.org/downloads/libarchive-v3.5.1-win64.zip)
   FetchContent_Populate(libarchive_src)
   set(LibArchive_INCLUDE_DIR "${CMAKE_BINARY_DIR}/libarchive_src-src/include")
   set(LibArchive_LIBRARY "${CMAKE_BINARY_DIR}/libarchive_src-src/lib/archive_static.lib")
endif(WIN32)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CMakeDependentOption)

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

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui PrintSupport Widgets LinguistTools Network DBus REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui PrintSupport Widgets LinguistTools Network DBus REQUIRED)

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
        MarianInterface.cpp
        MarianInterface.h
        ModelManager.cpp
        ModelManager.h
        Network.cpp
        Network.h
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
   if(APPLE)
     qt_add_executable(translateLocally MACOSX_BUNDLE
       ${PROJECT_SOURCES}
     )
   endif()

    qt_add_executable(translateLocally-bin
       ${PROJECT_SOURCES}
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
              ${PROJECT_SOURCES}
          )
        endif()
        add_executable(translateLocally-bin
              ${PROJECT_SOURCES}
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
    bergamot-translator
    ${CMAKE_THREAD_LIBS_INIT} # This should work on all platforms?
    ${LibArchive_LIBRARIES}
    ${CMAKE_DL_LIBS}) # On unix necessary sometimes

# Reserver the translateLocally name for the MacOS and Windows (potentially) executables. Rename the unix executable to that
target_link_libraries(translateLocally-bin PRIVATE ${LINK_LIBRARIES})
set_target_properties(translateLocally-bin PROPERTIES OUTPUT_NAME translateLocally)

if(APPLE)
  # Add the .app Target
  target_link_libraries(translateLocally PRIVATE ${LINK_LIBRARIES})
  # When on MacOS, we have a non-standard libarchive location because we get it via brew
  target_include_directories(translateLocally-bin PRIVATE ${LibArchive_INCLUDE_DIR})
  target_include_directories(translateLocally PRIVATE ${LibArchive_INCLUDE_DIR})
  # produce .dmg
  include(macdeployqt)
  macdeployqt(translateLocally)
endif()
