# - Try to find vorbisenc
# Once done this will define
#
# VORBISENC_FOUND - system has libvorbisenc
# VORBISENC_INCLUDE_DIRS - the libvorbisenc include directory
# VORBISENC_LIBRARIES - The libvorbisenc libraries

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_VORBISENC vorbisenc QUIET)
endif()

find_path(VORBISENC_INCLUDE_DIRS vorbis/vorbisenc.h
                                 PATHS ${PC_VORBISENC_INCLUDEDIR})
find_library(VORBISENC_LIBRARIES vorbisenc
                                 PATHS ${PC_VORBISENC_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VorbisEnc REQUIRED_VARS VORBISENC_INCLUDE_DIRS VORBISENC_LIBRARIES)

mark_as_advanced(VORBISENC_INCLUDE_DIRS VORBISENC_LIBRARIES)
