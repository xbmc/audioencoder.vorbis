project(audioencoder.vorbis)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)
if(NOT WIN32)
  find_package(VorbisEnc REQUIRED)
endif()

set(INCLUDE_DIRS ${KODI_INCLUDE_DIR}
                 ${OGG_INCLUDE_DIRS}
                 ${VORBIS_INCLUDE_DIRS})
if(NOT WIN32)
  list(APPEND INCLUDE_DIRS ${VORBISENC_INCLUDE_DIRS})
endif()

include_directories(${INCLUDE_DIRS})

set(VORBIS_SOURCES src/EncoderVorbis.cpp)

set(DEPLIBS ${OGG_LIBRARIES}
            ${VORBIS_LIBRARIES})
if(NOT WIN32)
  list(APPEND DEPLIBS ${VORBISENC_LIBRARIES})
endif()

build_addon(audioencoder.vorbis VORBIS DEPLIBS)

include(CPack)
