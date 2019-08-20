# - Try to find vorbis
# Once done this will define
#
# VORBIS_FOUND - system has libvorbis
# VORBIS_INCLUDE_DIRS - the libvorbis include directory
# VORBIS_LIBRARIES - The libvorbis libraries

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_VORBIS vorbis QUIET)
endif()

find_path(VORBIS_INCLUDE_DIRS vorbis/vorbisenc.h
                              PATHS ${PC_VORBIS_INCLUDEDIR})
find_library(VORBIS_LIBRARIES vorbis
                              PATHS ${PC_VORBIS_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vorbis REQUIRED_VARS VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)

mark_as_advanced(VORBIS_INCLUDE_DIRS VORBIS_LIBRARIES)
