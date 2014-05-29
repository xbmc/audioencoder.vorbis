project(audioencoder.vorbis)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(xbmc REQUIRED)
find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)
find_package(VorbisEnc REQUIRED)

include_directories(${XBMC_INCLUDE_DIR}
                    ${OGG_INCLUDE_DIRS}
                    ${VORBIS_INCLUDE_DIRS}
                    ${VORBISENC_INCLUDE_DIRS})

set(VORBIS_SOURCES src/EncoderVorbis.cpp)

set(DEPLIBS ${VORBIS_LIBRARIES}
            ${VORBISENC_LIBRARIES}
            ${OGG_LIBRARIES})

build_addon(audioencoder.vorbis VORBIS DEPLIBS)

include(CPack)
