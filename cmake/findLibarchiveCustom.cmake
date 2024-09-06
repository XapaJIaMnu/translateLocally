# This is a helper to find libarchive. On Linux we are just as happy with a static as we are
# with a dynamic link, as we can distribute the package through a package manager or build it
# locally. On windows, we can easily force to link to a static library installed via vcpkg.
# On Mac, we run into trouble, because we get libarchive via brew and we can't easily force a
# static link. On top that, we might not want brew at all, eg when running on High Sierra
# so we take the path to external libarchive.

if(BUILD_EXTERNAL_LIBARCHIVE) # This option will probably not work on WIN32 due to looking for libarchive.a
    include(ExternalProject)
    ExternalProject_Add(project_archive
    PREFIX deps/libarchive-3.5.1
    URL http://libarchive.org/downloads/libarchive-3.5.1.tar.gz
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${TOOLCHAIN_FILE}
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DENABLE_NETTLE:BOOL=OFF
        -DENABLE_ICONV:BOOL=OFF
        -DENABLE_CPIO:BOOL=OFF
        -DENABLE_TEST:BOOL=OFF
        -DENABLE_ACL:BOOL=OFF
    BINARY_DIR deps/libarchive-3.5.1/build
    INSTALL_DIR deps/libarchive-3.5.1/build
    )

    ExternalProject_Get_Property(project_archive INSTALL_DIR)
    set(LibArchive_INCLUDE_DIR ${INSTALL_DIR}/include)
    set(LibArchive_LIBRARIES ${INSTALL_DIR}/lib/libarchive.a)
    list (APPEND ${INCLUDE_DIRECTORIES} ${LibArchive_INCLUDE_DIR})
    set(LibArchive_FOUND TRUE)
else(BUILD_EXTERNAL_LIBARCHIVE)
    # @TODO optionally prefer static linking here?
    find_package(LibArchive REQUIRED)
endif(BUILD_EXTERNAL_LIBARCHIVE)
if(LibArchive_FOUND)
    message("Linking to libarchive ${LibArchive_LIBRARIES}")
else(LibArchive_FOUND)
    message(FATAL_ERROR "Could not find the libarchive library and development files. Either install them for your OS\
    or rerun cmake with -DBUILD_EXTERNAL_LIBARCHIVE=ON")
endif( LibArchive_FOUND )
